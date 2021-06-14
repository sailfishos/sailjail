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

#ifndef  APPINFO_H_
# define APPINFO_H_

# include <stdbool.h>
# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct stringset_t    stringset_t;
typedef struct applications_t applications_t;
typedef struct appinfo_t      appinfo_t;
typedef struct config_t       config_t;
typedef struct control_t      control_t;

typedef enum {
    APP_MODE_NORMAL,
    APP_MODE_COMPATIBILITY,
    APP_MODE_NONE,
} app_mode_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * APPINFO
 * ------------------------------------------------------------------------- */

appinfo_t *appinfo_create    (applications_t *applications, const gchar *id);
void       appinfo_delete    (appinfo_t *self);
void       appinfo_delete_cb (void *self);
gboolean   appinfo_equal     (const appinfo_t *self, const appinfo_t *that);
gboolean   appinfo_equal_cb  (gconstpointer a, gconstpointer b);
guint      appinfo_hash      (const appinfo_t *self);
guint      appinfo_hash_cb   (gconstpointer key);
GVariant  *appinfo_to_variant(const appinfo_t *self);
gchar     *appinfo_to_string (const appinfo_t *self);

/* ------------------------------------------------------------------------- *
 * APPINFO_ATTRIBUTE
 * ------------------------------------------------------------------------- */

bool            appinfo_valid       (const appinfo_t *self);
control_t      *appinfo_control     (const appinfo_t *self);
const config_t *appinfo_config      (const appinfo_t *self);
applications_t *appinfo_applications(const appinfo_t *self);
const gchar    *appinfo_id          (const appinfo_t *self);

/* ------------------------------------------------------------------------- *
 * APPINFO_PROPERTY
 * ------------------------------------------------------------------------- */

const gchar *appinfo_get_name             (const appinfo_t *self);
const gchar *appinfo_get_type             (const appinfo_t *self);
const gchar *appinfo_get_icon             (const appinfo_t *self);
const gchar *appinfo_get_exec             (const appinfo_t *self);
bool         appinfo_get_no_display       (const appinfo_t *self);
const gchar *appinfo_get_service          (const appinfo_t *self);
const gchar *appinfo_get_object           (const appinfo_t *self);
const gchar *appinfo_get_method           (const appinfo_t *self);
const gchar *appinfo_get_organization_name(const appinfo_t *self);
const gchar *appinfo_get_application_name (const appinfo_t *self);
const gchar *appinfo_get_data_directory   (const appinfo_t *self);
app_mode_t   appinfo_get_mode             (const appinfo_t *self);
void         appinfo_set_name             (appinfo_t *self, const gchar *name);
void         appinfo_set_type             (appinfo_t *self, const gchar *type);
void         appinfo_set_icon             (appinfo_t *self, const gchar *icon);
void         appinfo_set_exec             (appinfo_t *self, const gchar *exec);
void         appinfo_set_no_display       (appinfo_t *self, bool no_display);
void         appinfo_set_service          (appinfo_t *self, const gchar *service);
void         appinfo_set_object           (appinfo_t *self, const gchar *object);
void         appinfo_set_method           (appinfo_t *self, const gchar *method);
void         appinfo_set_organization_name(appinfo_t *self, const gchar *organization_name);
void         appinfo_set_application_name (appinfo_t *self, const gchar *application_name);
void         appinfo_set_data_directory   (appinfo_t *self, const gchar *data_directory);
void         appinfo_set_mode             (appinfo_t *self, app_mode_t mode);

/* ------------------------------------------------------------------------- *
 * APPINFO_PERMISSIONS
 * ------------------------------------------------------------------------- */

bool         appinfo_has_permission      (const appinfo_t *self, const gchar *perm);
stringset_t *appinfo_get_permissions     (const appinfo_t *self);
bool         appinfo_evaluate_permissions(appinfo_t *self);
void         appinfo_set_permissions     (appinfo_t *self, const stringset_t *in);
void         appinfo_clear_permissions   (appinfo_t *self);

/* ------------------------------------------------------------------------- *
 * APPINFO_PARSE
 * ------------------------------------------------------------------------- */

bool appinfo_parse_desktop(appinfo_t *self);

G_END_DECLS

#endif /* APPINFO_H_ */
