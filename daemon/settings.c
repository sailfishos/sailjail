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

#include "settings.h"

#include "util.h"
#include "logging.h"
#include "stringset.h"
#include "control.h"
#include "appinfo.h"
#include "config.h"

#include <errno.h>
#include <unistd.h>
#include <errno.h>

/* ========================================================================= *
 * Config
 * ========================================================================= */

#define APP_CONFIG_ALLOWLIST "Allowlist"

/* ========================================================================= *
 * Types
 * ========================================================================= */

static const char * const app_allowed_name[APP_ALLOWED_COUNT] = {
    [APP_ALLOWED_UNSET] = "UNSET",
    [APP_ALLOWED_ALWAYS]= "ALWAYS",
    [APP_ALLOWED_NEVER] = "NEVER",
};

static const char * const app_agreed_name[APP_AGREED_COUNT] = {
    [APP_AGREED_UNSET] = "UNSET",
    [APP_AGREED_YES]   = "YES",
    [APP_AGREED_NO]    = "NO",
};

static const char * const app_grant_name[APP_GRANT_COUNT] = {
    [APP_GRANT_DEFAULT] = "default",
    [APP_GRANT_ALWAYS]  = "always",
    [APP_GRANT_LAUNCH]  = "launch",
};

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SETTINGS
 * ------------------------------------------------------------------------- */

static void  settings_ctor     (settings_t *self, const config_t *config, control_t *control);
static void  settings_dtor     (settings_t *self);
settings_t  *settings_create   (const config_t *config, control_t *control);
void         settings_delete   (settings_t *self);
void         settings_delete_at(settings_t **pself);
void         settings_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * SETTINGS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static const config_t *settings_config     (const settings_t *self);
static control_t      *settings_control    (const settings_t *self);
appsettings_t         *settings_appsettings(settings_t *self, uid_t uid, const char *app);
static bool            settings_initialized(const settings_t *self);

/* ------------------------------------------------------------------------- *
 * SETTINGS_USERSETTINGS
 * ------------------------------------------------------------------------- */

usersettings_t *settings_get_usersettings   (const settings_t *self, uid_t uid);
usersettings_t *settings_add_usersettings   (settings_t *self, uid_t uid);
bool            settings_remove_usersettings(settings_t *self, uid_t uid);

/* ------------------------------------------------------------------------- *
 * SETTINGS_APPSETTING
 * ------------------------------------------------------------------------- */

appsettings_t *settings_get_appsettings   (const settings_t *self, uid_t uid, const char *appname);
appsettings_t *settings_add_appsettings   (settings_t *self, uid_t uid, const char *appname);
bool           settings_remove_appsettings(settings_t *self, uid_t uid, const char *appname);

/* ------------------------------------------------------------------------- *
 * SETTINGS_STORAGE
 * ------------------------------------------------------------------------- */

void            settings_load_all   (settings_t *self);
void            settings_save_all   (const settings_t *self);
void            settings_load_user  (settings_t *self, uid_t uid);
void            settings_save_user  (const settings_t *self, uid_t uid);
static void     settings_save_now   (settings_t *self);
static gboolean settings_save_cb    (gpointer aptr);
static void     settings_cancel_save(settings_t *self);
void            settings_save_later (settings_t *self, uid_t uid);

/* ------------------------------------------------------------------------- *
 * SETTINGS_RETHINK
 * ------------------------------------------------------------------------- */

void settings_rethink(settings_t *self);

/* ------------------------------------------------------------------------- *
 * SETTINGS_UTILITY
 * ------------------------------------------------------------------------- */

static gchar *settings_userdata_path        (uid_t uid);
static void   settings_remove_stale_userdata(uid_t uid);
static bool   settings_valid_user           (const settings_t *self, uid_t uid);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS
 * ------------------------------------------------------------------------- */

static void     usersettings_ctor     (usersettings_t *self, settings_t *settings, uid_t uid);
static void     usersettings_dtor     (usersettings_t *self);
usersettings_t *usersettings_create   (settings_t *settings, uid_t uid);
void            usersettings_delete   (usersettings_t *self);
void            usersettings_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static settings_t     *usersettings_settings(const usersettings_t *self);
static uid_t           usersettings_uid     (const usersettings_t *self);
static const config_t *usersettings_config  (const usersettings_t *self);
static control_t      *usersettings_control (const usersettings_t *self);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_APPSETTINGS
 * ------------------------------------------------------------------------- */

appsettings_t        *usersettings_get_appsettings   (const usersettings_t *self, const gchar *appname);
static appsettings_t *usersettings_add_appsettings_ex(usersettings_t *self, const gchar *appname, bool rethink);
appsettings_t        *usersettings_add_appsettings   (usersettings_t *self, const gchar *appname);
bool                  usersettings_remove_appsettings(usersettings_t *self, const gchar *appname);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_STORAGE
 * ------------------------------------------------------------------------- */

void usersettings_load(usersettings_t *self, const char *path);
void usersettings_save(const usersettings_t *self, const char *path);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_RETHINK
 * ------------------------------------------------------------------------- */

static void usersettings_rethink(usersettings_t *self);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS
 * ------------------------------------------------------------------------- */

static void    appsettings_ctor     (appsettings_t *self, usersettings_t *usersettings, const char *appname);
static void    appsettings_dtor     (appsettings_t *self);
appsettings_t *appsettings_create   (usersettings_t *usersettings, const char *appname);
void           appsettings_delete   (appsettings_t *self);
void           appsettings_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static const config_t *appsettings_config      (const appsettings_t *self);
static control_t      *appsettings_control     (const appsettings_t *self);
static settings_t     *appsettings_settings    (const appsettings_t *self);
static usersettings_t *appsettings_usersettings(const appsettings_t *self);
static uid_t           appsettings_uid         (appsettings_t *self);
static const gchar    *appsettings_appname     (const appsettings_t *self);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_NOTIFY
 * ------------------------------------------------------------------------- */

static void appsettings_notify_change_ex(appsettings_t *self, bool notify);
static void appsettings_notify_change   (appsettings_t *self);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_PROPERTIES
 * ------------------------------------------------------------------------- */

app_agreed_t              appsettings_get_agreed        (const appsettings_t *self);
static bool               appsettings_update_agreed     (appsettings_t *self, app_agreed_t agreed);
void                      appsettings_set_agreed        (appsettings_t *self, app_agreed_t agreed);
static const stringset_t *appsettings_get_permissions   (const appsettings_t *self);
static int                appsettings_update_permissions(appsettings_t *self, stringset_t *added);
static app_grant_t        appsettings_get_autogrant     (const appsettings_t *self);
static bool               appsettings_update_autogrant  (appsettings_t *self, app_grant_t autogrant);
app_allowed_t             appsettings_get_allowed       (const appsettings_t *self);
bool                      appsettings_update_allowed    (appsettings_t *self, app_allowed_t allowed);
void                      appsettings_set_allowed       (appsettings_t *self, app_allowed_t allowed);
const stringset_t        *appsettings_get_granted       (appsettings_t *self);
static bool               appsettings_update_granted    (appsettings_t *self, const stringset_t *granted);
void                      appsettings_set_granted       (appsettings_t *self, const stringset_t *granted);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_STORAGE
 * ------------------------------------------------------------------------- */

static void appsettings_decode(appsettings_t *self, GKeyFile *file);
static void appsettings_encode(const appsettings_t *self, GKeyFile *file);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_RETHINK
 * ------------------------------------------------------------------------- */

static void appsettings_rethink(appsettings_t *self);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_UTILITY
 * ------------------------------------------------------------------------- */

static app_grant_t appsettings_get_allowlisted(const appsettings_t *self);

/* ========================================================================= *
 * SETTINGS
 * ========================================================================= */

struct settings_t
{
    bool            stt_initialized;
    const config_t *stt_config;
    control_t      *stt_control;
    guint           stt_save_id;
    GHashTable     *stt_users;
    GHashTable     *stt_user_changes;
};

static void
settings_ctor(settings_t *self, const config_t *config, control_t *control)
{
    log_info("settings() created");
    self->stt_initialized  = false;
    self->stt_config       = config;
    self->stt_control      = control;
    self->stt_save_id      = 0;
    self->stt_users        = g_hash_table_new_full(g_direct_hash,
                                                   g_direct_equal,
                                                   NULL,
                                                   usersettings_delete_cb);
    self->stt_user_changes = g_hash_table_new(g_direct_hash, g_direct_equal);

    /* Get initial state */
    settings_load_all(self);

    /* Enable notifications */
    self->stt_initialized  = true;
}

static void
settings_dtor(settings_t *self)
{
    log_info("settings() deleted");
    self->stt_initialized  = false;

    if( self->stt_users ) {
        g_hash_table_unref(self->stt_users),
            self->stt_users = NULL;
    }

    if( self->stt_user_changes ) {
        g_hash_table_unref(self->stt_user_changes),
            self->stt_user_changes = 0;
    }
}

settings_t *
settings_create(const config_t *config, control_t *control)
{
    settings_t *self = g_malloc0(sizeof *self);
    settings_ctor(self, config, control);
    return self;
}

void
settings_delete(settings_t *self)
{
    if( self ) {
        settings_dtor(self);
        g_free(self);
    }
}

void
settings_delete_at(settings_t **pself)
{
    settings_delete(*pself), *pself = NULL;
}

void
settings_delete_cb(void *self)
{
    settings_delete(self);
}

/* ------------------------------------------------------------------------- *
 * SETTINGS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static const config_t *
settings_config(const settings_t *self)
{
    return self->stt_config;
}

static control_t *
settings_control(const settings_t *self)
{
    return self->stt_control;
}

appsettings_t *
settings_appsettings(settings_t *self, uid_t uid, const char *app)
{
    appsettings_t *appsettings = NULL;
    if( settings_valid_user(self, uid) &&
        control_valid_application(settings_control(self), app) )
        appsettings = settings_add_appsettings(self, uid, app);
    return appsettings;
}

static bool
settings_initialized(const settings_t *self)
{
    return self->stt_initialized;
}

/* ------------------------------------------------------------------------- *
 * SETTINGS_USERSETTINGS
 * ------------------------------------------------------------------------- */

usersettings_t *
settings_get_usersettings(const settings_t *self, uid_t uid)
{
    return g_hash_table_lookup(self->stt_users, GINT_TO_POINTER(uid));
}

usersettings_t *
settings_add_usersettings(settings_t *self, uid_t uid)
{
    usersettings_t *usersettings = settings_get_usersettings(self, uid);
    if( !usersettings ) {
        usersettings = usersettings_create(self, uid);
        g_hash_table_insert(self->stt_users, GINT_TO_POINTER(uid),
                            usersettings);
    }
    return usersettings;
}

bool
settings_remove_usersettings(settings_t *self, uid_t uid)
{
    return g_hash_table_remove(self->stt_users, GINT_TO_POINTER(uid));
}

/* ------------------------------------------------------------------------- *
 * SETTINGS_APPSETTING
 * ------------------------------------------------------------------------- */

appsettings_t *
settings_get_appsettings(const settings_t *self, uid_t uid, const char *appname)
{
    appsettings_t *appsettings = NULL;
    usersettings_t *usersettings = settings_get_usersettings(self, uid);
    if( usersettings )
        appsettings = usersettings_get_appsettings(usersettings, appname);
    return appsettings;
}

appsettings_t *
settings_add_appsettings(settings_t *self, uid_t uid, const char *appname)
{
    usersettings_t *usersettings = settings_add_usersettings(self, uid);
    return usersettings_add_appsettings(usersettings, appname);
}

bool
settings_remove_appsettings(settings_t *self, uid_t uid, const char *appname)
{
    bool removed = false;
    usersettings_t *usersettings = settings_get_usersettings(self, uid);
    if( usersettings )
        removed = usersettings_remove_appsettings(usersettings, appname);
    return removed;
}

/* ------------------------------------------------------------------------- *
 * SETTINGS_STORAGE
 * ------------------------------------------------------------------------- */

void
settings_load_all(settings_t *self)
{
    control_t *control = settings_control(self);
    uid_t min_uid = control_min_user(control);
    uid_t max_uid = control_max_user(control);

    for( uid_t uid = min_uid; uid <= max_uid; ++uid )
        settings_load_user(self, uid);
}

void
settings_save_all(const settings_t *self)
{
    control_t *control = settings_control(self);
    uid_t min_uid = control_min_user(control);
    uid_t max_uid = control_max_user(control);

    /* Save settings for all but guest user */
    for( uid_t uid = min_uid; uid <= max_uid; ++uid )
        settings_save_user(self, uid);
}

void
settings_load_user(settings_t *self, uid_t uid)
{
    if( settings_valid_user(self, uid) ) {
        gchar *path = settings_userdata_path(uid);
        usersettings_t *usersettings = settings_add_usersettings(self, uid);
        usersettings_load(usersettings, path);
        g_free(path);
    }
    else {
        settings_remove_usersettings(self, uid);
        settings_remove_stale_userdata(uid);
    }
}

void
settings_save_user(const settings_t *self, uid_t uid)
{
    if( settings_valid_user(self, uid) ) {
        gchar *path = settings_userdata_path(uid);
        usersettings_t *usersettings = settings_get_usersettings(self, uid);
        if( usersettings )
            usersettings_save(usersettings, path);
        g_free(path);
    }
}

static void
settings_save_now(settings_t *self)
{
    settings_cancel_save(self);

    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, self->stt_user_changes);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        uid_t uid = GPOINTER_TO_UINT(key);
        settings_save_user(self, uid);
    }

    g_hash_table_remove_all(self->stt_user_changes);
}

static gboolean
settings_save_cb(gpointer aptr)
{
    settings_t *self = aptr;
    self->stt_save_id = 0;
    settings_save_now(self);
    return G_SOURCE_REMOVE;
}

static void
settings_cancel_save(settings_t *self)
{
    if( self->stt_save_id ) {
        g_source_remove(self->stt_save_id),
            self->stt_save_id = 0;
    }
}

void
settings_save_later(settings_t *self, uid_t uid)
{
    /* Guest user settings are stored only volatile (in-memory) */
    if( !control_user_is_guest(settings_control(self), uid) ) {
        g_hash_table_add(self->stt_user_changes, GINT_TO_POINTER(uid));

        if( !self->stt_save_id ) {
            self->stt_save_id = g_timeout_add(1000, settings_save_cb, self);
        }
    }
}

/* ------------------------------------------------------------------------- *
 * SETTINGS_RETHINK
 * ------------------------------------------------------------------------- */

void
settings_rethink(settings_t *self)
{
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, self->stt_users);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        uid_t uid = usersettings_uid(value);
        if( settings_valid_user(self, uid) ) {
            usersettings_rethink(value);
        }
        else {
            g_hash_table_iter_remove(&iter);
            settings_remove_stale_userdata(uid);
        }
    }
}

/* ------------------------------------------------------------------------- *
 * SETTINGS_UTILITY
 * ------------------------------------------------------------------------- */

static gchar *
settings_userdata_path(uid_t uid)
{
    return g_strdup_printf(SETTINGS_DIRECTORY "/user-%u" SETTINGS_EXTENSION,
                           (unsigned)uid);
}

static void
settings_remove_stale_userdata(uid_t uid)
{
    gchar *path = settings_userdata_path(uid);
    if( unlink(path) == -1 && errno != ENOENT )
        log_err("%s: could not remove: %m", path);
    g_free(path);
}

static bool
settings_valid_user(const settings_t *self, uid_t uid)
{
    return control_valid_user(settings_control(self), uid);
}

/* ========================================================================= *
 * USERSETTINGS
 * ========================================================================= */

struct usersettings_t
{
    settings_t *ust_settings;
    uid_t       ust_uid;
    GHashTable *ust_apps;
};

static void
usersettings_ctor(usersettings_t *self, settings_t *settings, uid_t uid)
{
    self->ust_settings = settings;
    self->ust_uid      = uid;
    self->ust_apps     = g_hash_table_new_full(g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               appsettings_delete_cb);
    log_info("usersettings(%d) created", (int)usersettings_uid(self));
}

static void
usersettings_dtor(usersettings_t *self)
{
    log_info("usersettings(%d) deleted", (int)usersettings_uid(self));
    if( self->ust_apps ) {
        g_hash_table_unref(self->ust_apps),
            self->ust_apps = NULL;
    }
}

usersettings_t *
usersettings_create(settings_t *settings, uid_t uid)
{
    usersettings_t *self = g_malloc0(sizeof *self);
    usersettings_ctor(self, settings, uid);
    return self;
}

void
usersettings_delete(usersettings_t *self)
{
    if( self ) {
        usersettings_dtor(self);
        g_free(self);
    }
}

void
usersettings_delete_cb(void *self)
{
    usersettings_delete(self);
}

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static settings_t *
usersettings_settings(const usersettings_t *self)
{
    return self->ust_settings;
}

static uid_t
usersettings_uid(const usersettings_t *self)
{
    return self->ust_uid;
}

static const config_t *
usersettings_config(const usersettings_t *self)
{
    return settings_config(usersettings_settings(self));
}

static control_t *
usersettings_control(const usersettings_t *self)
{
    return settings_control(usersettings_settings(self));
}

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_APPSETTINGS
 * ------------------------------------------------------------------------- */

appsettings_t *
usersettings_get_appsettings(const usersettings_t *self, const gchar *appname)
{
    return g_hash_table_lookup(self->ust_apps, appname);
}

static appsettings_t *
usersettings_add_appsettings_ex(usersettings_t *self, const gchar *appname,
                                bool rethink)
{
    appsettings_t *appsettings = usersettings_get_appsettings(self, appname);
    if( !appsettings ) {
        appsettings = appsettings_create(self, appname);
        g_hash_table_insert(self->ust_apps, g_strdup(appname), appsettings);
        if( rethink )
            appsettings_rethink(appsettings);
    }
    return appsettings;
}

appsettings_t *
usersettings_add_appsettings(usersettings_t *self, const gchar *appname)
{
    return usersettings_add_appsettings_ex(self, appname, true);
}

bool
usersettings_remove_appsettings(usersettings_t *self, const gchar *appname)
{
    return g_hash_table_remove(self->ust_apps, appname);
}

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_STORAGE
 * ------------------------------------------------------------------------- */

void
usersettings_load(usersettings_t *self, const char *path)
{
    bool apps_changed = false;
    GKeyFile *file = g_key_file_new();
    keyfile_load(file, path);
    gchar **groups = g_key_file_get_groups(file, NULL);
    if( groups ) {
        for( size_t i = 0; groups[i]; ++i ) {
            const char *appname = groups[i];
            if( control_valid_application(usersettings_control(self), appname) ) {
                appsettings_t *appsettings =
                    usersettings_add_appsettings_ex(self, appname, false);
                appsettings_decode(appsettings, file);
            }
            else {
                apps_changed = true;
            }
        }
        g_strfreev(groups);
    }
    g_key_file_unref(file);

    if( apps_changed ) {
        /* Update settings file for removed application(s) */
        settings_save_later(usersettings_settings(self), usersettings_uid(self));
    }
}

void
usersettings_save(const usersettings_t *self, const char *path)
{
    GKeyFile *file = g_key_file_new();
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, self->ust_apps);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        const char *appname = key;
        if( control_valid_application(usersettings_control(self), appname) ) {
            appsettings_t *appsettings = value;
            appsettings_encode(appsettings, file);
        }
        else {
            g_hash_table_iter_remove(&iter);
        }
    }
    keyfile_save(file, path);
    g_key_file_unref(file);
}

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_RETHINK
 * ------------------------------------------------------------------------- */

static void
usersettings_rethink(usersettings_t *self)
{
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, self->ust_apps);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        if( control_valid_application(usersettings_control(self), appsettings_appname(value)) ) {
            appsettings_rethink(value);
        }
        else {
            g_hash_table_iter_remove(&iter);
            settings_save_later(usersettings_settings(self), usersettings_uid(self));
        }
    }
}

/* ========================================================================= *
 * APPSETTINGS
 * ========================================================================= */

struct appsettings_t
{
    usersettings_t *ast_usersettings;
    gchar          *ast_appname;

    app_allowed_t   ast_allowed;
    app_grant_t     ast_autogrant;
    app_agreed_t    ast_agreed;
    stringset_t    *ast_granted;
    stringset_t    *ast_permissions;
};

static void
appsettings_ctor(appsettings_t *self, usersettings_t *usersettings,
                 const char *appname)
{
    self->ast_usersettings = usersettings;
    self->ast_appname      = g_strdup(appname);

    self->ast_allowed      = APP_ALLOWED_UNSET;
    self->ast_autogrant    = APP_GRANT_DEFAULT;
    self->ast_agreed       = APP_AGREED_UNSET;
    self->ast_granted      = stringset_create();
    self->ast_permissions  = stringset_create();

    log_info("appsettings(%d, %s) created",
             (int)usersettings_uid(appsettings_usersettings(self)),
             appsettings_appname(self));

    /* Evaluate derived values to ensure sane initial state */
    appsettings_rethink(self);
}

static void
appsettings_dtor(appsettings_t *self)
{
    log_info("appsettings(%d, %s) deleted",
             (int)usersettings_uid(appsettings_usersettings(self)),
             appsettings_appname(self));

    change_string(&self->ast_appname, NULL);
    stringset_delete_at(&self->ast_granted);
    stringset_delete_at(&self->ast_permissions);
}

appsettings_t *
appsettings_create(usersettings_t *usersettings, const char *appname)
{
    appsettings_t *self = g_malloc0(sizeof *self);
    appsettings_ctor(self, usersettings, appname);
    return self;
}

void
appsettings_delete(appsettings_t *self)
{
    if( self ) {
        appsettings_dtor(self);
        g_free(self);
    }
}

void
appsettings_delete_cb(void *self)
{
    appsettings_delete(self);
}

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static const config_t *
appsettings_config(const appsettings_t *self)
{
    return usersettings_config(appsettings_usersettings(self));
}

static control_t *
appsettings_control(const appsettings_t *self)
{
    return settings_control(appsettings_settings(self));
}

static settings_t *
appsettings_settings(const appsettings_t *self)
{
    return usersettings_settings(appsettings_usersettings(self));
}

static usersettings_t *
appsettings_usersettings(const appsettings_t *self)
{
    return self->ast_usersettings;
}

static uid_t
appsettings_uid(appsettings_t *self)
{
    return usersettings_uid(appsettings_usersettings(self));
}

static const gchar *
appsettings_appname(const appsettings_t *self)
{
    return self->ast_appname;
}

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_NOTIFY
 * ------------------------------------------------------------------------- */

static void
appsettings_notify_change_ex(appsettings_t *self, bool notify)
{
    /* Forward application changes upwards */
    if( notify && settings_initialized(appsettings_settings(self)) )
        control_on_settings_change(appsettings_control(self),
                                   appsettings_appname(self));

    /* Schedule user settings saving */
    settings_save_later(appsettings_settings(self),
                        appsettings_uid(self));
}

static void
appsettings_notify_change(appsettings_t *self)
{
    appsettings_notify_change_ex(self, true);
}

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_PROPERTIES
 * ------------------------------------------------------------------------- */

app_agreed_t
appsettings_get_agreed(const appsettings_t *self)
{
    return self->ast_agreed;
}

static bool
appsettings_update_agreed(appsettings_t *self, app_agreed_t agreed)
{
    bool changed = false;

    /* Sanitize invalid values */
    if( (unsigned)agreed >= APP_AGREED_COUNT  )
        agreed = APP_AGREED_UNSET;

    if( self->ast_agreed != agreed ) {
        log_info("%s(uid=%d): agreed: %s -> %s",
                 appsettings_appname(self),
                 appsettings_uid(self),
                 app_agreed_name[self->ast_agreed],
                 app_agreed_name[agreed]);
        self->ast_agreed = agreed;
        changed = true;
    }

    if( changed )
        appsettings_notify_change(self);

    return changed;
}

void
appsettings_set_agreed(appsettings_t *self, app_agreed_t agreed)
{
    appsettings_update_agreed(self, agreed);
}

static const stringset_t *
appsettings_get_permissions(const appsettings_t *self)
{
    return self->ast_permissions;
}

static int
appsettings_update_permissions(appsettings_t *self, stringset_t *added)
{
    int change = 0; // no change == 0; new perms > 0; other change < 0

    const stringset_t *permissions = NULL;
    stringset_t *none = NULL;

    appinfo_t *appinfo = control_appinfo(appsettings_control(self),
                                         appsettings_appname(self));
    if( appinfo )
        permissions = appinfo_get_permissions(appinfo);

    if( !permissions )
        permissions = none = stringset_create();

    if( !stringset_equal(self->ast_permissions, permissions) ) {
        stringset_t *temp = stringset_filter_out(permissions,
                                                 self->ast_permissions);
        stringset_assign(added, temp);
        stringset_delete(temp);

        change = stringset_empty(added) ? -1 : +1;

        if( log_p(LOG_INFO) ) {
            gchar *prev = stringset_to_string(self->ast_permissions);
            gchar *curr = stringset_to_string(permissions);
            log_info("%s(uid=%d): permissions: %s -> %s%s",
                     appsettings_appname(self),
                     appsettings_uid(self),
                     prev, curr,
                     (change > 0) ? " (new permissions)" : "");
            g_free(prev);
            g_free(curr);
        }
        stringset_assign(self->ast_permissions, permissions);
    }

    /* Note: "permission" is internal cache -> no D-Bus notifications */
    if( change )
        appsettings_notify_change_ex(self, false);

    stringset_delete(none);

    return change;
}

static app_grant_t
appsettings_get_autogrant(const appsettings_t *self)
{
    return self->ast_autogrant;
}

static bool
appsettings_update_autogrant(appsettings_t *self, app_grant_t autogrant)
{
    bool changed = false;
    if( self->ast_autogrant != autogrant ) {
        log_info("%s(uid=%d): autogrant: %s -> %s",
                 appsettings_appname(self),
                 appsettings_uid(self),
                 app_grant_name[self->ast_autogrant],
                 app_grant_name[autogrant]);
        self->ast_autogrant = autogrant;
        changed = true;
    }

    /* Note: "autogrant" is internal cache -> no D-Bus notifications */
    if( changed )
        appsettings_notify_change_ex(self, false);

    return changed;
}

app_allowed_t
appsettings_get_allowed(const appsettings_t *self)
{
    return self->ast_allowed;
}

bool
appsettings_update_allowed(appsettings_t *self, app_allowed_t allowed)
{
    bool changed = false;

    /* Sanitize invalid values */
    if( (unsigned)allowed >= APP_ALLOWED_COUNT )
        allowed = APP_ALLOWED_UNSET;

    /* In case autogrant configuration exists,
     * it takes precedence over everything but APP_ALLOWED_NEVER.
     */
    switch( appsettings_get_autogrant(self) ) {
    case APP_GRANT_ALWAYS:
    case APP_GRANT_LAUNCH:
        if( allowed != APP_ALLOWED_NEVER )
            allowed = APP_ALLOWED_ALWAYS;
        break;
    default:
        break;
    }

    if( self->ast_allowed != allowed ) {
        log_info("%s(uid=%d): allowed: %s -> %s",
                 appsettings_appname(self),
                 appsettings_uid(self),
                 app_allowed_name[self->ast_allowed],
                 app_allowed_name[allowed]);
        self->ast_allowed = allowed;
        changed = true;
    }

    if( changed )
        appsettings_notify_change(self);

    return changed;
}

void
appsettings_set_allowed(appsettings_t *self, app_allowed_t allowed)
{
    if( appsettings_update_allowed(self, allowed) )
        appsettings_update_granted(self, appsettings_get_permissions(self));
}

const stringset_t *
appsettings_get_granted(appsettings_t *self)
{
    return self->ast_granted;
}

static bool
appsettings_update_granted(appsettings_t *self, const stringset_t *granted)
{
    /* Note: This must be kept so that it works also when
     *       'granted' arg is actually the current value,
     *       so that it can be used also for re-evaluating
     *       state after desktop file changes.
     */

    bool changed = false;

    stringset_t *none = NULL;

    if( appsettings_get_allowed(self) != APP_ALLOWED_ALWAYS )
        granted = NULL;

    if( !granted )
        granted = none = stringset_create();

    const stringset_t *permissions = appsettings_get_permissions(self);

    stringset_t *effective = stringset_filter_in(granted, permissions);

    if( !stringset_equal(self->ast_granted, effective) ) {
        if( log_p(LOG_INFO) ) {
            gchar *prev = stringset_to_string(self->ast_granted);
            gchar *curr = stringset_to_string(effective);
            log_info("%s(uid=%d): granted: %s -> %s",
                     appsettings_appname(self),
                     appsettings_uid(self),
                     prev, curr);
            g_free(prev);
            g_free(curr);
        }
        changed = stringset_assign(self->ast_granted, effective);
    }
    stringset_delete(effective);
    stringset_delete(none);

    if( changed )
        appsettings_notify_change(self);

    return changed;
}

void
appsettings_set_granted(appsettings_t *self, const stringset_t *granted)
{
    appsettings_update_granted(self, granted);
}

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_STORAGE
 * ------------------------------------------------------------------------- */

static void
appsettings_decode(appsettings_t *self, GKeyFile *file)
{
    /* Read values as-is.
     *
     * Note that at this stage the values may and are allowed to
     * be in conflict with each other and/or configuration.
     */
    const char *sec = appsettings_appname(self);
    self->ast_allowed = keyfile_get_integer(file, sec, "Allowed",
                                            APP_ALLOWED_UNSET);
    self->ast_agreed  = keyfile_get_integer(file, sec, "Agreed",
                                            APP_AGREED_UNSET);
    self->ast_autogrant = keyfile_get_integer(file, sec, "Autogrant",
                                              APP_GRANT_DEFAULT);

    stringset_delete_at(&self->ast_permissions);
    self->ast_permissions = keyfile_get_stringset(file, sec, "Permissions");

    stringset_delete_at(&self->ast_granted);
    self->ast_granted = keyfile_get_stringset(file, sec, "Granted");

    /* Re-evaluate values that depend on each other */
    appsettings_rethink(self);
}

static void
appsettings_encode(const appsettings_t *self, GKeyFile *file)
{
    const char *sec = appsettings_appname(self);
    keyfile_set_integer(file, sec, "Allowed", self->ast_allowed);
    keyfile_set_integer(file, sec, "Agreed", self->ast_agreed);
    keyfile_set_integer(file, sec, "Autogrant", self->ast_autogrant);
    keyfile_set_stringset(file, sec, "Granted", self->ast_granted);
    keyfile_set_stringset(file, sec, "Permissions", self->ast_permissions);
}

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_RETHINK
 * ------------------------------------------------------------------------- */

static void
appsettings_rethink(appsettings_t *self)
{
    log_info("%s(uid=%d): rethink",
             appsettings_appname(self),
             appsettings_uid(self));

    stringset_t *added = stringset_create();
    int permission_change = appsettings_update_permissions(self, added);

    const stringset_t *permissions = appsettings_get_permissions(self);
    const stringset_t *granted = appsettings_get_granted(self);

    if( appsettings_update_autogrant(self, appsettings_get_allowlisted(self)) ) {
        /* Autogrant config changed: choose all or nothing */
        if( appsettings_get_allowed(self) != APP_ALLOWED_NEVER ) {
            appsettings_update_allowed(self, APP_ALLOWED_UNSET);
            granted = permissions;
        }
    }
    else {
        /* Apply autogrant policy */
        switch( appsettings_get_autogrant(self) ) {
        case APP_GRANT_ALWAYS:
            /* Keep in sync with application requirements */
            granted = permissions;
            break;
        case APP_GRANT_LAUNCH:
            /* Automatically grant just added permissions */
            if( permission_change > 0 ) {
                stringset_extend(added, granted);
                granted = added;
            }
            break;
        default:
            /* Prompt user if new permissions are required */
            if( permission_change > 0 ) {
                if( appsettings_get_allowed(self) != APP_ALLOWED_NEVER )
                    appsettings_update_allowed(self, APP_ALLOWED_UNSET);
            }
        }
    }

    /* Re-evaluate granted list */
    appsettings_update_granted(self, granted);

    stringset_delete(added);
}

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_UTILITY
 * ------------------------------------------------------------------------- */

static app_grant_t
appsettings_get_allowlisted(const appsettings_t *self)
{
    app_grant_t  value = APP_GRANT_DEFAULT;
    bool         found = false;
    gchar       *conf  = config_string(appsettings_config(self),
                                       APP_CONFIG_ALLOWLIST,
                                       appsettings_appname(self),
                                       app_grant_name[APP_GRANT_DEFAULT]);
    for( app_grant_t grant = 0; grant < APP_GRANT_COUNT; ++grant ) {
        if( !strcmp(conf, app_grant_name[grant]) ) {
            value = grant;
            found = true;
            break;
        }
    }
    if( !found )
        log_warning("[" APP_CONFIG_ALLOWLIST "] key %s has invalid value: '%s'",
                    appsettings_appname(self), conf);

    g_free(conf);
    return value;
}
