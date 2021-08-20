/*
 * Copyright (c) 2013 - 2020 Jolla Ltd.
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

#include "appinfo.h"

#include "applications.h"
#include "control.h"
#include "config.h"
#include "stringset.h"
#include "logging.h"
#include "util.h"

#include <sys/stat.h>

#include <unistd.h>
#include <errno.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef enum
{
    APPINFO_STATE_UNSET,
    APPINFO_STATE_VALID,
    APPINFO_STATE_INVALID,
    APPINFO_STATE_DELETED,
} appinfo_state_t;

typedef enum
{
    APPINFO_FILE_UNCHANGED,  // Nothing has changed
    APPINFO_FILE_CHANGED,    // File timestamp has changed
    APPINFO_FILE_INVALID,    // File is inaccessible => error
    APPINFO_FILE_DELETED,    // File existed but was removed
    APPINFO_FILE_MISSING,    // File doesn't exist and was not read on previous scan
} appinfo_file_t;

typedef enum
{
    APPINFO_DIR_MAIN,
    APPINFO_DIR_ALT,
    APPINFO_DIR_COUNT
} appinfo_dir_t;

static const char * const appinfo_state_name[] =
{
    [APPINFO_STATE_UNSET]   = "UNSET",
    [APPINFO_STATE_VALID]   = "VALID",
    [APPINFO_STATE_INVALID] = "INVALID",
    [APPINFO_STATE_DELETED] = "DELETED",
};

typedef struct
{
    const char *key;
    void (*set)(appinfo_t *, const char *);
} appinfo_parser_t;

/* ========================================================================= *
 * Config
 * ========================================================================= */

#define APPINFO_DEFAULT_PROFILE_SECTION "Default Profile"
#define APPINFO_KEY_ENABLED             "Enabled"

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * APPINFO
 * ------------------------------------------------------------------------- */

static void  appinfo_ctor      (appinfo_t *self, applications_t *applications, const gchar *id);
static void  appinfo_dtor      (appinfo_t *self);
appinfo_t   *appinfo_create    (applications_t *applications, const gchar *id);
void         appinfo_delete    (appinfo_t *self);
void         appinfo_delete_cb (void *self);
gboolean     appinfo_equal     (const appinfo_t *self, const appinfo_t *that);
gboolean     appinfo_equal_cb  (gconstpointer a, gconstpointer b);
guint        appinfo_hash      (const appinfo_t *self);
guint        appinfo_hash_cb   (gconstpointer key);
GVariant    *appinfo_to_variant(const appinfo_t *self);
gchar       *appinfo_to_string (const appinfo_t *self);

/* ------------------------------------------------------------------------- *
 * APPINFO_ATTRIBUTE
 * ------------------------------------------------------------------------- */

bool            appinfo_valid          (const appinfo_t *self);
control_t      *appinfo_control        (const appinfo_t *self);
const config_t *appinfo_config         (const appinfo_t *self);
applications_t *appinfo_applications   (const appinfo_t *self);
const gchar    *appinfo_id             (const appinfo_t *self);
bool            appinfo_dbus_auto_start(const appinfo_t *self);

/* ------------------------------------------------------------------------- *
 * APPINFO_PROPERTY
 * ------------------------------------------------------------------------- */

static void             appinfo_set_dirty            (appinfo_t *self);
static bool             appinfo_clear_dirty          (appinfo_t *self);
static appinfo_state_t  appinfo_get_state            (const appinfo_t *self);
static void             appinfo_set_state            (appinfo_t *self, appinfo_state_t state);
const gchar            *appinfo_get_name             (const appinfo_t *self);
const gchar            *appinfo_get_type             (const appinfo_t *self);
const gchar            *appinfo_get_icon             (const appinfo_t *self);
const gchar            *appinfo_get_exec             (const appinfo_t *self);
bool                    appinfo_get_no_display       (const appinfo_t *self);
const gchar            *appinfo_get_service          (const appinfo_t *self);
const gchar            *appinfo_get_object           (const appinfo_t *self);
const gchar            *appinfo_get_method           (const appinfo_t *self);
const gchar            *appinfo_get_organization_name(const appinfo_t *self);
const gchar            *appinfo_get_application_name (const appinfo_t *self);
const gchar            *appinfo_get_exec_dbus        (const appinfo_t *self);
const gchar            *appinfo_get_data_directory   (const appinfo_t *self);
app_mode_t              appinfo_get_mode             (const appinfo_t *self);
static const gchar     *appinfo_get_mode_string      (const appinfo_t *self);
void                    appinfo_set_name             (appinfo_t *self, const gchar *name);
void                    appinfo_set_type             (appinfo_t *self, const gchar *type);
void                    appinfo_set_icon             (appinfo_t *self, const gchar *icon);
void                    appinfo_set_exec             (appinfo_t *self, const gchar *exec);
void                    appinfo_set_no_display       (appinfo_t *self, bool no_display);
void                    appinfo_set_service          (appinfo_t *self, const gchar *service);
void                    appinfo_set_object           (appinfo_t *self, const gchar *object);
void                    appinfo_set_method           (appinfo_t *self, const gchar *method);
void                    appinfo_set_organization_name(appinfo_t *self, const gchar *organization_name);
void                    appinfo_set_application_name (appinfo_t *self, const gchar *application_name);
void                    appinfo_set_exec_dbus        (appinfo_t *self, const gchar *exec_dbus);
void                    appinfo_set_data_directory   (appinfo_t *self, const gchar *data_directory);
void                    appinfo_set_mode             (appinfo_t *self, app_mode_t mode);

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

static appinfo_file_t  appinfo_combined_file_state    (appinfo_file_t state1, appinfo_file_t state2);
static appinfo_file_t  appinfo_check_desktop_from_path(appinfo_t *self, const gchar *path, appinfo_dir_t dir);
bool                   appinfo_parse_desktop          (appinfo_t *self);
static gchar          *appinfo_read_exec_dbus         (appinfo_t *self, GKeyFile *ini, const gchar *group);

/* ========================================================================= *
 * APPINFO
 * ========================================================================= */

/* Ref: data merged from all desktop files under /usr/share/applications
 *
 * [Desktop Entry]
 * Type=Application
 * Name=Settings
 * Exec=/usr/bin/sailjail -p voicecall-ui.desktop /usr/bin/voicecall-ui
 * NoDisplay=true
 * Icon=icon-launcher-settings
 * Comment=Sailfish MimeType Handler for Webcal URL
 * X-Desktop-File-Install-Version=0.26
 * X-MeeGo-Logical-Id=settings-ap-name
 * X-MeeGo-Translation-Catalog=settings
 * X-Maemo-Service=com.jolla.settings
 * X-Maemo-Object-Path=/com/jolla/settings/ui
 * X-Maemo-Method=com.jolla.settings.ui.importWebcal
 * MimeType=x-scheme-handler/webcal;x-scheme-handler/webcals;
 * Version=1.0
 * X-Maemo-Fixed-Args=application/x-vpnc
 *
 * [X-Sailjail]
 * Permissions=Phone;CallRecordings;Contacts;Bluetooth;Privileged;Sharing
 * OrganizationName=com.jolla
 * ApplicationName=voicecall
 * ExecDBus=/usr/bin/sailjail -p voicecall-ui.desktop /usr/bin/voicecall-ui -prestart
 */

struct appinfo_t
{
    // uplink
    applications_t  *anf_applications;

    gchar           *anf_appname;
    appinfo_state_t  anf_state;
    time_t           anf_dt_ctime[APPINFO_DIR_COUNT];
    bool             anf_dirty;
    app_mode_t       anf_mode;

    // desktop properties
    gchar           *anf_dt_name;       // DESKTOP_KEY_NAME
    gchar           *anf_dt_type;       // DESKTOP_KEY_TYPE
    gchar           *anf_dt_icon;       // DESKTOP_KEY_ICON
    gchar           *anf_dt_exec;       // DESKTOP_KEY_EXEC
    bool             anf_dt_no_display; // DESKTOP_KEY_NO_DISPLAY

    // maemo properties
    gchar           *anf_mo_service;    // MAEMO_KEY_SERVICE
    gchar           *anf_mo_object;     // MAEMO_KEY_OBJECT
    gchar           *anf_mo_method;     // MAEMO_KEY_METHOD

    // sailjail properties
    gchar           *anf_sj_organization_name; // SAILJAIL_KEY_ORGANIZATION_NAME
    gchar           *anf_sj_application_name;  // SAILJAIL_KEY_APPLICATION_NAME
    gchar           *anf_sj_exec_dbus;         // SAILJAIL_KEY_EXEC_DBUS
    gchar           *anf_sj_data_directory;    // SAILJAIL_KEY_DATA_DIRECTORY
    stringset_t     *anf_sj_permissions_in;    // SAILJAIL_KEY_PERMISSIONS
    stringset_t     *anf_sj_permissions_out;
};

/* Internally used placeholder for NULL string values.
 * Note that this is not exposed over D-Bus.
 */
static const char appinfo_unknown[] = "(undefined)";

static void
appinfo_ctor(appinfo_t *self, applications_t *applications, const gchar *id)
{
    self->anf_applications               = applications;
    self->anf_appname                    = g_strdup(id);

    self->anf_state                      = APPINFO_STATE_UNSET;
    self->anf_dt_ctime[APPINFO_DIR_MAIN] = -1;
    self->anf_dt_ctime[APPINFO_DIR_ALT]  = -1;
    self->anf_dirty                      = false;

    self->anf_mode                       = APP_MODE_NORMAL;

    self->anf_dt_name                    = NULL;
    self->anf_dt_type                    = NULL;
    self->anf_dt_icon                    = NULL;
    self->anf_dt_exec                    = NULL;
    self->anf_dt_no_display              = false;

    self->anf_mo_service                 = NULL;
    self->anf_mo_object                  = NULL;
    self->anf_mo_method                  = NULL;

    self->anf_sj_permissions_in          = stringset_create();
    self->anf_sj_permissions_out         = stringset_create();
    self->anf_sj_organization_name       = NULL;
    self->anf_sj_application_name        = NULL;
    self->anf_sj_exec_dbus               = NULL;
    self->anf_sj_data_directory          = NULL;

    log_info("appinfo(%s): create", appinfo_id(self));
}

static void
appinfo_dtor(appinfo_t *self)
{
    log_info("appinfo(%s): delete", appinfo_id(self));

    appinfo_set_name(self, NULL);
    appinfo_set_type(self, NULL);
    appinfo_set_icon(self, NULL);
    appinfo_set_exec(self, NULL);

    appinfo_set_service(self, NULL);
    appinfo_set_object(self, NULL);
    appinfo_set_method(self, NULL);

    appinfo_set_organization_name(self, NULL);
    appinfo_set_application_name(self, NULL);
    appinfo_set_exec_dbus(self, NULL);
    appinfo_set_data_directory(self, NULL);
    stringset_delete_at(&self->anf_sj_permissions_in);
    stringset_delete_at(&self->anf_sj_permissions_out);

    change_string(&self->anf_appname, NULL);
    self->anf_applications = NULL;
}

appinfo_t *
appinfo_create(applications_t *applications, const gchar *id)
{
    appinfo_t *self = g_malloc0(sizeof *self);
    appinfo_ctor(self, applications, id);
    return self;
}

void
appinfo_delete(appinfo_t *self)
{
    if( self ) {
        appinfo_dtor(self);
        g_free(self);
    }
}

void
appinfo_delete_cb(void *self)
{
    appinfo_delete(self);
}

gboolean
appinfo_equal(const appinfo_t *self, const appinfo_t *that)
{
    return g_str_equal(appinfo_id(self), appinfo_id(that));
}

gboolean
appinfo_equal_cb(gconstpointer a, gconstpointer b)
{
    return appinfo_equal(a, b);
}

guint
appinfo_hash(const appinfo_t *self)
{
    return g_str_hash(appinfo_id(self));
}

guint
appinfo_hash_cb(gconstpointer key)
{
    return appinfo_hash(key);
}

GVariant *
appinfo_to_variant(const appinfo_t *self)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    auto void add_boolean(const char *label, bool value) {
        g_variant_builder_add(builder, "{sv}", label,
                              g_variant_new_boolean(value));
    }

    auto void add_string(const char *label, const char *value) {
        if( value && value != appinfo_unknown ) {
            g_variant_builder_add(builder, "{sv}", label,
                                  g_variant_new_string(value));
        }
    }

    auto void add_stringset(const char *label, const stringset_t *value) {
        if( value ) {
            g_variant_builder_add(builder, "{sv}", label,
                                  stringset_to_variant(value));
        }
    }

    if( self ) {
        add_string("Id", appinfo_id(self));
        add_string("Mode", appinfo_get_mode_string(self));

        /* Desktop properties
         */
        add_string(DESKTOP_KEY_NAME, appinfo_get_name(self));
        add_string(DESKTOP_KEY_TYPE, appinfo_get_type(self));
        add_string(DESKTOP_KEY_ICON, appinfo_get_icon(self));
        add_string(DESKTOP_KEY_EXEC, appinfo_get_exec(self));
        add_boolean(DESKTOP_KEY_NO_DISPLAY, appinfo_get_no_display(self));

        /* Maemo properties
         */
        add_string(MAEMO_KEY_SERVICE, appinfo_get_service(self));
        add_string(MAEMO_KEY_OBJECT, appinfo_get_object(self));
        add_string(MAEMO_KEY_METHOD, appinfo_get_method(self));

        /* Sailjail properties
         */
        add_string(SAILJAIL_KEY_ORGANIZATION_NAME, appinfo_get_organization_name(self));
        add_string(SAILJAIL_KEY_APPLICATION_NAME, appinfo_get_application_name(self));
        add_string(SAILJAIL_KEY_EXEC_DBUS, appinfo_get_exec_dbus(self));
        add_string(SAILJAIL_KEY_DATA_DIRECTORY, appinfo_get_data_directory(self));
        add_stringset(SAILJAIL_KEY_PERMISSIONS, appinfo_get_permissions(self));
    }

    GVariant *variant = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);

    return variant;
}

gchar *
appinfo_to_string(const appinfo_t *self)
{
    gchar    *string  = NULL;
    GVariant *variant = appinfo_to_variant(self);
    if( variant ) {
        string = g_variant_print(variant, false);
        g_variant_unref(variant);
    }
    return string;
}

/* ------------------------------------------------------------------------- *
 * APPINFO_ATTRIBUTE
 * ------------------------------------------------------------------------- */

bool
appinfo_valid(const appinfo_t *self)
{
    return appinfo_get_state(self) == APPINFO_STATE_VALID;
}

control_t *
appinfo_control(const appinfo_t *self)
{
    return applications_control(appinfo_applications(self));
}

const config_t *
appinfo_config(const appinfo_t *self)
{
    return applications_config(appinfo_applications(self));
}

applications_t *
appinfo_applications(const appinfo_t *self)
{
    return self->anf_applications;
}

const gchar *
appinfo_id(const appinfo_t *self)
{
    return self->anf_appname;
}

bool
appinfo_dbus_auto_start(const appinfo_t *self)
{
    return self->anf_state == APPINFO_STATE_VALID &&
            self->anf_sj_organization_name &&
            self->anf_sj_application_name &&
            self->anf_sj_exec_dbus;
}

/* ------------------------------------------------------------------------- *
 * APPINFO_PROPERTY
 * ------------------------------------------------------------------------- */

static void
appinfo_set_dirty(appinfo_t *self)
{
    self->anf_dirty = true;
}

static bool
appinfo_clear_dirty(appinfo_t *self)
{
    bool was_dirty = self->anf_dirty;
    self->anf_dirty = false;
    return was_dirty;
}

static appinfo_state_t
appinfo_get_state(const appinfo_t *self)
{
    return self ? self->anf_state : APPINFO_STATE_DELETED;
}

static void
appinfo_set_state(appinfo_t *self, appinfo_state_t state)
{
    if( self->anf_state != state ) {
        log_debug("appinfo(%s): state: %s -> %s",
                  appinfo_id(self),
                  appinfo_state_name[self->anf_state],
                  appinfo_state_name[state]);
        self->anf_state = state;
        appinfo_set_dirty(self);
    }
}

/* - - - - - - - - - - - - - - - - - - - *
 * Getters
 * - - - - - - - - - - - - - - - - - - - */

const gchar *
appinfo_get_name(const appinfo_t *self)
{
    return self->anf_dt_name ?: appinfo_unknown;
}

const gchar *
appinfo_get_type(const appinfo_t *self)
{
    return self->anf_dt_type ?: appinfo_unknown;
}

const gchar *
appinfo_get_icon(const appinfo_t *self)
{
    return self->anf_dt_icon ?: appinfo_unknown;
}

const gchar *
appinfo_get_exec(const appinfo_t *self)
{
    return self->anf_dt_exec ?: appinfo_unknown;
}

bool
appinfo_get_no_display(const appinfo_t *self)
{
    return self->anf_dt_no_display;
}

const gchar *
appinfo_get_service(const appinfo_t *self)
{
    return self->anf_mo_service ?: appinfo_unknown;
}

const gchar *
appinfo_get_object(const appinfo_t *self)
{
    return self->anf_mo_object ?: appinfo_unknown;
}

const gchar *
appinfo_get_method(const appinfo_t *self)
{
    return self->anf_mo_method ?: appinfo_unknown;
}

const gchar *
appinfo_get_organization_name(const appinfo_t *self)
{
    return self->anf_sj_organization_name ?: appinfo_unknown;
}

const gchar *
appinfo_get_application_name(const appinfo_t *self)
{
    return self->anf_sj_application_name ?: appinfo_unknown;
}

const gchar *
appinfo_get_exec_dbus(const appinfo_t *self)
{
    return self->anf_sj_exec_dbus ?: appinfo_unknown;
}

const gchar *
appinfo_get_data_directory(const appinfo_t *self)
{
    return self->anf_sj_data_directory ?: appinfo_unknown;
}

app_mode_t
appinfo_get_mode(const appinfo_t *self)
{
    return self->anf_mode;
}

static const gchar *
appinfo_get_mode_string(const appinfo_t *self)
{
    static const gchar * const lut[] = {
        [APP_MODE_NORMAL]        = "Normal",
        [APP_MODE_COMPATIBILITY] = "Compatibility",
        [APP_MODE_NONE]          = "None",
    };
    return lut[self->anf_mode];
}

/* - - - - - - - - - - - - - - - - - - - *
 * Setters
 * - - - - - - - - - - - - - - - - - - - */

void
appinfo_set_name(appinfo_t *self, const gchar *name)
{
    if( change_string(&self->anf_dt_name, name) )
        appinfo_set_dirty(self);
}

void
appinfo_set_type(appinfo_t *self, const gchar *type)
{
    if( change_string(&self->anf_dt_type, type) )
        appinfo_set_dirty(self);
}

void
appinfo_set_icon(appinfo_t *self, const gchar *icon)
{
    if( change_string(&self->anf_dt_icon, icon) )
        appinfo_set_dirty(self);
}

void
appinfo_set_exec(appinfo_t *self, const gchar *exec)
{
    if( change_string(&self->anf_dt_exec, exec) )
        appinfo_set_dirty(self);
}

void
appinfo_set_no_display(appinfo_t *self, bool no_display)
{
    if( change_boolean(&self->anf_dt_no_display, no_display) )
        appinfo_set_dirty(self);
}

void
appinfo_set_service(appinfo_t *self, const gchar *service)
{
    if( change_string(&self->anf_mo_service, service) )
        appinfo_set_dirty(self);
}

void
appinfo_set_object(appinfo_t *self, const gchar *object)
{
    if( change_string(&self->anf_mo_object, object) )
        appinfo_set_dirty(self);
}

void
appinfo_set_method(appinfo_t *self, const gchar *method)
{
    if( change_string(&self->anf_mo_method, method) )
        appinfo_set_dirty(self);
}

void
appinfo_set_organization_name(appinfo_t *self, const gchar *organization_name)
{
    if( change_string(&self->anf_sj_organization_name, organization_name) )
        appinfo_set_dirty(self);
}

void
appinfo_set_application_name(appinfo_t *self, const gchar *application_name)
{
    if( change_string(&self->anf_sj_application_name, application_name) )
        appinfo_set_dirty(self);
}

void
appinfo_set_exec_dbus(appinfo_t *self, const gchar *exec_dbus)
{
    if( change_string(&self->anf_sj_exec_dbus, exec_dbus) )
        appinfo_set_dirty(self);
}

void
appinfo_set_data_directory(appinfo_t *self, const gchar *data_directory)
{
    if( change_string(&self->anf_sj_data_directory, data_directory) )
        appinfo_set_dirty(self);
}

void
appinfo_set_mode(appinfo_t *self, app_mode_t mode)
{
    if( self->anf_mode != mode ) {
        self->anf_mode = mode;
        appinfo_set_dirty(self);
    }
}

/* ------------------------------------------------------------------------- *
 * APPINFO_PERMISSIONS
 * ------------------------------------------------------------------------- */

bool
appinfo_has_permission(const appinfo_t *self, const gchar *perm)
{
    return stringset_has_item(appinfo_get_permissions(self), perm);
}

stringset_t *
appinfo_get_permissions(const appinfo_t *self)
{
    return self->anf_sj_permissions_out;
}

bool
appinfo_evaluate_permissions(appinfo_t *self)
{
    bool changed = false;
    const stringset_t *mask = control_available_permissions(appinfo_control(self));
    stringset_t *temp = stringset_filter_in(self->anf_sj_permissions_in, mask);

    if( stringset_assign(self->anf_sj_permissions_out, temp) )
        changed = true;

    stringset_delete(temp);

    return changed;
}

void
appinfo_set_permissions(appinfo_t *self, const stringset_t *in)
{
    stringset_assign(self->anf_sj_permissions_in, in);
    if( appinfo_evaluate_permissions(self) )
        appinfo_set_dirty(self);
}

void
appinfo_clear_permissions(appinfo_t *self)
{
    if( stringset_clear(appinfo_get_permissions(self)) )
        appinfo_set_dirty(self);
}

/* ------------------------------------------------------------------------- *
 * APPINFO_PARSE
 * ------------------------------------------------------------------------- */

static appinfo_file_t
appinfo_combined_file_state(appinfo_file_t state1, appinfo_file_t state2)
{
    /*  state[12] | UNCHANGED |  CHANGED  |  INVALID  |  DELETED  |  MISSING
     * ===========#===========#===========#===========#===========#==========
     *  UNCHANGED | UNCHANGED |  CHANGED  |  INVALID  |  CHANGED  | UNCHANGED
     * ===========#-----------+-----------+-----------+-----------+-----------
     *   CHANGED  |  CHANGED  |  CHANGED  |  INVALID  |  CHANGED  |  CHANGED
     * ===========#-----------+-----------+-----------+-----------+-----------
     *   INVALID  |  INVALID  |  INVALID  |  INVALID  |  INVALID  |  INVALID
     * ===========#-----------+-----------+-----------+-----------+-----------
     *   DELETED  |  CHANGED  |  CHANGED  |  INVALID  |  DELETED  |  DELETED
     * ===========#-----------+-----------+-----------+-----------+-----------
     *   MISSING  | UNCHANGED |  CHANGED  |  INVALID  |  DELETED  |  MISSING
     */

    if( state2 <= APPINFO_FILE_MISSING ) {
        switch (state1) {
        case APPINFO_FILE_UNCHANGED:
            if( state2 == APPINFO_FILE_DELETED )
                return APPINFO_FILE_CHANGED;
            if( state2 == APPINFO_FILE_MISSING )
                return APPINFO_FILE_UNCHANGED;
            return state2;
        case APPINFO_FILE_CHANGED:
            if( state2 == APPINFO_FILE_INVALID )
                return APPINFO_FILE_INVALID;
            return APPINFO_FILE_CHANGED;
        case APPINFO_FILE_INVALID:
            return APPINFO_FILE_INVALID;
        case APPINFO_FILE_DELETED:
            if( state2 <= APPINFO_FILE_CHANGED )
                return APPINFO_FILE_CHANGED;
            if( state2 == APPINFO_FILE_INVALID )
                return APPINFO_FILE_INVALID;
            return APPINFO_FILE_DELETED;
        case APPINFO_FILE_MISSING:
            return state2;
        }
    }
    log_err("Unknown state: (%d, %d)", state1, state2);
    return APPINFO_FILE_INVALID;
}

static appinfo_file_t
appinfo_check_desktop_from_path(appinfo_t *self, const gchar *path, appinfo_dir_t dir)
{
    appinfo_file_t state = APPINFO_FILE_UNCHANGED;

    /* Check if the file has changed since last parse */
    struct stat st = {};
    if( stat(path, &st) == -1 ) {
        if( errno == ENOENT ) {
            log_debug("%s: could not stat: %m", path);
            /* If file existed before, self->anf_dt_ctime[dir] != -1 */
            state = self->anf_dt_ctime[dir] != -1 ? APPINFO_FILE_DELETED : APPINFO_FILE_MISSING;
        }
        else {
            log_warning("%s: could not stat: %m", path);
            state = APPINFO_FILE_INVALID;
        }
        self->anf_dt_ctime[dir] = -1;
        goto EXIT;
    }

    if( self->anf_dt_ctime[dir] == st.st_ctime ) {
        /* Retain current state */
        goto EXIT;
    }

    self->anf_dt_ctime[dir] = st.st_ctime;

    /* Test file readability */
    if( access(path, R_OK) == -1 ) {
        log_warning("%s: not accessible: %m", path);
        state = APPINFO_FILE_INVALID;
        goto EXIT;
    }

    state = APPINFO_FILE_CHANGED;

EXIT:
    return state;
}

bool
appinfo_parse_desktop(appinfo_t *self)
{
    GKeyFile      *ini         = NULL;
    gchar         *path1       = NULL;
    gchar         *path2       = NULL;
    GError        *err         = NULL;
    gchar        **permissions = NULL;
    appinfo_file_t file1_state = 0;
    appinfo_file_t file2_state = 0;
    appinfo_file_t combined    = 0;

    path1 = path_from_desktop_name(appinfo_id(self));
    path2 = alt_path_from_desktop_name(appinfo_id(self));

    file1_state = appinfo_check_desktop_from_path(self, path1, APPINFO_DIR_MAIN);
    file2_state = appinfo_check_desktop_from_path(self, path2, APPINFO_DIR_ALT);
    combined = appinfo_combined_file_state(file1_state, file2_state);

    /* If combined state is CHANGED file(s) must be read, otherwise there is no need */
    if( combined != APPINFO_FILE_CHANGED ) {
        /* Invalid is always INVALID */
        if( combined == APPINFO_FILE_INVALID )
            appinfo_set_state(self, APPINFO_STATE_INVALID);
        /* Both files missing => DELETED */
        else if( combined >= APPINFO_FILE_DELETED )
            appinfo_set_state(self, APPINFO_STATE_DELETED);
        /* Or it could be that no files were changed */
        goto EXIT;
    }

    ini = g_key_file_new();
    if( file1_state <= APPINFO_FILE_CHANGED && !keyfile_merge(ini, path1) ) {
        appinfo_set_state(self, APPINFO_STATE_INVALID);
        goto EXIT;
    }
    if( file2_state <= APPINFO_FILE_CHANGED && !keyfile_merge(ini, path2) ) {
        appinfo_set_state(self, APPINFO_STATE_INVALID);
        goto EXIT;
    }

    //log_debug("appinfo(%s): updating", appinfo_id(self));

    /* Parse desktop properties */
    gchar *tmp;

    tmp = keyfile_get_string(ini, DESKTOP_SECTION, DESKTOP_KEY_NAME, 0),
        appinfo_set_name(self, tmp),
        g_free(tmp);

    tmp = keyfile_get_string(ini, DESKTOP_SECTION, DESKTOP_KEY_TYPE, 0),
        appinfo_set_type(self, tmp),
        g_free(tmp);

    tmp = keyfile_get_string(ini, DESKTOP_SECTION, DESKTOP_KEY_ICON, 0),
        appinfo_set_icon(self, tmp),
        g_free(tmp);

    tmp = keyfile_get_string(ini, DESKTOP_SECTION, DESKTOP_KEY_EXEC, 0),
        appinfo_set_exec(self, tmp),
        g_free(tmp);

    appinfo_set_no_display(self, keyfile_get_boolean(ini, DESKTOP_SECTION,
                                                     DESKTOP_KEY_NO_DISPLAY,
                                                     false));

    /* Parse maemo properties */
    tmp = keyfile_get_string(ini, MAEMO_SECTION, MAEMO_KEY_SERVICE, 0),
        appinfo_set_service(self, tmp),
        g_free(tmp);

    tmp = keyfile_get_string(ini, MAEMO_SECTION, MAEMO_KEY_OBJECT, 0),
        appinfo_set_object(self, tmp),
        g_free(tmp);

    tmp = keyfile_get_string(ini, MAEMO_SECTION, MAEMO_KEY_METHOD, 0),
        appinfo_set_method(self, tmp),
        g_free(tmp);

    /* Parse sailjail properties */
    const gchar *group = NULL;
    if( g_key_file_has_group(ini, SAILJAIL_SECTION_PRIMARY) )
        group = SAILJAIL_SECTION_PRIMARY;
    else if( g_key_file_has_group(ini, SAILJAIL_SECTION_SECONDARY) )
        group = SAILJAIL_SECTION_SECONDARY;
    /* else: legacy app => use default profile */

    /* Sandboxing=Disabled means that the app opts out of sandboxing and
     * launching via sailjail will result in use of compatibility mode.
     */
    gchar *sandboxing = NULL;
    if( group )
        sandboxing = keyfile_get_string(ini, group, SAILJAIL_KEY_SANDBOXING, 0);

    stringset_t *set;
    if( group && g_strcmp0(sandboxing, "Disabled") ) {
        tmp = keyfile_get_string(ini, group, SAILJAIL_KEY_ORGANIZATION_NAME, 0),
            appinfo_set_organization_name(self, tmp),
            g_free(tmp);

        tmp = keyfile_get_string(ini, group, SAILJAIL_KEY_APPLICATION_NAME, 0),
            appinfo_set_application_name(self, tmp),
            g_free(tmp);

        tmp = appinfo_read_exec_dbus(self, ini, group),
            appinfo_set_exec_dbus(self, tmp),
            g_free(tmp);

        tmp = keyfile_get_string(ini, group, SAILJAIL_KEY_DATA_DIRECTORY, 0),
            appinfo_set_data_directory(self, tmp),
            g_free(tmp);

        set = keyfile_get_stringset(ini, group, SAILJAIL_KEY_PERMISSIONS);

        appinfo_set_mode(self, APP_MODE_NORMAL);
    }
    else {
        /* Read default profile from configuration */
        const config_t *config = appinfo_config(self);
        set = config_stringset(config,
                               APPINFO_DEFAULT_PROFILE_SECTION,
                               SAILJAIL_KEY_PERMISSIONS);
        if( !g_strcmp0(sandboxing, "Disabled") ||
            !config_boolean(config,
                            APPINFO_DEFAULT_PROFILE_SECTION,
                            APPINFO_KEY_ENABLED, false) )
            appinfo_set_mode(self, APP_MODE_NONE);
        else
            appinfo_set_mode(self, APP_MODE_COMPATIBILITY);
    }
    appinfo_set_permissions(self, set);
    stringset_delete(set);
    g_free(sandboxing);

    /* Validate */
    if( appinfo_get_name(self) != appinfo_unknown &&
        appinfo_get_type(self) != appinfo_unknown &&
        appinfo_get_exec(self) != appinfo_unknown )
        appinfo_set_state(self, APPINFO_STATE_VALID);
    else
        appinfo_set_state(self, APPINFO_STATE_INVALID);

EXIT:
    g_clear_error(&err);
    g_strfreev(permissions);
    if( ini )
        g_key_file_unref(ini);
    g_free(path1);
    g_free(path2);

    return appinfo_clear_dirty(self);
}

static gchar *
appinfo_read_exec_dbus(appinfo_t *self, GKeyFile *ini, const gchar *group)
{
    gchar *exec = keyfile_get_string(ini, group, SAILJAIL_KEY_EXEC_DBUS, 0);
    if( exec ) {
        /* As in libcontentaction, add invoker to the command line if it doesn't exist already. */
        if( g_strstr_len(exec, -1, "invoker") != exec &&
            g_strstr_len(exec, -1, "/usr/bin/invoker") != exec ) {
            gchar *booster = keyfile_get_string(ini, DESKTOP_SECTION,
                    NEMO_KEY_APPLICATION_TYPE, NULL);
            if( booster == NULL || g_strcmp0(booster, "no-invoker") == 0 ) {
                /* Default booster type is "generic". This can be overridden via
                  "X-Nemo-Application-Type=<boostertype>".
                  "no-invoker" is synonymous to "generic" */
                g_free(booster);
                booster = g_strdup("generic");
            }

            gchar *single_instance = keyfile_get_string(ini, DESKTOP_SECTION,
                    NEMO_KEY_SINGLE_INSTANCE, NULL);

            gchar *tmp = g_strdup_printf("/usr/bin/invoker --type=%s --id=%s %s%s",
                                          booster,
                                          appinfo_id(self),
                                          g_strcmp0(single_instance, "no") ? "--single-instance " : "",
                                          exec);
            g_free(exec);
            g_free(booster);
            g_free(single_instance);

            exec = tmp;
        }
    }
    return exec;
}
