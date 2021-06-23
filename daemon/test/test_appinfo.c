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

#include "appinfo.h"
#include "stringset.h"

#include <glib.h>
#include <locale.h>

/* ========================================================================= *
 * MOCK DATA
 * ========================================================================= */

typedef struct {
    stringset_t *mck_ctl_available_permissions;
} appinfo_test_mock_t;

void
appinfo_test_mock_init(appinfo_test_mock_t *mock)
{
    mock->mck_ctl_available_permissions = stringset_create();
    stringset_add_item(mock->mck_ctl_available_permissions, "Audio");
    stringset_add_item(mock->mck_ctl_available_permissions, "Internet");
    stringset_add_item(mock->mck_ctl_available_permissions, "Pictures");
}

/* ========================================================================= *
 * MOCK CONTROL FUNCTIONS
 * ========================================================================= */

const stringset_t *
__wrap_control_available_permissions(const control_t *self)
{
    const appinfo_test_mock_t *mock = (const appinfo_test_mock_t *)self;
    return mock->mck_ctl_available_permissions;
}

/* ========================================================================= *
 * MOCK APPLICATIONS FUNCTIONS
 * ========================================================================= */

control_t *
__wrap_applications_control(const applications_t *self)
{
    const appinfo_test_mock_t *mock = (const appinfo_test_mock_t *)self;
    return (control_t *)mock;
}

config_t *
__wrap_applications_config(applications_t *self)
{
    const appinfo_test_mock_t *mock = (const appinfo_test_mock_t *)self;
    return (config_t *)mock;
}

/* ========================================================================= *
 * MOCK CONFIG_FUNCTIONS
 * ========================================================================= */

stringset_t *
__wrap_config_stringset(config_t *self, const gchar *sec, const gchar *key)
{
    (void)self; // unused
    (void)sec; // unused
    (void)key; // unused
    return stringset_create();
}

bool
__wrap_config_boolean(config_t *self, const gchar *sec, const gchar *key, bool def)
{
    (void)self; // unused
    (void)sec; // unused
    (void)key; // unused
    return def;
}

/* ========================================================================= *
 * APPINFO TESTS
 * ========================================================================= */

void test_appinfo_create_missing(gconstpointer user_data)
{
    appinfo_t *appinfo = appinfo_create((applications_t *)user_data, "test-not-an-app");
    g_assert_nonnull(appinfo);
    g_assert_true(appinfo_parse_desktop(appinfo));
    g_assert_false(appinfo_valid(appinfo));
    appinfo_delete(appinfo);
}

void test_appinfo_create_invalid(gconstpointer user_data)
{
    appinfo_t *appinfo = appinfo_create((applications_t *)user_data, "invalid-app");
    g_assert_nonnull(appinfo);
    g_assert_true(appinfo_parse_desktop(appinfo));
    g_assert_false(appinfo_valid(appinfo));
    appinfo_delete(appinfo);
}

void test_appinfo_read_properties(gconstpointer user_data)
{
    appinfo_t *appinfo = appinfo_create((applications_t *)user_data, "test-app");
    g_assert_nonnull(appinfo);
    g_assert_true(appinfo_parse_desktop(appinfo));
    g_assert_true(appinfo_valid(appinfo));
    g_assert_cmpstr(appinfo_get_name(appinfo), ==, "Test Application");
    g_assert_cmpstr(appinfo_get_type(appinfo), ==, "Application");
    g_assert_cmpstr(appinfo_get_icon(appinfo), ==, "test");
    g_assert_cmpstr(appinfo_get_exec(appinfo), ==, "/usr/bin/true");
    g_assert_cmpstr(appinfo_get_organization_name(appinfo), ==, "org.example");
    g_assert_cmpstr(appinfo_get_application_name(appinfo), ==, "TestApplication");
    appinfo_delete(appinfo);
}

void test_appinfo_permissions(gconstpointer user_data)
{
    appinfo_t *appinfo = appinfo_create((applications_t *)user_data, "test-app");
    g_assert_nonnull(appinfo);
    g_assert_true(appinfo_parse_desktop(appinfo));
    g_assert_true(appinfo_valid(appinfo));
    g_assert_true(appinfo_has_permission(appinfo, "Internet"));
    g_assert_false(appinfo_has_permission(appinfo, "Pictures"));
    g_assert_false(appinfo_has_permission(appinfo, "NonExistingPermission"));
    appinfo_delete(appinfo);
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int main(int argc, char **argv)
{
    appinfo_test_mock_t mock;
    appinfo_test_mock_init(&mock);

    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_data_func("/sailjaild/appinfo/create_missing", &mock, test_appinfo_create_missing);
    g_test_add_data_func("/sailjaild/appinfo/create_invalid", &mock, test_appinfo_create_invalid);
    g_test_add_data_func("/sailjaild/appinfo/read_properties", &mock, test_appinfo_read_properties);
    g_test_add_data_func("/sailjaild/appinfo/permissions", &mock, test_appinfo_permissions);

    return g_test_run();
}
