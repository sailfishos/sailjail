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
#include "jail_creds.h"
#include "jail_conf.h"
#include "jail_free.h"
#include "jail_run.h"

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
    char* profile;
    char* section;
    char* sailfish_app;
    char* trace_dir;
} JailArgs;

static int verbose_level = 0;

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
    JailArgs* args = data;
    g_free(args->trace_dir);
    args->trace_dir = value ? g_strdup(value) : g_get_current_dir();
    if (!g_file_test(args->trace_dir, G_FILE_TEST_IS_DIR)) {
        GWARN("%s: is not a directory", args->trace_dir);
        return FALSE;
    }
    return TRUE;
}

static
void
jail_args_clear(
    JailArgs* args)
{
    g_free(args->profile);
    g_free(args->section);
    g_free(args->sailfish_app);
    g_free(args->trace_dir);
    memset(args, 0, sizeof(*args));
}

static
GOptionContext *
jail_opt_context_new(
    const GOptionEntry* entries,
    JailArgs* args)
{
    GOptionContext* options = g_option_context_new("PROGRAM [ARGS...]");

    GOptionGroup* group = g_option_group_new (NULL, NULL, NULL, args, NULL);
    g_option_group_add_entries(group, entries);
    g_option_context_set_main_group(options, group);

    g_option_context_set_strict_posix(options, TRUE);
    g_option_context_set_summary(options, "Runs PROGRAM in a sandbox.");
    return options;
}

#if HAVE_FIREJAIL

static
int
sailjail_sandbox(
    int argc,
    char* argv[],
    const JailConf* conf,
    const JailArgs* args,
    const JailCreds* creds)
{
    int ret = RET_ERR;
    SailJail jail;
    JailConf actual_conf = *conf;
    JailLaunchHooks* hooks = jail_launch_hooks_new();
    JailPlugins* plugins;

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
            JailApp app;
            JailCmdLine cmd;
            JailRunUser user;
            JailRules* confirm;

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
                jail_run(argc-1, argv+1, conf, confirm, creds, args->trace_dir,
                    &error);
                jail_rules_unref(confirm);
                ret = RET_EXEC;
            } else {
                /* Launch denied */
                jail_launch_denied(hooks, &app, &cmd, &user);
                ret = RET_DENIED;
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

#endif

static
int
sailjail_main(
    int argc,
    char* argv[],
    const JailConf* conf,
    const JailArgs* args)
{
    int ret = RET_ERR;
    GError* error = NULL;
    const pid_t ppid = getppid();
    JailCreds* creds = jail_creds_for_pid(ppid, &error);

    if (creds) {
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
                g_string_append_printf(buf, " %u", (guint) creds->groups[i]);
            }
            GDEBUG("  Groups:%s", buf->str);
            g_string_free(buf, TRUE);
        }
#endif

#if HAVE_FIREJAIL
        if (!conf->passthrough) {
            ret = sailjail_sandbox(argc, argv, conf, args, creds);
        } else
#endif
        {
            /* Any return from jail_free is an error */
            jail_free(argc-1, argv+1, creds, &error);
            ret = RET_EXEC;
        }
        jail_creds_free(creds);
    }
    if (error) {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    return ret;
}

int main(int argc, char* argv[])
{
    JailConf* conf = jail_conf_new(); /* Default config */
    gboolean ok;
    int ret = RET_CMDLINE;
    JailArgs args;
    GError* error = NULL;
    GOptionContext* options;
    GOptionEntry entries[] = {
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
          G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, jail_arg_trace,
          "Enable libtrace and dbus proxy logging", "DIR" },
        { NULL }
    };

    /* Set up logging */
    gutil_log_default.name = "sailjail";
    gutil_log_default.level = GLOG_LEVEL_DEFAULT;
    gutil_log_timestamp = FALSE;

    /* The config file may be (and usually is) missing */
    if (g_file_test(DEFAULT_CONF_FILE, G_FILE_TEST_EXISTS)) {
        jail_conf_load(conf, DEFAULT_CONF_FILE, NULL);
    }

    /* Parse the command line */
    memset(&args, 0, sizeof(args));
    options = jail_opt_context_new(entries, &args);
    ok = g_option_context_parse(options, &argc, &argv, &error);

    if (ok) {
        /* Remove "--" from the list if it is there */
        if (argc > 1 && !strcmp(argv[1], "--")) {
            argv[1] = argv[0];
            argv++;
            argc--;
        }
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
