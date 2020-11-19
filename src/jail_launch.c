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

#include "jail_launch.h"
#include "jail_launch_hook_p.h"
#include "jail_rules.h"

#include <gutil_macros.h>

typedef struct jail_launch_hook_entry JailLaunchHookEntry;

struct jail_launch_hook_entry {
    JailLaunchHookEntry* next;
    JailLaunchHook* hook;
    guint id;
};

struct jail_launch_hooks {
    JailLaunchHookEntry* first;
    JailLaunchHookEntry* last;
    guint last_id;
};

static
void
jail_launch_entry_free(
    JailLaunchHookEntry* entry)
{
    jail_launch_hook_unref(entry->hook);
    gutil_slice_free(entry);
}

JailLaunchHooks*
jail_launch_hooks_new(
    void)
{
    return g_slice_new0(JailLaunchHooks);
}

void
jail_launch_hooks_free(
    JailLaunchHooks* hooks)
{
    if (hooks) {
        while (hooks->first) {
            JailLaunchHookEntry* entry = hooks->first;

            hooks->first = entry->next;
            jail_launch_entry_free(entry);
        }
        gutil_slice_free(hooks);
    }
}

guint
jail_launch_hook_add(
    SailJail* self,
    JailLaunchHook* hook)
{
    if (self && hook) {
        JailLaunchHookEntry* entry = g_slice_new0(JailLaunchHookEntry);
        JailLaunchHooks* hooks = self->hooks;

        entry->hook = jail_launch_hook_ref(hook);
        entry->id = ++(hooks->last_id);
        if (hooks->last) {
            hooks->last->next = entry;
        } else {
            hooks->first = entry;
        }
        hooks->last = entry;
        return entry->id;
    }
    return 0;
}

void
jail_launch_hook_remove(
    SailJail* self,
    guint id)
{
    if (self && id) {
        JailLaunchHooks* hooks = self->hooks;
        JailLaunchHookEntry* entry = hooks->first;
        JailLaunchHookEntry* prev = NULL;

        while (entry) {
            if (entry->id == id) {
                if (prev) {
                    prev->next = entry->next;
                } else {
                    hooks->first = entry->next;
                }
                if (!entry->next) {
                    hooks->last = prev;
                }
                jail_launch_entry_free(entry);
                break;
            }
            prev = entry;
            entry = entry->next;
        }
    }
}

JailRules*
jail_launch_confirm(
    JailLaunchHooks* hooks,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    JailRules* rules)
{
    JailRules* ret = jail_rules_ref(rules);
    JailLaunchHookEntry* ptr = hooks->first;

    while (ptr && ret) {
        JailLaunchHookEntry* next = ptr->next;
        JailRules* prev = ret;

        ret = jail_launch_hook_confirm_launch(ptr->hook, app, cmd, user, prev);
        jail_rules_unref(prev);
        ptr = next;
    }
    return ret;
}

void
jail_launch_confirmed(
    JailLaunchHooks* hooks,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user,
    const JailRules* rules)
{
    JailLaunchHookEntry* ptr = hooks->first;

    while (ptr) {
        JailLaunchHookEntry* next = ptr->next;

        jail_launch_hook_launch_confirmed(ptr->hook, app, cmd, user, rules);
        ptr = next;
    }
}

void
jail_launch_denied(
    JailLaunchHooks* hooks,
    const JailApp* app,
    const JailCmdLine* cmd,
    const JailRunUser* user)
{
    JailLaunchHookEntry* ptr = hooks->first;

    while (ptr) {
        JailLaunchHookEntry* next = ptr->next;

        jail_launch_hook_launch_denied(ptr->hook, app, cmd, user);
        ptr = next;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
