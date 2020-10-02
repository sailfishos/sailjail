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

#ifndef _JAIL_PLUGIN_H_
#define _JAIL_PLUGIN_H_

#include "jail_types.h"

#include <glib-object.h>

/* JailPlugin API exported by sailjail launcher */

G_BEGIN_DECLS

typedef enum jail_plugin_flags {
    JAIL_PLUGIN_FLAGS_NONE = 0x00,
    JAIL_PLUGIN_FLAG_MUST_START = 0x01,
    JAIL_PLUGIN_FLAG_DISABLED = 0x02   /* Disabled by default */
} JAIL_PLUGIN_FLAGS;

struct jail_plugin_desc {
    guint32 magic; /* JAIL_PLUGIN_MAGIC */
    JAIL_PLUGIN_FLAGS flags;
    const char* name;
    const char* description;
    const JailPluginModule* module;
    JailPlugin* (*create)(const JailPluginDesc* desc, GError** error);
};

#define JAIL_PLUGIN_MAGIC (0x4c504a53)

struct jail_plugin {
    GObject object;
    gconstpointer reserved;
    const JailPluginDesc* desc;
};

GType jail_plugin_get_type(void) SAILJAIL_EXPORT;
#define JAIL_TYPE_PLUGIN (jail_plugin_get_type())
#define JAIL_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        JAIL_TYPE_PLUGIN, JailPlugin))

G_END_DECLS

#endif /* _JAIL_PLUGIN_H_ */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
