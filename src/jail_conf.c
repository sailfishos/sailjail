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

#include "jail_conf.h"

#include <gutil_log.h>
#include <gutil_macros.h>

#define DEFAULT_EXEC "/usr/bin/firejail"
#define DEFAULT_DESKTOP_DIR "/usr/share/applications"
#define DEFAULT_PROFILE_DIR "/etc/sailjail"
#define DEFAULT_PERM_SUBDIR "permissions"
#define DEFAULT_PERM_DIR DEFAULT_PROFILE_DIR "/" DEFAULT_PERM_SUBDIR
#define DEFAULT_PASSTHROUGH FALSE

#define SECTION "Settings"
#define KEY_EXEC "Exec"
#define KEY_PLUGIN_DIR "PluginDir"
#define KEY_DESKTOP_DIR "DesktopDir"
#define KEY_PROFILE_DIR "ProfileDir"
#define KEY_PERM_DIR "PermissionsDir"
#define KEY_PASSTHROUGH "Passthrough"

typedef struct jail_conf_priv {
    JailConf pub;
    char* exec;
    char* plugin_dir;
    char* desktop_dir;
    char* profile_dir;
    char* perm_dir;
} JailConfPriv;

static inline
JailConfPriv*
jail_conf_cast(
    JailConf* conf)
{
    return G_CAST(conf, JailConfPriv, pub);
}

static
void
jail_conf_parse(
    GKeyFile* f,
    JailConfPriv* priv)
{
    JailConf* conf = &priv->pub;
    GError* error = NULL;
    gboolean b;
    char* str;
    char* perm_dir;

    str = g_key_file_get_string(f, SECTION, KEY_EXEC, NULL);
    if (str) {
        if (str[0] != '/') {
            /* Protecting user from bad decisions */
            GWARN("[sailjail] " KEY_EXEC "value '%s' must be an absolute path, ignoring!", str);
        } else {
            g_free(priv->exec);
            conf->exec = priv->exec = str;
            GDEBUG("  %s=%s", KEY_EXEC, conf->exec);
        }
    }

    str = g_key_file_get_string(f, SECTION, KEY_PLUGIN_DIR, NULL);
    if (str) {
        g_free(priv->plugin_dir);
        conf->plugin_dir = priv->plugin_dir = str;
        GDEBUG("  %s=%s", KEY_PLUGIN_DIR, conf->plugin_dir);
    }

    str = g_key_file_get_string(f, SECTION, KEY_DESKTOP_DIR, NULL);
    if (str) {
        g_free(priv->desktop_dir);
        conf->desktop_dir = priv->desktop_dir = str;
        GDEBUG("  %s=%s", KEY_DESKTOP_DIR, conf->desktop_dir);
    }

    str = g_key_file_get_string(f, SECTION, KEY_PROFILE_DIR, NULL);
    perm_dir = g_key_file_get_string(f, SECTION, KEY_PERM_DIR, NULL);
    if (str) {
        g_free(priv->profile_dir);
        conf->profile_dir = priv->profile_dir = str;
        GDEBUG("  %s=%s", KEY_PROFILE_DIR, conf->profile_dir);

        /* ProfileDir sets perm_dir too unless PermissionDir is given */
        if (!perm_dir) {
            g_free(priv->perm_dir);
            conf->perm_dir = priv->perm_dir = g_build_filename(str,
                DEFAULT_PERM_SUBDIR, NULL);
            GDEBUG("  %s=%s", KEY_PERM_DIR, conf->perm_dir);
        }
    }

    if (perm_dir) {
        g_free(priv->perm_dir);
        conf->perm_dir = priv->perm_dir = perm_dir;
        GDEBUG("  %s=%s", KEY_PERM_DIR, conf->perm_dir);
    }

    b = g_key_file_get_boolean(f, SECTION, KEY_PASSTHROUGH, &error);
    if (error) {
        g_error_free(error);
    } else {
        conf->passthrough = b;
        GDEBUG("  %s=%s", KEY_PASSTHROUGH, b ? "true" : "false");
    }
}

static
void
jail_conf_finalize(
    JailConfPriv* priv)
{
    g_free(priv->exec);
    g_free(priv->plugin_dir);
    g_free(priv->desktop_dir);
    g_free(priv->profile_dir);
    g_free(priv->perm_dir);
}

JailConf*
jail_conf_new(
    void)
{
    JailConfPriv* priv = g_new0(JailConfPriv, 1);
    JailConf* conf = &priv->pub;

    conf->exec = DEFAULT_EXEC;
    conf->plugin_dir = DEFAULT_PLUGIN_DIR;
    conf->desktop_dir = DEFAULT_DESKTOP_DIR;
    conf->profile_dir = DEFAULT_PROFILE_DIR;
    conf->perm_dir = DEFAULT_PERM_DIR;
    conf->passthrough = DEFAULT_PASSTHROUGH;
    return conf;
}

void
jail_conf_free(
    JailConf* conf)
{
    JailConfPriv* priv = jail_conf_cast(conf);

    jail_conf_finalize(priv);
    g_free(priv);
}

gboolean
jail_conf_load(
    JailConf* conf,
    const char* path,
    GError** error)
{
    JailConfPriv* priv = jail_conf_cast(conf);
    GKeyFile* f = g_key_file_new();
    gboolean ok = g_key_file_load_from_file(f, path, 0, error);

    if (ok) {
        GDEBUG("Loading %s", path);
        jail_conf_parse(f, priv);
    }
    g_key_file_free(f);
    return ok;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
