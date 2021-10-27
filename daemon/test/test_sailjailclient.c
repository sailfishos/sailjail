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

#include "sailjailclient_wrapper.h"
#include "config.h"

#include <glib.h>
#include <locale.h>

/* ========================================================================= *
 * MOCK CONFIG FUNCTIONS
 * ========================================================================= */

config_t *
__wrap_config_create()
{
    return NULL;
}

void
__wrap_config_delete(config_t *config)
{
    (void)config; // unused
}

/* ========================================================================= *
 * SAILJAILCLIENT TESTS
 * ========================================================================= */

void test_sailjailclient_match_argv()
{
    {
        const char *tplt[] = {"--image", "%f", NULL};
        const char *args[] = {"--image", "foo.png", NULL};
        g_assert_true(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"--image", "%f", NULL};
        const char *args[] = {"--image", NULL};
        g_assert_true(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"--convert", "%F", NULL};
        const char *args[] = {"--convert", "foo.png", "bar.png", NULL};
        g_assert_true(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"--convert", "%F", NULL};
        const char *args[] = {"--convert", NULL};
        g_assert_true(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%k", NULL};
        const char *args[] = {"foo", NULL};
        g_assert_true(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%k", NULL};
        const char *args[] = {NULL};
        g_assert_false(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%k", NULL};
        const char *args[] = {"foo", "bar", NULL};
        g_assert_false(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%i", NULL};
        const char *args[] = {"--icon", "foo", NULL};
        g_assert_true(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%i", NULL};
        const char *args[] = {"--icon", NULL};
        g_assert_false(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%i", NULL};
        const char *args[] = {"foo", NULL};
        g_assert_false(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%i", NULL};
        const char *args[] = {"foo", "bar", NULL};
        g_assert_false(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%m", NULL};
        const char *args[] = {"foo", NULL};
        g_assert_false(sailjailclient_match_argv_wrapper(tplt, args));
    }
    {
        const char *tplt[] = {"%x", NULL};
        const char *args[] = {"foo", NULL};
        g_assert_false(sailjailclient_match_argv_wrapper(tplt, args));
    }
}

void test_sailjailclient_validate_argv()
{
    {
        const char *argv[] = {"/usr/bin/true", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("true", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("/usr/bin/true", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/false", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper("true", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", "--foo", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("true --foo", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper("true --foo", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", "--bar", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper("true --foo", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", "-a", "-b", "-c", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("true -a -b -c", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", "-a", "-b", "-b", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper("true -a -b -c", argv, false));
    }
}

void test_sailjailclient_validate_argv_compatibility()
{
    {
        const char *argv[] = {"/usr/bin/testapp", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("testapp", argv, true));
    }
    {
        const char *argv[] = {"/usr/libexec/testapp", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper("testapp", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/other", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper("testapp", argv, true));
    }
    {
        const char *argv[] = {"/usr/bin/testapp", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("/usr/bin/testapp", argv, true));
    }
}

void test_sailjailclient_validate_argv_dbus()
{
    {
        const char *argv[] = {"/usr/bin/false", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("/usr/bin/invoker --type=generic --id=exec-test1 --single-instance /usr/bin/false", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/false", "--dbus", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("/usr/bin/invoker --type=generic --id=exec-test1 --single-instance /usr/bin/false --dbus", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/invoker", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper("/usr/bin/invoker --type=generic --id=exec-test1 --single-instance /usr/bin/false", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", "-prestart", NULL};
        g_assert_true(sailjailclient_validate_argv_wrapper("true", argv, false));
    }
    {
        const char *argv[] = {"/usr/bin/true", NULL};
        g_assert_false(sailjailclient_validate_argv_wrapper(NULL, argv, false));
    }
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int __wrap_main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/sailjaild/sailjailclient/match_argv", test_sailjailclient_match_argv);
    g_test_add_func("/sailjaild/sailjailclient/validate_argv", test_sailjailclient_validate_argv);
    g_test_add_func("/sailjaild/sailjailclient/validate_argv_compatibility", test_sailjailclient_validate_argv_compatibility);
    g_test_add_func("/sailjaild/sailjailclient/validate_argv_dbus", test_sailjailclient_validate_argv_dbus);

    return g_test_run();
}
