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

#define _GNU_SOURCE /* setresuid() and setresgid() */

#include "jail_run.h"
#include "jail_conf.h"
#include "jail_rules.h"

#include <gutil_log.h>
#include <gutil_strv.h>

#include <glib-unix.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <sys/fsuid.h>

static const char FIREJAIL_QUIET_OPT[] = "--quiet";
static const char FIREJAIL_DEBUG_OPT[] = "--debug";
static const char FIREJAIL_PROFILE_OPT[] = "--profile=";
static const char FIREJAIL_WHITELIST_OPT[] = "--whitelist=";
static const char FIREJAIL_BLACKLIST_OPT[] = "--blacklist=";

static const char FIREJAIL_DBUS_USER_FILTER[] = "--dbus-user=filter";
static const char FIREJAIL_DBUS_USER_LOG[] = "--dbus-user.log";
static const char FIREJAIL_DBUS_USER_SEE[] = "--dbus-user.see=";
static const char FIREJAIL_DBUS_USER_TALK[] = "--dbus-user.talk=";
static const char FIREJAIL_DBUS_USER_OWN[] = "--dbus-user.own=";

static const char FIREJAIL_DBUS_SYSTEM_FILTER[] = "--dbus-system=filter";
static const char FIREJAIL_DBUS_SYSTEM_LOG[] = "--dbus-system.log";
static const char FIREJAIL_DBUS_SYSTEM_SEE[] = "--dbus-system.see=";
static const char FIREJAIL_DBUS_SYSTEM_TALK[] = "--dbus-system.talk=";
static const char FIREJAIL_DBUS_SYSTEM_OWN[] = "--dbus-system.own=";

static const char FIREJAIL_FINISH_OPT[] = "--";

static const char PRIVILEGED_GROUP[] = "privileged";

static
guint
jail_ptrv_length(
    gconstpointer v)
{
    if (G_LIKELY(v)) {
        guint n = 0;
        const void* const* ptr = v;

        while (*ptr++) n++;
        return n;
    } else {
        return 0;
    }
}

static
void
jail_run_add_dbus_opts(
    GPtrArray* args,
    GPtrArray* args_alloc,
    const JailDBus* dbus,
    const char* own_opt,
    const char* see_opt,
    const char* talk_opt)
{
    const JailDBusName* const* ptr;

    ptr = dbus->own;
    if (ptr) {
        while (*ptr) {
            const JailDBusName* dbus = *ptr++;
            char* opt = g_strconcat(own_opt, dbus->name, NULL);

            g_ptr_array_add(args, opt);
            g_ptr_array_add(args_alloc, opt);
        }
    }

    /* Talking implies seeing */
    ptr = dbus->talk;
    if (ptr) {
        while (*ptr) {
            const JailDBusName* dbus = *ptr++;
            char* opt1 = g_strconcat(see_opt, dbus->name, NULL);
            char* opt2 = g_strconcat(talk_opt, dbus->name, NULL);

            g_ptr_array_add(args, opt1);
            g_ptr_array_add(args, opt2);
            g_ptr_array_add(args_alloc, opt1);
            g_ptr_array_add(args_alloc, opt2);
        }
    }
}

void
jail_run(
    int argc,
    char* argv[],
    const JailConf* conf,
    const JailRules* rules,
    const JailCreds* creds,
    GError** error)
{
    /*
     * Extra entries:
     *
     * 1. firejail
     * 2. --quiet or --debug (optional)
    * ... options
     * 3. --
     * ... program name and arguments
     * 4. NULL terminator
     *
     * And each path entry produces 2 command line options.
     * D-Bus rules may cause additional options to be added.
     */
    const JailPath* const* path_ptr;
    const JailPermit* const* perm_ptr;
    const JailProfile* const* pro_ptr;
    gid_t egid = creds->egid;
    guint i;
    GPtrArray* args = g_ptr_array_new(); /* All arguments */
    GPtrArray* args_alloc = g_ptr_array_new(); /* Only the allocated ones */
    const JailDBus* dbus_user = rules->dbus_user;
    const JailDBus* dbus_system = rules->dbus_system;

    g_ptr_array_set_free_func(args_alloc, g_free);

    /* 1. firejail */
    g_ptr_array_add(args, (gpointer) conf->exec);

    /* 2. --quiet (optional) */
    if (gutil_log_default.level < GLOG_LEVEL_DEBUG) {
        g_ptr_array_add(args, (gpointer) FIREJAIL_QUIET_OPT);
    } else if (gutil_log_default.level > GLOG_LEVEL_DEBUG) {
        g_ptr_array_add(args, (gpointer) FIREJAIL_DEBUG_OPT);
    }

    /* Profiles */
    for (pro_ptr = rules->profiles; *pro_ptr; pro_ptr++) {
        char* opt = g_strconcat(FIREJAIL_PROFILE_OPT, (*pro_ptr)->path, NULL);

        g_ptr_array_add(args, opt);
        g_ptr_array_add(args_alloc, opt);
    }

    /* Files and directories */
    for (path_ptr = rules->paths; *path_ptr; path_ptr++) {
        const JailPath* jp = *path_ptr;
        char* opt = g_strconcat(jp->allow ? FIREJAIL_WHITELIST_OPT :
            FIREJAIL_BLACKLIST_OPT, jp->path, NULL);

        g_ptr_array_add(args, opt);
        g_ptr_array_add(args_alloc, opt);
    }

    /* D-Bus names and interfaces, session bus */
    if (jail_ptrv_length(dbus_user->own) ||
        jail_ptrv_length(dbus_user->talk)) {
        g_ptr_array_add(args, (gpointer) FIREJAIL_DBUS_USER_FILTER);
        if (gutil_log_default.level > GLOG_LEVEL_DEBUG) {
            g_ptr_array_add(args, (gpointer) FIREJAIL_DBUS_USER_LOG);
        }
        jail_run_add_dbus_opts(args, args_alloc, dbus_user,
            FIREJAIL_DBUS_USER_OWN,
            FIREJAIL_DBUS_USER_SEE,
            FIREJAIL_DBUS_USER_TALK);
    }

    /* D-Bus names and interfaces, system bus */
    if (jail_ptrv_length(dbus_system->own) ||
        jail_ptrv_length(dbus_system->talk)) {
        g_ptr_array_add(args, (gpointer) FIREJAIL_DBUS_SYSTEM_FILTER);
        if (gutil_log_default.level > GLOG_LEVEL_DEBUG) {
            g_ptr_array_add(args, (gpointer) FIREJAIL_DBUS_SYSTEM_LOG);
        }
        jail_run_add_dbus_opts(args, args_alloc, dbus_system,
            FIREJAIL_DBUS_SYSTEM_OWN,
            FIREJAIL_DBUS_SYSTEM_SEE,
            FIREJAIL_DBUS_SYSTEM_TALK);
    }

    /* 3. Done with firejail options */
    g_ptr_array_add(args, (gpointer) FIREJAIL_FINISH_OPT);

    /* Append the program name and its arguments */
    for (i = 0; i < argc; i++) {
        g_ptr_array_add(args, argv[i]);
    }

    /* 4. NULL-terminate the array */
    g_ptr_array_add(args, NULL);

    /* Dump the full command line if debug log is enabled */
#ifdef GUTIL_LOG_DEBUG
    if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
        const char* const* ptr = (const char**) args->pdata;
        GString* tmp = g_string_new(*ptr++);

        while (*ptr) g_string_append_printf(tmp, " %s", *ptr++);
        GDEBUG("%s", tmp->str);
        g_string_free(tmp, TRUE);
    }
#endif /* GUTIL_LOG_DEBUG */

    /* Handle special privileges */
    for (perm_ptr = rules->permits; *perm_ptr; perm_ptr++) {
        if ((*perm_ptr)->type == JAIL_PERMIT_PRIVILEGED) {
            const char* group = PRIVILEGED_GROUP;
            const struct group* gr = getgrnam(group);

            if (gr) {
                egid = gr->gr_gid;
                GDEBUG("Setting effective GID to %s (%u)", group, (uint)egid);
            } else {
                GWARN("Group '%s' is missing", group);
            }
        }
    }

    setfsuid(creds->euid);
    setfsgid(egid);

    /* FIXME: In case of privileged application we want to
     *        use egid=privileged, but as firejail seems
     *        to have problems preserving that, for now both
     *        real and effective gid are set to privileged.
     */
    if (setgroups(creds->ngroups, creds->groups)) {
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "setgroups error: %s", strerror(errno)));
    } else if (setresgid(egid, egid, creds->rgid)) {
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "setresgid(%u,%u,%u) error: %s", (guint)creds->rgid,
            (guint)egid, (guint)creds->sgid, strerror(errno)));
    } else if (setresuid(creds->ruid, creds->euid, creds->suid)) {
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "setresuid(%u,%u,%u) error: %s", (guint)creds->ruid,
            (guint)creds->euid, (guint)creds->suid, strerror(errno)));
    } else {
        fflush(NULL);
        execvp(conf->exec, (char**) args->pdata);
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "exec(%s) error: %s", conf->exec, strerror(errno)));
    }

    g_ptr_array_free(args, TRUE);
    g_ptr_array_free(args_alloc, TRUE);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
