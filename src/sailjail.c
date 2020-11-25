/*
 * Copyright (c) 2020 Open Mobile Platform LLC.
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#include "jail_rules_p.h"
#include "jail_plugins.h"
#include "jail_launch.h"
#include "jail_conf.h"
#include "jail_run.h"

#include <gutil_intarray.h>
#include <gutil_ints.h>
#include <gutil_misc.h>
#include <gutil_log.h>

#include <sys/types.h>
#include <unistd.h>

#if !GLIB_CHECK_VERSION(2,42,0)
#  define G_OPTION_FLAG_NONE 0
#endif

#if !GLIB_CHECK_VERSION(2,44,0)
/*
 * g_option_context_set_ignore_unknown_options() is definitely not
 * a replacement for g_option_context_set_strict_posix() but at least
 * it's a chance to test on a platform with old glib. It's not a problem
 * with Sailfish OS which requires a much newer version of glib.
 */
#  define g_option_context_set_strict_posix(x, y) \
    g_option_context_set_ignore_unknown_options(x,y)
#endif

#define DEFAULT_CONF_FILE "/etc/sailjail.conf"

#define RET_CMDLINE 1
#define RET_CONFIG 2
#define RET_DENIED 3
#define RET_ERR 4
#define RET_EXEC 5

typedef struct jail_args {
    char* conf_file;
    char* plugin_dir;
    char* profile;
    char* section;
    char* sailfish_app;
} JailArgs;

static int verbose_level = 0;

static
GUtilInts*
jail_parse_ids(
    const char* status,
    gsize len,
    const char* prefix)
{
    /* Find the prefix */
    const char* ptr = g_strstr_len(status, len, prefix);
    GUtilInts* result = NULL;

    if (ptr) {
        gboolean ok = TRUE;
        GString* buf = g_string_sized_new(7);
        GUtilIntArray* parsed = gutil_int_array_new();
        const char* end;

        /* Skip the prefix */
        ptr += strlen(prefix);

        /* Find the end of line */
        len -= ptr - status;
        end = g_strstr_len(ptr, len, "\n");
        if (!end) end = ptr + len;

        /* Skip the spaces */
        while (ptr < end && g_ascii_isspace(*ptr)) ptr++;

        /* Read the numbers */
        while (ptr < end) {
            int val;

            /* Read and parse the number (only expecting positives) */
            g_string_set_size(buf, 0);
            while (ptr < end && g_ascii_isdigit(*ptr)) {
                g_string_append_c(buf, *ptr++);
            }
            if (gutil_parse_int(buf->str, 0, &val) && val >= 0) {
                /* Store the value, skip the spaces and continue */
                gutil_int_array_append(parsed, val);
                while (ptr < end && g_ascii_isspace(*ptr)) ptr++;
                continue;
            }
            /* Bummer */
            ok = FALSE;
            break;
        }

        /* Expecting at least one number */
        if (ok && parsed->count > 0) {
            result = gutil_int_array_free_to_ints(parsed);
        } else {
            gutil_int_array_free(parsed, TRUE);
        }
        g_string_free(buf, TRUE);
    }
    return result;
}

static
JailCreds*
jail_get_creds(
    pid_t pid,
    GError** error)
{
    char* fname = g_strdup_printf("/proc/%u/status", (guint)pid);
    JailCreds* creds = NULL;
    char* status;
    gsize len;

    if (g_file_get_contents(fname, &status, &len, error)) {
        guint nuids, ngids, ngroups;
        GUtilInts* puids = jail_parse_ids(status, len, "\nUid:");
        GUtilInts* pgids = jail_parse_ids(status, len, "\nGid:");
        GUtilInts* pgroups = jail_parse_ids(status, len, "\nGroups:");
        const int* uids = gutil_ints_get_data(puids, &nuids);
        const int* gids = gutil_ints_get_data(pgids, &ngids);
        const int* groups = gutil_ints_get_data(pgroups, &ngroups);

        if (nuids == 4 && ngids == 4 && ngroups > 0) {
            gid_t* cred_groups;
            guint i;

            creds = g_malloc(sizeof(*creds) + ngroups * sizeof(gid_t));
            cred_groups = (gid_t*)(creds + 1);
            for (i = 0; i < ngroups; i++) {
                cred_groups[i] = groups[i];
            }

            creds->ruid = uids[0];
            creds->euid = uids[1];
            creds->suid = uids[2];
            creds->fsuid = uids[3];
            creds->rgid = gids[0];
            creds->egid = gids[1];
            creds->sgid = gids[2];
            creds->fsgid = gids[3];
            creds->groups = cred_groups;
            creds->ngroups = ngroups;
        }
        gutil_ints_unref(puids);
        gutil_ints_unref(pgids);
        gutil_ints_unref(pgroups);
    }
    g_free(fname);
    return creds;
}

static
gboolean
jail_arg_log_output(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    if (gutil_log_set_type(value, NULL)) {
        return TRUE;
    } else {
        g_propagate_error(error, g_error_new(G_OPTION_ERROR,
            G_OPTION_ERROR_BAD_VALUE, "Invalid log type \'%s\'", value));
        return FALSE;
    }
}

static
gboolean
jail_arg_verbose(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    verbose_level++;
    gutil_log_default.level = (verbose_level < 2) ?
        GLOG_LEVEL_DEBUG : GLOG_LEVEL_VERBOSE;
    return TRUE;
}

static
gboolean
jail_arg_quiet(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    gutil_log_default.level = GLOG_LEVEL_NONE;
    return TRUE;
}

static
gboolean
jail_arg_trace(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    jail_run_enable_trace();
    return TRUE;
}

static
void
jail_args_clear(
    JailArgs* args)
{
    g_free(args->conf_file);
    g_free(args->plugin_dir);
    g_free(args->profile);
    g_free(args->section);
    g_free(args->sailfish_app);
    memset(args, 0, sizeof(*args));
}

static
GOptionContext *
jail_opt_context_new(
    const GOptionEntry* entries)
{
    GOptionContext* options = g_option_context_new("PROGRAM [ARGS...]");

    g_option_context_add_main_entries(options, entries, NULL);
    g_option_context_set_strict_posix(options, TRUE);
    g_option_context_set_summary(options, "Runs PROGRAM in a sandbox.");
    return options;
}

static
int
sailjail_main(
    int argc,
    char* argv[],
    const JailConf* conf,
    const JailArgs* args)
{
    int ret = RET_ERR;
    SailJail jail;
    JailConf actual_conf = *conf;
    JailLaunchHooks* hooks = jail_launch_hooks_new();
    JailPlugins* plugins;

    /* Directory specified on the command line overrides the config */
    if (args->plugin_dir) {
        actual_conf.plugin_dir = args->plugin_dir;
    }

    memset(&jail, 0, sizeof(jail));
    jail.hooks = hooks;
    jail.conf = &actual_conf;

    /* Load the plugins */
    plugins = jail_plugins_new(actual_conf.plugin_dir, NULL, NULL);
    if (jail_plugins_start(plugins, &jail)) {
        GError* error = NULL;
        char* profile = NULL;
        char* section = NULL;
        JailRules* rules;
        JailRulesOpt opt;

        memset(&opt, 0, sizeof(opt));
        opt.profile = args->profile;
        opt.section = args->section;
        opt.sailfish_app = args->sailfish_app;
        rules = jail_rules_new(argv[1], conf, &opt, &profile, &section, &error);
        if (rules) {
            const pid_t ppid = getppid();
            JailCreds* creds = jail_get_creds(ppid, &error);

            if (creds) {
                JailApp app;
                JailCmdLine cmd;
                JailRunUser user;
                JailRules* confirm;

#if GUTIL_LOG_DEBUG
                GDEBUG("Parent PID: %u", (guint)ppid);
                GDEBUG("  rUID:%u eUID:%u sUID:%u fsUID:%u",
                    (guint)creds->ruid, (guint)creds->euid,
                    (guint)creds->suid, (guint)creds->fsuid);
                GDEBUG("  rGID:%u eGID:%u sGID:%u fsGID:%u",
                    (guint)creds->rgid, (guint)creds->egid,
                    (guint)creds->sgid, (guint)creds->fsgid);
                if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                    GString* buf = g_string_new(NULL);
                    guint i;

                    for (i = 0; i < creds->ngroups; i++) {
                        g_string_append_printf(buf, " %u", (guint)
                            creds->groups[i]);
                    }
                    GDEBUG("  Groups:%s", buf->str);
                    g_string_free(buf, TRUE);
                }
#endif

                memset(&app, 0, sizeof(app));
                app.file = profile;
                app.section = section;

                memset(&cmd, 0, sizeof(cmd));
                cmd.argc = argc - 1;
                cmd.argv = (const char**)argv + 1;

                memset(&user, 0, sizeof(user));
                user.euid = creds->euid;
                user.egid = creds->egid;
                user.groups = creds->groups;
                user.ngroups = creds->ngroups;

                /* Confirm the launch */
                confirm = jail_launch_confirm(hooks, &app, &cmd, &user, rules);
                if (confirm) {
                    /* Any return from jail_run is an error */
                    jail_launch_confirmed(hooks, &app, &cmd, &user, confirm);
                    jail_run(argc-1, argv+1, conf, confirm, creds, &error);
                    jail_rules_unref(confirm);
                    ret = RET_EXEC;
                } else {
                    /* Launch denied */
                    jail_launch_denied(hooks, &app, &cmd, &user);
                    ret = RET_DENIED;
                }
                g_free(creds);
            }
            jail_rules_unref(rules);
        } else {
            ret = RET_CONFIG;
        }
        if (error) {
            GERR("%s", GERRMSG(error));
            g_error_free(error);
        }
        g_free(profile);
        g_free(section);
        jail_plugins_stop(plugins);
    }

    jail_plugins_free(plugins);
    jail_launch_hooks_free(hooks);
    return ret;
}

int main(int argc, char* argv[])
{
    JailConf* conf = jail_conf_new(); /* Default config */
    gboolean ok;
    int ret = RET_CMDLINE;
    JailArgs args;
    GError* error = NULL;
    int pre_argc;
    char** pre_argv;
    GOptionContext* options;
    GOptionEntry entries[] = {
        { "config", 'c',
          G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, &args.conf_file,
          "Sailjail config file [" DEFAULT_CONF_FILE "]", "FILE" },
        { "plugin-dir", 'd', 0, G_OPTION_ARG_FILENAME, &args.plugin_dir,
          "Plugin directory [" DEFAULT_PLUGIN_DIR "]", "DIR" },
        { "profile", 'p',
          G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, &args.profile,
          "Application profile", "FILE" },
        { "section", 's',
          G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args.section,
          "Sailjail section in the profile [" DEFAULT_PROFILE_SECTION "]",
          "NAME" },
        { "app", 'a',
          G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args.sailfish_app,
          "Force adding Sailfish application directories", "APP" },
        { "output", 'o',
          G_OPTION_FLAG_NONE, G_OPTION_ARG_CALLBACK, jail_arg_log_output,
          "Where to output log (stdout|syslog|glib) [stdout]", "OUT" },
        { "verbose", 'v',
          G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, jail_arg_verbose,
          "Enable verbose logging (repeatable)", NULL },
        { "quiet", 'q',
          G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, jail_arg_quiet,
          "Disable all sailjail output", NULL },
        { "trace", 't',
          G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, jail_arg_trace,
          "Enable libtrace and dbus proxy logging", NULL },
        { NULL }
    };

    /* Set up logging */
    gutil_log_default.name = "sailjail";
    gutil_log_default.level = GLOG_LEVEL_DEFAULT;
    gutil_log_timestamp = FALSE;

    /*
     * First pre-parse the command line to get the config file name.
     * Let it damage the temporarary list of arguments. We will the
     * real one later, when we actually parse the command line.
     */
    memset(&args, 0, sizeof(args));
    options = jail_opt_context_new(entries);
    pre_argc = argc;
    pre_argv = g_memdup(argv, sizeof(argv[0]) * argc);
    ok = g_option_context_parse(options, &pre_argc, &pre_argv, &error);
    g_free(pre_argv);

    /*
     * If pre-parsing succeeds, we need to read the config before
     * parsing the rest of the command line, to allow command line
     * options to overwrite those specified in the config file.
     */
    if (ok) {
        if (args.conf_file) {
            /* Config file was specified on the command line */
            ok = jail_conf_load(conf, args.conf_file, &error);
        } else {
            /* The default config file may be (and usually is) missing */
            if (g_file_test(DEFAULT_CONF_FILE, G_FILE_TEST_EXISTS)) {
                jail_conf_load(conf, DEFAULT_CONF_FILE, NULL);
            }
        }

        if (ok) {
            /* Allocate new parser */
            g_option_context_free(options);

            /* Reset the state */
            jail_args_clear(&args);
            verbose_level = 0;
            gutil_log_default.level = GLOG_LEVEL_DEFAULT;

            /* Re-parse the command line */
            options = jail_opt_context_new(entries);
            ok = g_option_context_parse(options, &argc, &argv, &error);
        } else if (error) {
            /* Improve error message by prepending the file name */
            GError* details = g_error_new(error->domain, error->code,
                "%s: %s", args.conf_file, error->message);

            g_error_free(error);
            error = details;
        }
    }

    if (ok) {
        if (argc > 1) {
            ret = sailjail_main(argc, argv, conf, &args);
        } else {
            char* help = g_option_context_get_help(options, TRUE, NULL);

            fprintf(stderr, "%s", help);
            g_free(help);
        }
    }

    if (error) {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    jail_args_clear(&args);
    g_option_context_free(options);
    jail_conf_free(conf);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
