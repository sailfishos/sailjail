/*
 * Copyright (c) 2020 Open Mobile Platform LLC.
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
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
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

#include "test_common.h"
#include "jail_rules_p.h"
#include "jail_conf.h"

#include <stdlib.h>

static TestOpt test_opt;

#define TMP_DIR_TEMPLATE "sailjail-test-rules-XXXXXX"

static
void
test_assert_rules_not_null(
    const JailRules* rules)
{
    g_assert(rules);
    g_assert(rules->permits);
    g_assert(rules->profiles);
    g_assert(rules->paths);
    g_assert(rules->dbus_user);
    g_assert(rules->dbus_user->own);
    g_assert(rules->dbus_user->talk);
    g_assert(rules->dbus_system->own);
    g_assert(rules->dbus_system->talk);
}

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    SailJail jail = { NULL, NULL };
    g_assert_cmpint(jail_rules_permit_parse(NULL), == ,JAIL_PERMIT_INVALID);
    g_assert_null(jail_rules_keyfile_parse(NULL, NULL, NULL, NULL));
    g_assert_null(jail_rules_keyfile_parse(&jail, NULL, NULL, NULL));
    g_assert_null(jail_rules_ref(NULL));
    jail_rules_unref(NULL);
    g_assert_null(jail_rules_restrict(NULL, NULL, NULL, NULL, NULL, NULL));
}

/*==========================================================================*
 * permit_name
 *==========================================================================*/

static
void
test_permit_name(
    void)
{
    char* name = jail_rules_permit_format(JAIL_PERMIT_PRIVILEGED);

    g_assert_cmpstr(name, == ,"Privileged");
    g_free(name);

    g_assert_null(jail_rules_permit_format((JAIL_PERMIT_TYPE)-1));
    g_assert_null(jail_rules_permit_format(JAIL_PERMIT_INVALID));
}

/*==========================================================================*
 * missing_profile
 *==========================================================================*/

static
void
test_missing_profile(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    JailConf* conf = jail_conf_new();
    JailRules* rules;
    JailRulesOpt opt;
    GError* error = NULL;
    char* path = NULL;
    char* section = NULL;

    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;

    rules = jail_rules_new("foo", conf, &opt, &path, &section, &error);
    test_assert_rules_not_null(rules);
    g_assert_null(path);
    g_assert_null(section);
    g_assert_null(error);

    /* There's nothing there, not even Base */
    g_assert_null(rules->permits[0]);
    g_assert_null(rules->profiles[0]);
    g_assert_null(rules->paths[0]);
    g_assert_null(rules->dbus_user->own[0]);
    g_assert_null(rules->dbus_user->talk[0]);
    g_assert_null(rules->dbus_system->own[0]);
    g_assert_null(rules->dbus_system->talk[0]);

    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(dir);
    g_free(dir);
}

/*==========================================================================*
 * default_profile
 *==========================================================================*/

static
void
test_default_profile(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.profile", NULL);
    char* base_profile = g_build_filename(dir, "Base.profile", NULL);
    JailConf* conf = jail_conf_new();
    JailRules* rules;
    JailRulesOpt opt;
    char* path = NULL;
    char* section = NULL;
    GError* error = NULL;
    static const char dummy_data[] = "dummy";
    static const char profile_data[] =
        "[Sailjail]\n"
        "FileAccess=?/tmp,/tmp"; /* Required overrides optional */

    g_assert(g_file_set_contents(base_profile, dummy_data, -1, NULL));
    g_assert(g_file_set_contents(profile, profile_data, -1, NULL));
    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;

    rules = jail_rules_new("foo", conf, &opt, &path, &section, &error);
    test_assert_rules_not_null(rules);
    g_assert_cmpstr(path, == ,profile);
    g_assert_cmpstr(section, == ,"Sailjail");
    g_assert_null(error);
    g_free(path);
    g_free(section);

    /* There's nothing there except Base and optional /tmp dir */
    g_assert_null(rules->permits[0]);

    /* Base must be there */
    g_assert(rules->profiles[0]);
    g_assert_true(rules->profiles[0]->require);
    g_assert_cmpstr(rules->profiles[0]->path, == ,base_profile);
    g_assert_null(rules->profiles[1]);

    g_assert(rules->paths[0]);
    g_assert_true(rules->paths[0]->require);
    g_assert_true(rules->paths[0]->allow);
    g_assert_cmpstr(rules->paths[0]->path, == ,"/tmp");
    g_assert_null(rules->paths[1]);

    g_assert_null(rules->dbus_user->own[0]);
    g_assert_null(rules->dbus_user->talk[0]);
    g_assert_null(rules->dbus_system->own[0]);
    g_assert_null(rules->dbus_system->talk[0]);

    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(base_profile);
    remove(profile);
    remove(dir);

    g_free(base_profile);
    g_free(profile);
    g_free(dir);
}

/*==========================================================================*
 * desktop
 *==========================================================================*/

static
void
test_desktop(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.desktop", NULL);
    char* base_profile = g_build_filename(dir, "Base.profile", NULL);
    JailConf* conf = jail_conf_new();
    JailRules* rules;
    JailRulesOpt opt;
    GError* error = NULL;
    static const char dummy_data[] = "dummy";
    static const char profile_data[] =
        "[Desktop Entry]\n"
        "Type = Application\n"
        "[foo]\n"
        "DBusUserOwn = foo.bar";

    g_assert(g_file_set_contents(base_profile, dummy_data, -1, NULL));
    g_assert(g_file_set_contents(profile, profile_data, -1, NULL));
    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = conf->desktop_dir = dir;
    opt.profile = "foo.desktop";

    /* Make sure HOME is defined */
    setenv("HOME", "/home", FALSE);
    rules = jail_rules_new("foo", conf, &opt, NULL, NULL, &error);
    g_assert_null(error);

    test_assert_rules_not_null(rules);
    g_assert_null(rules->permits[0]);

    /* Base must be there */
    g_assert(rules->profiles[0]);
    g_assert_true(rules->profiles[0]->require);
    g_assert_cmpstr(rules->profiles[0]->path, == ,base_profile);
    g_assert_null(rules->profiles[1]);

    /* Three optional app directories */
    g_assert(rules->paths[0]);
    g_assert(rules->paths[1]);
    g_assert_cmpstr(rules->paths[0]->path, == ,"/usr/share/foo");
    g_assert_false(rules->paths[0]->require);
    g_assert_cmpstr(rules->paths[1]->path, == ,profile);
    g_assert_false(rules->paths[1]->require);
    g_assert(g_str_has_suffix(rules->paths[2]->path,"/.local/share/foo"));
    g_assert_false(rules->paths[2]->require);
    g_assert_null(rules->paths[3]);

    /* And a D-Bus name */
    g_assert(rules->dbus_user->own[0]);
    g_assert_cmpstr(rules->dbus_user->own[0]->name, == ,"foo.bar");
    g_assert_true(rules->dbus_user->own[0]->require);
    g_assert_null(rules->dbus_user->own[1]);
    g_assert_null(rules->dbus_user->talk[0]);
    g_assert_null(rules->dbus_system->own[0]);
    g_assert_null(rules->dbus_system->talk[0]);

    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(base_profile);
    remove(profile);
    remove(dir);

    g_free(base_profile);
    g_free(profile);
    g_free(dir);
}

/*==========================================================================*
 * bad_profile_name
 *==========================================================================*/

static
void
test_bad_profile_name(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    JailConf* conf = jail_conf_new();
    JailRulesOpt opt;
    GError* error = NULL;
    char* path = NULL;
    char* section = NULL;

    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;
    opt.profile = "foo.bar";

    /* This is a relative path pointing to a non-existent file */
    g_assert_null(jail_rules_new("foo", conf, &opt, &path, &section, &error));
    g_assert(error);
    g_assert_null(path);
    g_assert_null(section);
    g_error_free(error);

    /* Error and path are optional */
    g_assert_null(jail_rules_new("foo", conf, &opt, NULL, NULL, NULL));

    jail_conf_free(conf);

    remove(dir);
    g_free(dir);
}

/*==========================================================================*
 * bad_profile
 *==========================================================================*/

static
void
test_bad_profile(
    gconstpointer profile_data)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.profile", NULL);
    JailConf* conf = jail_conf_new();
    JailRulesOpt opt;
    GError* error = NULL;
    char* path = NULL;
    char* section = NULL;

    g_assert(g_file_set_contents(profile, profile_data, -1, NULL));
    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;
    opt.profile = profile;
    opt.section = "No such section";

    /* We fail to load this profile */
    g_assert_null(jail_rules_new("foo", conf, &opt, &path, &section, &error));
    g_assert(error);
    g_assert_null(path);
    g_assert_null(section);
    g_error_free(error);

    /* Error and path is optional */
    g_assert_null(jail_rules_new("foo", conf, &opt, NULL, NULL, NULL));

    jail_conf_free(conf);

    remove(profile);
    remove(dir);

    g_free(profile);
    g_free(dir);
}

static const char* const bad_profile_tests[] = {
    "[Whatever]\nfoo=bar",
    "not a key-value file at all"
};

/*==========================================================================*
 * keyfile
 *==========================================================================*/

static
void
test_keyfile(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* base_profile = g_build_filename(dir, "Base.profile", NULL);
    char* test_profile = g_build_filename(dir, "Test.profile", NULL);
    GKeyFile* keyfile = g_key_file_new();
    JailConf* conf = jail_conf_new();
    SailJail jail = { NULL, conf };
    JailRules* rules;
    static const char profile_data[] =
        "[Sailjail]\n"
        "Permissions=Test\n"
        "FileAccess=";

    g_assert(g_key_file_load_from_data(keyfile, profile_data, -1, 0, NULL));

    /* Create Base.profile and Test.profile */
    conf->profile_dir = conf->perm_dir = dir;
    g_assert(g_file_set_contents(base_profile, NULL, 0, NULL));
    g_assert(g_file_set_contents(test_profile, NULL, 0, NULL));

    rules = jail_rules_keyfile_parse(&jail, keyfile, NULL, NULL);
    test_assert_rules_not_null(rules);

    g_assert_null(rules->permits[0]);
    g_assert_null(rules->paths[0]);

    /* Both Base and Test profiles must be there */
    g_assert(rules->profiles[0]);
    g_assert_cmpstr(rules->profiles[0]->path, == ,base_profile);
    g_assert(rules->profiles[1]);
    g_assert_cmpstr(rules->profiles[1]->path, == ,test_profile);
    g_assert_null(rules->profiles[2]);

    /* Now try non-existen section */
    jail_rules_unref(rules);
    rules = jail_rules_keyfile_parse(&jail, keyfile, "foo", NULL);
    test_assert_rules_not_null(rules);

    g_assert_null(rules->permits[0]);
    g_assert_null(rules->paths[0]);

    /* Only Base must be there */
    g_assert(rules->profiles[0]);
    g_assert_cmpstr(rules->profiles[0]->path, == ,base_profile);
    g_assert_null(rules->profiles[1]);

    jail_rules_unref(rules);
    jail_conf_free(conf);
    g_key_file_unref(keyfile);

    remove(base_profile);
    remove(test_profile);
    remove(dir);

    g_free(base_profile);
    g_free(test_profile);
    g_free(dir);
}

/*==========================================================================*
 * basic/1
 *==========================================================================*/

static
void
test_basic1(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.profile", NULL);
    JailConf* conf = jail_conf_new();
    JailDBusRestrict none;
    JailRules* rules;
    JailRules* rules2;
    JailRulesOpt opt;
    char* path = NULL;
    char* section = NULL;
    static const char profile_data[] =
        "[Sailjail]\n"
        "Permissions=Test\n"
        "FileAccess=";

    g_assert(g_file_set_contents(profile, profile_data, -1, NULL));
    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;
    opt.profile = "foo.profile"; /* Not the full path */
    rules = jail_rules_new("foo", conf, &opt, &path, &section, NULL);
    test_assert_rules_not_null(rules);
    g_assert_cmpstr(path, == ,profile);
    g_assert_cmpstr(section, == ,"Sailjail");
    g_free(path);
    g_free(section);

    /* There's nothing there */
    g_assert_null(rules->permits[0]);
    g_assert_null(rules->profiles[0]);
    g_assert_null(rules->paths[0]);

    /* Test refcounting */
    g_assert(jail_rules_ref(rules) == rules);
    jail_rules_unref(rules);

    /* Restrict nothing */
    rules2 = jail_rules_restrict(rules, NULL, NULL, NULL, NULL, NULL);
    g_assert(rules2);
    jail_rules_unref(rules2);

    memset(&none, 0, sizeof(none));
    rules2 = jail_rules_restrict(rules, NULL, NULL, NULL, &none, &none);
    g_assert(rules2);
    jail_rules_unref(rules2);

    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(profile);
    remove(dir);

    g_free(profile);
    g_free(dir);
}

/*==========================================================================*
 * basic/2
 *==========================================================================*/

static
void
test_basic2(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.profile", NULL);
    char* base_profile = g_build_filename(dir, "Base.profile", NULL);
    char* test_profile = g_build_filename(dir, "Test.profile", NULL);
    JailConf* conf = jail_conf_new();
    JailRules* rules;
    JailRulesOpt opt;
    char* path = NULL;
    char* section = NULL;
    static const char dummy_data[] = "dummy";
    /*
     * Required Privileged overrides the optional one,
     * but optional Base doesn't overrides the implicit required one.
     * Also test all possible list separators (:;,)
     */
    static const char profile_data[] =
        "[Section]\n" /* Lonely ? and - are ignored */
        "Permissions = ?: ? Base; ? Test, ? Privileged, ! Privileged\n"
        "FileAccess = ?: -, /tmp; /tmp\n" /* Only one path remains */
        "DBusUserOwn = foo.bar, ."; /* . is an invalid name and is ignored */

    g_assert(g_file_set_contents(profile, profile_data, -1, NULL));
    g_assert(g_file_set_contents(base_profile, dummy_data, -1, NULL));
    g_assert(g_file_set_contents(test_profile, dummy_data, -1, NULL));
    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;
    opt.profile = profile;
    opt.section = "Section";
    rules = jail_rules_new("foo", conf, &opt, &path, &section, NULL);
    g_assert_cmpstr(path, == ,profile);
    g_assert_cmpstr(section, == ,"Section");
    g_free(path);
    g_free(section);

    test_assert_rules_not_null(rules);
    g_assert(rules->permits[0]);
    g_assert(rules->permits[0]->require);
    g_assert_cmpint(rules->permits[0]->type, == ,JAIL_PERMIT_PRIVILEGED);
    g_assert_null(rules->permits[1]);

    /* Base and Test profiles must be there, in that order */
    g_assert(rules->profiles[0]);
    g_assert(rules->profiles[1]);
    g_assert_null(rules->profiles[2]);
    g_assert_true(rules->profiles[0]->require);
    g_assert_cmpstr(rules->profiles[0]->path, == ,base_profile);
    g_assert_false(rules->profiles[1]->require); /* Test is optional */
    g_assert_cmpstr(rules->profiles[1]->path, == ,test_profile);

    g_assert_true(rules->paths[0]->require);
    g_assert_true(rules->paths[0]->allow);
    g_assert_cmpstr(rules->paths[0]->path, == ,"/tmp");
    g_assert_null(rules->paths[1]);

    g_assert(rules->dbus_user->own[0]);
    g_assert_cmpstr(rules->dbus_user->own[0]->name, == ,"foo.bar");
    g_assert_true(rules->dbus_user->own[0]->require);
    g_assert_null(rules->dbus_user->own[1]);
    g_assert_null(rules->dbus_user->talk[0]);
    g_assert_null(rules->dbus_system->own[0]);
    g_assert_null(rules->dbus_system->talk[0]);

    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(base_profile);
    remove(test_profile);
    remove(profile);
    remove(dir);

    g_free(base_profile);
    g_free(test_profile);
    g_free(profile);
    g_free(dir);
}

/*==========================================================================*
 * restrict_basic
 *==========================================================================*/

static
void
test_restrict_basic(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.profile", NULL);
    char* base_profile = g_build_filename(dir, "Base.profile", NULL);
    char* foo_profile = g_build_filename(dir, "Foo.profile", NULL);
    char* bar_profile = g_build_filename(dir, "Bar.profile", NULL);
    JailConf* conf = jail_conf_new();
    JailRules* rules;
    JailRules* rules2;
    JailRulesOpt opt;
    char* path = NULL;
    char* section = NULL;
    static const char dummy_data[] = "dummy";
    /*
     * Required Privileged overrides the optional one,
     * but optional Base doesn't overrides the implicit required one.
     */
    static const char profile_data[] =
        "[Sailjail]\n"
        "Permissions = Foo, ?Bar, ?Privileged\n"
        "FileAccess = ?/tmp,-/tmp/x\n"
        "DBusUserOwn = foo.bar\n";
    static const JAIL_PERMIT_TYPE remain_permits[] = {
        JAIL_PERMIT_PRIVILEGED, JAIL_PERMIT_INVALID
    };
    const char* remain_profiles[] = { foo_profile, NULL };
    static const char* const remain_paths[] = { "/tmp", NULL };
    static const char* const remain_dbus_names[] = { NULL };
    static const JailDBusRestrict remain_dbus = { remain_dbus_names, NULL };

    g_assert(g_file_set_contents(profile, profile_data, -1, NULL));
    g_assert(g_file_set_contents(base_profile, dummy_data, -1, NULL));
    g_assert(g_file_set_contents(foo_profile, dummy_data, -1, NULL));
    g_assert(g_file_set_contents(bar_profile, dummy_data, -1, NULL));
    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;
    opt.profile = profile;
    rules = jail_rules_new("foo", conf, &opt, &path, &section, NULL);
    g_assert_cmpstr(path, == ,profile);
    g_assert_cmpstr(section, == ,"Sailjail");
    g_free(path);
    g_free(section);

    test_assert_rules_not_null(rules);
    g_assert(rules->permits[0]);
    g_assert_false(rules->permits[0]->require);
    g_assert_cmpint(rules->permits[0]->type, == ,JAIL_PERMIT_PRIVILEGED);
    g_assert_null(rules->permits[1]);

    /* Base and Test profiles must be there, in that order */
    g_assert(rules->profiles[0]);
    g_assert(rules->profiles[1]);
    g_assert(rules->profiles[2]);
    g_assert_null(rules->profiles[3]);
    g_assert_true(rules->profiles[0]->require);
    g_assert_cmpstr(rules->profiles[0]->path, == ,base_profile);
    g_assert_true(rules->profiles[1]->require); /* Foo is required */
    g_assert_cmpstr(rules->profiles[1]->path, == ,foo_profile);
    g_assert_false(rules->profiles[2]->require); /* Bar is optional */
    g_assert_cmpstr(rules->profiles[2]->path, == ,bar_profile);

    g_assert(rules->paths[0]);
    g_assert(rules->paths[1]);
    g_assert_null(rules->paths[2]);
    g_assert_false(rules->paths[0]->require);
    g_assert_true(rules->paths[0]->allow);
    g_assert_cmpstr(rules->paths[0]->path, == ,"/tmp");
    g_assert_false(rules->paths[1]->require);
    g_assert_false(rules->paths[1]->allow);
    g_assert_cmpstr(rules->paths[1]->path, == ,"/tmp/x");

    g_assert(rules->dbus_user->own[0]);
    g_assert_cmpstr(rules->dbus_user->own[0]->name, == ,"foo.bar");
    g_assert_true(rules->dbus_user->own[0]->require);
    g_assert_null(rules->dbus_user->own[1]);
    g_assert_null(rules->dbus_user->talk[0]);
    g_assert_null(rules->dbus_system->own[0]);
    g_assert_null(rules->dbus_system->talk[0]);

    /* Derive restricted rules */
    rules2 = jail_rules_restrict(rules, remain_permits, remain_profiles,
        remain_paths, &remain_dbus, NULL);
    test_assert_rules_not_null(rules2);

    /* Permission remained there */
    g_assert(rules2->permits[0]);
    g_assert_false(rules2->permits[0]->require);
    g_assert_cmpint(rules2->permits[0]->type, == ,JAIL_PERMIT_PRIVILEGED);
    g_assert_null(rules2->permits[1]);

    /* Base and Foo are still there because they are required, Bar is gone */
    g_assert(rules2->profiles[0]);
    g_assert(rules2->profiles[1]);
    g_assert_null(rules2->profiles[2]);
    g_assert_true(rules2->profiles[0]->require);
    g_assert_cmpstr(rules2->profiles[0]->path, == ,base_profile);
    g_assert_true(rules2->profiles[1]->require); /* Foo is required */
    g_assert_cmpstr(rules2->profiles[1]->path, == ,foo_profile);

    /* Both paths are still there */
    g_assert(rules2->paths[0]);
    g_assert(rules2->paths[1]);
    g_assert_null(rules2->paths[2]);
    g_assert_false(rules2->paths[0]->require);
    g_assert_true(rules2->paths[0]->allow);
    g_assert_cmpstr(rules2->paths[0]->path, == ,"/tmp");
    g_assert_false(rules2->paths[1]->require);
    g_assert_false(rules2->paths[1]->allow);
    g_assert_cmpstr(rules2->paths[1]->path, == ,"/tmp/x");

    /* D-Bus names didn't change too */
    g_assert(rules2->dbus_user->own[0]);
    g_assert_cmpstr(rules2->dbus_user->own[0]->name, == ,"foo.bar");
    g_assert_true(rules2->dbus_user->own[0]->require);
    g_assert_null(rules2->dbus_user->own[1]);
    g_assert_null(rules2->dbus_user->talk[0]);
    g_assert_null(rules2->dbus_system->own[0]);
    g_assert_null(rules2->dbus_system->talk[0]);

    jail_rules_unref(rules2);
    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(base_profile);
    remove(foo_profile);
    remove(bar_profile);
    remove(profile);
    remove(dir);

    g_free(base_profile);
    g_free(foo_profile);
    g_free(bar_profile);
    g_free(profile);
    g_free(dir);
}

/*==========================================================================*
 * restrict_empty
 *==========================================================================*/

static
void
test_restrict_empty(
    void)
{
    JailRules rules;
    JailRules* restricted;

    memset(&rules, 0, sizeof(rules));
    restricted = jail_rules_restrict(&rules, NULL, NULL, NULL, NULL, NULL);
    test_assert_rules_not_null(restricted);
    jail_rules_unref(restricted);
}

/*==========================================================================*
 * restrict_required_permit
 *==========================================================================*/

static
void
test_restrict_required_permit(
    void)
{
    static const JailPermit privileged = { TRUE, JAIL_PERMIT_PRIVILEGED };
    static const JailPermit* permits[] = { &privileged, NULL };
    static const JAIL_PERMIT_TYPE restrict_types[] = {
        JAIL_PERMIT_INVALID
    };

    JailRules rules;
    JailRules* restricted;

    memset(&rules, 0, sizeof(rules));
    rules.permits = permits;
    restricted = jail_rules_restrict(&rules, restrict_types,
        NULL, NULL, NULL, NULL);
    test_assert_rules_not_null(restricted);

    /* Required JAIL_PERMIT_PRIVILEGED remains there */
    g_assert(restricted->permits[0]);
    g_assert(restricted->permits[0]->require);
    g_assert_cmpint(restricted->permits[0]->type, == ,privileged.type);
    g_assert_null(restricted->permits[1]);

    /* Same thing with NULL types */
    jail_rules_unref(restricted);
    restricted = jail_rules_restrict(&rules, NULL, NULL, NULL, NULL, NULL);
    test_assert_rules_not_null(restricted);

    /* Required JAIL_PERMIT_PRIVILEGED is still there */
    g_assert(restricted->permits[0]);
    g_assert(restricted->permits[0]->require);
    g_assert_cmpint(restricted->permits[0]->type, == ,privileged.type);
    g_assert_null(restricted->permits[1]);

    jail_rules_unref(restricted);
}

/*==========================================================================*
 * restrict_invalid_permit
 *==========================================================================*/

static
void
test_restrict_invalid_permit(
    void)
{
    static const JailPermit privileged = { FALSE, JAIL_PERMIT_PRIVILEGED };
    static const JailPermit* permits[] = { &privileged, NULL };
    static const JAIL_PERMIT_TYPE restrict_types[] = {
        (JAIL_PERMIT_TYPE) -1, /* Invalid but not JAIL_PERMIT_INVALID */
        JAIL_PERMIT_INVALID
    };

    JailRules rules;
    JailRules* restricted;

    memset(&rules, 0, sizeof(rules));
    rules.permits = permits;
    restricted = jail_rules_restrict(&rules, restrict_types,
        NULL, NULL, NULL, NULL);
    test_assert_rules_not_null(restricted);

    /* Optional JAIL_PERMIT_PRIVILEGED gets removed */
    g_assert_null(restricted->permits[0]);

    /* Same thing with NULL types */
    jail_rules_unref(restricted);
    restricted = jail_rules_restrict(&rules, NULL, NULL, NULL, NULL, NULL);
    test_assert_rules_not_null(restricted);

    /* Optional JAIL_PERMIT_PRIVILEGED gets removed again */
    g_assert_null(restricted->permits[0]);

    jail_rules_unref(restricted);
}

/*==========================================================================*
 * restrict_dbus
 *==========================================================================*/

static
void
test_restrict_dbus(
    void)
{
    static const char* foo[] = { "foo", NULL };
    static const char* bar[] = { "bar", NULL };
    static const JailDBusName foo_req = { TRUE, "foo" };
    static const JailDBusName bar_req = { TRUE, "bar" };
    static const JailDBusName foo_opt = { FALSE, "foo" };
    static const JailDBusName bar_opt = { FALSE, "bar" };
    static const JailDBusName* foo_req_list[] = { &foo_req, NULL };
    static const JailDBusName* bar_req_list[] = { &bar_req, NULL };
    static const JailDBusName* foo_opt_list[] = { &foo_opt, NULL };
    static const JailDBusName* bar_opt_list[] = { &bar_opt, NULL };
    static const JailDBus dbus_user = { foo_req_list, bar_opt_list };
    static const JailDBus dbus_system = { foo_opt_list, bar_req_list };
    static const JailDBus dbus_null = { NULL, NULL };
    static const JailDBusRestrict restrict_user = { foo, bar };
    static const JailDBusRestrict restrict_system = { bar, foo };

    JailRules rules;
    JailRules* restricted;

    memset(&rules, 0, sizeof(rules));

    /* First try empty lists. There's nothing to restrict there */
    rules.dbus_user = &dbus_null;
    restricted = jail_rules_restrict(&rules, NULL, NULL, NULL, NULL, NULL);
    test_assert_rules_not_null(restricted);
    g_assert_null(restricted->dbus_user->own[0]);
    g_assert_null(restricted->dbus_user->talk[0]);
    g_assert_null(restricted->dbus_system->own[0]);
    g_assert_null(restricted->dbus_system->talk[0]);

    /* Then try completely restricting non-trivial rules */
    rules.dbus_user = &dbus_user;
    rules.dbus_system = &dbus_system;

    jail_rules_unref(restricted);
    restricted = jail_rules_restrict(&rules, NULL, NULL, NULL, NULL, NULL);
    test_assert_rules_not_null(restricted);

    /* Required things remain */
    g_assert(restricted->dbus_user->own[0]);
    g_assert_true(restricted->dbus_user->own[0]->require);
    g_assert_cmpstr(restricted->dbus_user->own[0]->name, == , "foo");
    g_assert_null(restricted->dbus_user->own[1]);

    g_assert_null(restricted->dbus_user->talk[0]);
    g_assert_null(restricted->dbus_system->own[0]);

    g_assert(restricted->dbus_system->talk[0]);
    g_assert_true(restricted->dbus_system->talk[0]->require);
    g_assert_cmpstr(restricted->dbus_system->talk[0]->name, == , "bar");
    g_assert_null(restricted->dbus_system->talk[1]);

    /* Non-trivial restricting and non-trivial rules */
    jail_rules_unref(restricted);
    restricted = jail_rules_restrict(&rules, NULL, NULL, NULL, &restrict_user,
        &restrict_system);
    test_assert_rules_not_null(restricted);

    /* This "foo" is required and allowed => remains there */
    g_assert(restricted->dbus_user->own[0]);
    g_assert_true(restricted->dbus_user->own[0]->require);
    g_assert_cmpstr(restricted->dbus_user->own[0]->name, == , "foo");
    g_assert_null(restricted->dbus_user->own[1]);

    /* This "bar" is optional and allowed => remains there */
    g_assert(restricted->dbus_user->talk[0]);
    g_assert_false(restricted->dbus_user->talk[0]->require);
    g_assert_cmpstr(restricted->dbus_user->talk[0]->name, == , "bar");
    g_assert_null(restricted->dbus_user->talk[1]);

    /* This "foo" is optional and not allowed => gets removed */
    g_assert_null(restricted->dbus_system->own[0]);

    /* This "bar" is required and not allowed => but remains there */
    g_assert(restricted->dbus_system->talk[0]);
    g_assert_true(restricted->dbus_system->talk[0]->require);
    g_assert_cmpstr(restricted->dbus_system->talk[0]->name, == , "bar");
    g_assert_null(restricted->dbus_system->talk[1]);

    jail_rules_unref(restricted);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/rules/" name

int main(int argc, char* argv[])
{
    guint i;
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("null"), test_null);
    g_test_add_func(TEST_("permit_name"), test_permit_name);
    g_test_add_func(TEST_("missing_profile"), test_missing_profile);
    g_test_add_func(TEST_("default_profile"), test_default_profile);
    g_test_add_func(TEST_("desktop"), test_desktop);
    g_test_add_func(TEST_("bad_profile_name"), test_bad_profile_name);
    for (i = 0; i < G_N_ELEMENTS(bad_profile_tests); i++) {
        char* name = g_strdup_printf(TEST_("bad_profile/%d"), i + 1);

        g_test_add_data_func(name, bad_profile_tests[i], test_bad_profile);
        g_free(name);
    }
    g_test_add_func(TEST_("keyfile"), test_keyfile);
    g_test_add_func(TEST_("basic/1"), test_basic1);
    g_test_add_func(TEST_("basic/2"), test_basic2);
    g_test_add_func(TEST_("restrict/basic"), test_restrict_basic);
    g_test_add_func(TEST_("restrict/dbus"), test_restrict_dbus);
    g_test_add_func(TEST_("restrict/empty"), test_restrict_empty);
    g_test_add_func(TEST_("restrict/invalid_permit"),
        test_restrict_invalid_permit);
    g_test_add_func(TEST_("restrict/required_permit"),
        test_restrict_required_permit);
    test_init(&test_opt, argc, argv);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

