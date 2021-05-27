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

#ifndef  SETTINGS_H_
# define SETTINGS_H_

# include <stdbool.h>
# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct config_t       config_t;
typedef struct settings_t     settings_t;
typedef struct usersettings_t usersettings_t;
typedef struct appsettings_t  appsettings_t;
typedef struct control_t      control_t;
typedef struct stringset_t    stringset_t;

typedef enum
{
    APP_ALLOWED_UNSET,
    APP_ALLOWED_ALWAYS,
    APP_ALLOWED_NEVER,
    APP_ALLOWED_COUNT
    // keep app_allowed_name[] in sync
} app_allowed_t;

typedef enum
{
    APP_AGREED_UNSET,
    APP_AGREED_YES,
    APP_AGREED_NO,
    APP_AGREED_COUNT
    // keep app_agreed_name[] in sync
} app_agreed_t;

typedef enum {
    APP_GRANT_DEFAULT, // Take default
    APP_GRANT_ALWAYS,  // Always allow all permissions
    APP_GRANT_LAUNCH,  // Allow launching, user may control permissions
    APP_GRANT_COUNT
    // keep app_grant_name[] in sync
} app_grant_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SETTINGS
 * ------------------------------------------------------------------------- */

settings_t *settings_create   (const config_t *config, control_t *control);
void        settings_delete   (settings_t *self);
void        settings_delete_at(settings_t **pself);
void        settings_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * SETTINGS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

appsettings_t *settings_appsettings(settings_t *self, uid_t uid, const char *app);

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

void settings_load_all  (settings_t *self);
void settings_save_all  (const settings_t *self);
void settings_load_user (settings_t *self, uid_t uid);
void settings_save_user (const settings_t *self, uid_t uid);
void settings_save_later(settings_t *self, uid_t uid);

/* ------------------------------------------------------------------------- *
 * SETTINGS_RETHINK
 * ------------------------------------------------------------------------- */

void settings_rethink(settings_t *self);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS
 * ------------------------------------------------------------------------- */

usersettings_t *usersettings_create   (settings_t *settings, uid_t uid);
void            usersettings_delete   (usersettings_t *self);
void            usersettings_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_APPSETTINGS
 * ------------------------------------------------------------------------- */

appsettings_t *usersettings_get_appsettings   (const usersettings_t *self, const gchar *appname);
appsettings_t *usersettings_add_appsettings   (usersettings_t *self, const gchar *appname);
bool           usersettings_remove_appsettings(usersettings_t *self, const gchar *appname);

/* ------------------------------------------------------------------------- *
 * USERSETTINGS_STORAGE
 * ------------------------------------------------------------------------- */

void usersettings_load(usersettings_t *self, const char *path);
void usersettings_save(const usersettings_t *self, const char *path);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS
 * ------------------------------------------------------------------------- */

appsettings_t *appsettings_create   (usersettings_t *usersettings, const char *appname);
void           appsettings_delete   (appsettings_t *self);
void           appsettings_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * APPSETTINGS_PROPERTIES
 * ------------------------------------------------------------------------- */

app_allowed_t      appsettings_get_allowed(const appsettings_t *self);
app_agreed_t       appsettings_get_agreed (const appsettings_t *self);
const stringset_t *appsettings_get_granted(appsettings_t *self);
void               appsettings_set_allowed(appsettings_t *self, app_allowed_t allowed);
void               appsettings_set_agreed (appsettings_t *self, app_agreed_t agreed);
void               appsettings_set_granted(appsettings_t *self, const stringset_t *granted);

G_END_DECLS

#endif /* SETTINGS_H_ */
