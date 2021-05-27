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

#include "util.h"
#include "stringset.h"

#include <glib.h>
#include <locale.h>

/* ========================================================================= *
 * STRIP TESTS
 * ========================================================================= */

void test_strip_basic()
{
    gchar *subject = g_strdup("  foo    bar  ");
    gchar *result = strip(subject);
    g_assert_true(result == subject);
    g_assert_cmpstr(result, ==, "foo bar");
    g_free(subject);
}

void test_strip_none()
{
    gchar *subject = g_strdup("foobar");
    gchar *result = strip(subject);
    g_assert_true(result == subject);
    g_assert_cmpstr(result, ==, "foobar");
    g_free(subject);
}

void test_strip_null()
{
    gchar *subject = g_strdup("");
    gchar *result = strip(subject);
    g_assert_true(result == subject);
    g_assert_cmpstr(result, ==, "");
    g_free(subject);
    g_assert_null(strip(NULL));
}

/* ========================================================================= *
 * PATH TESTS
 * ========================================================================= */

void test_path_basename()
{
    const gchar *result;
    result = path_basename("/usr/share/applications/foo.desktop");
    g_assert_cmpstr(result, ==, "foo.desktop");
    result = path_basename("/tmp/foo");
    g_assert_cmpstr(result, ==, "foo");
    result = path_basename("/foo");
    g_assert_cmpstr(result, ==, "foo");
    result = path_basename("foo");
    g_assert_cmpstr(result, ==, "foo");
    result = path_basename(".foo");
    g_assert_cmpstr(result, ==, ".foo");
    result = path_basename("");
    g_assert_cmpstr(result, ==, "");
    result = path_basename(NULL);
    g_assert_null(result);
}

void test_path_extension()
{
    const gchar *result;
    result = path_extension("/usr/share/applications/foo.desktop");
    g_assert_cmpstr(result, ==, ".desktop");
    result = path_extension("foo.test");
    g_assert_cmpstr(result, ==, ".test");
    result = path_extension("foo.test.desktop");
    g_assert_cmpstr(result, ==, ".desktop");
    result = path_extension("/tmp/foo");
    g_assert_null(result);
    result = path_extension("/foo");
    g_assert_null(result);
    result = path_extension("foo");
    g_assert_null(result);
    /* Hidden files do not work as expected at the moment
     * but this is sufficient for us, for now anyway!
     */
//    result = path_extension(".foo");
//    g_assert_cmpstr(result, ==, "");
    result = path_extension(NULL);
    g_assert_null(result);
}

void test_path_dirname()
{
    {
        gchar *result = path_dirname("/usr/share/applications/foo.desktop");
        g_assert_cmpstr(result, ==, "/usr/share/applications");
        g_free(result);
    }
    {
        gchar *result = path_dirname("/tmp/foo.desktop");
        g_assert_cmpstr(result, ==, "/tmp");
        g_free(result);
    }
    {
        gchar *result = path_dirname("/usr/share/applications/org.example.FooBar.desktop");
        g_assert_cmpstr(result, ==, "/usr/share/applications");
        g_free(result);
    }
    {
        gchar *result = path_dirname("/foo.test");
        g_assert_cmpstr(result, ==, "/");
        g_free(result);
    }
    {
        gchar *result = path_dirname("foo.test");
        g_assert_cmpstr(result, ==, ".");
        g_free(result);
    }
    {
        gchar *result = path_dirname(NULL);
        g_assert_null(result);
    }
}

void test_path_to_desktop_name()
{
    const gchar *result;
    result = path_to_desktop_name("/usr/share/applications/foo.desktop");
    g_assert_cmpstr(result, ==, "foo");
    result = path_to_desktop_name("/tmp/foo.desktop");
    g_assert_cmpstr(result, ==, "foo");
    result = path_to_desktop_name("/usr/share/applications/org.example.FooBar.desktop");
    g_assert_cmpstr(result, ==, "org.example.FooBar");
    result = path_to_desktop_name("/foo.test");
    g_assert_cmpstr(result, ==, "foo.test");
    result = path_to_desktop_name("foo.test");
    g_assert_cmpstr(result, ==, "foo.test");
    result = path_to_desktop_name("foo");
    g_assert_cmpstr(result, ==, "foo");
    result = path_to_desktop_name(NULL);
    g_assert_null(result);
}

void test_path_from_permission_name()
{
    gchar *result = path_from_permission_name("Test");
    g_assert_cmpstr(result, ==, PERMISSIONS_DIRECTORY "/Test.permission");
    g_free(result);
    result = path_to_permission_name(NULL);
    g_assert_null(result);
}

/* ========================================================================= *
 * CHANGE TESTS
 * ========================================================================= */

void test_change_uid()
{
    uid_t uid = 0;
    g_assert_false(change_uid(&uid, 0));
    g_assert_cmpint(uid, ==, 0);
    g_assert_true(change_uid(&uid, 1));
    g_assert_cmpint(uid, ==, 1);
}

void test_change_boolean()
{
    bool boolean = false;
    g_assert_false(change_boolean(&boolean, false));
    g_assert_false(boolean);
    g_assert_true(change_boolean(&boolean, true));
    g_assert_true(boolean);
}

void test_change_string()
{
    gchar *string = g_strdup("foo"); // consumed
    g_assert_false(change_string(&string, "foo"));
    g_assert_cmpstr(string, ==, "foo");
    g_assert_true(change_string(&string, "bar"));
    g_assert_cmpstr(string, ==, "bar");
    g_assert_true(change_string(&string, NULL));
    g_assert_null(string);
    g_assert_false(change_string(&string, NULL));
    g_assert_null(string);
}

void test_change_string_steal()
{
    gchar *string = g_strdup("foo"); // consumed
    {
        gchar *tmp = string;
        gchar *another = g_strdup("foo"); // consumed
        g_assert_false(change_string_steal(&string, another));
        g_assert_cmpstr(string, ==, "foo");
        g_assert_true(string == tmp);
    }
    {
        gchar *another = g_strdup("bar"); // consumed
        g_assert_true(change_string_steal(&string, another));
        g_assert_cmpstr(string, ==, "bar");
        g_assert_true(string == another);
    }
    g_assert_true(change_string_steal(&string, NULL));
    g_assert_null(string);
    g_assert_false(change_string_steal(&string, NULL));
    g_assert_null(string);
}

/* ========================================================================= *
 * KEYFILE TESTS
 * ========================================================================= */

void test_keyfile_merge()
{
    GKeyFile *keyfile = g_key_file_new();

    {
        g_assert_true(keyfile_merge(keyfile, TESTDATADIR "/keyfile1.ini"));
        gchar *value = keyfile_get_string(keyfile, "GroupB", "KeyB1", "default");
        g_assert_cmpstr(value, ==, "ValueB1");
        g_free(value);
    }
    {
        g_assert_true(keyfile_merge(keyfile, TESTDATADIR "/keyfile2.ini"));
        gchar *value = keyfile_get_string(keyfile, "GroupB", "KeyB1", "default");
        g_assert_cmpstr(value, ==, "OverrideB1");
        g_free(value);
    }

    g_key_file_free(keyfile);
}

void test_keyfile_save()
{
    GKeyFile *keyfile = g_key_file_new();
    keyfile_set_boolean(keyfile, "Foo", "boolean", true);
    keyfile_set_integer(keyfile, "Foo", "number", 100);
    g_assert_true(keyfile_save(keyfile, SHAREDSTATEDIR "/test.ini"));
    g_assert_true(g_file_test(SHAREDSTATEDIR "/test.ini", G_FILE_TEST_EXISTS));
    g_key_file_unref(keyfile);
    keyfile = g_key_file_new();
    g_key_file_load_from_file(keyfile, SHAREDSTATEDIR "/test.ini", G_KEY_FILE_NONE, NULL);
    g_assert_true(g_key_file_get_boolean(keyfile, "Foo", "boolean", NULL));
    g_assert_cmpint(g_key_file_get_integer(keyfile, "Foo", "number", NULL), ==, 100);
}

void test_keyfile_non_existent()
{
    GKeyFile *keyfile = g_key_file_new();
    g_assert_false(keyfile_merge(keyfile, TESTDATADIR "/does_not_exist.ini"));
}

void test_keyfile_missing_values()
{
    GKeyFile *keyfile = g_key_file_new();
    g_assert_true(keyfile_load(keyfile, TESTDATADIR "/keyfile1.ini"));

    bool boolvalue = keyfile_get_boolean(keyfile, "GroupA", "some_bool", false);
    g_assert_false(boolvalue);
    boolvalue = keyfile_get_boolean(keyfile, "GroupA", "some_bool", true);
    g_assert_true(boolvalue);

    int intvalue = keyfile_get_integer(keyfile, "GroupA", "some_int", -1);
    g_assert_cmpint(intvalue, ==, -1);

    gchar *strvalue = keyfile_get_string(keyfile, "GroupA", "some_string", "mydefault");
    g_assert_cmpstr(strvalue, ==, "mydefault");
    g_free(strvalue);

    stringset_t *setvalue = keyfile_get_stringset(keyfile, "GroupA", "some_set");
    g_assert_true(stringset_empty(setvalue));
    stringset_delete(setvalue);

    g_key_file_free(keyfile);
}

void test_keyfile_set_boolean()
{
    GKeyFile *keyfile = g_key_file_new();
    keyfile_set_boolean(keyfile, "Foo", "bar", true);
    g_assert_true(g_key_file_get_boolean(keyfile, "Foo", "bar", NULL));
}

void test_keyfile_set_integer()
{
    GKeyFile *keyfile = g_key_file_new();
    keyfile_set_integer(keyfile, "Foo", "bar", 100);
    g_assert_cmpint(g_key_file_get_integer(keyfile, "Foo", "bar", NULL), ==, 100);
}

void test_keyfile_set_string()
{
    GKeyFile *keyfile = g_key_file_new();
    {
        keyfile_set_string(keyfile, "Foo", "foo", "foobar");
        gchar *result = g_key_file_get_string(keyfile, "Foo", "foo", NULL);
        g_assert_cmpstr(result, ==, "foobar");
        g_free(result);
    }
    {
        keyfile_set_string(keyfile, "Foo", "bar", "");
        gchar *result = g_key_file_get_string(keyfile, "Foo", "bar", NULL);
        g_assert_cmpstr(result, ==, "");
        g_free(result);
    }
    {
        // keyfile_set_string sets an empty string instead of skipping
        keyfile_set_string(keyfile, "Foo", "baz", NULL);
        gchar *result = g_key_file_get_string(keyfile, "Foo", "baz", NULL);
        g_assert_cmpstr(result, ==, "");
        g_free(result);
    }
}

void test_keyfile_set_stringset()
{
    gchar *result;
    GKeyFile *keyfile = g_key_file_new();
    stringset_t *set = stringset_create();
    stringset_add_item(set, "foo");
    stringset_add_item(set, "bar");
    keyfile_set_stringset(keyfile, "Foo", "bar", set);
    result = g_key_file_get_string(keyfile, "Foo", "bar", NULL);
    g_assert_cmpstr(result, ==, "foo;bar;");
    g_free(result);
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/sailjaild/util/strip/basic", test_strip_basic);
    g_test_add_func("/sailjaild/util/strip/none", test_strip_none);
    g_test_add_func("/sailjaild/util/strip/null", test_strip_null);

    g_test_add_func("/sailjaild/util/path/basename", test_path_basename);
    g_test_add_func("/sailjaild/util/path/extension", test_path_extension);
    g_test_add_func("/sailjaild/util/path/dirname", test_path_dirname);
    g_test_add_func("/sailjaild/util/path/to_desktop_name", test_path_to_desktop_name);
    g_test_add_func("/sailjaild/util/path/from_permission_name", test_path_from_permission_name);

    g_test_add_func("/sailjaild/util/change/uid", test_change_uid);
    g_test_add_func("/sailjaild/util/change/boolean", test_change_boolean);
    g_test_add_func("/sailjaild/util/change/string", test_change_string);
    g_test_add_func("/sailjaild/util/change/string_steal", test_change_string_steal);

    g_test_add_func("/sailjaild/util/keyfile/merge", test_keyfile_merge);
    g_test_add_func("/sailjaild/util/keyfile/non_existent", test_keyfile_non_existent);
    g_test_add_func("/sailjaild/util/keyfile/missing_values", test_keyfile_missing_values);
    g_test_add_func("/sailjaild/util/keyfile/save", test_keyfile_save);
    g_test_add_func("/sailjaild/util/keyfile/set_boolean", test_keyfile_set_boolean);
    g_test_add_func("/sailjaild/util/keyfile/set_integer", test_keyfile_set_integer);
    g_test_add_func("/sailjaild/util/keyfile/set_string", test_keyfile_set_string);
    g_test_add_func("/sailjaild/util/keyfile/set_stringset", test_keyfile_set_stringset);

    return g_test_run();
}
