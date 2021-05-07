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

#ifndef  CONTROL_H_
# define CONTROL_H_

# include <stdbool.h>
# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct stringset_t    stringset_t;
typedef struct config_t       config_t;
typedef struct users_t        users_t;
typedef struct session_t      session_t;
typedef struct permissions_t  permissions_t;
typedef struct applications_t applications_t;
typedef struct control_t      control_t;
typedef struct service_t      service_t;
typedef struct appinfo_t      appinfo_t;
typedef struct settings_t     settings_t;
typedef struct appsettings_t  appsettings_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * CONTROL
 * ------------------------------------------------------------------------- */

control_t *control_create   (const config_t *config);
void       control_delete   (control_t *self);
void       control_delete_at(control_t **pself);
void       control_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * CONTROL_ATTRIBUTES
 * ------------------------------------------------------------------------- */

const config_t    *control_config                (const control_t *self);
users_t           *control_users                 (const control_t *self);
session_t         *control_session               (const control_t *self);
permissions_t     *control_permissions           (const control_t *self);
applications_t    *control_applications          (const control_t *self);
service_t         *control_service               (const control_t *self);
settings_t        *control_settings              (const control_t *self);
appsettings_t     *control_appsettings           (control_t *self, uid_t uid, const char *app);
appinfo_t         *control_appinfo               (const control_t *self, const char *appname);
uid_t              control_current_user          (const control_t *self);
bool               control_valid_user            (const control_t *self, uid_t uid);
uid_t              control_min_user              (const control_t *self);
uid_t              control_max_user              (const control_t *self);
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

G_END_DECLS

#endif /* CONTROL_H_ */
