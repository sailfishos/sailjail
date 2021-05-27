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

#include "permissions.h"
#include "stringset.h"
#include "util.h"

#include <glib.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>

/* ========================================================================= *
 * MOCK DATA
 * ========================================================================= */

typedef struct {
    bool mck_change_signaled;
    GMainLoop *main_loop;
} permissions_test_mock_t;

void
permissions_test_mock_init(permissions_test_mock_t *mock)
{
    mock->mck_change_signaled = false;
    mock->main_loop = g_main_loop_new(NULL, TRUE);
}

/* ========================================================================= *
 * MOCK CONTROL FUNCTIONS
 * ========================================================================= */

void
__wrap_control_on_permissions_change(control_t *self)
{
    permissions_test_mock_t *mock = (permissions_test_mock_t *)self;
    mock->mck_change_signaled = true;
    g_main_loop_quit(mock->main_loop);
}

/* ========================================================================= *
 * Utility
 * ========================================================================= */

gboolean
permissions_test_timeout(gpointer user_data)
{
    permissions_test_mock_t *mock = (permissions_test_mock_t *)user_data;
    g_main_loop_quit(mock->main_loop);
    return G_SOURCE_REMOVE;
}

void
permissions_test_add_timeout(permissions_test_mock_t *mock)
{
    /* Timeout the test after ten seconds and quit main loop
     * if the expected signal doesn't fire
     */
    static guint timeout = 0;
    if( timeout )
        g_source_remove(timeout);
    timeout = g_timeout_add_seconds_full(G_PRIORITY_LOW, 10,
                                         permissions_test_timeout, mock, NULL);
}

/* ========================================================================= *
 * PERMISSIONS TESTS
 * ========================================================================= */

void test_permissions_create_delete()
{
    permissions_t *permissions = permissions_create(NULL);
    g_assert_nonnull(permissions);
    permissions_delete_at(&permissions);
    g_assert_null(permissions);
    permissions_delete_at(&permissions);
    g_assert_null(permissions);
}

void test_permissions_available()
{
    permissions_t *permissions = permissions_create(NULL);
    const stringset_t *available = permissions_available(permissions);
    g_assert_cmpint(stringset_size(available), ==, 4);
    g_assert_true(stringset_has_item(available, "Audio"));
    g_assert_false(stringset_has_item(available, "Base"));
    g_assert_true(stringset_has_item(available, "Camera"));
    g_assert_true(stringset_has_item(available, "Contacts"));
    g_assert_true(stringset_has_item(available, "Privileged"));
    g_assert_false(stringset_has_item(available, "some-app"));
    g_assert_false(stringset_has_item(available, "some-app.profile"));
    permissions_delete(permissions);
}

void test_permissions_changed(gconstpointer user_data)
{
    permissions_test_mock_t *mock = (permissions_test_mock_t *)user_data;
    permissions_t *permissions = permissions_create((control_t *)mock);
    const stringset_t *available = permissions_available(permissions);
    g_assert_false(stringset_has_item(available, "Test"));
    /* First test adding a permission */
    permissions_test_add_timeout(mock);
    FILE *file = fopen(PERMISSIONS_DIRECTORY "/Test.permission", "a");
    g_assert_nonnull(file);
    g_assert_cmpint(fclose(file), ==, 0);
    g_main_loop_run(mock->main_loop);
    g_assert_true(mock->mck_change_signaled);
    available = permissions_available(permissions);
    g_assert_true(stringset_has_item(available, "Test"));
    /* Second test removing a permission */
    permissions_test_add_timeout(mock);
    mock->mck_change_signaled = false;
    g_assert_cmpint(unlink(PERMISSIONS_DIRECTORY "/Test.permission"), ==, 0);
    g_main_loop_run(mock->main_loop);
    g_assert_true(mock->mck_change_signaled);
    available = permissions_available(permissions);
    g_assert_false(stringset_has_item(available, "Test"));
    permissions_delete(permissions);
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int main(int argc, char **argv)
{
    permissions_test_mock_t mock;
    permissions_test_mock_init(&mock);

    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/sailjaild/permissions/create_and_delete", test_permissions_create_delete);
    g_test_add_func("/sailjaild/permissions/available", test_permissions_available);
    g_test_add_data_func("/sailjaild/permissions/changed", &mock, test_permissions_changed);

    return g_test_run();
}
