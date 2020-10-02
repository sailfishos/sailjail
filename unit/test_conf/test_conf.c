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

static TestOpt test_opt;

static const char TMP_DIR_TEMPLATE[] = "sailjail-test-conf-XXXXXX";

static const char DEFAULT_EXEC[] = "/usr/bin/firejail";
static const char DEFAULT_DESKTOP_DIR[] = "/usr/share/applications";
static const char DEFAULT_PROFILE_DIR[] = "/etc/sailjail";
static const char DEFAULT_PERM_DIR[] = "/etc/sailjail/permissions";

#define SECTION "Settings"
#define KEY_EXEC "Exec"
#define KEY_PLUGIN_DIR "PluginDir"
#define KEY_DESKTOP_DIR "DesktopDir"
#define KEY_PROFILE_DIR "ProfileDir"
#define KEY_PERM_DIR "PermissionsDir"

/*==========================================================================*
 * basic
 *==========================================================================*/

static
void
test_basic(
    void)
{
    JailConf* conf = jail_conf_new();

    g_assert(conf);
    g_assert(conf->plugin_dir); /* This one may depend on the architecture */
    g_assert(!g_strcmp0(conf->exec, DEFAULT_EXEC));
    g_assert(!g_strcmp0(conf->desktop_dir, DEFAULT_DESKTOP_DIR));
    g_assert(!g_strcmp0(conf->profile_dir, DEFAULT_PROFILE_DIR));
    g_assert(!g_strcmp0(conf->perm_dir, DEFAULT_PERM_DIR));
    jail_conf_free(conf);
}

/*==========================================================================*
 * load_error
 *==========================================================================*/

static
void
test_load_error(
    void)
{
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* file = g_build_filename(dir, "test.conf", NULL); /* Doesn't exist */
    JailConf* conf = jail_conf_new();
    GError* error = NULL;

    /* Expect jail_conf_load to fail */
    g_assert(!jail_conf_load(conf, file, &error));
    g_error_free(error);

    jail_conf_free(conf);
    remove(dir);
    g_free(file);
    g_free(dir);
}

/*==========================================================================*
 * load
 *==========================================================================*/

typedef struct test_load_data {
    const char* name;
    const char* in;
    JailConf out;
} TestLoadData;

static
void
test_load(
    gconstpointer data)
{
    const TestLoadData* test = data;
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* file = g_build_filename(dir, "test.conf", NULL);
    JailConf* default_conf = jail_conf_new();
    JailConf* conf = jail_conf_new();
    GError* error = NULL;

    g_assert(g_file_set_contents(file, test->in, -1, NULL));
    g_assert(jail_conf_load(conf, file, &error));
    g_assert(!error);

    g_assert_cmpstr(conf->exec, == ,test->out.exec ?
        test->out.exec : default_conf->exec);
    g_assert_cmpstr(conf->plugin_dir, == ,test->out.plugin_dir ?
        test->out.plugin_dir : default_conf->plugin_dir);
    g_assert_cmpstr(conf->desktop_dir, == ,test->out.desktop_dir ?
        test->out.desktop_dir : default_conf->desktop_dir);
    g_assert_cmpstr(conf->profile_dir, == ,test->out.profile_dir ?
        test->out.profile_dir : default_conf->profile_dir);
    g_assert_cmpstr(conf->perm_dir, == ,test->out.perm_dir ?
        test->out.perm_dir : default_conf->perm_dir);

    jail_conf_free(default_conf);
    jail_conf_free(conf);

    remove(file);
    g_free(file);

    remove(dir);
    g_free(dir);
}

static const TestLoadData load_tests[] = {
    {
        .name = KEY_EXEC,
        .in = "[" SECTION "]\n" KEY_EXEC "=/bin/false\n",
        .out.exec = "/bin/false"
    },{
        .name = KEY_PLUGIN_DIR,
        .in = "[" SECTION "]\n" KEY_PLUGIN_DIR "=/plugin_dir\n",
        .out.plugin_dir = "/plugin_dir"
    },{
        .name = KEY_DESKTOP_DIR,
        .in = "[" SECTION "]\n" KEY_DESKTOP_DIR "=/desktop_dir\n",
        .out.desktop_dir = "/desktop_dir"
    },{
        .name = KEY_PROFILE_DIR,
        .in = "[" SECTION "]\n" KEY_PROFILE_DIR "=/profile_dir\n",
        .out = { /* ProfileDir affects PermissionsDir */
            .profile_dir = "/profile_dir",
            .perm_dir = "/profile_dir/permissions"
        }
    },{
        .name = KEY_PERM_DIR,
        .in = "[" SECTION "]\n" KEY_PERM_DIR "=/perm_dir\n",
        .out.perm_dir = "/perm_dir"
    },{
        .name = KEY_PROFILE_DIR "+" KEY_PERM_DIR,
        .in = "[" SECTION "]\n"
            KEY_PROFILE_DIR "=/profile_dir\n"
            KEY_PERM_DIR "=/perm_dir\n",
        .out = {
            .profile_dir = "/profile_dir",
            .perm_dir = "/perm_dir"
        }
    }
};

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/launch/" name

int main(int argc, char* argv[])
{
    guint i;

    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("basic"), test_basic);
    g_test_add_func(TEST_("load_error"), test_load_error);
    for (i = 0; i < G_N_ELEMENTS(load_tests); i++) {
        const TestLoadData* test = load_tests + i;
        char* name = g_strconcat(TEST_("load/"), test->name, NULL);

        g_test_add_data_func(name, test, test_load);
        g_free(name);
    }
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

