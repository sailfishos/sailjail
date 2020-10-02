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

#include "jail_launch_hook_p.h"
#include "jail_launch_hook_impl.h"

static TestOpt test_opt;

/*==========================================================================*
 * Test hook
 *==========================================================================*/

typedef JailLaunchHook TestHook;
typedef JailLaunchHookClass TestHookClass;
G_DEFINE_TYPE(TestHook, test_hook, JAIL_TYPE_LAUNCH_HOOK)
#define TEST_HOOK_TYPE (test_hook_get_type())

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
}

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    g_assert(!jail_launch_hook_ref(NULL));
    jail_launch_hook_unref(NULL);
    g_assert(!jail_launch_hook_confirm_launch(NULL, NULL, NULL, NULL, NULL));
    jail_launch_hook_launch_confirmed(NULL, NULL, NULL, NULL, NULL);
    jail_launch_hook_launch_denied(NULL, NULL, NULL, NULL);
}

/*==========================================================================*
 * basic
 *==========================================================================*/

static
void
test_basic(
    void)
{
    JailLaunchHook* hook = g_object_new(TEST_HOOK_TYPE, NULL);

    g_assert(hook);
    g_assert(jail_launch_hook_ref(hook) == hook);
    jail_launch_hook_unref(hook);
    g_assert(!jail_launch_hook_confirm_launch(hook, NULL, NULL, NULL, NULL));
    jail_launch_hook_launch_confirmed(hook, NULL, NULL, NULL, NULL);
    jail_launch_hook_launch_denied(hook, NULL, NULL, NULL);
    jail_launch_hook_unref(hook);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/launch_hook/" name

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("null"), test_null);
    g_test_add_func(TEST_("basic"), test_basic);
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

