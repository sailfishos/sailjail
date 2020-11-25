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
#include "jail_creds.h"

#include <sys/types.h>
#include <unistd.h>

static TestOpt test_opt;

static const char TMP_DIR_TEMPLATE[] = "sailjail-test-creds-XXXXXX";

/*==========================================================================*
 * basic
 *==========================================================================*/

static
void
test_basic(
    void)
{
    GError* error = NULL;
    JailCreds* creds = jail_creds_for_pid(getpid(), &error);

    g_assert(creds);
    g_assert_null(error);
    jail_creds_free(creds);
    jail_creds_free(NULL); /* NULL is tolerated */

    g_assert_null(jail_creds_for_pid((pid_t)-1, &error));
    g_assert(error);
    g_error_free(error);
}

/*==========================================================================*
 * parse
 *==========================================================================*/

typedef struct test_parse_data {
    const char* name;
    const char* in;
    JailCreds out;
} TestParseData;

static
void
test_parse(
    gconstpointer data)
{
    const TestParseData* test = data;
    const JailCreds* expected = &test->out;
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* file = g_build_filename(dir, "status", NULL);
    GError* error = NULL;
    JailCreds* creds;

    g_assert(g_file_set_contents(file, test->in, -1, NULL));
    creds = jail_creds_from_file(file, &error);
    g_assert(creds);
    g_assert_null(error);

    g_assert_cmpuint(expected->ruid, == ,creds->ruid);
    g_assert_cmpuint(expected->euid, == ,creds->euid);
    g_assert_cmpuint(expected->suid, == ,creds->suid);
    g_assert_cmpuint(expected->fsuid, == ,creds->fsuid);
    g_assert_cmpuint(expected->rgid, == ,creds->rgid);
    g_assert_cmpuint(expected->egid, == ,creds->egid);
    g_assert_cmpuint(expected->sgid, == ,creds->sgid);
    g_assert_cmpuint(expected->fsgid, == ,creds->fsgid);
    g_assert_cmpuint(expected->ngroups, == ,creds->ngroups);
    g_assert_cmpint(memcmp(expected->groups, creds->groups,
        sizeof(gid_t) * creds->ngroups), == ,0);

    jail_creds_free(creds);

    remove(file);
    g_free(file);

    remove(dir);
    g_free(dir);
}

const static gid_t parse_tests_groups4[] = { 21, 22, 23, 24 };
static const TestParseData parse_tests[] = {
    {
        "ok",
        "Name:\ttest\n"
        "Pid:\t1\n"
        "PPid:\t1\n"
        "Uid:\t3\t4\t5\t6\n"
        "Gid:\t7\t8\t9\t10\n"
        "Ngid:\t11\n"
        "Groups:\t21 22 23 24", /* No EOL but it's OK */
        {
            3, 4, 5, 6, 7, 8, 9, 10,
            TEST_ARRAY_AND_COUNT(parse_tests_groups4)
        }
    }
};

/*==========================================================================*
 * error
 *==========================================================================*/

typedef struct test_error_data {
    const char* name;
    const char* in;
} TestErrorData;

static
void
test_error(
    gconstpointer data)
{
    const TestErrorData* test = data;
    char* dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
    char* file = g_build_filename(dir, "status", NULL);
    GError* error = NULL;

    g_assert(g_file_set_contents(file, test->in, -1, NULL));
    g_assert_null(jail_creds_from_file(file, NULL)); /* Error is optional */
    g_assert_null(jail_creds_from_file(file, &error));
    g_assert(error);
    g_error_free(error);

    remove(file);
    g_free(file);

    remove(dir);
    g_free(dir);
}

static const TestErrorData error_tests[] = {
    {
        "header",
        /* Uid: line must not be the first one */
        "Uid:\t3\t4\t5\t6\n"
        "Gid:\t7\t8\t9\t10\n"
        "Groups:\t21 22 23 24\n"
    },{
        "uids",
        "Name:\ttest\n"
        "Uid:\t3\t4\t5\n" /* <= Missing fsuid */
        "Gid:\t7\t8\t9\t10\n"
        "Groups:\t21 22 23 24\n"
    },{
        "gids",
        "Name:\ttest\n"
        "Uid:\t3\t4\t5\t6\n"
        "Gid:\t7\t8\t9\n"  /* <= Missing fsgid */
        "Groups:\t21 22 23 24\n"
    },{
        "nogroups1",
        "Name:\ttest\n"
        "Uid:\t3\t4\t5\t6\n"
        "Gid:\t7\t8\t9\t10\n"
        "Groups:\n"
    },{
        "nogroups2",
        "Name:\ttest\n"
        "Uid:\t3\t4\t5\t6\n"
        "Gid:\t7\t8\t9\t10\n"
    },{
        "badnum",
        "Name:\ttest\n"
        "Uid:\t3\t4\t5\t6\n"
        "Gid:\t7\txxx\t8\t9\n" /* xxx is not a number */
        "Groups:\t21 22 23 24\n"
    },{
        "negnum",
        "Name:\ttest\n"
        "Uid:\t3\t4\t5\t6\n"
        "Gid:\t7\t-1\t8\t9\n" /* negatives are not accepted */
        "Groups:\t21 22 23 24\n"
    }
};

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/creds/" name

int main(int argc, char* argv[])
{
    guint i;

    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("basic"), test_basic);
    for (i = 0; i < G_N_ELEMENTS(parse_tests); i++) {
        const TestParseData* test = parse_tests + i;
        char* name = g_strconcat(TEST_("parse/"), test->name, NULL);

        g_test_add_data_func(name, test, test_parse);
        g_free(name);
    }
    for (i = 0; i < G_N_ELEMENTS(error_tests); i++) {
        const TestErrorData* test = error_tests + i;
        char* name = g_strconcat(TEST_("error/"), test->name, NULL);

        g_test_add_data_func(name, test, test_error);
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

