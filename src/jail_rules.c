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

static const char SAILJAIL_PROFILE_SUFFIX[] = ".profile";
static const char SAILJAIL_DESKTOP_SUFFIX[] = ".desktop";
#define SAILJAIL_DESKTOP_SUFFIX_LEN (sizeof(SAILJAIL_DESKTOP_SUFFIX) - 1)

static const char SAILJAIL_BASE_PERM[] = "Base.profile";

static const char DESKTOP_GROUP_DESKTOP_ENTRY[] = "Desktop Entry";
static const char DESKTOP_KEY_DESKTOP_ENTRY_TYPE[] = "Type";
static const char DESKTOP_ENTRY_TYPE_APPLICATION[] = "Application";

static const char SAILJAIL_SECTION_DEFAULT[] = DEFAULT_PROFILE_SECTION;
static const char SAILJAIL_LIST_SEPARATORS[] = ":;,";

static const char SAILJAIL_KEY_PERMS[] = "Permissions";
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
    GPtrArray* permits;
    GPtrArray* profiles;
    GPtrArray* paths;
    GPtrArray* dbus_user_own;
    GPtrArray* dbus_user_talk;
    GPtrArray* dbus_system_own;
    GPtrArray* dbus_system_talk;
} JailRulesData;

typedef struct jail_rules_priv {
    JailRules rules;
    JailDBus dbus_user;
    JailDBus dbus_system;
    gint ref_count;
} JailRulesPriv;

static const char* const jail_rules_none[] = { NULL };

static inline
JailRulesPriv*
jail_rules_cast(
    JailRules* rules)
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
    char* path;

    if (g_str_has_suffix(name, SAILJAIL_PROFILE_SUFFIX)) {
        path = g_build_filename(conf->perm_dir, name, NULL);
    } else {
        char* file = g_strconcat(name, SAILJAIL_PROFILE_SUFFIX, NULL);

        path = g_build_filename(conf->perm_dir, file, NULL);
        g_free(file);
    }

    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
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
    } else {
        GWARN("Profile %s doesn't exist", path);
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

    /* Always include required base profile */
    jail_rules_add_profile(data, conf, SAILJAIL_BASE_PERM, TRUE);
    return data;
}

static
JailRulesData*
jail_rules_parse_section(
    GKeyFile* kf,
    const char* section,
    const JailConf* conf,
    const JailRulesOpt* opt)
{
    JailRulesData* data = jail_rules_data_new(conf);
    const char* app = opt->sailfish_app;
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
            permit = jail_rules_permit_parse(name);
            if (permit != JAIL_PERMIT_INVALID) {
                jail_rules_add_permit(data, permit, require);
            } else if (*name) {
                jail_rules_add_profile(data, conf, name, require);
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

        jail_rules_add_path(data, usr, TRUE, FALSE);
        jail_rules_add_path(data, desktop, TRUE, FALSE);
        g_free(usr);
        g_free(desktop);
        g_free(app_desktop);

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

    return data;
}

static
JailRulesData*
jail_rules_parse(
    GKeyFile* keyfile,
    const char* fname,
    const char* program,
    const JailConf* conf,
    const JailRulesOpt* opt,
    GError** error)
{
    JailRulesData* data = NULL;
    const char* section = opt->section;
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
         * and then [Sailjail]
         */
        if (gutil_strv_contains(groups, basename)) {
            section = basename;
        } else if (gutil_strv_contains(groups, SAILJAIL_SECTION_DEFAULT)) {
            section = SAILJAIL_SECTION_DEFAULT;
        }
    }

    if (section) {
        char* auto_app = NULL;
        JailRulesOpt auto_opt;

        if (!opt->sailfish_app &&
            g_str_has_suffix(fname, SAILJAIL_DESKTOP_SUFFIX)) {
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
                auto_opt = *opt;
                auto_opt.sailfish_app = auto_app;
                opt = &auto_opt;
            }
            g_free(type);
        }

        GDEBUG("Parsing [%s] section from %s", section, fname);
        data = jail_rules_parse_section(keyfile, section, conf, opt);
        g_free(auto_app);
    }

    if (!data && error) {
        g_propagate_error(error, g_error_new(G_KEY_FILE_ERROR,
            G_KEY_FILE_ERROR_GROUP_NOT_FOUND, "No section for %s in %s",
            program, fname));
    }

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
            /* Appends the suffix only if it's not already there */
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
    GError** error)
{
    char* fname = jail_rules_profile_path(prog, conf, opt, error);

    if (fname) {
        JailRulesData* data = NULL;

        if (!opt->profile && !g_file_test(fname, G_FILE_TEST_EXISTS)) {
            GDEBUG("No specific profile found for %s", prog);
            data = jail_rules_data_new(conf);
            if (profile_path) {
                *profile_path = NULL;
            }
        } else {
            GKeyFile* keyfile = g_key_file_new();

            if (g_key_file_load_from_file(keyfile, fname, 0, error)) {
                data = jail_rules_parse(keyfile, fname, prog, conf, opt, error);
            } else if (error) {
                /* Improve error message by prepending the file name */
                GError* details = g_error_new((*error)->domain, (*error)->code,
                    "%s: %s", fname, (*error)->message);

                g_error_free(*error);
                *error = details;
            }
            g_key_file_free(keyfile);
            if (profile_path) {
                if (data) {
                    /* Steal fname */
                    *profile_path = fname;
                    fname = NULL;
                } else {
                    *profile_path = NULL;
                }
            }
        }
        g_free(fname);
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
    const char* prog,
    const JailConf* conf,
    const JailRulesOpt* opt,
    char** path,
    GError** err)
{
    return jail_rules_from_data(jail_rules_build(prog, conf, opt, path, err));
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
            g_free(priv);
        }
    }
}

/* Only leaves requires and the specified optional items */
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

        return jail_rules_from_data(data);
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
