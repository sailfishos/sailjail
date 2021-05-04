/*
 * Copyright (c) 2021 Open Mobile Platform LLC.
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

#include "migrator.h"

#include "appinfo.h"
#include "control.h"
#include "logging.h"
#include "util.h"
#include "settings.h"
#include "stringset.h"

#include <glib.h>
#include <glob.h>
#include <unistd.h>

/* Migrate existing permission data in /var/lib/sailjail-homescreen/
 * directory to sailjaild's appsettings. Old data is deleted after
 * appsettings has been saved.
 *
 * Approved permissions for app Y which has its permissions specified in
 * section X-Sailjail of /usr/share/applications/Y.desktop are stored in
 * /var/lib/sailjail-homescreen/UID/usr/share/applications/Y.desktop/X-Sailjail
 * where UID is the uid of the user who runs the app. The old format
 * looks like this:
 *
 *     [Permissions]
 *     Permissions=WebView;Audio;Location;Internet;Downloads;Documents;Sharing
 *     OrganizationName=org.sailfishos
 *     ApplicationName=browser
 *
 *     [Approval]
 *     PluginVersion=1.0.21
 *     Timestamp=1620304265
 *     Exec=/usr/bin/sailfish-browser
 *
 * If the file exists, the application has been approved with the
 * permissions listed in Permissions key. OrganizationName and
 * ApplicationName have been copied straight from the application
 * profile. Exec key contains binary path to the application that was
 * launched after approving the permissions but that value is ignored
 * here.
 */

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct migrator_t migrator_t;
typedef struct approval_t approval_t;
typedef struct settings_t settings_t;
typedef struct control_t  control_t;

typedef enum migrator_state_t {
    MIGRATOR_STATE_UNINITIALIZED,
    MIGRATOR_STATE_INITIALIZING,
    MIGRATOR_STATE_MIGRATING,
    MIGRATOR_STATE_MIGRATED,
    MIGRATOR_STATE_FINAL
} migrator_state_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * MIGRATOR_STATE
 * ------------------------------------------------------------------------- */

static const char *migrator_state_repr(migrator_state_t state);

/* ------------------------------------------------------------------------- *
 * MIGRATOR
 * ------------------------------------------------------------------------- */

migrator_t *migrator_create   (settings_t *settings);
void        migrator_delete   (migrator_t *self);
void        migrator_delete_at(migrator_t **pself);
void        migrator_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * MIGRATOR_PROPERTIES
 * ------------------------------------------------------------------------- */

static settings_t      *migrator_settings   (const migrator_t *self);
static control_t       *migrator_control    (const migrator_t *self);
static const appinfo_t *migrator_appinfo    (const migrator_t *self, const gchar *appname);

/* ------------------------------------------------------------------------- *
 * MIGRATOR_STM
 * ------------------------------------------------------------------------- */

static migrator_state_t migrator_get_state       (const migrator_t *self);
static void             migrator_enter_state     (migrator_t *self);
static void             migrator_leave_state     (migrator_t *self);
static void             migrator_eval_state_now  (migrator_t *self);
static void             migrator_eval_state_later(migrator_t *self);
static gboolean         migrator_eval_state_cb   (gpointer aptr);
static bool             migrator_state_transfer_p(migrator_state_t prev, migrator_state_t next);
static void             migrator_set_state       (migrator_t *self, migrator_state_t state);

/* ------------------------------------------------------------------------- *
 * MIGRATOR_QUEUE
 * ------------------------------------------------------------------------- */

static void   migrator_build_queue(migrator_t *self);
static bool   migrator_queued     (migrator_t *self);
static gchar *migrator_dequeue    (migrator_t *self);
static void   migrator_clear_queue(migrator_t *self);

/* ------------------------------------------------------------------------- *
 * MIGRATOR_REMOVAL_QUEUE
 * ------------------------------------------------------------------------- */

static void migrator_removal_enqueue    (migrator_t *self, const gchar *path);
static bool migrator_removal_queued     (migrator_t *self);
static void migrator_removal_dequeue_all(migrator_t *self);
static void migrator_removal_clear_queue(migrator_t *self);

/* ------------------------------------------------------------------------- *
 * MIGRATOR_PROCESS
 * ------------------------------------------------------------------------- */

static void migrator_process_file(migrator_t *self, const gchar *path);

/* ------------------------------------------------------------------------- *
 * MIGRATOR_SLOTS
 * ------------------------------------------------------------------------- */

void migrator_on_settings_saved(migrator_t *self);

/* ------------------------------------------------------------------------- *
 * APPROVAL
 * ------------------------------------------------------------------------- */

static void  approval_ctor     (approval_t *self, const migrator_t *migrator, const gchar *approval_file);
static void  approval_dtor     (approval_t *self);
approval_t  *approval_create   (const migrator_t *migrator, const gchar *approval_file);
void         approval_delete   (approval_t *self);
void         approval_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * APPROVAL_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static bool              approval_valid       (const approval_t *self);
static const gchar      *approval_profile     (const approval_t *self);
static uid_t             approval_uid         (const approval_t *self);
static stringset_t      *approval_permissions (const approval_t *self);
static const gchar      *approval_organization(const approval_t *self);
static const gchar      *approval_application (const approval_t *self);
static const migrator_t *approval_migrator    (const approval_t *self);
static const control_t  *approval_control     (const approval_t *self);

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static void         remove_approval_path      (const gchar *path);
static bool         may_remove_approval_path  (const gchar *path);
static gchar       *profile_from_approval_path(const gchar *path);
static uid_t        uid_from_approval_path    (const gchar *path, const control_t *control);
static const gchar *without_leading_data_path (const gchar *path);

/* ------------------------------------------------------------------------- *
 * APPROVAL_DATA
 * ------------------------------------------------------------------------- */

#define APPROVAL_SECTION "Approval"
#define HOMESCREEN_DATA_PATH "/var/lib/sailjail-homescreen"
#define APPROVAL_PATTERN HOMESCREEN_DATA_PATH "/*" APPLICATIONS_DIRECTORY "/" APPLICATIONS_PATTERN "/*"
#define MIGRATION_UID_UNDEFINED ((uid_t)(-1))

/* ========================================================================= *
 * MIGRATOR_STATE
 * ========================================================================= */

static const char *
migrator_state_repr(migrator_state_t state)
{
    static const char * const lut[] =  {
        [MIGRATOR_STATE_UNINITIALIZED] = "UNINITIALIZED",
        [MIGRATOR_STATE_INITIALIZING]  = "INITIALIZING",
        [MIGRATOR_STATE_MIGRATING]     = "MIGRATING",
        [MIGRATOR_STATE_MIGRATED]      = "MIGRATED",
        [MIGRATOR_STATE_FINAL]         = "FINAL",
    };
    return lut[state];
}

/* ========================================================================= *
 * MIGRATOR
 * ========================================================================= */

struct migrator_t
{
    settings_t       *mgr_settings;
    migrator_state_t  mgr_state;
    guint             mgr_later_id;
    GQueue           *mgr_queue;         // gchar *
    GQueue           *mgr_removal_queue; // gchar *
};

static void
migrator_ctor(migrator_t *self, settings_t *settings)
{
    log_info("migrator() created");
    self->mgr_settings      = settings;
    self->mgr_state         = MIGRATOR_STATE_UNINITIALIZED;
    self->mgr_later_id      = 0;
    self->mgr_queue         = g_queue_new();
    self->mgr_removal_queue = g_queue_new();
    migrator_set_state(self, MIGRATOR_STATE_INITIALIZING);
}

static void
migrator_dtor(migrator_t *self)
{
    log_info("migrator() deleted");
    migrator_set_state(self, MIGRATOR_STATE_FINAL);

    change_timer(&self->mgr_later_id, 0);

    g_queue_free(self->mgr_queue),
        self->mgr_queue = NULL;

    g_queue_free(self->mgr_removal_queue),
        self->mgr_removal_queue = NULL;
}

migrator_t *
migrator_create(settings_t *settings)
{
    migrator_t *self = g_malloc0(sizeof *self);
    migrator_ctor(self, settings);
    return self;
}

void
migrator_delete(migrator_t *self)
{
    if( self ) {
        migrator_dtor(self);
        g_free(self);
    }
}

void
migrator_delete_at(migrator_t **pself)
{
    migrator_delete(*pself), *pself = NULL;
}

void
migrator_delete_cb(void *self)
{
    migrator_delete(self);
}

/* ------------------------------------------------------------------------- *
 * MIGRATOR_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static settings_t *
migrator_settings(const migrator_t *self)
{
    return self->mgr_settings;
}

static control_t *
migrator_control(const migrator_t *self)
{
    return settings_control(self->mgr_settings);
}

static const appinfo_t *
migrator_appinfo(const migrator_t *self, const gchar *appname)
{
    return control_appinfo(migrator_control(self), appname);
}

/* ------------------------------------------------------------------------- *
 * MIGRATOR_STM
 * ------------------------------------------------------------------------- */

static migrator_state_t
migrator_get_state(const migrator_t *self)
{
    return self->mgr_state;
}

static void
migrator_enter_state(migrator_t *self)
{
    switch( migrator_get_state(self) ) {
    case MIGRATOR_STATE_UNINITIALIZED:
        abort();
        break;

    case MIGRATOR_STATE_INITIALIZING:
        migrator_build_queue(self);
        break;

    case MIGRATOR_STATE_MIGRATING:
        break;

    case MIGRATOR_STATE_MIGRATED:
        break;

    case MIGRATOR_STATE_FINAL:
        /* Dequeue without migrating */
        migrator_clear_queue(self);
        /* Dequeue without removing */
        migrator_removal_clear_queue(self);
        break;
    }
}

static void
migrator_leave_state(migrator_t *self)
{
    switch( migrator_get_state(self) ) {
    case MIGRATOR_STATE_UNINITIALIZED:
        break;

    case MIGRATOR_STATE_INITIALIZING:
        break;

    case MIGRATOR_STATE_MIGRATING:
        break;

    case MIGRATOR_STATE_MIGRATED:
        break;

    case MIGRATOR_STATE_FINAL:
        abort();
        break;
    }
}

static void
migrator_eval_state_now(migrator_t *self)
{
    change_timer(&self->mgr_later_id, 0);

    switch( migrator_get_state(self) ) {
    case MIGRATOR_STATE_UNINITIALIZED:
        break;

    case MIGRATOR_STATE_INITIALIZING:
        if( migrator_queued(self) )
            migrator_set_state(self, MIGRATOR_STATE_MIGRATING);
        else
            /* Nothing to migrate => go straight to final state */
            migrator_set_state(self, MIGRATOR_STATE_FINAL);
        break;

    case MIGRATOR_STATE_MIGRATING:
        if( migrator_queued(self) ) {
            gchar *path = migrator_dequeue(self);
            migrator_process_file(self, path);
            migrator_eval_state_later(self);
            g_free(path);
        }
        else {
            migrator_set_state(self, MIGRATOR_STATE_MIGRATED);
        }
        break;

    case MIGRATOR_STATE_MIGRATED:
        if( !migrator_removal_queued(self) )
            migrator_set_state(self, MIGRATOR_STATE_FINAL);
        break;

    case MIGRATOR_STATE_FINAL:
        break;
    }
}

static void
migrator_eval_state_later(migrator_t *self)
{
    if( !self->mgr_later_id )
        self->mgr_later_id = g_idle_add(migrator_eval_state_cb, self);
}

static gboolean
migrator_eval_state_cb(gpointer aptr)
{
    migrator_t *self = aptr;
    self->mgr_later_id = 0;
    migrator_eval_state_now(self);
    return G_SOURCE_REMOVE;
}

static bool
migrator_state_transfer_p(migrator_state_t prev, migrator_state_t next)
{
    if( prev == MIGRATOR_STATE_FINAL )
        return false;
    if( next == MIGRATOR_STATE_UNINITIALIZED )
        return false;
    return true;
}

static void
migrator_set_state(migrator_t *self, migrator_state_t state)
{
    if( self->mgr_state != state ) {
        if( !migrator_state_transfer_p(self->mgr_state, state) ) {
            log_err("migrator: rejected transition: %s -> %s",
                    migrator_state_repr(self->mgr_state),
                    migrator_state_repr(state));
        }
        else {
            log_info("migrator: state transition: %s -> %s",
                     migrator_state_repr(self->mgr_state),
                     migrator_state_repr(state));

            migrator_leave_state(self);
            self->mgr_state = state;
            migrator_enter_state(self);

            migrator_eval_state_later(self);
        }
    }
}

/* ------------------------------------------------------------------------- *
 * MIGRATOR_QUEUE
 * ------------------------------------------------------------------------- */

static void
migrator_build_queue(migrator_t *self)
{
    glob_t gl = {};

    if( !glob(APPROVAL_PATTERN, 0, 0, &gl) ) {
        for( int i = 0; i < gl.gl_pathc; ++i ) {
            const gchar *section = path_basename(gl.gl_pathv[i]);
            if( !g_strcmp0(section, SAILJAIL_SECTION_PRIMARY) ||
                !g_strcmp0(section, SAILJAIL_SECTION_SECONDARY) )
                g_queue_push_tail(self->mgr_queue, g_strdup(gl.gl_pathv[i]));
        }
    }

    globfree(&gl);
}

static bool
migrator_queued(migrator_t *self)
{
    return self->mgr_queue->length != 0;
}

static gchar *
migrator_dequeue(migrator_t *self)
{
    gchar *file = g_queue_pop_head(self->mgr_queue);
    log_debug("migrator: dequeue: %s", file);
    return file;
}

static void
migrator_clear_queue(migrator_t *self)
{
    g_queue_clear_full(self->mgr_queue, g_free);
}

/* ------------------------------------------------------------------------- *
 * MIGRATOR_REMOVAL_QUEUE
 * ------------------------------------------------------------------------- */

static void
migrator_removal_enqueue(migrator_t *self, const gchar *path)
{
    g_queue_push_tail(self->mgr_removal_queue, g_strdup(path));
}

static bool
migrator_removal_queued(migrator_t *self)
{
    return self->mgr_removal_queue->length != 0;
}

static void
migrator_removal_dequeue_all(migrator_t *self)
{
    for( GList *iter = self->mgr_removal_queue->head; iter; iter = iter->next ) {
        gchar *path = iter->data;
        iter->data = NULL;
        remove_approval_path(path);
        g_free(path);
    }
    g_queue_clear_full(self->mgr_removal_queue, NULL);
}

static void
migrator_removal_clear_queue(migrator_t *self)
{
    g_queue_clear_full(self->mgr_removal_queue, g_free);
}

/* ------------------------------------------------------------------------- *
 * MIGRATOR_PROCESS
 * ------------------------------------------------------------------------- */

static void
migrator_process_file(migrator_t *self, const gchar *path)
{
    bool           migrated    = false;
    gchar         *appname     = NULL;
    appsettings_t *appsettings = NULL;
    approval_t    *approval    = approval_create(self, path);
    stringset_t   *granted     = stringset_create();

    if( approval_valid(approval) ) {
        appname = path_to_desktop_name(approval_profile(approval));
        if( control_valid_application(migrator_control(self), appname) ) {
            const appinfo_t *appinfo = migrator_appinfo(self, appname);

            if( !g_strcmp0(approval_organization(approval), appinfo_get_organization_name(appinfo)) &&
                !g_strcmp0(approval_application(approval), appinfo_get_application_name(appinfo)) ) {
                appsettings = settings_appsettings(migrator_settings(self), approval_uid(approval), appname);
                stringset_extend(granted, appsettings_get_granted(appsettings));
                stringset_extend(granted, approval_permissions(approval));
                appsettings_set_granted(appsettings, granted);
                appsettings_set_allowed(appsettings, APP_ALLOWED_ALWAYS);
                migrated = true;
                log_info("%s migrated", path);
            }
        }
    }

    if( !migrated )
        log_warning("%s was not migrated", path);

    migrator_removal_enqueue(self, path);

    stringset_delete(granted);
    approval_delete(approval);
    g_free(appname);
}

/* ------------------------------------------------------------------------- *
 * MIGRATOR_SLOTS
 * ------------------------------------------------------------------------- */

void
migrator_on_settings_saved(migrator_t *self)
{
    log_notice("*** settings saved notification");
    migrator_removal_dequeue_all(self);
    migrator_eval_state_later(self);
}

/* ------------------------------------------------------------------------- *
 * APPROVAL
 * ------------------------------------------------------------------------- */

struct approval_t {
    const migrator_t *apr_migrator;
    gchar            *apr_profile;
    uid_t             apr_uid;
    stringset_t      *apr_permissions;
    gchar            *apr_organization;
    gchar            *apr_application;
};

static void
approval_ctor(approval_t *self, const migrator_t *migrator, const gchar *approval_file)
{
    GKeyFile *keyfile = g_key_file_new();

    self->apr_migrator = migrator;
    self->apr_profile = profile_from_approval_path(approval_file);
    self->apr_uid = uid_from_approval_path(approval_file, approval_control(self));

    if( keyfile_load(keyfile, approval_file) ) {
        self->apr_permissions = keyfile_get_stringset(keyfile, SAILJAIL_KEY_PERMISSIONS,
                                                      SAILJAIL_KEY_PERMISSIONS);
        self->apr_organization = keyfile_get_string(keyfile, SAILJAIL_KEY_PERMISSIONS,
                                                    SAILJAIL_KEY_ORGANIZATION_NAME, NULL);
        self->apr_application = keyfile_get_string(keyfile, SAILJAIL_KEY_PERMISSIONS,
                                                   SAILJAIL_KEY_APPLICATION_NAME, NULL);
    } else {
        self->apr_uid = MIGRATION_UID_UNDEFINED;
        self->apr_permissions = NULL;
        self->apr_organization = NULL;
        self->apr_application = NULL;
    }

    g_key_file_unref(keyfile);
    log_info("approval(%s.%s) created", approval_organization(self), approval_application(self));
}

static void
approval_dtor(approval_t *self)
{
    log_info("approval(%s.%s) deleted", approval_organization(self), approval_application(self));
    change_string(&self->apr_profile, NULL);
    self->apr_uid = MIGRATION_UID_UNDEFINED;
    stringset_delete_at(&self->apr_permissions);
    change_string(&self->apr_organization, NULL);
    change_string(&self->apr_application, NULL);
}

approval_t *
approval_create(const migrator_t *migrator, const gchar *approval_file)
{
    approval_t *self = g_malloc0(sizeof *self);
    approval_ctor(self, migrator, approval_file);
    return self;
}

void
approval_delete(approval_t *self)
{
    if( self ) {
        approval_dtor(self);
        g_free(self);
    }
}

void
approval_delete_cb(void *self)
{
    approval_delete(self);
}

/* ------------------------------------------------------------------------- *
 * APPROVAL_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static bool
approval_valid(const approval_t *self)
{
    return approval_uid(self) != MIGRATION_UID_UNDEFINED;
}

static const gchar *
approval_profile(const approval_t *self)
{
    /* Application profile path, i.e. application's desktop entry file path */
    return self->apr_profile;
}

static uid_t
approval_uid(const approval_t *self)
{
    /* UID from approval file path */
    return self->apr_uid;
}

static stringset_t *
approval_permissions(const approval_t *self)
{
    /* Approved permissions */
    return self->apr_permissions;
}

static const gchar *
approval_organization(const approval_t *self)
{
    /* The same as OrganizationName in Sailjail section */
    return self->apr_organization;
}

static const gchar *
approval_application(const approval_t *self)
{
    /* The same as ApplicationName in Sailjail section */
    return self->apr_application;
}

static const migrator_t *
approval_migrator(const approval_t *self)
{
    return self->apr_migrator;
}

static const control_t *
approval_control(const approval_t *self)
{
    return migrator_control(approval_migrator(self));
}

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static void
remove_approval_path(const gchar *path)
{
    /* Remove path and any empty parent directories until HOMESCREEN_DATA_PATH */
    if( unlink(path) == -1 ) {
        log_err("%s could not remove: %m", path);
    }
    else {
        gchar *dir_path = path_dirname(path);
        while( may_remove_approval_path(dir_path) && rmdir(dir_path) == 0 ) {
            gchar *temp = dir_path;
            dir_path = path_dirname(temp);
            g_free(temp);
        }
        log_debug("%s cleaned up to %s", path, dir_path);
        g_free(dir_path);
    }
}

static bool
may_remove_approval_path(const gchar *path)
{
    path = without_leading_data_path(path);
    return path && *path;
}

static gchar *
profile_from_approval_path(const gchar *path)
{
    /* Returns application profile path from approval file path, i.e.
     * "/var/lib/sailjail-homescreen/UID/usr/share/applications/Y.desktop/X-Sailjail"
     * returns "/usr/share/applications/Y.desktop".
     */
    path = without_leading_data_path(path);
    if( path )
        path = strchr(path, '/');
    return path_dirname(path);
}

static uid_t
uid_from_approval_path(const gchar *path, const control_t *control)
{
    gint64 uid = MIGRATION_UID_UNDEFINED;
    path = without_leading_data_path(path);
    if( path ) {
        uid = g_ascii_strtoll(path, NULL, 10);
        if( uid < control_min_user(control) ||
            uid > control_max_user(control) )
            return MIGRATION_UID_UNDEFINED;
    }
    return (uid_t)uid;
}

static const gchar *
without_leading_data_path(const gchar *path)
{
    /* Check that path begins with HOMESCREEN_DATA_PATH and then return
     * the part that begins with the uid (no slash at the beginning).
     */
    size_t prefix_length = strlen(HOMESCREEN_DATA_PATH "/");
    if( path && !strncmp(path, HOMESCREEN_DATA_PATH "/", prefix_length) )
        return path + prefix_length;
    return NULL;
}
