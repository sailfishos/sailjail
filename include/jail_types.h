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

#ifndef _JAIL_TYPES_H_
#define _JAIL_TYPES_H_

#include <gutil_types.h>

#include <sys/types.h>

G_BEGIN_DECLS

typedef struct jail_fish SailJail; /* opaque */
typedef struct jail_launch_hook JailLaunchHook;
typedef struct jail_plugin JailPlugin;
typedef struct jail_plugin_desc JailPluginDesc;
typedef struct jail_plugin_module JailPluginModule;
typedef struct jail_rules JailRules;

typedef struct jail_app {
    const char* file;
    const char* section;
} JailApp;

typedef struct jail_cmd_line {
    int argc;
    const char** argv;
} JailCmdLine;

typedef struct jail_run_user {
    uid_t euid;
    gid_t egid;
    const gid_t* groups;
    guint ngroups;
} JailRunUser;

typedef enum jail_launch_prompt {
    JAIL_LAUNCH_PROMPT_NEVER,
    JAIL_LAUNCH_PROMPT_IF_NEEDED,
    JAIL_LAUNCH_PROMPT_ALWAYS
} JAIL_LAUNCH_PROMPT;

#ifndef SAILJAIL_EXPORT
#  define SAILJAIL_EXPORT __attribute__((weak))
#endif

G_END_DECLS

#endif /* _JAIL_TYPES_H_ */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
