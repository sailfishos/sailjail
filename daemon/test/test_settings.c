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
#include "appinfo.h"
#include "stringset.h"

#include <glib.h>
#include <locale.h>

/* ========================================================================= *
 * MOCK DATA
 * ========================================================================= */

#define MIN_USER 1000
#define MAX_USER 1000
#define GUEST_USER 1050

typedef struct {
    bool mck_guest_valid;
    const gchar *mck_allowlist_value;
    stringset_t *mck_ctl_available_permissions;
    stringset_t *mck_ctl_valid_applications;
} settings_test_mock_t;

void
settings_test_mock_init(settings_test_mock_t *mock)
{
    mock->mck_guest_valid = true;
    mock->mck_allowlist_value = NULL;
    mock->mck_ctl_available_permissions = stringset_create();
    stringset_add_item(mock->mck_ctl_available_permissions, "Audio");
    stringset_add_item(mock->mck_ctl_available_permissions, "Internet");
    stringset_add_item(mock->mck_ctl_available_permissions, "Pictures");
    mock->mck_ctl_valid_applications = stringset_create();
    stringset_add_item(mock->mck_ctl_valid_applications, "test-app");
    stringset_add_item(mock->mck_ctl_valid_applications, "default-app");
    stringset_add_item(mock->mck_ctl_valid_applications, "disabled-app");
}

/* ========================================================================= *
 * MOCK CONTROL FUNCTIONS
 * ========================================================================= */

uid_t
__wrap_control_min_user(const control_t *self)
{
    (void)self; // unused
    return MIN_USER;
}

uid_t
__wrap_control_max_user(const control_t *self)
{
    (void)self; // unused
    return MAX_USER;
}

bool
__wrap_control_user_is_guest(const control_t *self, uid_t uid)
{
    (void)self; // unused
    return uid == GUEST_USER;
}

bool
__wrap_control_valid_user(const control_t *self, uid_t uid)
{
    const settings_test_mock_t *mock = (const settings_test_mock_t *)self;
    if( uid == GUEST_USER )
        return mock->mck_guest_valid;
    return uid >= MIN_USER && uid <= MAX_USER;
}

bool
__wrap_control_valid_application(const control_t *self, const char *appname)
{
    const settings_test_mock_t *mock = (const settings_test_mock_t *)self;
    return stringset_has_item(mock->mck_ctl_valid_applications, appname);
}

const stringset_t *
__wrap_control_available_permissions(const control_t *self)
{
    const settings_test_mock_t *mock = (const settings_test_mock_t *)self;
    return mock->mck_ctl_available_permissions;
}

appinfo_t *
__wrap_control_appinfo(control_t *self, const gchar *appname)
{
    settings_test_mock_t *mock = (settings_test_mock_t *)self;
    appinfo_t *appinfo = appinfo_create((applications_t *)mock, appname);
    appinfo_parse_desktop(appinfo);
    return appinfo;
}

void
__wrap_control_on_settings_change(control_t *self, const gchar *appname)
{
    (void)self; // unused
    (void)appname; // unused
}

/* ========================================================================= *
 * MOCK CONFIG FUNCTIONS
 * ========================================================================= */

gchar *
__wrap_config_string(const config_t *self, const gchar *sec, const gchar *key, const gchar *def)
{
    const settings_test_mock_t *mock = (const settings_test_mock_t *)self;
    if( mock->mck_allowlist_value &&
        !g_strcmp0(sec, "Allowlist") &&
        !g_strcmp0(key, "test-app") )
        return g_strdup(mock->mck_allowlist_value);
    return g_strdup(def);
}

stringset_t *
__wrap_config_stringset(config_t *self, const gchar *sec, const gchar *key)
{
    (void)self; // unused
    stringset_t *set = stringset_create();
    if( !g_strcmp0(sec, "Default Profile") &&
        !g_strcmp0(key, "Permissions") )
        stringset_add_item(set, "Internet");
    return set;
}

bool
__wrap_config_boolean(config_t *self, const gchar *sec, const gchar *key, bool def)
{
    (void)self; // unused
    if( !g_strcmp0(sec, "Default Profile") &&
        !g_strcmp0(key, "Enabled") )
        return true;
    return def;
}

static gchar *
test_settings_user_path(void)
{
    return g_strdup_printf(SHAREDSTATEDIR "/sailjail/settings/user-1000.settings");
}

static void
test_settings_write_user_data(const gchar *data)
{
    gchar *path = test_settings_user_path();
    g_assert_true(g_file_set_contents(path, data, -1, NULL));
    g_free(path);
}

static gchar *
test_settings_read_user_data(void)
{
    gchar *path = test_settings_user_path();
    gchar *data = NULL;
    g_assert_true(g_file_get_contents(path, &data, NULL, NULL));
    g_free(path);
    return data;
}

/* ========================================================================= *
 * MOCK APPLICATIONS FUNCTIONS
 * ========================================================================= */

control_t *
__wrap_applications_control(const applications_t *self)
{
    settings_test_mock_t *mock = (settings_test_mock_t *)self;
    return (control_t *)mock;
}

config_t *
__wrap_applications_config(applications_t *self)
{
    settings_test_mock_t *mock = (settings_test_mock_t *)self;
    return (config_t *)mock;
}

/* ========================================================================= *
 * MOCK MIGRATOR FUNCTIONS
 * ========================================================================= */

migrator_t *
__wrap_migrator_create(control_t *control)
{
    return NULL;
}

void
__wrap_migrator_delete_at(migrator_t **pself)
{
    (void)pself; // unused
}

void
__wrap_migrator_on_settings_saved(migrator_t *self)
{
    (void)self; // unused
}

/* ========================================================================= *
 * SETTINGS TESTS
 * ========================================================================= */

void test_settings_create_delete(gconstpointer user_data)
{
    settings_t *settings = settings_create((config_t *)user_data, (control_t *)user_data);
    g_assert_nonnull(settings);
    settings_delete_at(&settings);
    g_assert_null(settings);
    settings_delete_at(&settings);
    g_assert_null(settings);
}

void test_settings_load(gconstpointer user_data)
{
    settings_t *settings = settings_create((config_t *)user_data, (control_t *)user_data);
    settings_load_user(settings, 1000);
    usersettings_t *usersettings = settings_get_usersettings(settings, 1000);
    g_assert_nonnull(usersettings);
    appsettings_t *appsettings = settings_get_appsettings(settings, 1000, "test-app");
    g_assert_nonnull(appsettings);
    g_assert_cmpint(appsettings_get_allowed(appsettings), ==, APP_ALLOWED_ALWAYS);
    g_assert_cmpint(appsettings_get_agreed(appsettings), ==, APP_ALLOWED_UNSET);
    const stringset_t *granted = appsettings_get_granted(appsettings);
    g_assert_cmpint(stringset_size(granted), ==, 1);
    g_assert_true(stringset_has_item(granted, "Internet"));
    settings_delete(settings);
}

void test_settings_allowlist_launch(gconstpointer user_data)
{
    settings_test_mock_t *mock = (settings_test_mock_t *)user_data;
    mock->mck_allowlist_value = "launch";

    settings_t *settings = settings_create((config_t *)mock, (control_t *)mock);
    g_assert_nonnull(settings);

    appsettings_t *appsettings = settings_add_appsettings(settings, 1000, "test-app");
    g_assert_nonnull(appsettings);
    g_assert_cmpint(appsettings_get_allowed(appsettings), ==, APP_ALLOWED_ALWAYS);
    g_assert_true(stringset_has_item(appsettings_get_granted(appsettings), "Internet"));

    settings_delete(settings);
    mock->mck_allowlist_value = NULL;
}

void test_settings_compatibility_permissions_persist(gconstpointer user_data)
{
    settings_t *settings = settings_create((config_t *)user_data, (control_t *)user_data);
    g_assert_nonnull(settings);

    appsettings_t *appsettings = settings_add_appsettings(settings, 1000, "default-app");
    g_assert_nonnull(appsettings);

    settings_save_user(settings, 1000);

    gchar *data = test_settings_read_user_data();
    g_assert_nonnull(g_strstr_len(data, -1, "[default-app]"));
    g_assert_nonnull(g_strstr_len(data, -1, "Permissions=Internet"));
    g_free(data);

    settings_delete(settings);
}

void test_settings_mode_transition_compatibility(gconstpointer user_data)
{
    settings_t *settings = settings_create((config_t *)user_data, (control_t *)user_data);
    g_assert_nonnull(settings);

    test_settings_write_user_data(
        "[default-app]\n"
        "Allowed=1\n"
        "Agreed=0\n"
        "Autogrant=0\n"
        "Granted=Internet\n"
        "Permissions=Internet\n");

    settings_load_user(settings, 1000);
    appsettings_t *appsettings = settings_get_appsettings(settings, 1000, "default-app");
    g_assert_nonnull(appsettings);
    g_assert_cmpint(appsettings_get_allowed(appsettings), ==, APP_ALLOWED_UNSET);
    g_assert_cmpint(stringset_size(appsettings_get_granted(appsettings)), ==, 0);

    settings_delete(settings);
}

void test_settings_mode_transition_none(gconstpointer user_data)
{
    settings_t *settings = settings_create((config_t *)user_data, (control_t *)user_data);
    g_assert_nonnull(settings);

    test_settings_write_user_data(
        "[disabled-app]\n"
        "Allowed=1\n"
        "Agreed=0\n"
        "Autogrant=0\n"
        "Granted=Internet\n"
        "Permissions=Internet\n");

    settings_load_user(settings, 1000);
    appsettings_t *appsettings = settings_get_appsettings(settings, 1000, "disabled-app");
    g_assert_nonnull(appsettings);
    g_assert_cmpint(appsettings_get_allowed(appsettings), ==, APP_ALLOWED_UNSET);
    g_assert_cmpint(stringset_size(appsettings_get_granted(appsettings)), ==, 0);

    settings_delete(settings);
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int main(int argc, char **argv)
{
    settings_test_mock_t mock;
    settings_test_mock_init(&mock);

    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_data_func("/sailjaild/settings/settings/create_and_delete", &mock, test_settings_create_delete);
    g_test_add_data_func("/sailjaild/settings/settings/load", &mock, test_settings_load);
    g_test_add_data_func("/sailjaild/settings/settings/allowlist_launch", &mock, test_settings_allowlist_launch);
    g_test_add_data_func("/sailjaild/settings/settings/compatibility_permissions_persist", &mock, test_settings_compatibility_permissions_persist);
    g_test_add_data_func("/sailjaild/settings/settings/mode_transition_compatibility", &mock, test_settings_mode_transition_compatibility);
    g_test_add_data_func("/sailjaild/settings/settings/mode_transition_none", &mock, test_settings_mode_transition_none);

    return g_test_run();
}
