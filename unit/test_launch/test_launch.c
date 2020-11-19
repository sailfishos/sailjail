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

#include "jail_conf.h"
#include "jail_launch.h"
#include "jail_launch_hook_impl.h"
#include "jail_rules_p.h"

#include <gutil_log.h>

static TestOpt test_opt;

#define TMP_DIR_TEMPLATE "sailjail-test-launch-XXXXXX"

/*==========================================================================*
 * Test hooks
 *==========================================================================*/

typedef struct test_hook {
    JailLaunchHook parent;
    int confirmed;
    int denied;
} TestHook, TestDenyHook;

typedef JailLaunchHookClass TestHookClass;
G_DEFINE_TYPE(TestHook, test_hook, JAIL_TYPE_LAUNCH_HOOK)
#define TEST_HOOK_TYPE (test_hook_get_type())
#define TEST_HOOK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        TEST_HOOK_TYPE, TestHook))

static
JailRules*
test_hook_confirm_launch(
    JailLaunchHook* hook,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    JailRules* rules)
{
    GDEBUG("Launch approved");
    return JAIL_LAUNCH_HOOK_CLASS(test_hook_parent_class)->
        confirm_launch(hook, app, cmd, user, rules);
}

static
void
test_hook_launch_confirmed(
    JailLaunchHook* hook,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    const JailRules* rules)
{
    TEST_HOOK(hook)->confirmed++;
    GDEBUG("Launch confirmed");
}

static
void
test_hook_launch_denied(
    JailLaunchHook* hook,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user)
{
    TEST_HOOK(hook)->denied++;
    GDEBUG("Launch denied");
}

static
void
test_hook_init(
    TestHook* self)
{
}

static
void
test_hook_class_init(
    TestHookClass* klass)
{
    klass->confirm_launch = test_hook_confirm_launch;
    klass->launch_confirmed = test_hook_launch_confirmed;
    klass->launch_denied = test_hook_launch_denied;
}

typedef TestHook TestDenyHook;
typedef TestHookClass TestDenyHookClass;
G_DEFINE_TYPE(TestDenyHook, test_deny_hook, TEST_HOOK_TYPE)
#define TEST_DENY_HOOK_TYPE (test_deny_hook_get_type())

static
JailRules*
test_deny_hook_confirm_launch(
    JailLaunchHook* hook,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    JailRules* rules)
{
    GDEBUG("Launch denied");
    return NULL;
}

static
void
test_deny_hook_init(
    TestHook* self)
{
}

static
void
test_deny_hook_class_init(
    TestDenyHookClass* klass)
{
    klass->confirm_launch = test_deny_hook_confirm_launch;
}

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    JailConf* conf = jail_conf_new();
    SailJail jail = { jail_launch_hooks_new(), conf };

    g_assert(!jail_launch_confirm(jail.hooks, NULL, NULL, NULL, NULL));
    g_assert(!jail_launch_hook_add(NULL, NULL));
    g_assert(!jail_launch_hook_add(&jail, NULL));
    jail_launch_hook_remove(NULL, 0);
    jail_launch_hook_remove(&jail, 0);
    jail_launch_hooks_free(NULL);
    jail_launch_hooks_free(jail.hooks);
    jail_conf_free(conf);
}

/*==========================================================================*
 * basic1
 *==========================================================================*/

static
void
test_basic1(
    void)
{
    JailConf* conf = jail_conf_new();
    SailJail jail = { jail_launch_hooks_new(), conf };
    JailLaunchHook* hook1 = g_object_new(TEST_HOOK_TYPE, NULL);
    JailLaunchHook* hook2 = g_object_new(TEST_HOOK_TYPE, NULL);
    JailLaunchHook* hook3 = g_object_new(TEST_HOOK_TYPE, NULL);
    guint id1 = jail_launch_hook_add(&jail, hook1); /* Adds a ref */
    guint id2 = jail_launch_hook_add(&jail, hook2); /* Adds a ref */
    guint id3 = jail_launch_hook_add(&jail, hook3); /* Adds a ref */

    g_assert(id1);
    g_assert(id2);
    g_assert(id3);
    g_assert_cmpuint(id1, != ,id2);
    g_assert_cmpuint(id1, != ,id3);
    g_assert_cmpuint(id2, != ,id3);
    jail_launch_hook_remove(&jail, 0); /* Does nothing */
    jail_launch_hook_remove(&jail, id2); /* Unrefs */
    jail_launch_hook_remove(&jail, id3); /* Unrefs */
    jail_launch_hook_remove(&jail, id1); /* Unrefs */
    jail_launch_hook_remove(&jail, id1); /* Does nothing (already removed) */

    /* And again */
    id1 = jail_launch_hook_add(&jail, hook1); /* Adds a ref */
    id2 = jail_launch_hook_add(&jail, hook2); /* Adds a ref */
    g_assert(id1);
    g_assert(id2);
    g_assert_cmpuint(id1, != ,id2);
    g_assert_cmpuint(id1, != ,id3);
    g_assert_cmpuint(id2, != ,id3);

    jail_launch_hook_unref(hook1);  /* Drop our ref */
    jail_launch_hook_unref(hook2);  /* Drop our ref */
    jail_launch_hook_unref(hook3);  /* Drop our ref */
    jail_launch_hooks_free(jail.hooks);
    jail_conf_free(conf);
}

/*==========================================================================*
 * basic2
 *==========================================================================*/

static
void
test_basic2(
    void)
{
    JailConf* conf = jail_conf_new();
    SailJail jail = { jail_launch_hooks_new(), conf };
    JailLaunchHook* hook1 = g_object_new(TEST_HOOK_TYPE, NULL);
    JailLaunchHook* hook2 = g_object_new(TEST_HOOK_TYPE, NULL);
    guint id1 = jail_launch_hook_add(&jail, hook1); /* Adds a ref */
    guint id2 = jail_launch_hook_add(&jail, hook2); /* Adds a ref */

    g_assert(id1);
    g_assert(id2);
    g_assert_cmpuint(id1, != ,id2);
    jail_launch_hook_unref(hook1);  /* Drop our ref */
    jail_launch_hook_unref(hook2);  /* Drop our ref */

    jail_launch_hooks_free(jail.hooks); /* Unrefs both hooks */
    jail_conf_free(conf);
}

/*==========================================================================*
 * confirm
 *==========================================================================*/

static
void
test_confirm(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.profile", NULL);
    JailConf* conf = jail_conf_new();
    SailJail jail = { jail_launch_hooks_new(), conf };
    JailLaunchHook* hook1 = g_object_new(TEST_HOOK_TYPE, NULL);
    JailLaunchHook* hook2 = g_object_new(TEST_HOOK_TYPE, NULL);
    JailRules* rules;
    JailRules* rules2;
    JailRulesOpt opt;
    JailApp app;
    JailCmdLine cmd;
    JailRunUser user;
    static const char* argv[] = { "/bin/foo", "bar" };

    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;
    rules = jail_rules_new("foo", conf, &opt, NULL, NULL, NULL);
    g_assert(rules);

    memset(&app, 0, sizeof(app));
    app.file = profile;
    app.section = "Failjail";

    memset(&cmd, 0, sizeof(cmd));
    cmd.argc = G_N_ELEMENTS(argv);
    cmd.argv = argv;

    memset(&user, 0, sizeof(user));

    /* Without any hooks it returns unmodified unput */
    rules2 = jail_launch_confirm(jail.hooks, &app, &cmd, &user, rules);
    g_assert(rules == rules2);
    jail_rules_unref(rules2);

    /* Add hooks */
    jail_launch_hook_add(&jail, hook1); /* Adds a ref */
    jail_launch_hook_add(&jail, hook2); /* Adds a ref */
    jail_launch_hook_unref(hook1);  /* Drop our ref */
    jail_launch_hook_unref(hook2);  /* Drop our ref */

    /* Since our test hooks are simple, input is returned unmodified */
    rules2 = jail_launch_confirm(jail.hooks, &app, &cmd, &user, rules);
    g_assert(rules == rules2);

    /* Tell the hooks what happened */
    jail_launch_confirmed(jail.hooks, &app, &cmd, &user, rules2);
    g_assert_cmpint(TEST_HOOK(hook1)->confirmed, == ,1);
    g_assert_cmpint(TEST_HOOK(hook2)->confirmed, == ,1);
    g_assert_cmpint(TEST_HOOK(hook1)->denied, == ,0);
    g_assert_cmpint(TEST_HOOK(hook2)->denied, == ,0);
    jail_rules_unref(rules2);

    /* Unrefs both hooks */
    jail_launch_hooks_free(jail.hooks);
    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(profile);
    remove(dir);
    g_free(profile);
    g_free(dir);
}

/*==========================================================================*
 * deny
 *==========================================================================*/

static
void
test_deny_impl(
    GType type1,
    GType type2)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* profile = g_build_filename(dir, "foo.profile", NULL);
    JailConf* conf = jail_conf_new();
    SailJail jail = { jail_launch_hooks_new(), conf };
    JailLaunchHook* hook1 = g_object_new(type1, NULL);
    JailLaunchHook* hook2 = g_object_new(type2, NULL);
    JailRules* rules;
    JailRulesOpt opt;
    JailApp app;
    JailCmdLine cmd;
    JailRunUser user;
    static const char* argv[] = { "/bin/foo", "bar" };

    /* Add hooks */
    jail_launch_hook_add(&jail, hook1); /* Adds a ref */
    jail_launch_hook_add(&jail, hook2); /* Adds a ref */
    jail_launch_hook_unref(hook1);  /* Drop our ref */
    jail_launch_hook_unref(hook2);  /* Drop our ref */

    memset(&opt, 0, sizeof(opt));
    conf->profile_dir = conf->perm_dir = dir;
    rules = jail_rules_new("foo", conf, &opt, NULL, NULL, NULL);
    g_assert(rules);

    memset(&app, 0, sizeof(app));
    app.file = profile;
    app.section = "Failjail";

    memset(&cmd, 0, sizeof(cmd));
    cmd.argc = G_N_ELEMENTS(argv);
    cmd.argv = argv;

    memset(&user, 0, sizeof(user));

    /* Launch is denied */
    g_assert(!jail_launch_confirm(jail.hooks, &app, &cmd, &user, rules));

    /* Tell the hooks what happened */
    jail_launch_denied(jail.hooks, &app, &cmd, &user);
    g_assert_cmpint(TEST_HOOK(hook1)->confirmed, == ,0);
    g_assert_cmpint(TEST_HOOK(hook2)->confirmed, == ,0);
    g_assert_cmpint(TEST_HOOK(hook1)->denied, == ,1);
    g_assert_cmpint(TEST_HOOK(hook2)->denied, == ,1);

    /* Unrefs both hooks */
    jail_launch_hooks_free(jail.hooks);
    jail_rules_unref(rules);
    jail_conf_free(conf);

    remove(profile);
    remove(dir);
    g_free(profile);
    g_free(dir);
}

static
void
test_deny1(
    void)
{
    test_deny_impl(TEST_HOOK_TYPE, TEST_DENY_HOOK_TYPE);
}

static
void
test_deny2(
    void)
{
    test_deny_impl(TEST_DENY_HOOK_TYPE, TEST_HOOK_TYPE);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/launch/" name

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("null"), test_null);
    g_test_add_func(TEST_("basic/1"), test_basic1);
    g_test_add_func(TEST_("basic/2"), test_basic2);
    g_test_add_func(TEST_("confirm"), test_confirm);
    g_test_add_func(TEST_("deny/1"), test_deny1);
    g_test_add_func(TEST_("deny/2"), test_deny2);
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

