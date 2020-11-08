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

#include "jail_plugins.h"
#include "jail_plugin_p.h"
#include "jail_plugin_module.h"
#include "jail_version.h"

#include <gutil_log.h>
#include <gutil_strv.h>

#include <stdlib.h>
#include <dlfcn.h>

struct jail_plugins {
    GSList* plugins;
};

static
GStrV*
jail_plugins_scan_plugin_dir(
    const char* plugin_dir)
{
    GStrV* files = NULL;
    GDir* dir = g_dir_open(plugin_dir, 0, NULL);

    if (dir) {
	const char* file;

        while ((file = g_dir_read_name(dir)) != NULL) {
            if (!g_str_has_prefix(file, "lib") &&
                g_str_has_suffix(file, ".so")) {
                files = gutil_strv_add(files, file);
            }
        }
        g_dir_close(dir);

        /* Sort file names to guarantee precedence order in case of conflict */
        gutil_strv_sort(files, TRUE);
    }
    return files;
}

static
JailPlugin*
jail_plugins_find(
    JailPlugins* self,
    const char* name)
{
    GSList* l;

    for (l = self->plugins; l; l = l->next) {
        JailPlugin* plugin = l->data;
        const JailPluginDesc* desc = plugin->desc;

        if (!g_strcmp0(desc->name, name)) {
            return plugin;
        }
    }
    return NULL;
}

static
gboolean
jail_plugins_init_module(
    JailPlugins* self,
    const JailPluginModule* module,
    const char* path,
    const GStrV* enable,
    const GStrV* disable)
{
    int n = 0;
    const JailPluginDesc** p = module->plugins_start;

    while (p <  module->plugins_end) {
        const JailPluginDesc* desc = *p++;

        if (desc->magic != JAIL_PLUGIN_MAGIC || !desc->name || !desc->create) {
            GWARN("Invalid plugin \"%s\" in %s (ignored)", desc->name, path);
        } else if (jail_plugins_find(self, desc->name)) {
            GWARN("Duplicate plugin \"%s\" in %s (ignored)", desc->name, path);
        } else {
            gboolean load;

            if (gutil_strv_contains(disable, desc->name)) {
                if (!(desc->flags & JAIL_PLUGIN_FLAG_DISABLED)) {
                    GDEBUG("Plugin \"%s\" is disabled", desc->name);
                }
                load = FALSE;
            } else if (gutil_strv_contains(enable, desc->name)) {
                if (desc->flags & JAIL_PLUGIN_FLAG_DISABLED) {
                    GDEBUG("Plugin \"%s\" is enabled", desc->name);
                }
                load = TRUE;
            } else {
                load = !(desc->flags & JAIL_PLUGIN_FLAG_DISABLED);
            }

            if (load) {
                GError* error = NULL;
                JailPlugin* plugin = desc->create(desc, &error);

                if (plugin) {
                    GDEBUG("Loaded plugin \"%s\" from %s", desc->name, path);
                    plugin->desc = desc;
                    self->plugins = g_slist_append(self->plugins, plugin);
                    n++;
                } else {
                    GERR("Plugin \"%s\" failed to initialize: %s", desc->name,
                        GERRMSG(error));
                    g_error_free(error);
                }
            }
        }
    }
    return (n > 0);
}

static
gboolean
jail_plugins_load_module(
    JailPlugins* self,
    void* handle,
    const char* path,
    const GStrV* enable,
    const GStrV* disable)
{
    const char* sym = G_STRINGIFY(JAIL_PLUGIN_MODULE_SYMBOL);
    const JailPluginModule* mod = dlsym(handle, sym);

    if (mod) {
        /* Validate JailPluginModule */
        if (mod->magic != JAIL_PLUGIN_MODULE_MAGIC || !mod->plugins_start ||
            mod->plugins_start > mod->plugins_end) {
            GWARN("Invalid plugin module %s (ignored)", path);
        } else if (mod->jail_version > JAIL_VERSION) {
            GWARN("Plugin module %s requries sailjail %d.%d.%d (ignored)",
                path, JAIL_VERSION_GET_MAJOR(mod->jail_version),
                JAIL_VERSION_GET_MINOR(mod->jail_version),
                JAIL_VERSION_GET_RELEASE(mod->jail_version));
        } else {
            GDEBUG("Loading module %s", path);
            if (jail_plugins_init_module(self, mod, path, enable, disable)) {
                return TRUE;
            } else {
                GDEBUG("No plugins loaded from %s", path);
            }
        }
    } else {
        GERR("Symbol \"%s\" not found in %s", sym, path);
    }
    return FALSE;
}

JailPlugins*
jail_plugins_new(
    const char* dir,
    const GStrV* enable,
    const GStrV* disable)
{
    JailPlugins* self = g_new0(JailPlugins, 1);

    if (dir) {
        GStrV* files = jail_plugins_scan_plugin_dir(dir);

        GDEBUG("Loading modules from %s", dir);
        if (files) {
            char** ptr = files;
            int may_try_again = 1;
            GPtrArray* paths = g_ptr_array_new_with_free_func(g_free);

            /* Build full paths */
            while (*ptr) {
                g_ptr_array_add(paths, g_build_filename(dir, *ptr++, NULL));
            }

            /*
             * Keep trying to load plugins until at least one gets loaded
             * during the loop. Loaded plugins are removed from the list.
             * More than one loop may be necessary in case when plugin
             * modules link to each other.
             *
             * We don't need to keep handles to successfully loaded modules
             * because we never unload those.
             */
            while (paths->len > 0 && may_try_again) {
                int i;

                for (i = 0, may_try_again = 0; i < (int) paths->len; i++) {
                    const char* path = paths->pdata[i];
                    void* handle = dlopen(path, RTLD_NOW);

                    if (handle) {
                        if (!jail_plugins_load_module(self, handle, path,
                            enable, disable)) {
                            dlclose(handle);
                        }
                        g_ptr_array_remove_index(paths, i--);
                        may_try_again++;
                    } else {
                        /* It's not necessarily fatal yet... */
                        GDEBUG("Failed to load %s: %s", path, dlerror());
                    }
                }
            }

#if GUTIL_LOG_ERR
            /* One more loop, just to print errors to the log */
            if (paths->len) {
                guint i;

                for (i = 0; i < paths->len; i++) {
                    const char* path = paths->pdata[i];

                    /*
                     * They all failed to load at least once, we are
                     * definitely not expecting them to load now. We
                     * still need to call dlopen() for each of them
                     * to obtain the right dlerror().
                     */
                    GVERIFY_EQ(dlopen(path, RTLD_NOW), NULL);
                    GERR("Failed to load %s: %s", path, dlerror());
                }
            }
#endif /* GUTIL_LOG_ERR */

            g_ptr_array_free(paths, TRUE);
            g_strfreev(files);
        }
    }

    return self;
}

void
jail_plugins_free(
    JailPlugins* self)
{
    if (G_LIKELY(self)) {
        g_slist_free_full(self->plugins, g_object_unref);
        g_free(self);
    }
}

gboolean
jail_plugins_start(
    JailPlugins* self,
    SailJail* jail)
{
    if (G_LIKELY(self)) {
        gboolean ok = TRUE;
        GSList* l;

        for (l = self->plugins; ok && l; l = l->next) {
            JailPlugin* plugin = JAIL_PLUGIN(l->data);

            if (!jail_plugin_start(plugin, jail)) {
                const JailPluginDesc* desc = plugin->desc;

                if (desc->flags & JAIL_PLUGIN_FLAG_MUST_START) {
                    ok = FALSE;
                    GERR("Plugin \"%s\" failed to start, bailing out",
                        desc->name);
                } else {
                    GWARN("Plugin \"%s\" failed to start", desc->name);
                }
            }
        }
        return ok;
    }
    return FALSE;
}

void
jail_plugins_stop(
    JailPlugins* self)
{
    if (G_LIKELY(self)) {
        GSList* l;

        for (l = self->plugins; l; l = l->next) {
            jail_plugin_stop(JAIL_PLUGIN(l->data));
        }
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
