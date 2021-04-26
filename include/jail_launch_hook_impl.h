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

#ifndef _JAIL_LAUNCH_HOOK_IMPL_H_
#define _JAIL_LAUNCH_HOOK_IMPL_H_

#include "jail_launch_hook.h"

/* Internal API for use by JailLaunchHook implemenations */

G_BEGIN_DECLS

typedef struct jail_launch_hook_class {
    GObjectClass parent;

    JailRules* (*confirm_launch)(JailLaunchHook* hook,
        const JailApp* app, const JailCmdLine* cmd,
        const JailRunUser* user, JailRules* rules,
        JAIL_LAUNCH_PROMPT prompt);
    void (*launch_confirmed)(JailLaunchHook* hook,
        const JailApp* app, const JailCmdLine* cmd,
        const JailRunUser* user, const JailRules* rules);
    void (*launch_denied)(JailLaunchHook* hook,
        const JailApp* app, const JailCmdLine* cmd,
        const JailRunUser* user);

    /* Padding for future expansion */
    void (*_reserved1)(void);
    void (*_reserved2)(void);
    void (*_reserved3)(void);
    void (*_reserved4)(void);
    void (*_reserved5)(void);
} JailLaunchHookClass;

#define JAIL_LAUNCH_HOOK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, \
        JAIL_TYPE_LAUNCH_HOOK, JailLaunchHookClass))

G_END_DECLS

#endif /* _JAIL_LAUNCH_HOOK_IMPL_H_ */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
