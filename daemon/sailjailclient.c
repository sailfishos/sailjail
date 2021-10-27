/*
 * Copyright (c) 2021 Open Mobile Platform LLC.
 * Copyright (c) 2021 Jolla Ltd.
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

#include "config.h"
#include "logging.h"
#include "util.h"
#include "service.h"
#include "stringset.h"

#include <sys/stat.h>

#include <pwd.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <fnmatch.h>

#include <gio/gio.h>

#ifdef UNITTEST
#include "test/sailjailclient_wrapper.c"
#endif

/* ========================================================================= *
 * Constants
 * ========================================================================= */

#define LAUNCHNOTIFY_SERVICE                    "org.nemomobile.lipstick"
#define LAUNCHNOTIFY_OBJECT                     "/LauncherModel"
#define LAUNCHNOTIFY_INTERFACE                  "org.nemomobile.lipstick.LauncherModel"
#define LAUNCHNOTIFY_METHOD_LAUNCH_CANCELED     "cancelNotifyLaunching"

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct client_t client_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static bool empty_p        (const char *str);
static bool path_dirname_eq(const char *path, const char *target);

/* ------------------------------------------------------------------------- *
 * CLIENT
 * ------------------------------------------------------------------------- */

static void  client_ctor     (client_t *self);
static void  client_dtor     (client_t *self);
client_t    *client_create   (void);
void         client_delete   (client_t *self);
void         client_delete_at(client_t **pself);
void         client_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * CLIENT_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static GDBusConnection  *client_system_bus                      (client_t *self);
static GDBusConnection  *client_session_bus                     (client_t *self);
const char              *client_desktop_exec                    (client_t *self);
const char              *client_sailjail_exec_dbus              (client_t *self);
const char              *client_sailjail_organization_name      (client_t *self);
const char              *client_sailjail_application_name       (client_t *self);
const char              *client_sailjail_data_directory         (client_t *self);
const char             **client_sailjail_application_permissions(client_t *self);
const char              *client_maemo_service                   (client_t *self);
const char              *client_maemo_method                    (client_t *self);
const char              *client_mode                            (client_t *self);

/* ------------------------------------------------------------------------- *
 * CLIENT_PROPERTIES
 * ------------------------------------------------------------------------- */

static bool          client_is_privileged      (const client_t *self);
static void          client_set_privileged     (client_t *self, bool is_privileged);
static const gchar **client_get_argv           (const client_t *self, int *pargc);
static void          client_set_argv           (client_t *self, int argc, char **argv);
static const gchar **client_get_granted        (const client_t *self);
static void          client_set_granted        (client_t *self, gchar **granted);
static const gchar  *client_get_desktop1_path  (const client_t *self);
static void          client_set_desktop1_path  (client_t *self, const char *path);
static const gchar  *client_get_desktop2_path  (const client_t *self);
static void          client_set_desktop2_path  (client_t *self, const char *path);
static bool          client_get_debug_mode     (const client_t *self);
static void          client_set_debug_mode     (client_t *self, bool debug_mode);
static bool          client_get_dry_run        (const client_t *self);
static void          client_set_dry_run        (client_t *self, bool dry_run);
static const gchar  *client_get_trace_dir      (const client_t *self);
static void          client_set_trace_dir      (client_t *self, const char *path);
static void          client_set_appinfo_variant(client_t *self, const char *key, GVariant *val);
static GVariant     *client_get_appinfo_variant(const client_t *self, const char *key);
const char          *client_get_appinfo_string (const client_t *self, const char *key);
const char         **client_get_appinfo_strv   (const client_t *self, const char *key);

/* ------------------------------------------------------------------------- *
 * CLIENT_OPTIONS
 * ------------------------------------------------------------------------- */

static const GList *client_get_firejail_options   (const client_t *self);
static void         client_add_firejail_option    (client_t *self, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void         client_add_firejail_permission(client_t *self, const char *name);
static void         client_add_firejail_profile   (client_t *self, const char *name);
static void         client_add_firejail_directory (client_t *self, bool create, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/* ------------------------------------------------------------------------- *
 * CLIENT_IPC
 * ------------------------------------------------------------------------- */

static bool client_prompt_permissions(client_t *self, const char *application);
static bool client_query_appinfo     (client_t *self, const char *application);

/* ------------------------------------------------------------------------- *
 * CLIENT_LAUNCH
 * ------------------------------------------------------------------------- */

static int client_launch_application(client_t *self);

/* ------------------------------------------------------------------------- *
 * CLIENT_NOTIFY
 * ------------------------------------------------------------------------- */

static void client_notify_launch_status  (client_t *self, const char *method, const char *desktop);
static void client_notify_launch_canceled(client_t *self, const char *desktop);

/* ------------------------------------------------------------------------- *
 * SAILJAILCLIENT
 * ------------------------------------------------------------------------- */

static int  sailjailclient_get_field_code(const char *arg);
static bool sailjailclient_is_option     (const char *arg);
static bool sailjailclient_ignore_arg    (const char *arg);
static bool sailjailclient_match_argv    (const char **tpl_argv, const char **app_argv);
static bool sailjailclient_validate_argv (const char *exec, const gchar **app_argv, bool use_compatibility);
static bool sailjailclient_test_elf      (const char *filename);
static void sailjailclient_print_usage   (const char *progname);
static bool sailjailclient_binary_check  (const char *binary_path);
int         sailjailclient_main          (int argc, char **argv);

/* ------------------------------------------------------------------------- *
 * MAIN
 * ------------------------------------------------------------------------- */

int main(int argc, char **argv);

/* ========================================================================= *
 * UTILITY
 * ========================================================================= */

static bool empty_p(const char *str)
{
    return !str || !*str;
}

static bool path_dirname_eq(const char *path, const char *target)
{
    gchar *dir_path = path_dirname(path);
    bool result = !strcmp(dir_path, target);
    g_free(dir_path);
    return result;
}

/* ========================================================================= *
 * CLIENT
 * ========================================================================= */

struct client_t
{
    int              cli_argc;
    gchar          **cli_argv;
    gchar           *cli_desktop1_path;
    gchar           *cli_desktop2_path;
    gchar           *cli_trace_dir;
    bool             cli_debug_mode;
    bool             cli_dry_run;
    GDBusConnection *cli_system_bus;
    GDBusConnection *cli_session_bus;
    gchar          **cli_granted;
    GHashTable      *cli_appinfo;

    stringset_t     *cli_firejail_args; /* Note: using ordered set allows us
                                         *       not to care about possibly
                                         *       adding duplicate options.
                                         */
    bool            cli_is_privileged;
};

static void
client_ctor(client_t *self)
{
    self->cli_argc          = 0;
    self->cli_argv          = NULL;
    self->cli_desktop1_path = NULL;
    self->cli_desktop2_path = NULL;
    self->cli_trace_dir     = NULL;
    self->cli_debug_mode    = false;
    self->cli_dry_run       = false;
    self->cli_system_bus    = NULL;
    self->cli_session_bus   = NULL;
    self->cli_granted       = NULL;
    self->cli_appinfo       = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify)g_variant_unref);
    self->cli_firejail_args = stringset_create();
    self->cli_is_privileged = false;
}

static void
client_dtor(client_t *self)
{
    stringset_delete_at(&self->cli_firejail_args);

    g_hash_table_unref(self->cli_appinfo),
        self->cli_appinfo = NULL;

    client_set_granted(self, NULL);

    if( self->cli_system_bus ) {
        g_object_unref(self->cli_system_bus),
            self->cli_system_bus = NULL;
    }

    if( self->cli_session_bus ) {
        g_object_unref(self->cli_session_bus),
            self->cli_session_bus = NULL;
    }

    client_set_desktop1_path(self, NULL);
    client_set_desktop2_path(self, NULL);
    client_set_trace_dir(self, NULL);
    client_set_argv(self, 0, NULL);
}

client_t *
client_create(void)
{
    client_t *self = g_malloc0(sizeof *self);
    client_ctor(self);
    return self;
}

void
client_delete(client_t *self)
{
    if( self != 0 )
    {
        client_dtor(self);
        g_free(self);
    }
}

void
client_delete_at(client_t **pself)
{
    client_delete(*pself), *pself = NULL;
}

void
client_delete_cb(void *self)
{
    client_delete(self);
}

/* ------------------------------------------------------------------------- *
 * CLIENT_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static GDBusConnection *
client_system_bus(client_t *self)
{
    if( !self->cli_system_bus ) {
        GError *err = NULL;
        self->cli_system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
        if( !self->cli_system_bus ) {
            log_err("failed to connect to D-Bus SystemBus: %s", err->message);
            exit(EXIT_FAILURE);
        }
        g_clear_error(&err);
    }
    return self->cli_system_bus;
}

static GDBusConnection *
client_session_bus(client_t *self)
{
    if( !self->cli_session_bus ) {
        GError *err = NULL;
        self->cli_session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
        if( !self->cli_session_bus ) {
            log_err("failed to connect to D-Bus SessionBus: %s", err->message);
            exit(EXIT_FAILURE);
        }
        g_clear_error(&err);
    }
    return self->cli_session_bus;
}

const char *
client_desktop_exec(client_t *self)
{
    return client_get_appinfo_string(self, DESKTOP_KEY_EXEC);
}

const char *
client_sailjail_exec_dbus(client_t *self)
{
    return client_get_appinfo_string(self, SAILJAIL_KEY_EXEC_DBUS);
}

const char *
client_sailjail_organization_name(client_t *self)
{
    return client_get_appinfo_string(self, SAILJAIL_KEY_ORGANIZATION_NAME);
}

const char *
client_sailjail_application_name(client_t *self)
{
    return client_get_appinfo_string(self, SAILJAIL_KEY_APPLICATION_NAME);
}

const char *
client_sailjail_data_directory(client_t *self)
{
    return client_get_appinfo_string(self, SAILJAIL_KEY_DATA_DIRECTORY);
}

const char **
client_sailjail_application_permissions(client_t *self)
{
    return client_get_appinfo_strv(self, SAILJAIL_KEY_PERMISSIONS);
}

const char *
client_maemo_service(client_t *self)
{
    return client_get_appinfo_string(self, MAEMO_KEY_SERVICE);
}

const char *
client_maemo_method(client_t *self)
{
    return client_get_appinfo_string(self, MAEMO_KEY_METHOD);
}

const char *
client_mode(client_t *self)
{
    return client_get_appinfo_string(self, "Mode");
}

/* ------------------------------------------------------------------------- *
 * CLIENT_PROPERTIES
 * ------------------------------------------------------------------------- */

static bool
client_is_privileged(const client_t *self)
{
    return self->cli_is_privileged;
}

static void
client_set_privileged(client_t *self, bool is_privileged)
{
    self->cli_is_privileged = is_privileged;
}

static const gchar **
client_get_argv(const client_t *self, int *pargc)
{
    if( pargc )
        *pargc = self->cli_argc;
    return (const gchar **)self->cli_argv;
}

static void
client_set_argv(client_t *self, int argc, char **argv)
{
    self->cli_argc = argc;
    self->cli_argv = argv;
}

static const gchar **
client_get_granted(const client_t *self)
{
    return (const gchar **)self->cli_granted;
}

static void
client_set_granted(client_t *self, gchar **granted)
{
    g_strfreev(self->cli_granted),
        self->cli_granted = granted;
}

static const gchar *
client_get_desktop1_path(const client_t *self)
{
    return self->cli_desktop1_path;
}

static void
client_set_desktop1_path(client_t *self, const char *path)
{
    change_string(&self->cli_desktop1_path, path);
}

static const gchar *
client_get_desktop2_path(const client_t *self)
{
    return self->cli_desktop2_path;
}

static void
client_set_desktop2_path(client_t *self, const char *path)
{
    change_string(&self->cli_desktop2_path, path);
}

static bool
client_get_debug_mode(const client_t *self)
{
    return self->cli_debug_mode;
}

static void
client_set_debug_mode(client_t *self, bool debug_mode)
{
    self->cli_debug_mode = debug_mode;
}

static bool
client_get_dry_run(const client_t *self)
{
    return self->cli_dry_run;
}

static void
client_set_dry_run(client_t *self, bool dry_run)
{
    self->cli_dry_run = dry_run;
}

static const gchar *
client_get_trace_dir(const client_t *self)
{
    return self->cli_trace_dir;
}

static void
client_set_trace_dir(client_t *self, const char *path)
{
    if( path ) {
        struct stat st = {};
        if( access(path, W_OK) == -1 ||
            stat(path, &st) == -1 ||
            (st.st_mode & S_IFMT) != S_IFDIR ) {
            log_warning("%s: is not already existing writable directory",
                        path);
            path = NULL;
        }
    }
    change_string(&self->cli_trace_dir, path);
}

static void
client_set_appinfo_variant(client_t *self, const char *key, GVariant *val)
{
    g_hash_table_insert(self->cli_appinfo, g_strdup(key), g_variant_ref(val));
}

static GVariant *
client_get_appinfo_variant(const client_t *self, const char *key)
{
    return g_hash_table_lookup(self->cli_appinfo, key);
}

const char *
client_get_appinfo_string(const client_t *self, const char *key)
{
    const char *value = NULL;
    GVariant *variant = client_get_appinfo_variant(self, key);
    if( variant ) {
        const GVariantType *type = g_variant_get_type(variant);
        if( g_variant_type_equal(type, G_VARIANT_TYPE_STRING) )
            value = g_variant_get_string(variant, NULL);
    }
    return value;
}

const char **
client_get_appinfo_strv(const client_t *self, const char *key)
{
    const char **value = NULL;
    GVariant *variant = client_get_appinfo_variant(self, key);
    if( variant ) {
        const GVariantType *type = g_variant_get_type(variant);
        if( g_variant_type_equal(type, G_VARIANT_TYPE("as")) )
            value = g_variant_get_strv(variant, NULL);
    }
    return value;
}

/* ------------------------------------------------------------------------- *
 * CLIENT_OPTIONS
 * ------------------------------------------------------------------------- */

static const GList *
client_get_firejail_options(const client_t *self)
{
    return stringset_list(self->cli_firejail_args);
}

static void
client_add_firejail_option(client_t *self, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    gchar *option = g_strdup_vprintf(fmt, va);
    va_end(va);
    stringset_add_item_steal(self->cli_firejail_args, option);
}

static void
client_add_firejail_permission(client_t *self, const char *name)
{
    gchar *path = path_from_permission_name(name);
    if( path && access(path, R_OK) == 0 )
        client_add_firejail_option(self, "--profile=%s", path);
    g_free(path);
}

static void
client_add_firejail_profile(client_t *self, const char *name)
{
    gchar *path = path_from_profile_name(name);
    if( path && access(path, R_OK) == 0 )
        client_add_firejail_option(self, "--profile=%s", path);
    g_free(path);
}

static void
client_add_firejail_directory(client_t *self, bool create, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    gchar *path = g_strdup_vprintf(fmt, va);
    va_end(va);

    if( create )
        client_add_firejail_option(self, "--mkdir=%s", path);
    client_add_firejail_option(self, "--whitelist=%s", path);
    g_free(path);
}

/* ------------------------------------------------------------------------- *
 * CLIENT_IPC
 * ------------------------------------------------------------------------- */

static bool
client_prompt_permissions(client_t *self, const char *application)
{
    GError *err = NULL;
    GVariant *reply = NULL;
    gchar **permissions = NULL;

    reply = g_dbus_connection_call_sync(client_system_bus(self),
                                        PERMISSIONMGR_SERVICE,
                                        PERMISSIONMGR_OBJECT,
                                        PERMISSIONMGR_INTERFACE,
                                        PERMISSIONMGR_METHOD_PROMPT,
                                        g_variant_new("(s)", application),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        G_MAXINT,
                                        NULL,
                                        &err);
    if( err || !reply ) {
        log_err("%s.%s(%s): failed: %s",
                PERMISSIONMGR_INTERFACE, PERMISSIONMGR_METHOD_PROMPT,
                application, err ? err->message : "no reply");
        goto EXIT;
    }

    g_variant_get(reply, "(^as)", &permissions);
    if( !permissions ) {
        log_err("%s.%s(%s): failed: %s",
                PERMISSIONMGR_INTERFACE, PERMISSIONMGR_METHOD_PROMPT,
                application, "invalid reply");
    }

EXIT:
    if( reply )
        g_variant_unref(reply);
    g_clear_error(&err);

    client_set_granted(self, permissions);
    return permissions != NULL;
}

static bool
client_query_appinfo(client_t *self, const char *application)
{
    bool        ack     = false;
    GError     *err     = NULL;
    GVariant   *reply   = NULL;

    reply = g_dbus_connection_call_sync(client_system_bus(self),
                                        PERMISSIONMGR_SERVICE,
                                        PERMISSIONMGR_OBJECT,
                                        PERMISSIONMGR_INTERFACE,
                                        PERMISSIONMGR_METHOD_GET_APPINFO,
                                        g_variant_new("(s)", application),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &err);
    if( err || !reply ) {
        log_err("%s.%s(%s): failed: %s",
                PERMISSIONMGR_INTERFACE, PERMISSIONMGR_METHOD_PROMPT,
                application, err ? err->message : "no reply");
        goto EXIT;
    }
    GVariantIter  *iter_array = NULL;
    g_variant_get(reply, "(a{sv})", &iter_array);
    if( !iter_array )
        goto EXIT;

    const char *key = NULL;
    GVariant   *val = NULL;
    while( g_variant_iter_loop (iter_array, "{&sv}", &key, &val) ) {
        const GVariantType *type = g_variant_get_type(val);
        if( g_variant_type_equal(type, G_VARIANT_TYPE_BOOLEAN) )
            log_debug("%s=%s", key, g_variant_get_boolean(val) ? "true" : "false");
        else if( g_variant_type_equal(type, G_VARIANT_TYPE_STRING) )
            log_debug("%s='%s'", key, g_variant_get_string(val, NULL));
        else
            log_debug("%s=%s@%p", key, (char *)type, val);
        client_set_appinfo_variant(self, key, val);
    }
    g_variant_iter_free(iter_array);

    ack = true;

EXIT:
    if( reply )
        g_variant_unref(reply);
    g_clear_error(&err);

    return ack;
}

/* ------------------------------------------------------------------------- *
 * CLIENT_LAUNCH
 * ------------------------------------------------------------------------- */

static int
client_launch_application(client_t *self)
{
    int               exit_code     = EXIT_FAILURE;
    int               argc          = 0;
    const gchar     **argv          = client_get_argv(self, &argc);
    const gchar      *desktop1_path = client_get_desktop1_path(self);
    const gchar      *desktop2_path = client_get_desktop2_path(self);
    gchar            *desktop_name  = path_to_desktop_name(desktop1_path ?:
                                                           desktop2_path);
    gchar            *booster_path  = NULL;
    const gchar      *booster_name  = NULL;
    gchar            *binary_path   = NULL;;
    const char       *binary_name   = NULL;

    if( fnmatch(BOOSTER_DIRECTORY "/" BOOSTER_PATTERN, *argv, FNM_PATHNAME) == 0 ) {
        /* If these are set, we are launching application booster */
        booster_path = g_strdup(*argv);
        booster_name = path_basename(booster_path);

        /* FIXME: this requires that /usr/share/applications/APP.desktop
         *        is used for launching /usr/bin/APP
         */
        binary_path  = path_construct("/usr/bin", desktop_name, NULL);

        /* The booster binary was already checked, do the same for the
         * application binary that booster will be launching.
         */
        if( !sailjailclient_binary_check(binary_path) )
            goto EXIT;
    }
    else {
        binary_path = g_strdup(*argv);
    }

    binary_name  = path_basename(binary_path);

    /* Prompt for launch permission */
    if( booster_name ) {
        /* Application boosters are launched without prompting,
         * with full set of permissions required by the application.
         */
        log_debug("booster launch - skip permission query");
    }
    else if( !client_prompt_permissions(self, desktop_name) ) {
        goto EXIT;
    }

    /* Obtain application details */
    if( !client_query_appinfo(self, desktop_name) )
        goto EXIT;

    const char  *exec        = client_desktop_exec(self);
    const char  *exec_dbus   = client_sailjail_exec_dbus(self);
    const char  *org_name    = client_sailjail_organization_name(self);
    const char  *app_name    = client_sailjail_application_name(self);
    const char  *data_dir    = client_sailjail_data_directory(self);
    const char **permissions = client_sailjail_application_permissions(self);
    const char  *service     = client_maemo_service(self);
    const char  *method      = client_maemo_method(self);

    const gchar **granted = (booster_name ?
                             client_sailjail_application_permissions(self) :
                             client_get_granted(self));

    if( !granted ) {
        log_err("permissions not defined / granted");
        goto EXIT;
    }

    /* Check if privileged launch is needed / possible */
    bool privileged = false;
    for( int i = 0; !privileged && granted[i]; ++i )
        privileged = !strcmp(granted[i], "Privileged");

    if( privileged && !client_is_privileged(self) ) {
        log_err("privileged launch is needed but not possible");
        goto EXIT;
    }

    if( log_p(LOG_DEBUG) ) {
        log_debug("exec      = %s", exec);
        log_debug("exec_dbus = %s", exec_dbus ?: "(none)");
        log_debug("org_name  = %s", org_name);
        log_debug("app_name  = %s", app_name);
        log_debug("service   = %s", service);
        log_debug("method    = %s", method);
        for( int i = 0; permissions && permissions[i]; ++i )
            log_debug("permissions += %s", permissions[i]);
        for( int i = 0; granted && granted[i]; ++i )
            log_debug("granted     += %s", granted[i]);
    }

    /* Check that command line we have matches Exec line in desktop file */
    if( !exec ) {
        log_err("Exec line not defined");
        goto EXIT;
    }

    /* Interpret both "Compatibility" and "None" as compatibility mode */
    bool use_compatibility = g_strcmp0(client_mode(self), "Normal");

    if( booster_name ) {
        /* Application booster validates Exec line when it gets
         * launch requrest from invoker.
         */
    }
    else if( !sailjailclient_validate_argv(exec, argv, use_compatibility) &&
             !sailjailclient_validate_argv(exec_dbus, argv, use_compatibility) ) {
        gchar *args = g_strjoinv(" ", (gchar **)argv);
        log_err("Command line does not match template%s", exec_dbus ? "s" : "");
        log_err("Exec: %s", exec);
        if( exec_dbus )
            log_err("ExecDBus: %s", exec_dbus);
        log_err("Command: %s", args);
        if( !log_p(LOG_INFO) )
            log_err("Increase verbosity for more information");
        g_free(args);
        goto EXIT;
    }

    /* Construct firejail command to execute */
    client_add_firejail_option(self, "/usr/bin/firejail");
    if( client_get_debug_mode(self) )
        client_add_firejail_option(self, "--debug");
    else
        client_add_firejail_option(self, "--quiet");

    if( org_name )
        client_add_firejail_option(self, "--template=OrganizationName:%s", org_name);
    if( app_name )
        client_add_firejail_option(self, "--template=ApplicationName:%s", app_name);

    client_add_firejail_option(self, "--private-bin=%s", binary_name);
    client_add_firejail_option(self, "--whitelist=/usr/share/%s", data_dir ?: desktop_name);

    /* Watch out for alternate desktop files in /etc as whitelisting
     * them while private-etc is used leads to problems */
    if( desktop1_path && strncmp(desktop1_path, "/etc/", 5) )
        client_add_firejail_option(self, "--whitelist=%s", desktop1_path);

    /* Legacy app binary based data directories are made available.
     * But they are not created unless legacy app sandboxing is used. */
    client_add_firejail_directory(self, use_compatibility, "${HOME}/.local/share/%s", desktop_name);
    client_add_firejail_directory(self, use_compatibility, "${HOME}/.config/%s", desktop_name);
    client_add_firejail_directory(self, use_compatibility, "${HOME}/.cache/%s", desktop_name);

    if( !empty_p(org_name) && !empty_p(app_name) ) {
        client_add_firejail_directory(self, true,  "${HOME}/.cache/%s/%s", org_name, app_name);
        client_add_firejail_directory(self, true,  "${HOME}/.local/share/%s/%s", org_name, app_name);
        client_add_firejail_directory(self, true,  "${HOME}/.config/%s/%s", org_name, app_name);

        client_add_firejail_option(self, "--dbus-user.own=%s.%s", org_name, app_name);
    }

    if( !empty_p(service) )
        client_add_firejail_option(self, "--dbus-user.own=%s", service);

    /* Include booster type specific profile */
    client_add_firejail_profile(self, booster_name);

    /* Include application specific profile */
    client_add_firejail_profile(self, desktop_name);

    /* Include granted permissions */
    for( size_t i = 0; granted[i]; ++i )
        client_add_firejail_permission(self, granted[i]);
    client_add_firejail_permission(self, "Base");

    /* Tracing options */
    const gchar *trace_dir = client_get_trace_dir(self);
    if( trace_dir ) {
        client_add_firejail_option(self, "--output-stderr=%s/firejail-stderr.log", trace_dir);
        client_add_firejail_option(self, "--trace=%s/firejail-trace.log", trace_dir);
        client_add_firejail_option(self, "--dbus-log=%s/firejail-dbus.log", trace_dir);
        client_add_firejail_option(self, "--dbus-user=filter");
        client_add_firejail_option(self, "--dbus-system=filter");
        client_add_firejail_option(self, "--dbus-user.log");
        client_add_firejail_option(self, "--dbus-system.log");
    }

    /* End of firejail options */
    client_add_firejail_option(self, "--");

    /* Construct command line to execute */
    GArray *array = g_array_new(true, false, sizeof(gchar *));
    for( const GList *iter = client_get_firejail_options(self); iter; iter = iter->next )
        g_array_append_val(array, iter->data);
    for( int i = 0; i < argc; ++i )
        g_array_append_val(array, argv[i]);
    char **args = (char **)g_array_free(array, false);

    log_notice("Launching '%s' via sailjailclient...", binary_name);
    if( log_p(LOG_INFO) ) {
        for( int i = 0; args[i]; ++i )
            log_info("arg[%02d] = %s", i, args[i]);
    }

    /* Choose regular / privileged launch
     *
     * Note that effective gid does not survive firejail sandboxing, so
     * we need to tweak real gid to get app running in privileged group.
     *
     * Also, if sailjailclient is not running with egid=privileged and
     * privileged launch is needed, we should have already bailed out
     * before getting here.
     */
    gid_t gid = privileged ? getegid() : getgid();
    if( setresgid(gid, gid, gid) == -1 ) {
        log_err("failed to set group: %m");
        goto EXIT;
    }

    /* Handle --dry-run */
    if( client_get_dry_run(self) ) {
        for( size_t i = 0; args[i]; ++i )
            printf("%s%s", i ? " " : "", args[i]);
        printf("\n");
        exit_code = EXIT_SUCCESS;
        goto EXIT;
    }

    /* Execute the application */
    fflush(NULL);
    errno = 0;
    execv(*args, args);
    log_err("%s: exec failed: %m", *args);
    g_free(args);

    if( !booster_name && desktop1_path )
        client_notify_launch_canceled(self, desktop1_path);

EXIT:
    g_free(desktop_name);
    g_free(binary_path);
    g_free(booster_path);

    return exit_code;
}

/* ------------------------------------------------------------------------- *
 * CLIENT_NOTIFY
 * ------------------------------------------------------------------------- */

static void
client_notify_launch_status(client_t *self, const char *method,
                            const char *desktop)
{
    GDBusConnection *connection = client_session_bus(self);
    if( connection ) {
        /* We do not care too much about whether the call succeeds or not */
        g_dbus_connection_call(connection, LAUNCHNOTIFY_SERVICE,
                               LAUNCHNOTIFY_OBJECT, LAUNCHNOTIFY_INTERFACE,
                               method, g_variant_new("(s)", desktop),
                               NULL, G_DBUS_CALL_FLAGS_NO_AUTO_START,
                               -1, NULL, NULL, NULL);
        /* But we do want it to go out before we exec*() / exit() */
        g_dbus_connection_flush_sync(connection, NULL, NULL);
    }
}

static void
client_notify_launch_canceled(client_t *self, const char *desktop)
{
    client_notify_launch_status(self, LAUNCHNOTIFY_METHOD_LAUNCH_CANCELED,
                                desktop);
}

/* ========================================================================= *
 * SAILJAILCLIENT
 * ========================================================================= */

static int
sailjailclient_get_field_code(const char *arg)
{
    // Non-null string starting with a '%' followed by exactly one character
    return arg && arg[0] == '%' && arg[1] && !arg[2] ? arg[1] : 0;
}

static bool
sailjailclient_is_option(const char *arg)
{
    // Non-null string starting with a hyphen
    return arg && arg[0] == '-';
}

static bool
sailjailclient_ignore_arg(const char *arg)
{
    return !g_strcmp0(arg, "-prestart");
}

static bool
sailjailclient_match_argv(const char **tpl_argv, const char **app_argv)
{
    bool matching = false;

    /* Match each arg in template */
    for( ;; ) {
        const char *want = *tpl_argv++;

        /* Allow some slack e.g. regarding "-prestart" options */
        while( *app_argv && g_strcmp0(*app_argv, want) &&
               sailjailclient_ignore_arg(*app_argv) ) {
            log_warning("ignoring argument: %s", *app_argv);
            ++app_argv;
        }

        if( !want ) {
            /* Template args exhausted */
            if( *app_argv ) {
                /* Excess application args */
                log_info("argv has unwanted '%s'", *app_argv);
                goto EXIT;
            }
            break;
        }

        int field_code = sailjailclient_get_field_code(want);

        if( !field_code ) {
            /* Exact match needed */
            if( g_strcmp0(*app_argv, want) ) {
                /* Application args has something else */
                log_info("argv is missing '%s'", want);
                goto EXIT;
            }
            ++app_argv;
            continue;
        }

        /* Field code explanations from "Desktop Entry Specification"
         *
         * https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
         */
        int code_args = 0;
        switch( field_code ) {
        case 'f':  /* A single file name (or none) */
        case 'u':  /* A single URL (or none) */
            code_args = -1;
            break;

        case 'c':  /* The translated name of the application */
        case 'k':  /* The location of the desktop file */
            code_args = 1;
            break;
        case 'F':  /* A list of files */
        case 'U':  /* A list of URLs */
            code_args = INT_MIN;
            break;
        case 'i':
            /* The Icon key of the desktop entry expanded as two
             * arguments, first --icon and then the value of the
             * Icon key. Should not expand to any arguments if
             * the Icon key is empty or missing.
             */
            if( !g_strcmp0(*app_argv, "--icon") )
                ++app_argv, code_args = 1;
            break;
        case 'd':
        case 'D':
        case 'n':
        case 'N':
        case 'v':
        case 'm':
            /* Deprecated */
            log_err("Exec line has deprecated field code '%s'", want);
            goto EXIT;
        default:
            /* Unknown */
            log_err("Exec line has unknown field code '%s'", want);
            goto EXIT;
        }

        if( code_args < 0 ) {
            /* Variable number of args */
            if( sailjailclient_get_field_code(*tpl_argv) ) {
                log_info("Can't validate '%s %s' combination", want, *tpl_argv);
                goto EXIT;
            }
            for( ; code_args < 0; ++code_args ) {
                if( !*app_argv || !g_strcmp0(*app_argv, *tpl_argv) )
                    break;
                if( sailjailclient_is_option(*app_argv) ) {
                    log_info("option '%s' at field code '%s'", *app_argv, want);
                    goto EXIT;
                }
                ++app_argv;
            }
        }
        else {
            /* Specified number of args */
            for( ; code_args > 0; --code_args ) {
                if( !*app_argv ) {
                    log_info("missing args for field code '%s'", want);
                    goto EXIT;
                }
                if( sailjailclient_is_option(*app_argv) ) {
                    log_info("option '%s' at field code '%s'", *app_argv, want);
                    goto EXIT;
                }
                ++app_argv;
            }
        }
    }

    matching = true;

EXIT:
    return matching;
}

static bool
sailjailclient_validate_argv(const char *exec, const gchar **app_argv, bool use_compatibility)
{
    bool          validated = false;
    GError       *err       = NULL;
    gchar       **exec_argv = NULL;

    if( !exec ) {
        /* No exec line provided -> invalid */
        goto EXIT;
    }

    if( !app_argv || !*app_argv ) {
        log_err("application argv not defined");
        goto EXIT;
    }

    if( use_compatibility && !path_dirname_eq(app_argv[0], BINDIR) ) {
        /* Legacy apps must always be located in BINDIR */
        log_err("Legacy apps must be in: " BINDIR "/");
        goto EXIT;
    }

    /* Split desktop Exec line into argv */
    if( !g_shell_parse_argv(exec, NULL, &exec_argv, &err) ) {
        log_err("Exec line parse failure: %s", err->message);
        goto EXIT;
    }

    if( !exec_argv || !*exec_argv ) {
        log_err("Exec line not defined");
        goto EXIT;
    }

    /* Expectation: Exec line might have leading 'wrapper' executables
     * such as sailjail, invoker, etc -> make an attempt to skip those
     * by looking for argv[0] for command we are about to launch.
     *
     * App may also be defined without absolute path, in which case it
     * must reside in BINDIR and it must not have any 'wrapper'
     * executables. Thus check the path of app_argv[0] and compare Exec
     * line to the binary name.
     */
    const char **tpl_argv = (const char **)exec_argv;
    if( !path_dirname_eq(app_argv[0], BINDIR) ||
        g_strcmp0(*tpl_argv, path_basename(app_argv[0])) ) {
        /* App is not specified without path as the first argument,
         * => there might be 'wrappers' and we match to full path.
         */
        for( ; *tpl_argv; ++tpl_argv ) {
            if( !g_strcmp0(*tpl_argv, app_argv[0]) )
                break;
        }

        if( !*tpl_argv ) {
            log_err("Exec line does not contain '%s'", *app_argv);
            goto EXIT;
        }
    }

    /* Argument zero has been checked already */
    if( !sailjailclient_match_argv(tpl_argv + 1, app_argv + 1) ) {
        /* Leave error reporting to caller */
        goto EXIT;
    }

    validated = true;

EXIT:
    g_strfreev(exec_argv);
    g_clear_error(&err);

    return validated;
}

static bool
sailjailclient_test_elf(const char* filename)
{
    static const char elf[4] = {0x7f, 'E', 'L', 'F'};

    bool ret = false;
    FILE *file = fopen(filename, "rb");
    if( !file ) {
        log_err("%s: could not open: %m", filename);
    }
    else {
        char data[sizeof elf];
        if( fread(data, sizeof data, 1, file) != 1 ) {
            log_err("%s: could not read", filename);
        }
        else if( memcmp(data, elf, sizeof data) == 0 ) {
            ret = true;
        }
        fclose(file);
    }
    return ret;
}

static const char sailjailclient_usage_template[] = ""
"NAME\n"
"  %s  --  command line utility for launching sandboxed application\n"
"\n"
"SYNOPSIS\n"
"  %s <option> [--] <application_path> [args]\n"
"\n"
"DESCRIPTION\n"
"  This tool gets application lauch permissions from sailjaild and\n"
"  then starts the application in appropriate firejail sandbox.\n"
"\n"
"OPTIONS\n"
"  -h --help\n"
"        Writes this help text to stdout\n"
"  -V --version\n"
"        Writes tool version to stdout.\n"
"  -v --verbose\n"
"        Makes tool more verbose.\n"
"  -q --quiet\n"
"        Makes tool less verbose.\n"
"  -o, --output=OUT\n"
"        Where to output log (stdout|syslog)\n"
"        (defaults to stderr when executed from shell, syslog otherwise)\n"
"  -p --profile=<desktop>\n"
"        Define application file instead of using heuristics based\n"
"        on path to launched application\n"
"\n"
"        Note: previously arbitrary paths could be used with this\n"
"        option, but now it needs to be name of a desktop file that\n"
"        exists in /usr/share/applications or /etc/sailjail/applications\n"
"        directory. If a path is given, all directory components are\n"
"        ignored. And \".desktop\" extension can be omitted.\n"
"  -t, --trace=DIR\n"
"        Enable libtrace and dbus proxy logging\n"
"  -d, --debug-mode\n"
"        Execute firejail in debug verbosity\n"
"  -D, --dry-run\n"
"        Print out firejail command line instead of executing it\n"
"\n"
"BACKWARDS COMPATIBILITY\n"
"  -s, --section=NAME\n"
"        Sailjail section in the profile [Sailjail|X-Sailjail]\n"
"        (silently ignored)\n"
"  -a, --app=APP\n"
"        Force adding Sailfish application directories\n"
"        (silently ignored)\n"
"\n"
"EXAMPLES\n"
"  %s -- /usr/bin/bar\n"
"        Launch application bar using permissions from bar.desktop\n"
"  %s -p org.foo.bar -- /usr/bin/bar\n"
"        Launch application bar using permissions from org.foo.bar.desktop\n"
"\n"
"COPYRIGHT\n"
"  Copyright (c) 2021 Open Mobile Platform LLC.\n"
"  Copyright (c) 2021 Jolla Ltd.\n"
"\n"
"SEE ALSO\n"
"  sailjaild\n"
"\n";

static const char sailjailclient_usage_hint[] = "(use --help for instructions)\n";

static const struct option long_options[] = {
    {"help",         no_argument,       NULL, 'h'},
    {"version",      no_argument,       NULL, 'V'},
    {"verbose",      no_argument,       NULL, 'v'},
    {"quiet",        no_argument,       NULL, 'q'},
    {"output",       required_argument, NULL, 'o'},
    {"profile",      required_argument, NULL, 'p'},
    {"trace",        required_argument, NULL, 't'},
    {"debug-mode",   no_argument,       NULL, 'd'},
    {"dry-run",      no_argument,       NULL, 'D'},
    // bw compat
    {"section",      required_argument, NULL, 's'},
    {"app",          required_argument, NULL, 'a'},
    // unadvertised debug features
    {"match-exec",   required_argument, NULL, 'm'},
    {0, 0, 0, 0}
};
static const char short_options[] =\
"+"  // use posix rules
"h"  // --help
"V"  // --version
"v"  // --verbose
"q"  // --quiet
"p:" // --profile
"s:" // --section
"a:" // --app
"o:" // --output
"t:" // --trace
"m:" // --match-exec
"d"  // --debug-mode
"D"  // --dry-run
;

static void
sailjailclient_print_usage(const char *progname)
{
    printf(sailjailclient_usage_template,
           progname,
           progname,
           progname,
           progname);
}

static bool
sailjailclient_binary_check(const char *binary_path)
{
    bool is_valid = false;

    if( *binary_path != '/' )
        log_err("%s: is not absolute path", binary_path);
    else if( access(binary_path, R_OK | X_OK) == -1 )
        log_err("%s: is not executable: %m", binary_path);
    else if( !sailjailclient_test_elf(binary_path) )
        log_err("%s: is not elf binary: %m", binary_path);
    else
        is_valid = true;

    return is_valid;
}

int
sailjailclient_main(int argc, char **argv)
{
    int         exit_code     = EXIT_FAILURE;
    const char *progname      = path_basename(*argv);
    config_t   *config        = config_create();
    client_t   *client        = client_create();
    const char *desktop_file  = NULL;
    gchar      *desktop1_path = NULL;
    gchar      *desktop2_path = NULL;
    const char *match_exec    = 0;

    log_set_target(isatty(STDIN_FILENO) ? LOG_TO_STDERR : LOG_TO_SYSLOG);

    /* We want to see client options explicitly separated from app args */
    int command = 0;
    for( int i = 1; i < argc; ++i ) {
        if( !strcmp(argv[i], "--") ) {
            command = i + 1;
            break;
        }
    }

    /* Parse client options */
    for( ;; ) {
        int opt = getopt_long(argc, argv, short_options, long_options, 0);

        if( opt == -1 )
            break;

        switch( opt ) {
        case 'h':
            sailjailclient_print_usage(progname);
            exit_code = EXIT_SUCCESS;
            goto EXIT;
        case 'v':
            log_set_level(log_get_level() + 1);
            break;
        case 'q':
            log_set_level(log_get_level() - 1);
            break;
        case 'V':
            printf("%s\n", VERSION);
            exit_code = EXIT_SUCCESS;
            goto EXIT;
        case 'p':
            desktop_file = optarg;
            break;
        case 'm':
            match_exec = optarg;
            break;
        case 't':
            client_set_trace_dir(client, optarg);
            break;
        case 'd':
            client_set_debug_mode(client, true);
            break;
        case 'D':
            client_set_dry_run(client, true);
            break;
        case 's':
        case 'a':
            log_warning("unsupported sailjail option '-%c' ignored", opt);
            break;
        case 'o':
            if( !g_strcmp0(optarg, "syslog") )
                log_set_target(LOG_TO_SYSLOG);
            else
                log_set_target(LOG_TO_STDERR);
            break;
        case '?':
            fputs(sailjailclient_usage_hint, stderr);
            goto EXIT;
        }
    }

    /* Block root user from this point onwards */
    if( getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0 ) {
        log_err("Launching apps is not applicable to root user");
        goto EXIT;
    }

    /* Remaining arguments is: command line to execute */
    argv += optind;
    argc -= optind;

    if( argc < 1 ) {
        log_err("No application to launch given\n%s", sailjailclient_usage_hint);
        goto EXIT;
    }

    if( command == 0 ) {
        /* Legacy is to rely on posix option parsing, so this is expected */
        log_info("executed without '--' separating options from launch command");
    }
    else if( optind != command ) {
        log_err("executed with '--' and parsing stopped at unexpected position");
        goto EXIT;
    }

    client_set_argv(client, argc, argv);

    if( match_exec ) {
        if( !sailjailclient_validate_argv(match_exec,
                                          client_get_argv(client, NULL), false) ) {
            gchar *args = g_strjoinv(" ", (gchar **)client_get_argv(client, NULL));
            log_err("Application args do not match template");
            log_err("exec: %s", match_exec);
            log_err("args: %s", args);
            g_free(args);
        }
        else {
            exit_code = EXIT_SUCCESS;
        }
        goto EXIT;
    }

    /* Sanity check application binary path */
    const char *binary = *argv;

    if( !sailjailclient_binary_check(binary) )
        goto EXIT;

    /* Sanity check desktop file path */
    desktop1_path = path_from_desktop_name(desktop_file ?: binary);
    desktop2_path = alt_path_from_desktop_name(desktop_file ?: binary);
    if( access(desktop1_path, R_OK) == 0 ) {
        client_set_desktop1_path(client, desktop1_path);
        g_free(desktop1_path), desktop1_path = NULL;
    }
    if( access(desktop2_path, R_OK) == 0 ) {
        client_set_desktop2_path(client, desktop2_path);
        g_free(desktop2_path), desktop2_path = NULL;
    }
    if( desktop1_path && desktop2_path ) {
        log_warning("Neither '%s' nor '%s' is available/accessible",
                    desktop1_path, desktop2_path);
        log_warning("Application permissions are not defined");
        goto EXIT;
    }

    /* Check if privileged application handling is possible */
    struct passwd *pw = getpwnam("privileged");
    if( !pw ) {
        log_warning("Privileged user does not exist");
    }
    else if( pw->pw_gid != getegid() ) {
        log_warning("Effective group is not privileged");
    }
    else {
        client_set_privileged(client, true);
    }

    /* Execute */
    exit_code = client_launch_application(client);

EXIT:
    client_delete_at(&client);
    g_free(desktop1_path);
    g_free(desktop2_path);
    config_delete(config);
    log_debug("exit %d", exit_code);
    return exit_code;
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int
main(int argc, char **argv)
{
    return sailjailclient_main(argc, argv);
}
