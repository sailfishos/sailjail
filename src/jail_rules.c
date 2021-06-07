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

#include "jail_rules_p.h"
#include "jail_conf.h"

#include <gutil_log.h>
#include <gutil_strv.h>
#include <gutil_macros.h>

#include <gio/gio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char SAILJAIL_PERMISSION_SUFFIX[] = ".permission";
static const char SAILJAIL_PROFILE_SUFFIX[] = ".profile";
static const char SAILJAIL_DESKTOP_SUFFIX[] = ".desktop";
#define SAILJAIL_DESKTOP_SUFFIX_LEN (sizeof(SAILJAIL_DESKTOP_SUFFIX) - 1)

static const char SAILJAIL_BASE_PERM[] = "Base.permission";

static const char DESKTOP_GROUP_DESKTOP_ENTRY[] = "Desktop Entry";
static const char DESKTOP_KEY_DESKTOP_ENTRY_TYPE[] = "Type";
static const char DESKTOP_ENTRY_TYPE_APPLICATION[] = "Application";

/* desktop-file-install wants X- prefix */
static const char SAILJAIL_SECTION_DESKTOP_DEFAULT[] =
        ALTERNATE_DEFAULT_PROFILE_SECTION;
static const char SAILJAIL_SECTION_DEFAULT[] = DEFAULT_PROFILE_SECTION;
static const char SAILJAIL_LIST_SEPARATORS[] = ":;,";

static const char SAILJAIL_KEY_PERMS[] = PERMISSION_LIST_KEY;
#define SAILJAIL_KEY_PERM_REQUIRED '!'
#define SAILJAIL_KEY_PERM_OPTIONAL '?'

static const char SAILJAIL_KEY_FILE_ACCESS[] = "FileAccess";
#define SAILJAIL_KEY_FILE_ALLOW_REQUIRED '+'
#define SAILJAIL_KEY_FILE_ALLOW_OPTIONAL '?'
#define SAILJAIL_KEY_FILE_DISALLOW_REQUIRED '!'
#define SAILJAIL_KEY_FILE_DISALLOW_OPTIONAL '-'

static const char SAILJAIL_KEY_DBUS_USER_OWN[] = "DBusUserOwn";
static const char SAILJAIL_KEY_DBUS_USER_TALK[] = "DBusUserTalk";
static const char SAILJAIL_KEY_DBUS_SYSTEM_OWN[] = "DBusSystemOwn";
static const char SAILJAIL_KEY_DBUS_SYSTEM_TALK[] = "DBusSystemTalk";

/* Special permissions */
static const char SAILJAIL_PERM_PRIVILEGED[] = "Privileged";

/* Context for parsing jail rules */
typedef struct jail_rules_data {
    char *org_name;
    char *app_name;
    GPtrArray* permits;
    GPtrArray* profiles;
    GPtrArray* paths;
    GPtrArray* dbus_user_own;
    GPtrArray* dbus_user_talk;
    GPtrArray* dbus_system_own;
    GPtrArray* dbus_system_talk;
} JailRulesData;

typedef struct jail_rules_priv {
    char *org_name;
    char *app_name;
    JailRules rules;
    JailDBus dbus_user;
    JailDBus dbus_system;
    gint ref_count;
} JailRulesPriv;

static const char* const jail_rules_none[] = { NULL };

static inline
JailRulesPriv*
jail_rules_cast(
    const JailRules* rules)
{
    return G_CAST(rules, JailRulesPriv, rules);
}

static
const char*
jail_rules_basename(
    const char* fname)
{
    /*
     * g_basename got deprecated but it does exactly what we need here,
     * let's use it anyway.
     */
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    return g_basename(fname);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static
const char*
jail_rules_permit_name(
    JAIL_PERMIT_TYPE type)
{
    switch (type) {
    case JAIL_PERMIT_PRIVILEGED:
        return SAILJAIL_PERM_PRIVILEGED;
    case JAIL_PERMIT_INVALID:
        break;
    }
    return NULL;
}

char*
jail_rules_permit_format(
    JAIL_PERMIT_TYPE type)
{
    return g_strdup(jail_rules_permit_name(type));
}

JAIL_PERMIT_TYPE
jail_rules_permit_parse(
    const char* string)
{
    return g_strcmp0(string, SAILJAIL_PERM_PRIVILEGED) ?
        JAIL_PERMIT_INVALID : JAIL_PERMIT_PRIVILEGED;
}

static
int
jail_rules_permit_list_index(
    GPtrArray* list,
    JAIL_PERMIT_TYPE type)
{
    int i;

    for (i = 0; i < list->len; i++) {
        const JailPermit* permit = g_ptr_array_index(list, i);

        if (permit->type == type) {
            return i;
        }
    }
    return -1;
}

static
gboolean
jail_rules_permit_list_contains(
    const JAIL_PERMIT_TYPE* permits,
    JAIL_PERMIT_TYPE permit)
{
    if (permits) {
        int i;

        for (i = 0; permits[i] != JAIL_PERMIT_INVALID; i++) {
            if (permits[i] == permit) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static
JailPermit*
jail_rules_permit_new(
    JAIL_PERMIT_TYPE type,
    gboolean require)
{
    JailPermit* permit = g_new(JailPermit, 1);

    permit->type = type;
    permit->require = require;
    return permit;
}

static
void
jail_rules_add_permit(
    JailRulesData* data,
    JAIL_PERMIT_TYPE type,
    gboolean require)
{
    GPtrArray* list = data->permits;
    const int prev_index = jail_rules_permit_list_index(list, type);

    GDEBUG("Adding permission %s%s", jail_rules_permit_name(type),
        require ? "" : " (optional)");
    if (prev_index >= 0) {
        JailPermit* prev = g_ptr_array_index(list, prev_index);

        /* If the previous one was required, this one must be too */
        if (prev->require) require = TRUE;

        /* Remove the previous occurrence and complain about it */
        GWARN("Permission %s is specified more than once",
            jail_rules_permit_name(type));
        g_ptr_array_remove_index(list, prev_index);
    }
    g_ptr_array_add(list, jail_rules_permit_new(type, require));
}

static
int
jail_rules_find_profile(
    GPtrArray* list,
    const char* path)
{
    int i;

    for (i = 0; i < list->len; i++) {
        const JailProfile* profile = g_ptr_array_index(list, i);

        if (!strcmp(profile->path, path)) {
            return i;
        }
    }
    return -1;
}

static
JailProfile*
jail_rules_profile_new(
    const char* path,
    gboolean require)
{
    /*
     * Allocate the whole thing from a single memory block, so that a NULL
     * terminated array of these things can be deallocated with g_strfreev.
     */
    gsize len = strlen(path);
    JailProfile* jail_pro = g_malloc(G_ALIGN8(sizeof(JailPath)) + len + 1);
    char* ptr = (char*)jail_pro + G_ALIGN8(sizeof(JailPath));

    memcpy(ptr, path, len + 1);
    jail_pro->path = ptr;
    jail_pro->require = require;
    return jail_pro;
}

static
void
jail_rules_add_profile(
    JailRulesData* data,
    const JailConf* conf,
    const char* name,
    gboolean require)
{
    /*
     * The name here is guaranteed to have either .permission or .profile
     * suffix.
     */
    const char* base = jail_rules_basename(name);
    char* path = g_build_filename(conf->perm_dir, base, NULL);

    if (strchr(name, '/') && strcmp(name, path)) {
        GWARN("%s: ignored due to not matching %s", name, conf->perm_dir);
    } else if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        GWARN("%s: profile does not exist", name);
    } else {
        GPtrArray* list = data->profiles;
        const int prev_index = jail_rules_find_profile(list, path);

        GDEBUG("Adding profile %s%s", path, require ? "" : " (optional)");
        if (prev_index >= 0) {
            JailProfile* prev = g_ptr_array_index(list, prev_index);

            /* If the previous one was required, this one must be too */
            if (prev->require) require = TRUE;

            /* Remove the previous occurrence and complain about it */
            GWARN("Profile %s is specified more than once", path);
            g_ptr_array_remove_index(list, prev_index);
        }
        g_ptr_array_add(list, jail_rules_profile_new(path, require));
    }
    g_free(path);
}

static
int
jail_rules_find_path(
    GPtrArray* list,
    const char* path)
{
    int i;

    for (i = 0; i < list->len; i++) {
        const JailPath* jp = g_ptr_array_index(list, i);

        if (!strcmp(jp->path, path)) {
            return i;
        }
    }
    return -1;
}

static
JailPath*
jail_rules_path_new(
    const char* path,
    gboolean allow,
    gboolean require)
{
    /*
     * Allocate the whole thing from a single memory block, so that a NULL
     * terminated array of these things can be deallocated with g_strfreev.
     */
    gsize len = strlen(path);
    JailPath* jail_path = g_malloc(G_ALIGN8(sizeof(*jail_path) + len + 1));
    char* dest = (char*)jail_path + G_ALIGN8(sizeof(*jail_path));

    memcpy(dest, path, len + 1);
    jail_path->path = dest;
    jail_path->allow = allow;
    jail_path->require = require;
    return jail_path;
}

static
void
jail_rules_add_path(
    JailRulesData* data,
    const char* path,
    gboolean allow,
    gboolean require)
{
    GPtrArray* list = data->paths;
    const int prev_index = jail_rules_find_path(list, path);

    GDEBUG("%s path %s%s", allow ? "Allowing" : "Disallowing", path,
        require ? "" : " (optional)");
    if (prev_index >= 0) {
        JailPath* prev = g_ptr_array_index(list, prev_index);

        /* If the previous one was required, this one must be too */
        if (prev->require) require = TRUE;

        /* Remove the previous occurrence and complain about it */
        GWARN("Path %s is specified more than once", path);
        g_ptr_array_remove_index(list, prev_index);
    }
    g_ptr_array_add(list, jail_rules_path_new(path, allow, require));
}

static
JailDBusName*
jail_rules_dbus_name_new(
    const char* name,
    gboolean require)
{
    /*
     * Allocate the whole thing from a single memory block, so that a NULL
     * terminated array of these things can be deallocated with g_strfreev.
     */
    gsize len = strlen(name);
    JailDBusName* dbus_name = g_malloc(G_ALIGN8(sizeof(*dbus_name) + len + 1));
    char* dest = (char*)dbus_name + G_ALIGN8(sizeof(*dbus_name));

    memcpy(dest, name, len + 1);
    dbus_name->name = dest;
    dbus_name->require = require;
    return dbus_name;
}

static
void
jail_rules_parse_dbus_names(
    GKeyFile* keyfile,
    const char* section,
    const char* key,
    GPtrArray* list)
{
    char* val = g_key_file_get_string(keyfile, section, key, NULL);

    if (val) {
        char** vals = g_strsplit_set(val, ",", -1);
        char** ptr = vals;

        for (ptr = vals; *ptr; ptr++) {
            char* val = g_strstrip(*ptr);

            if (g_dbus_is_name(val)) {
                /* Consider all of them as required for now */
                g_ptr_array_add(list, jail_rules_dbus_name_new(val, TRUE));
            }
        }

        g_strfreev(vals);
        g_free(val);
    }
}

static
void
jail_rules_restrict_dbus_names(
    const JailDBusName* const* in,
    GPtrArray* out,
    const char* const* names)
{
    if (in) {
        int i;

        for (i = 0; in[i]; i++) {
            const JailDBusName* dbus = in[i];

            if (dbus->require ||
                gutil_strv_contains((GStrV*)names, dbus->name)) {
                g_ptr_array_add(out, jail_rules_dbus_name_new
                    (dbus->name, dbus->require));
            }
        }
    }
}

static
JailRulesData*
jail_rules_data_alloc(
    void)
{
    JailRulesData* data = g_new0(JailRulesData, 1);

    data->permits = g_ptr_array_new_with_free_func(g_free);
    data->profiles = g_ptr_array_new_with_free_func(g_free);
    data->paths = g_ptr_array_new_with_free_func(g_free);
    data->dbus_user_own = g_ptr_array_new_with_free_func(g_free);
    data->dbus_user_talk = g_ptr_array_new_with_free_func(g_free);
    data->dbus_system_own = g_ptr_array_new_with_free_func(g_free);
    data->dbus_system_talk = g_ptr_array_new_with_free_func(g_free);
    return data;
}

static
JailRulesData*
jail_rules_data_new(
    const JailConf* conf)
{
    JailRulesData* data = jail_rules_data_alloc();
    return data;
}

static
gboolean
jail_rules_is_valid_str(
    const char *str,
    JAIL_VALIDATE_STR_TYPE type,
    gboolean def_value)
{
    int i;

    if (!str)
        return def_value;

    /* Must start with a letter and end with alphanumeric, */
    if (!g_ascii_isalpha(*str) || !g_ascii_isalnum(str[strlen(str) - 1]))
        return FALSE;

    for (i = 1 ; str[i] ; i++) {
        if (g_ascii_isalnum(str[i])) {
            /* Any element separated with dot cannot begin with digit */
            if (g_ascii_isdigit(str[i]) && str[i-1] == '.')
                return FALSE;

            continue;
        }

        switch (type) {
        /* Dots and hyphens are allowed in org name.*/
        case JAIL_VALIDATE_STR_ORG:
            if (str[i] == '.') {
                /* usage of two dots is discouraged */
                if (str[i-1] == '.')
                    return FALSE;

                continue;
            }

            if (str[i] == '-')
                continue;

            /* fall through */
        case JAIL_VALIDATE_STR_COMMON:
            if (str[i] == '_')
                continue;

            break;
        }

        return FALSE;
    }

    return TRUE;
}

static
JailRulesData*
jail_rules_parse_section(
    GKeyFile* kf,
    const char* section,
    const char* app,
    const JailConf* conf,
    GError** error)
{
    JailRulesData* data = jail_rules_data_new(conf);
    char* val;

    /* Permissions= */
    val = g_key_file_get_string(kf, section, SAILJAIL_KEY_PERMS, NULL);
    if (val) {
        JAIL_PERMIT_TYPE permit;
        char** vals = g_strsplit_set(val, SAILJAIL_LIST_SEPARATORS, -1);
        char** ptr = vals;

        for (ptr = vals; *ptr; ptr++) {
            const char* name = g_strstrip(*ptr);
            gboolean require = TRUE;

            switch (*name) {
            case SAILJAIL_KEY_PERM_OPTIONAL:
                require = FALSE;
                /* fallthrough */
            case SAILJAIL_KEY_PERM_REQUIRED:
                name++;
                break;
            }

            /* Skip the spaces between the modifier and the name */
            while (*name && g_ascii_isspace(*name)) name++;

            /*
             * Note: "Privileged" can trigger both adding a permit and
             * including a permission file if one exists.
             */
            permit = jail_rules_permit_parse(name);
            if (permit != JAIL_PERMIT_INVALID) {
                jail_rules_add_permit(data, permit, require);
            }
            if (*name) {
                char* file = g_strconcat(name, SAILJAIL_PERMISSION_SUFFIX,
                    NULL);
                char* file_path = g_build_filename(conf->perm_dir, file, NULL);

                if (permit == JAIL_PERMIT_INVALID ||
                    g_file_test(file_path, G_FILE_TEST_EXISTS)) {
                    jail_rules_add_profile(data, conf, file, require);
                }
                g_free(file);
                g_free(file_path);
            }
        }
        g_strfreev(vals);
        g_free(val);
    }

    /* FileAccess= */
    val = g_key_file_get_string(kf, section, SAILJAIL_KEY_FILE_ACCESS, NULL);
    if (val) {
        char** vals = g_strsplit_set(val, SAILJAIL_LIST_SEPARATORS, -1);
        char** ptr = vals;

        for (ptr = vals; *ptr; ptr++) {
            const char* path = g_strstrip(*ptr);
            gboolean allow = TRUE;
            gboolean require = TRUE;

            switch (path[0]) {
            case SAILJAIL_KEY_FILE_ALLOW_OPTIONAL:
                require = FALSE;
                /* fallthrough */
            case SAILJAIL_KEY_FILE_ALLOW_REQUIRED:
                path++;
                break;
            case SAILJAIL_KEY_FILE_DISALLOW_OPTIONAL:
                require = FALSE;
                /* fallthrough */
            case SAILJAIL_KEY_FILE_DISALLOW_REQUIRED:
                allow = FALSE;
                path++;
                break;
            }

            if (path[0]) {
                jail_rules_add_path(data, path, allow, require);
            }
        }
        g_strfreev(vals);
        g_free(val);
    }

    /* Application directories */
    if (app) {
        const char* home = getenv("HOME");
        char* usr = g_build_filename("/usr/share", app, NULL);
        char* app_desktop = g_strconcat(app, SAILJAIL_DESKTOP_SUFFIX, NULL);
        char* desktop = g_build_filename(conf->desktop_dir, app_desktop, NULL);
        char* app_profile = g_strconcat(app, SAILJAIL_PROFILE_SUFFIX, NULL);
        char* profile = g_build_filename(conf->perm_dir, app_profile, NULL);

        /*
         * If APPNAME.profile exists, it's implicitly pulled in
         * unless it's already there.
         */
        if (g_file_test(profile, G_FILE_TEST_EXISTS) &&
            jail_rules_find_profile(data->profiles, profile) < 0) {
            jail_rules_add_profile(data, conf, profile, TRUE);
        }

        jail_rules_add_path(data, usr, TRUE, FALSE);
        jail_rules_add_path(data, desktop, TRUE, FALSE);
        g_free(usr);
        g_free(desktop);
        g_free(app_desktop);
        g_free(profile);
        g_free(app_profile);

        if (home) {
            char* local = g_build_filename(home, ".local/share", app, NULL);

            jail_rules_add_path(data, local, TRUE, FALSE);
            g_free(local);
        }
    }

    /* D-Bus */
    jail_rules_parse_dbus_names(kf, section,
        SAILJAIL_KEY_DBUS_USER_OWN, data->dbus_user_own);
    jail_rules_parse_dbus_names(kf, section,
        SAILJAIL_KEY_DBUS_USER_TALK, data->dbus_user_talk);
    jail_rules_parse_dbus_names(kf, section,
        SAILJAIL_KEY_DBUS_SYSTEM_OWN, data->dbus_system_own);
    jail_rules_parse_dbus_names(kf, section,
        SAILJAIL_KEY_DBUS_SYSTEM_TALK, data->dbus_system_talk);


    /* D-Bus / whitelist rules derived from Orgnanization/ApplicationName */
    {
        data->org_name = g_key_file_get_string(kf, section, SAILJAIL_KEY_ORGANIZATION_NAME, NULL);
        if (!jail_rules_is_valid_str(data->org_name, JAIL_VALIDATE_STR_ORG, TRUE)) {
            g_propagate_error(error, g_error_new(G_KEY_FILE_ERROR,
                    G_KEY_FILE_ERROR_INVALID_VALUE, "Invalid value %s for %s",
                    data->org_name, SAILJAIL_KEY_ORGANIZATION_NAME));
            goto err;
        }

        data->app_name = g_key_file_get_string(kf, section, SAILJAIL_KEY_APPLICATION_NAME, NULL);
        if (!jail_rules_is_valid_str(data->app_name, JAIL_VALIDATE_STR_COMMON, TRUE)) {
            g_propagate_error(error, g_error_new(G_KEY_FILE_ERROR,
                    G_KEY_FILE_ERROR_INVALID_VALUE, "Invalid value %s for %s",
                    data->app_name, SAILJAIL_KEY_APPLICATION_NAME));
            goto err;
        }

        /* No proper way to exit here so ignore adding of invalid org / app name content. */
        if (data->org_name && data->app_name) {
            char *tmp;
            if ((tmp = g_strdup_printf("%s.%s", data->org_name,
                                                            data->app_name))) {
                if (g_dbus_is_name(tmp)) {
                    GDEBUG("Allowing dbus-user.own %s", tmp);
                    g_ptr_array_add(data->dbus_user_own, jail_rules_dbus_name_new(tmp, TRUE));
                }
                g_free(tmp);
            }

            /* If it is left up to the application to create these
             * directories, they might end up being in tmpfs mounts
             * that disappears in thin air when application exits.
             * Make an attempt to ensure that the directories get
             * created during sanbox setup, so that whitelisting
             * is applied to already existing real directories.
             *
             * FIXME: This should utilize explicit separate list for
             *        "mkdir directory" options, but for now jail_run()
             *        just assumes that all required whitelistings
             *        under $HOME are directories and adds a mkdir too.
             */
            if ((tmp = g_strdup_printf("${HOME}/.cache/%s/%s", data->org_name,
                                                            data->app_name))) {
                jail_rules_add_path(data, tmp, TRUE, SAILJAIL_KEY_FILE_ALLOW_REQUIRED);
                g_free(tmp);
            }
            if ((tmp = g_strdup_printf("${HOME}/.local/share/%s/%s",
                                            data->org_name, data->app_name))) {
                jail_rules_add_path(data, tmp, TRUE, SAILJAIL_KEY_FILE_ALLOW_REQUIRED);
                g_free(tmp);
            }
            if ((tmp = g_strdup_printf("${HOME}/.config/%s/%s", data->org_name,
                                                            data->app_name))) {
                jail_rules_add_path(data, tmp, TRUE, SAILJAIL_KEY_FILE_ALLOW_REQUIRED);
                g_free(tmp);
            }
        }
    }

    return data;

err:
    g_free(data->org_name);
    g_free(data->app_name);
    g_free(data);
    return NULL;
}

static
JailRulesData*
jail_rules_parse_file(
    GKeyFile* keyfile,
    const char* fname,
    const char* program,
    const JailConf* conf,
    const JailRulesOpt* opt,
    char** out_section,
    GError** error)
{
    JailRulesData* data = NULL;
    const char* section = opt->section;
    const char* app = opt->sailfish_app;
    char** groups = g_key_file_get_groups(keyfile, NULL);

    if (section) {
        /* If section is defined, it must be there */
        if (!gutil_strv_contains(groups, section)) {
            section = NULL;
        }
    } else {
        const char* basename = jail_rules_basename(program);

        /*
         * If no section is defined, try the one matching the program name
         * and then [X-Sailjail] for .desktop files and [Sailjail] for
         * everything else.
         */
        if (gutil_strv_contains(groups, basename)) {
            section = basename;
        } else if (app && gutil_strv_contains(groups, app)) {
            section = app;
        } else {
            const char* default_section =
                g_str_has_suffix(fname, SAILJAIL_DESKTOP_SUFFIX) ?
                    SAILJAIL_SECTION_DESKTOP_DEFAULT :
                    SAILJAIL_SECTION_DEFAULT;

            if (gutil_strv_contains(groups, default_section)) {
                section = default_section;
            }
        }
    }

    if (section) {
        char* auto_app = NULL;

        if (!app && g_str_has_suffix(fname, SAILJAIL_DESKTOP_SUFFIX)) {
            /*
             * If this is a .desktop file with Type=Application in
             * [Desktop Entry] section then assume that the basename
             * without .desktop suffix is the application name.
             */
            char* type = g_key_file_get_string(keyfile,
                DESKTOP_GROUP_DESKTOP_ENTRY,
                DESKTOP_KEY_DESKTOP_ENTRY_TYPE, NULL);

            if (!g_strcmp0(type, DESKTOP_ENTRY_TYPE_APPLICATION)) {
                const char* base = jail_rules_basename(fname);
                const gsize len = strlen(base) - SAILJAIL_DESKTOP_SUFFIX_LEN;

                auto_app = g_malloc(len + 1);
                memcpy(auto_app, base, len);
                auto_app[len] = 0;

                GDEBUG("Assuming app name %s", auto_app);
                app = auto_app;
            }
            g_free(type);
        }

        GDEBUG("Parsing [%s] section from %s", section, fname);
        data = jail_rules_parse_section(keyfile, section, app, conf, error);

        if (out_section) {
            *out_section = g_strdup(section);
        }
        g_free(auto_app);

        if (!data)
            goto out;
    }

    if (!data && error) {
        g_propagate_error(error, g_error_new(G_KEY_FILE_ERROR,
            G_KEY_FILE_ERROR_GROUP_NOT_FOUND, "No section for %s in %s",
            program, fname));
    }

out:
    g_strfreev(groups);
    return data;
}

static
char*
jail_rules_profile_path(
    const char* program,
    const JailConf* conf,
    const JailRulesOpt* opt,
    GError** error)
{
    const char* profile = opt->profile;
    const char* basename;
    char* file;
    char* path;

    if (profile) {
        if (jail_rules_basename(profile) == profile) {
            /* Determine directory from the suffix */
            const char* default_dir =
                g_str_has_suffix(profile, SAILJAIL_PROFILE_SUFFIX) ?
                conf->profile_dir :
                g_str_has_suffix(profile, SAILJAIL_DESKTOP_SUFFIX) ?
                conf->desktop_dir :
                NULL;

            if (default_dir) {
                /* Profile has a known suffix */
                return g_build_filename(default_dir, profile, NULL);
            } else {
                /* No idea what this is */
                if (error) {
                    g_propagate_error(error, g_error_new_literal(G_FILE_ERROR,
                        G_FILE_ERROR_FAILED, profile));
                }
                return NULL;
            }
        } else {
            /* Looks like the full path, use it as is */
            return g_strdup(profile);
        }
    } else {
        basename = jail_rules_basename(program);
    }

    /* Build path from the basename */
    file = g_strconcat(basename, SAILJAIL_PROFILE_SUFFIX, NULL);
    path = g_build_filename(conf->profile_dir, file, NULL);
    g_free(file);
    return path;
}

static
JailRulesData*
jail_rules_build(
    const char* prog,
    const JailConf* conf,
    const JailRulesOpt* opt,
    char** profile_path,
    char** section,
    GError** error)
{
    char* path = jail_rules_profile_path(prog, conf, opt, error);

    if (path) {
        JailRulesData* data = NULL;

        if (!opt->profile && !g_file_test(path, G_FILE_TEST_EXISTS)) {
            GDEBUG("No specific profile found for %s", prog);
            data = jail_rules_data_new(conf);
            if (profile_path) {
                *profile_path = NULL;
            }
        } else {
            GKeyFile* keyfile = g_key_file_new();

            if (g_key_file_load_from_file(keyfile, path, 0, error)) {
                data = jail_rules_parse_file(keyfile, path, prog, conf, opt,
                    section, error);
            } else if (error) {
                const char *message = (*error)->message;
                if (g_error_matches(*error, G_KEY_FILE_ERROR,
                            G_KEY_FILE_ERROR_PARSE)) {
                    /* Substitute better error message, show orginal with -v */
                    GDEBUG("%s: %s", path, message);
                    message = "Does not look like application profile";
                }
                /* Improve error message by prepending the file name */
                GError* details = g_error_new((*error)->domain, (*error)->code,
                    "%s: %s", path, message);

                g_error_free(*error);
                *error = details;
            }
            g_key_file_free(keyfile);
            if (profile_path) {
                if (data) {
                    /* Steal fname */
                    *profile_path = path;
                    path = NULL;
                } else {
                    *profile_path = NULL;
                }
            }
        }

        if (data) {
            /*
             * Always include required base profile. Append it as the very last
             * profile to allow noblacklist in other profiles.
             */
            jail_rules_add_profile(data, conf, SAILJAIL_BASE_PERM, TRUE);
        }

        g_free(path);
        return data;
    }
    return NULL;
}

static
gconstpointer
jail_rules_ptrv_new(
    GPtrArray* list)
{
    if (list->len > 0) {
        g_ptr_array_add(list, NULL);
        return g_ptr_array_free(list, FALSE);
    } else {
        /* Nothing was allocated */
        g_ptr_array_free(list, TRUE);
        return jail_rules_none;
    }
}

static
void
jail_rules_ptrv_free(
    gconstpointer v)
{
    /* jail_rules_none is a special case, see jail_rules_ptrv() */
    if (v != jail_rules_none) {
        g_strfreev((char**)v);
    }
}

static
JailRules*
jail_rules_from_data(
    JailRulesData* data)
{
    if (data) {
        JailRulesPriv* priv = g_new0(JailRulesPriv, 1);
        JailRules* rules = &priv->rules;

        rules->permits = jail_rules_ptrv_new(data->permits);
        rules->profiles = jail_rules_ptrv_new(data->profiles);
        rules->paths = jail_rules_ptrv_new(data->paths);
        rules->dbus_user = &priv->dbus_user;
        rules->dbus_system = &priv->dbus_system;
        priv->org_name = data->org_name; // Allocated when read
        priv->app_name = data->app_name; // Allocated when read
        priv->dbus_user.own = jail_rules_ptrv_new(data->dbus_user_own);
        priv->dbus_user.talk = jail_rules_ptrv_new(data->dbus_user_talk);
        priv->dbus_system.own = jail_rules_ptrv_new(data->dbus_system_own);
        priv->dbus_system.talk = jail_rules_ptrv_new(data->dbus_system_talk);
        g_atomic_int_set(&priv->ref_count, 1);
        g_free(data);
        return rules;
    }
    return NULL;
}

JailRules*
jail_rules_new(
    const char* program,
    const JailConf* conf,
    const JailRulesOpt* opt,
    char** profile_path,
    char** section,
    GError** error)
{
    JailRules* rules = jail_rules_from_data(jail_rules_build(program,
        conf, opt, profile_path, section, error));
    if (error && *error && opt->profile && conf->desktop_dir &&
            g_str_has_suffix(opt->profile, SAILJAIL_PROFILE_SUFFIX) &&
            (g_error_matches(*error, G_FILE_ERROR, G_FILE_ERROR_NOENT) ||
             g_error_matches(*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE))
            ) {
        /* User supplied .profile file which wasn't appropriate */
        char* name = g_path_get_basename(opt->profile);
        *strrchr(name, '.') = 0; /* The suffix is there and begins with a dot */
        char* app_desktop = g_strconcat(name, SAILJAIL_DESKTOP_SUFFIX, NULL);
        char* desktop = g_build_filename(conf->desktop_dir, app_desktop, NULL);
        if (g_file_test(desktop, G_FILE_TEST_IS_REGULAR)) {
            /* There is a matching .desktop file, suggest that instead */
            GError* suggest = g_error_new((*error)->domain, (*error)->code,
                "%s, however %s exists, maybe try that instead",
                (*error)->message, app_desktop);
            g_error_free(*error);
            *error = suggest;
        }
        g_free(desktop);
        g_free(app_desktop);
        g_free(name);
    }
    return rules;
}

JailRules*
jail_rules_ref(
    JailRules* rules)
{
    if (rules) {
        JailRulesPriv* priv = jail_rules_cast(rules);

        GASSERT(priv->ref_count > 0);
        g_atomic_int_inc(&priv->ref_count);
    }
    return rules;
}

void
jail_rules_unref(
    JailRules* rules)
{
    if (rules) {
        JailRulesPriv* priv = jail_rules_cast(rules);

        GASSERT(priv->ref_count > 0);
        if (g_atomic_int_dec_and_test(&priv->ref_count)) {
            jail_rules_ptrv_free(rules->permits);
            jail_rules_ptrv_free(rules->profiles);
            jail_rules_ptrv_free(rules->paths);
            jail_rules_ptrv_free(priv->dbus_user.own);
            jail_rules_ptrv_free(priv->dbus_user.talk);
            jail_rules_ptrv_free(priv->dbus_system.own);
            jail_rules_ptrv_free(priv->dbus_system.talk);
            g_free(priv->org_name);
            g_free(priv->app_name);
            g_free(priv);
        }
    }
}

JailRules*
jail_rules_keyfile_parse(
    SailJail* jail,
    GKeyFile* keyfile,
    const char* section,
    const char* app) /* Since 1.0.2 */
{
    GError *error = NULL;

    if (jail && keyfile) {
        JailRulesData* data = jail_rules_parse_section(keyfile,
            section ? section : SAILJAIL_SECTION_DEFAULT, app, jail->conf,
            &error);
        if (data && !error) {
            jail_rules_add_profile(data, jail->conf, SAILJAIL_BASE_PERM, TRUE);
        }

        if (error) {
            GERR("%s", GERRMSG(error));
            g_error_free(error);
        }

        return jail_rules_from_data(data);
    }
    return NULL;
}

/* Only leaves required and the specified optional items */
JailRules*
jail_rules_restrict(
    JailRules* rules,
    const JAIL_PERMIT_TYPE* permits,
    const char* const* profiles,
    const char* const* paths,
    const JailDBusRestrict* dbus_user,
    const JailDBusRestrict* dbus_system)
{
    if (rules) {
        JailRulesData* data = jail_rules_data_alloc();
        int i;

        if (rules->permits) {
            for (i = 0; rules->permits[i]; i++) {
                const JailPermit* permit = rules->permits[i];

                if (permit->require ||
                    jail_rules_permit_list_contains(permits, permit->type)) {
                    g_ptr_array_add(data->permits, jail_rules_permit_new
                        (permit->type, permit->require));
                }
            }
        }

        if (rules->profiles) {
            for (i = 0; rules->profiles[i]; i++) {
                const JailProfile* profile = rules->profiles[i];

                if (profile->require ||
                    gutil_strv_contains((GStrV*)profiles, profile->path)) {
                    g_ptr_array_add(data->profiles, jail_rules_profile_new
                        (profile->path, profile->require));
                }
            }
        }

        if (rules->paths) {
            for (i = 0; rules->paths[i]; i++) {
                const JailPath* path = rules->paths[i];

                if (path->require || !path->allow ||
                    gutil_strv_contains((GStrV*)paths, path->path)) {
                    g_ptr_array_add(data->paths, jail_rules_path_new
                        (path->path, path->allow, path->require));
                }
            }
        }

        if (rules->dbus_user) {
            jail_rules_restrict_dbus_names(rules->dbus_user->own,
                data->dbus_user_own, dbus_user ? dbus_user->own : NULL);
            jail_rules_restrict_dbus_names(rules->dbus_user->talk,
                data->dbus_user_talk, dbus_user ? dbus_user->talk : NULL);
        }

        if (rules->dbus_system) {
            jail_rules_restrict_dbus_names(rules->dbus_system->own,
                data->dbus_system_own, dbus_system ? dbus_system->own : NULL);
            jail_rules_restrict_dbus_names(rules->dbus_system->talk,
                data->dbus_system_talk, dbus_system ? dbus_system->talk : NULL);
        }

        // copy org and app names
        data->org_name = g_strdup(jail_rules_get_value(rules,
                                            SAILJAIL_KEY_ORGANIZATION_NAME));
        data->app_name = g_strdup(jail_rules_get_value(rules,
                                            SAILJAIL_KEY_APPLICATION_NAME));

        return jail_rules_from_data(data);
    }
    return NULL;
}

const char*
jail_rules_get_value(
    const JailRules* rules,
    const char *key)
{
    if (rules) {
        JailRulesPriv* priv = jail_rules_cast(rules);

        GASSERT(priv->ref_count > 0);

        if (!g_strcmp0(key, SAILJAIL_KEY_APPLICATION_NAME))
            return priv->app_name;
        else if (!g_strcmp0(key, SAILJAIL_KEY_ORGANIZATION_NAME))
           return priv->org_name;
    }
    return NULL;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
