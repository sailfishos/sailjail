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

#ifndef _JAIL_RULES_H_
#define _JAIL_RULES_H_

#include "jail_types.h"

G_BEGIN_DECLS

typedef enum jail_permit_type {
    JAIL_PERMIT_INVALID,
    JAIL_PERMIT_PRIVILEGED
} JAIL_PERMIT_TYPE;

typedef struct jail_permit {
    gboolean require;
    JAIL_PERMIT_TYPE type;
} JailPermit;

typedef struct jail_profile {
    gboolean require;
    const char* path;
} JailProfile;

typedef struct jail_path {
    gboolean require;
    gboolean allow;
    const char* path;
} JailPath;

typedef struct jail_dbus_name {
    gboolean require;
    const char* name;
} JailDBusName;

typedef struct jail_dbus {
    const JailDBusName* const* own;
    const JailDBusName* const* talk; /* Implies seeing the name */
} JailDBus;

typedef struct jail_dbus_restrict {
    const char* const* own;
    const char* const* talk;
} JailDBusRestrict;

struct jail_rules {
    const JailPermit* const* permits;
    const JailProfile* const* profiles;
    const JailPath* const* paths;
    const JailDBus* dbus_user;
    const JailDBus* dbus_system;
};

char*
jail_rules_permit_format(
    JAIL_PERMIT_TYPE type)
    SAILJAIL_EXPORT
    G_GNUC_WARN_UNUSED_RESULT
    G_GNUC_MALLOC;

JAIL_PERMIT_TYPE
jail_rules_permit_parse(
    const char* name)
    SAILJAIL_EXPORT;

JailRules*
jail_rules_ref(
    JailRules* rules)
    SAILJAIL_EXPORT;

void
jail_rules_unref(
    JailRules* rules)
    SAILJAIL_EXPORT;

/* Only leaves requires and the specified optional items */
JailRules*
jail_rules_restrict(
    JailRules* rules,
    const JAIL_PERMIT_TYPE* permits,
    const char* const* profiles,
    const char* const* paths,
    const JailDBusRestrict* dbus_user,
    const JailDBusRestrict* dbus_system)
    SAILJAIL_EXPORT;

G_END_DECLS

#endif /* _JAIL_RULES_H_ */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
