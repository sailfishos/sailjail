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

#include "stringset.h"

#include <glib.h>
#include <locale.h>

/* ========================================================================= *
 * SET UP AND TEAR DOWN
 * ========================================================================= */

typedef struct {
    stringset_t *empty;
    stringset_t *set;
    char **names;
} stringset_test_data_t;

static void
stringset_test_set_up(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    data->empty = stringset_create();
    data->set   = stringset_create();
    stringset_add_item(data->set, "foo");
    stringset_add_item(data->set, "bar");
    stringset_add_item(data->set, "baz");
    data->names = g_malloc0_n(4, sizeof(*data->names));
    data->names[0] = "foo";
    data->names[1] = "bar";
    data->names[2] = "baz";
}

static void
stringset_test_tear_down(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    stringset_delete_at(&data->empty);
    stringset_delete_at(&data->set);
}

/* ========================================================================= *
 * STRINGSET TESTS
 * ========================================================================= */

void test_stringset_create_delete()
{
    stringset_t *set = stringset_create();
    g_assert_nonnull(set);
    stringset_delete_at(&set);
    g_assert_null(set);
    stringset_delete_at(&set);
    g_assert_null(set);
}

void test_stringset_add_item()
{
    stringset_t *set = stringset_create();
    g_assert_true(stringset_add_item(set, "foo"));
    g_assert_true(stringset_add_item(set, "bar"));
    g_assert_false(stringset_add_item(set, "foo"));
    stringset_delete(set);
}

void test_stringset_remove_item(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    g_assert_true(stringset_remove_item(data->set, "foo"));
    g_assert_true(stringset_remove_item(data->set, "bar"));
    g_assert_false(stringset_remove_item(data->set, "foo"));
    g_assert_cmpint(stringset_size(data->set), ==, 1);
}

void test_stringset_size(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    g_assert_cmpint(stringset_size(data->empty), ==, 0);
    g_assert_cmpint(stringset_size(data->set), ==, 3);
}

void test_stringset_empty(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    g_assert_true(stringset_empty(data->empty));
    g_assert_false(stringset_empty(data->set));
}

void test_stringset_has_item(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    g_assert_true(stringset_has_item(data->set, "bar"));
    g_assert_false(stringset_has_item(data->set, "foobar"));
}

void test_stringset_to_string(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    gchar *text = stringset_to_string(data->set);
    g_assert_cmpstr(text, ==, "foo,bar,baz");
    g_free(text);
}

void test_stringset_to_strv(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    gchar **names = stringset_to_strv(data->set);
    g_assert_cmpuint(g_strv_length(names), ==, g_strv_length(data->names));
    for( size_t i = 0; names[i] && data->names[i]; ++i )
        g_assert_cmpstr(names[i], ==, data->names[i]);
    g_strfreev(names);
}

void test_stringset_from_strv(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    stringset_t *nameset = stringset_from_strv(data->names);
    g_assert_true(stringset_equal(nameset, data->set));
    stringset_delete(nameset);
}

void test_stringset_from_null_strv()
{
    stringset_t *set = stringset_from_strv(NULL);
    g_assert_nonnull(set);
    g_assert_true(stringset_empty(set));
}

void test_stringset_extend(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    stringset_t *extended = stringset_create();
    stringset_add_item(extended, "1");
    stringset_add_item(extended, "2");
    stringset_add_item(extended, "bar");
    g_assert_false(stringset_has_item(extended, "foo"));
    g_assert_true(stringset_has_item(extended, "bar"));
    g_assert_false(stringset_has_item(extended, "baz"));
    stringset_extend(extended, data->set);
    g_assert_true(stringset_has_item(extended, "1"));
    g_assert_true(stringset_has_item(extended, "2"));
    g_assert_true(stringset_has_item(extended, "foo"));
    g_assert_true(stringset_has_item(extended, "bar"));
    g_assert_true(stringset_has_item(extended, "baz"));
    stringset_delete(extended);
}

void test_stringset_copy(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    stringset_t *copy = stringset_copy(data->set);
    g_assert_true(stringset_equal(data->set, copy));
    g_assert_true(stringset_has_item(copy, "foo"));
    g_assert_true(stringset_has_item(copy, "bar"));
    g_assert_true(stringset_has_item(copy, "baz"));
    stringset_add_item(copy, "1");
    stringset_add_item(copy, "2");
    g_assert_false(stringset_equal(data->set, copy));
    g_assert_false(stringset_has_item(data->set, "1"));
    g_assert_false(stringset_has_item(data->set, "2"));
    g_assert_true(stringset_has_item(copy, "1"));
    g_assert_true(stringset_has_item(copy, "2"));
    stringset_delete(copy);
}

void test_stringset_swap(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    stringset_swap(data->empty, data->set);
    g_assert_true(stringset_has_item(data->empty, "foo"));
    g_assert_true(stringset_has_item(data->empty, "bar"));
    g_assert_true(stringset_has_item(data->empty, "baz"));
    g_assert_false(stringset_has_item(data->set, "foo"));
    g_assert_false(stringset_has_item(data->set, "bar"));
    g_assert_false(stringset_has_item(data->set, "baz"));
}

void test_stringset_assign(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    g_assert_true(stringset_assign(data->empty, data->set));
    g_assert_true(stringset_equal(data->empty, data->set));
}

void test_stringset_nonequal(stringset_test_data_t *data, gconstpointer user_data)
{
    (void)user_data; // unused
    stringset_t *another = stringset_copy(data->set);
    g_assert_true(stringset_remove_item(another, "baz"));
    g_assert_true(stringset_add_item(another, "xxx"));
    g_assert_false(stringset_equal(data->set, another));
    stringset_delete(another);
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/sailjaild/stringset/create_and_delete", test_stringset_create_delete);
    g_test_add_func("/sailjaild/stringset/add_item", test_stringset_add_item);
    g_test_add("/sailjaild/stringset/remove_item", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_remove_item, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/size", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_size, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/empty", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_empty, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/has_item", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_has_item, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/to_string", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_to_string, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/to_strv", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_to_strv, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/from_strv", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_from_strv, stringset_test_tear_down);
    g_test_add_func("/sailjaild/stringset/from_null_strv", test_stringset_from_null_strv);
    g_test_add("/sailjaild/stringset/extend", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_extend, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/copy", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_copy, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/swap", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_swap, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/assign", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_assign, stringset_test_tear_down);
    g_test_add("/sailjaild/stringset/nonequal", stringset_test_data_t, NULL,
               stringset_test_set_up, test_stringset_nonequal, stringset_test_tear_down);

    return g_test_run();
}
