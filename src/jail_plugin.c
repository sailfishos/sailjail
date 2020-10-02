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
#include "jail_plugin_impl.h"
#include "jail_plugin_p.h"

#include <gutil_log.h>

typedef struct jail_plugin_private {
    gboolean started;
} JailPluginPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(JailPlugin, jail_plugin, \
    G_TYPE_OBJECT, G_ADD_PRIVATE(JailPlugin))
#define THIS(obj) JAIL_PLUGIN(obj)
#define PARENT_CLASS jail_plugin_parent_class
#define GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS((obj), \
    JAIL_TYPE_PLUGIN, JailPluginClass)

/*==========================================================================*
 * Internal interface
 *==========================================================================*/

JailPlugin*
jail_plugin_ref(
    JailPlugin* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(THIS(self));
    }
    return self;
}

void
jail_plugin_unref(
    JailPlugin* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(THIS(self));
    }
}

gboolean
jail_plugin_start(
    JailPlugin* self,
    SailJail* jail)
{
    if (G_LIKELY(self)) {
        JailPluginPrivate* priv = jail_plugin_get_instance_private(self);

        /* Don't start twice */
        if (priv->started) {
            /* Already started */
            return TRUE;
        } else {
            GError* error = NULL;

            if (GET_CLASS(self)->start(self, jail, &error)) {
                /* Started successfully */
                priv->started = TRUE;
                return TRUE;
            }
            GERR("Plugin \"%s\" failed to start: %s", self->desc->name,
                GERRMSG(error));
            g_error_free(error);
        }
    }
    return FALSE;
}

void
jail_plugin_stop(
    JailPlugin* self)
{
    if (G_LIKELY(self)) {
        JailPluginPrivate* priv = jail_plugin_get_instance_private(self);

        /* Only stop if started */
        if (priv->started) {
            GET_CLASS(self)->stop(self);
            priv->started = FALSE;
        }
    }
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
gboolean
jail_plugin_default_start(
    JailPlugin* self,
    SailJail* jail,
    GError** error)
{
    return FALSE;
}

static
void
jail_plugin_default_stop(
    JailPlugin* self)
{
}

static
void
jail_plugin_init(
    JailPlugin* self)
{
}

static
void
jail_plugin_dispose(
    GObject* object)
{
    JailPlugin* self = THIS(object);

    jail_plugin_stop(self);
    G_OBJECT_CLASS(PARENT_CLASS)->dispose(object);
}

static
void
jail_plugin_class_init(
    JailPluginClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = jail_plugin_dispose;
    klass->start = jail_plugin_default_start;
    klass->stop = jail_plugin_default_stop;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
