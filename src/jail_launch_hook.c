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

#include "jail_types_p.h"
#include "jail_launch_hook_impl.h"
#include "jail_launch_hook_p.h"
#include "jail_rules.h"

#include <gutil_log.h>

G_DEFINE_ABSTRACT_TYPE(JailLaunchHook, jail_launch_hook, G_TYPE_OBJECT)
#define THIS(obj) JAIL_LAUNCH_HOOK(obj)
#define GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS((obj), \
    JAIL_TYPE_LAUNCH_HOOK, JailLaunchHookClass)

/*==========================================================================*
 * Internal interface
 *==========================================================================*/

JailLaunchHook*
jail_launch_hook_ref(
    JailLaunchHook* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(THIS(self));
    }
    return self;
}

void
jail_launch_hook_unref(
    JailLaunchHook* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(THIS(self));
    }
}

JailRules*
jail_launch_hook_confirm_launch(
    JailLaunchHook* self,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    JailRules* rules)
{
    return G_LIKELY(self) ?
        GET_CLASS(self)->confirm_launch(self, app, cmd, user, rules) :
        NULL;
}

void
jail_launch_hook_launch_confirmed(
    JailLaunchHook* self,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    const JailRules* rules)
{
    if (G_LIKELY(self)) {
        GET_CLASS(self)->launch_confirmed(self, app, cmd, user, rules);
    }
}

void
jail_launch_hook_launch_denied(
    JailLaunchHook* self,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user)
{
    if (G_LIKELY(self)) {
        GET_CLASS(self)->launch_denied(self, app, cmd, user);
    }
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
JailRules*
jail_launch_hook_default_confirm_launch(
    JailLaunchHook* self,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    JailRules* rules)
{
    return jail_rules_ref(rules);
}

static
void
jail_launch_hook_default_launch_confirmed(
    JailLaunchHook* self,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    const JailRules* rules)
{
}

static
void
jail_launch_hook_default_launch_denied(
    JailLaunchHook* self,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user)
{
}

static
void
jail_launch_hook_init(
    JailLaunchHook* self)
{
}

static
void
jail_launch_hook_class_init(
    JailLaunchHookClass* klass)
{
    klass->confirm_launch = jail_launch_hook_default_confirm_launch;
    klass->launch_confirmed = jail_launch_hook_default_launch_confirmed;
    klass->launch_denied = jail_launch_hook_default_launch_denied;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
