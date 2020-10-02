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

#ifndef _JAIL_PLUGIN_MODULE_IMPL_H_
#define _JAIL_PLUGIN_MODULE_IMPL_H_

#include "jail_plugin_module.h"
#include "jail_version.h"

/* API for defining Sailjail plugin modules */

G_BEGIN_DECLS

#define JAIL_PLUGIN_MODULE_ATTR __attribute__ ((visibility("default")))
#define JAIL_MODULE_PLUGINS_START_SYMBOL __start__jail_plugins
#define JAIL_MODULE_PLUGINS_STOP_SYMBOL __stop__jail_plugins

extern const JailPluginDesc* JAIL_MODULE_PLUGINS_START_SYMBOL[];
extern const JailPluginDesc* JAIL_MODULE_PLUGINS_STOP_SYMBOL[];

/*
 * JAIL_PLUGIN_MODULE_DEFINE - simple way to define JailPluginModule with
 * a single or no log module, and no flags.
 *
 * JailPluginModule is the main data structure of a plugin module, and
 * it's looked up by name. It contains pointers to the start and the end
 * of _jail_plugins section, which in turn contains pointers to actual
 * JailPluginDesc descriptors, which represent individual plugins.

 * In other words, it goes like that:
 *
 *        jail_plugin_module
 *        (exported by name)
 *        +------------------+
 *        | JailPluginModule |
 *        | ...
 *        | plugins_start    |-->-----+
 *        | plugins_end      |-->--+  |
 *        +------------------+     |  |
 *                                 |  |
 * +-------------------------------+  |
 * | +--------------------------------+
 * | |
 * | |    _jail_plugins section
 * | +--> +-----------------+
 * |      | JailPluginDesc* |-->-----+
 * |      + ...             +        |
 * |      | JailPluginDesc* |-->--+  |
 * +----> +-----------------+     |  |
 *                                |  |
 * +------------------------------+  |
 * | +-------------------------------+
 * | |
 * | |    Actual plugin descriptors:
 * | |
 * | +--> +-----------------+
 * |      | JailPluginDesc  |
 * |      | ...             |
 * |      +-----------------+
 * |
 * +----> +-----------------+
 *        | JailPluginDesc  |
 *        | ...             |
 *        +-----------------+
 *
 * That allows a) compiling multiple plugins into a single loadable module
 * and b) extending the structures without breaking plugin ABI.
 */

extern const JailPluginModule JAIL_PLUGIN_MODULE_SYMBOL \
    JAIL_PLUGIN_MODULE_ATTR;

#ifdef GLOG_MODULE_NAME
#  define JAIL_PLUGIN_MODULE_LOGS _jail_plugin_module_logs
#  define JAIL_PLUGIN_MODULE_DEFINE(description) \
    static GLogModule* const JAIL_PLUGIN_MODULE_LOGS[] = \
        { &GLOG_MODULE_NAME, NULL }; \
    const JailPluginModule JAIL_PLUGIN_MODULE_SYMBOL \
    JAIL_PLUGIN_MODULE_ATTR = \
        { JAIL_PLUGIN_MODULE_MAGIC, JAIL_VERSION, description, \
          JAIL_PLUGIN_MODULE_LOGS, \
          JAIL_MODULE_PLUGINS_START_SYMBOL, JAIL_MODULE_PLUGINS_STOP_SYMBOL, \
          JAIL_PLUGIN_MODULE_FLAGS_NONE };

#else
#  define JAIL_PLUGIN_MODULE_DEFINE(description) const JailPluginModule \
    JAIL_PLUGIN_MODULE_SYMBOL JAIL_PLUGIN_MODULE_ATTR = \
        { JAIL_PLUGIN_MODULE_MAGIC, JAIL_VERSION, description, NULL, \
          JAIL_MODULE_PLUGINS_START_SYMBOL, JAIL_MODULE_PLUGINS_STOP_SYMBOL, \
          JAIL_PLUGIN_MODULE_FLAGS_NONE};
#endif

G_END_DECLS

#endif /* _JAIL_PLUGIN_MODULE_IMPL_H_ */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
