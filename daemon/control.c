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

#include "control.h"

#include "logging.h"
#include "users.h"
#include "permissions.h"
#include "stringset.h"
#include "session.h"
#include "applications.h"
#include "settings.h"
#include "service.h"
#include "appservices.h"
#include "prompter.h"
#include "later.h"

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct config_t config_t;
typedef struct control_t control_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * CONTROL
 * ------------------------------------------------------------------------- */

static void  control_ctor     (control_t *self, const config_t *config);
static void  control_dtor     (control_t *self);
control_t   *control_create   (const config_t *config);
void         control_delete   (control_t *self);
void         control_delete_at(control_t **pself);
void         control_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * CONTROL_ATTRIBUTES
 * ------------------------------------------------------------------------- */

const config_t    *control_config                (const control_t *self);
users_t           *control_users                 (const control_t *self);
session_t         *control_session               (const control_t *self);
permissions_t     *control_permissions           (const control_t *self);
applications_t    *control_applications          (const control_t *self);
service_t         *control_service               (const control_t *self);
static prompter_t *control_prompter              (const control_t *self);
settings_t        *control_settings              (const control_t *self);
appservices_t     *control_appservices           (const control_t *self);
appsettings_t     *control_appsettings           (control_t *self, uid_t uid, const char *app);
appinfo_t         *control_appinfo               (const control_t *self, const char *appname);
uid_t              control_current_user          (const control_t *self);
bool               control_valid_user            (const control_t *self, uid_t uid);
uid_t              control_min_user              (const control_t *self);
uid_t              control_max_user              (const control_t *self);
bool               control_user_is_guest         (const control_t *self, uid_t uid);
const stringset_t *control_available_permissions (const control_t *self);
bool               control_valid_permission      (const control_t *self, const char *perm);
const stringset_t *control_available_applications(const control_t *self);
bool               control_valid_application     (const control_t *self, const char *appname);

/* ------------------------------------------------------------------------- *
 * CONTROL_SLOTS
 * ------------------------------------------------------------------------- */

void control_on_users_changed     (control_t *self);
void control_on_session_changed   (control_t *self);
void control_on_permissions_change(control_t *self);
void control_on_application_change(control_t *self, GHashTable *changed);
void control_on_settings_change   (control_t *self, const char *app);
void control_on_appservices_change(control_t *self);

/* ------------------------------------------------------------------------- *
 * CONTROL_RETHINK
 * ------------------------------------------------------------------------- */

static void control_rethink_applications_cb(gpointer aptr);
static void control_rethink_settings_cb    (gpointer aptr);
static void control_rethink_prompter_cb    (gpointer aptr);
static void control_rethink_broadcast_cb   (gpointer aptr);
static void control_rethink_appservices_cb (gpointer aptr);
static void control_rethink_dbusconfig_cb  (gpointer aptr);

/* ========================================================================= *
 * CONTROL
 * ========================================================================= */

struct control_t
{
    const config_t *ctl_config;

    uid_t           ctl_session_user;

    stringset_t    *ctl_changed_applications;
    later_t        *ctl_rethink_applications;
    later_t        *ctl_rethink_settings;
    later_t        *ctl_rethink_prompter;
    later_t        *ctl_rethink_broadcast;
    later_t        *ctl_rethink_appservices;
    later_t        *ctl_rethink_dbusconfig;

    users_t        *ctl_users;
    session_t      *ctl_session;
    permissions_t  *ctl_permissions;
    applications_t *ctl_applications;
    settings_t     *ctl_settings;
    service_t      *ctl_service;
    appservices_t  *ctl_appservices;
};

/* ========================================================================= *
 * CONTROL
 * ========================================================================= */

static void
control_ctor(control_t *self, const config_t *config)
{
    log_info("control() create");
    self->ctl_config = config;
    self->ctl_session_user = SESSION_UID_UNDEFINED;

    /* Init re-evaluation pipeline */
    self->ctl_changed_applications = stringset_create();
    self->ctl_rethink_applications =
        later_create("applications", 0, 0,
                     control_rethink_applications_cb, self);
    self->ctl_rethink_settings =
        later_create("settings", 10, 0,
                     control_rethink_settings_cb, self);
    self->ctl_rethink_prompter =
        later_create("prompter", 20, 0,
                     control_rethink_prompter_cb, self);
    self->ctl_rethink_broadcast  =
        later_create("broadcast", 30, 0,
                     control_rethink_broadcast_cb, self);
    self->ctl_rethink_appservices  =
        later_create("appservices", 40, 0,
                     control_rethink_appservices_cb, self);
    self->ctl_rethink_dbusconfig  =
        later_create("dbusconfig", 50, 0,
                     control_rethink_dbusconfig_cb, self);

    /* Init data tracking */
    self->ctl_users        = users_create(self);
    self->ctl_session      = session_create(self);
    self->ctl_permissions  = permissions_create(self);
    self->ctl_applications = applications_create(self);
    self->ctl_settings     = settings_create(config, self);

    /* Set correct session user */
    self->ctl_session_user = session_current_user(control_session(self));

    /* Init D-Bus service */
    self->ctl_service      = service_create(self);

    self->ctl_appservices  = appservices_create(self);
}

static void
control_dtor(control_t *self)
{
    log_info("control() delete");

    /* Quit D-Bus service */
    service_delete_at(&self->ctl_service);

    /* Quit data tracking */
    settings_delete_at(&self->ctl_settings);
    applications_delete_at(&self->ctl_applications);
    permissions_delete_at(&self->ctl_permissions);
    session_delete_at(&self->ctl_session);
    appservices_delete_at(&self->ctl_appservices);
    users_delete_at(&self->ctl_users);

    /* Quit re-evaluation pipeline */
    later_delete_at(&self->ctl_rethink_dbusconfig);
    later_delete_at(&self->ctl_rethink_appservices);
    later_delete_at(&self->ctl_rethink_broadcast);
    later_delete_at(&self->ctl_rethink_prompter);
    later_delete_at(&self->ctl_rethink_settings);
    later_delete_at(&self->ctl_rethink_applications);
    stringset_delete_at(&self->ctl_changed_applications);
}

control_t *
control_create(const config_t *config)
{
    control_t *self = g_malloc0(sizeof *self);
    control_ctor(self, config);
    return self;
}

void
control_delete(control_t *self)
{
    if( self ) {
        control_dtor(self);
        g_free(self);
    }
}

void
control_delete_at(control_t **pself)
{
    control_delete(*pself), *pself = NULL;
}

void
control_delete_cb(void *self)
{
    control_delete(self);
}

/* ------------------------------------------------------------------------- *
 * CONTROL_ATTRIBUTES
 * ------------------------------------------------------------------------- */

const config_t *
control_config(const control_t *self)
{
    return self->ctl_config;
}

users_t *
control_users(const control_t *self)
{
    return self->ctl_users;
}

session_t *
control_session(const control_t *self)
{
    return self->ctl_session;
}

permissions_t *
control_permissions(const control_t *self)
{
    return self->ctl_permissions;
}

applications_t *
control_applications(const control_t *self)
{
    return self->ctl_applications;
}

service_t *
control_service(const control_t *self)
{
    return self->ctl_service;
}

static prompter_t *
control_prompter(const control_t *self)
{
    return service_prompter(control_service(self));
}

settings_t *
control_settings(const control_t *self)
{
    return self->ctl_settings;
}

appservices_t *
control_appservices(const control_t *self)
{
    return self->ctl_appservices;
}

appsettings_t *
control_appsettings(control_t *self, uid_t uid, const char *app)
{
    return settings_appsettings(control_settings(self), uid, app);
}

appinfo_t *
control_appinfo(const control_t *self, const char *appname)
{
    return applications_appinfo(control_applications(self), appname);
}

uid_t
control_current_user(const control_t *self)
{
    return session_current_user(control_session(self));
}

bool
control_valid_user(const control_t *self, uid_t uid)
{
    /* Guest user is considered invalid if it doesn't have an active
     * user session.
     */
    if( control_user_is_guest(self, uid) && control_current_user(self) != uid )
        return false;
    return users_user_exists(control_users(self), uid);
}

uid_t
control_min_user(const control_t *self)
{
    return users_first_user(control_users(self));
}

uid_t
control_max_user(const control_t *self)
{
    return users_last_user(control_users(self));
}

bool
control_user_is_guest(const control_t *self, uid_t uid)
{
    return users_user_is_guest(control_users(self), uid);
}

const stringset_t *
control_available_permissions(const control_t *self)
{
    return permissions_available(control_permissions(self));
}

bool
control_valid_permission(const control_t *self, const char *perm)
{
    return stringset_has_item(control_available_permissions(self), perm);
}

const stringset_t *
control_available_applications(const control_t *self)
{
    return applications_available(control_applications(self));
}

bool
control_valid_application(const control_t *self, const char *appname)
{
    return stringset_has_item(control_available_applications(self), appname);
}

/* ------------------------------------------------------------------------- *
 * CONTROL_SLOTS
 * ------------------------------------------------------------------------- */

void
control_on_users_changed(control_t *self)
{
    log_notice("*** users changed notification");

    users_t *users = control_users(self);
    uid_t first = users_first_user(users);
    uid_t last  = users_last_user(users);

    for( uid_t uid = first; uid <= last; ++uid ) {
        log_notice("uid[%u] = %s",
                   (unsigned)uid,
                   users_user_exists(users, uid) ? "exists" : "n/a");
    }

    later_schedule(self->ctl_rethink_settings);
    // -> control_rethink_settings_cb()
}

void
control_on_session_changed(control_t *self)
{
    log_notice("*** session changed notification");

    session_t *session = control_session(self);

    /* To drop guest user settings from memory when guest user session ends */
    if( control_user_is_guest(self, self->ctl_session_user) )
        later_schedule(self->ctl_rethink_settings);
        // -> control_rethink_settings_cb()

    later_schedule(self->ctl_rethink_prompter);
    // -> control_rethink_prompter_cb()

    later_schedule(self->ctl_rethink_appservices);
    // -> control_rethink_appservices_cb()

    self->ctl_session_user = session_current_user(session);
    log_notice("session uid = %d", (int)self->ctl_session_user);

    const char *path = "/etc/sailjail/config/user-grantlist.conf";
    GError *err = NULL;
    GKeyFile *file = g_key_file_new();
    bool ret = g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, &err);
    if ( ret ) {
        gchar **keys = g_key_file_get_keys(file, "Grantlist", NULL, NULL);
        if ( keys ) {
            for ( size_t i = 0; keys[i]; ++i ) {
                stringset_t *granted = keyfile_get_stringset(file, "Grantlist", keys[i]);
                appsettings_t *appsettings = control_appsettings(self, session_current_user(session), keys[i]);
                appsettings_set_granted(appsettings, granted);
            }
        }
        g_strfreev(keys);
    }
    g_key_file_unref(file);
    g_clear_error(&err);
}

void
control_on_permissions_change(control_t *self)
{
    log_notice("*** permissions changed notification");

    permissions_t *permissions = control_permissions(self);
    gchar *perms = stringset_to_string(permissions_available(permissions));
    log_notice("available permissions = %s", perms);
    g_free(perms);

    later_schedule(self->ctl_rethink_applications);
    // -> control_rethink_applications_cb()
}

void
control_on_application_change(control_t *self, GHashTable *changed)
{
    log_notice("*** applications changed notification");

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, changed);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        log_debug("application change: %s", (char *)key);
        stringset_add_item(self->ctl_changed_applications, key);
    }

    later_schedule(self->ctl_rethink_settings);
    // -> control_rethink_settings_cb()
    later_schedule(self->ctl_rethink_broadcast);
    // -> control_rethink_broadcast_cb()
}

void
control_on_settings_change(control_t *self, const char *app)
{
    log_notice("*** settings changed notification: %s", app);
    stringset_add_item(self->ctl_changed_applications, app);
    later_schedule(self->ctl_rethink_broadcast);
    // -> control_rethink_broadcast_cb()
}

void
control_on_appservices_change(control_t *self)
{
    log_notice("*** app services changed notification");
    later_schedule(self->ctl_rethink_dbusconfig);
    // -> control_rethink_dbusconfig_cb()
}

/* ------------------------------------------------------------------------- *
 * CONTROL_RETHINK
 * ------------------------------------------------------------------------- */

static void
control_rethink_applications_cb(gpointer aptr)
{
    log_notice("*** rethink applications data");
    control_t *self = aptr;
    applications_rethink(control_applications(self));
    // -> control_on_application_change()
}

static void
control_rethink_settings_cb(gpointer aptr)
{
    log_notice("*** rethink settings data");
    control_t *self = aptr;
    settings_rethink(control_settings(self));
    // -> control_on_settings_change()
}

static void
control_rethink_prompter_cb(gpointer aptr)
{
    log_notice("*** rethink prompter data");
    control_t *self = aptr;
    prompter_session_changed(control_prompter(self));
}

static void
control_rethink_broadcast_cb(gpointer aptr)
{
    log_notice("*** rethink broadcast data");
    control_t *self = aptr;
    service_applications_changed(control_service(self),
                                 self->ctl_changed_applications);
    stringset_clear(self->ctl_changed_applications);
}

static void
control_rethink_appservices_cb(gpointer aptr)
{
    log_notice("*** rethink appservices data");
    control_t *self = aptr;
    appservices_rethink(control_appservices(self));
}

static void
control_rethink_dbusconfig_cb(gpointer aptr)
{
    log_notice("*** rethink dbusconfig");
    control_t *self = aptr;
    prompter_dbus_reload_config(service_prompter(control_service(self)));
}
