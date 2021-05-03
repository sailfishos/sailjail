/*
 * Copyright (c) 2021 Open Mobile Platform LLC.
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

#include "util.h"

#include "logging.h"
#include "stringset.h"

#include <ctype.h>

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

char *strip(char *str);

/* ------------------------------------------------------------------------- *
 * PATH
 * ------------------------------------------------------------------------- */

const gchar  *path_basename             (const gchar *path);
const gchar  *path_extension            (const gchar *path);
gchar        *path_dirname              (const gchar *path);
static gchar *path_stemname             (const gchar *path, const char *ext_to_remove);
gchar        *path_to_desktop_name      (const gchar *path);
gchar        *path_from_desktop_name    (const gchar *stem);
gchar        *alt_path_from_desktop_name(const gchar *stem);
gchar        *path_to_permission_name   (const gchar *path);
gchar        *path_from_permission_name (const gchar *stem);
gchar        *path_from_profile_name    (const gchar *stem);

/* ------------------------------------------------------------------------- *
 * GUTIL
 * ------------------------------------------------------------------------- */

guint gutil_add_watch(int fd, GIOCondition cnd, GIOFunc cb, gpointer aptr);

/* ------------------------------------------------------------------------- *
 * CHANGE
 * ------------------------------------------------------------------------- */

bool change_uid         (uid_t *where, uid_t val);
bool change_boolean     (bool *where, bool val);
bool change_string      (gchar **pstr, const gchar *val);
bool change_string_steal(gchar **pstr, gchar *val);

/* ------------------------------------------------------------------------- *
 * KEYFILE
 * ------------------------------------------------------------------------- */

bool         keyfile_save         (GKeyFile *file, const gchar *path);
bool         keyfile_load         (GKeyFile *file, const gchar *path);
bool         keyfile_merge        (GKeyFile *file, const gchar *path);
bool         keyfile_get_boolean  (GKeyFile *file, const gchar *sec, const gchar *key, bool def);
gint         keyfile_get_integer  (GKeyFile *file, const gchar *sec, const gchar *key, gint def);
gchar       *keyfile_get_string   (GKeyFile *file, const gchar *sec, const gchar *key, const gchar *def);
stringset_t *keyfile_get_stringset(GKeyFile *file, const gchar *sec, const gchar *key);
void         keyfile_set_boolean  (GKeyFile *file, const gchar *sec, const gchar *key, bool val);
void         keyfile_set_integer  (GKeyFile *file, const gchar *sec, const gchar *key, gint val);
void         keyfile_set_string   (GKeyFile *file, const gchar *sec, const gchar *key, const gchar *val);
void         keyfile_set_stringset(GKeyFile *file, const gchar *sec, const gchar *key, const stringset_t *val);

/* ========================================================================= *
 * UTILITY
 * ========================================================================= */

char *
strip(char *str)
{
    if( str ) {
        char *src = str;
        char *dst = str;
        while( *src && isspace(*src) )
            ++src;
        for( ;; ) {
            while( *src && !isspace(*src) )
                *dst++ = *src++;
            while( *src && isspace(*src) )
                ++src;
            if( !*src )
                break;
            *dst++ = ' ';
        }
        *dst = 0;
    }
    return str;
}

/* ========================================================================= *
 * PATH
 * ========================================================================= */

const gchar *
path_basename(const gchar *path)
{
    const gchar *base = NULL;
    if( path ) {
        gchar *end = strrchr(path, '/');
        base = end ? end + 1 : path;
    }
    return base;
}

const gchar *
path_extension(const gchar *path)
{
    const gchar *ext  = NULL;
    const gchar *base = path_basename(path);
    if( base )
        ext = strrchr(base, '.');
    return ext;
}

gchar *
path_dirname(const gchar *path)
{
    gchar *dirp = NULL;
    if( path )
        dirp = g_path_get_dirname(path);
    return dirp;
}

static gchar *
path_stemname(const gchar *path, const char *ext_to_remove)
{
    gchar       *stem = NULL;
    const gchar *base = path_basename(path);
    if( base ) {
        const gchar *end = path_extension(base);
        if( !end || g_strcmp0(end, ext_to_remove) )
            end = strchr(base, 0);
        stem = g_strndup(base, end - base);
    }
    return stem;
}

gchar *
path_to_desktop_name(const gchar *path)
{
    return path_stemname(path, APPLICATIONS_EXTENSION);
}

gchar *
path_from_desktop_name(const gchar *stem)
{
    gchar *path = 0;
    gchar *norm = path_to_desktop_name(stem);
    if( norm )
        path = g_strdup_printf(APPLICATIONS_DIRECTORY "/"
                               "%s" APPLICATIONS_EXTENSION,
                               norm);
    g_free(norm);
    return path;
}

gchar *
alt_path_from_desktop_name(const gchar *stem)
{
    gchar *path = 0;
    gchar *norm = path_to_desktop_name(stem);
    if( norm )
        path = g_strdup_printf(SAILJAIL_APP_DIRECTORY "/"
                               "%s" APPLICATIONS_EXTENSION,
                               norm);
    g_free(norm);
    return path;
}

gchar *
path_to_permission_name(const gchar *path)
{
    return path_stemname(path, PERMISSIONS_EXTENSION);
}

gchar *
path_from_permission_name(const gchar *stem)
{
    gchar *path = 0;
    gchar *norm = path_to_permission_name(stem);
    if( norm )
        path = g_strdup_printf(PERMISSIONS_DIRECTORY "/"
                               "%s" PERMISSIONS_EXTENSION,
                               norm);
    g_free(norm);
    return path;
}

gchar *
path_from_profile_name(const gchar *stem)
{
    gchar *path = 0;
    gchar *norm = path_to_permission_name(stem);
    if( norm )
        path = g_strdup_printf(PERMISSIONS_DIRECTORY "/"
                               "%s" PROFILES_EXTENSION,
                               norm);
    g_free(norm);
    return path;
}

/* ========================================================================= *
 * GUTIL
 * ========================================================================= */

guint
gutil_add_watch(int fd, GIOCondition cnd, GIOFunc cb, gpointer aptr)
{
    guint       id  = 0;
    GIOChannel *chn = 0;

    cnd |= G_IO_ERR | G_IO_HUP | G_IO_NVAL;

    if( !(chn = g_io_channel_unix_new(fd)) )
        goto EXIT;

    id = g_io_add_watch(chn, cnd, cb, aptr);

EXIT:
    if( chn != 0 )
        g_io_channel_unref(chn);

    return id;
}

/* ========================================================================= *
 * CHANGE
 * ========================================================================= */

bool
change_uid(uid_t *where, uid_t val)
{
    bool changed = false;
    if( *where != val ) {
        *where = val;
        changed = true;
    }
    return changed;
}

bool
change_boolean(bool *where, bool val)
{
    bool changed = false;
    if( *where != val ) {
        *where = val;
        changed = true;
    }
    return changed;
}

bool
change_string(gchar **pstr, const gchar *val)
{
    bool changed = false;
    if( g_strcmp0(*pstr, val) ) {
        g_free(*pstr), *pstr = g_strdup(val);
        changed = true;
    }
    return changed;
}

bool
change_string_steal(gchar **pstr, gchar *val)
{
    bool changed = false;
    if( g_strcmp0(*pstr, val) ) {
        g_free(*pstr), *pstr = val;
        changed = true;
    }
    else if( *pstr != val ) {
        g_free(val);
    }
    return changed;
}

/* ========================================================================= *
 * KEYFILE
 * ========================================================================= */

bool
keyfile_save(GKeyFile *file, const gchar *path)
{
    GError *err = NULL;
    bool    ack = g_key_file_save_to_file(file, path, &err);
    if( !ack )
        log_err("%s: save failed: %s", path, err->message);
    else
        log_info("%s: saved succesfully", path);
    g_clear_error(&err);
    return ack;
}

bool
keyfile_load(GKeyFile *file, const gchar *path)
{
    GError *err = NULL;
    bool    ack = g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, &err);

    if( ack )
        log_debug("%s: loaded succesfully", path);
    else if( g_error_matches(err, G_FILE_ERROR, G_FILE_ERROR_NOENT) )
        log_debug("%s: load failed: %s", path, err->message);
    else
        log_err("%s: load failed: %s", path, err->message);

    g_clear_error(&err);
    return ack;
}

bool
keyfile_merge(GKeyFile *file, const gchar *path)
{
    bool       ack = false;
    GKeyFile  *tmp = g_key_file_new();

    if( !(ack = keyfile_load(tmp, path)) )
        goto EXIT;

    gchar **groups = g_key_file_get_groups(tmp, NULL);
    if( groups ) {
        for( size_t i = 0; groups[i]; ++i ) {
            gchar **keys = g_key_file_get_keys(tmp, groups[i], NULL, NULL);
            if( keys ) {
                for( size_t j = 0; keys[j]; ++j ) {
                    gchar *val = g_key_file_get_value(tmp, groups[i],
                                                      keys[j], NULL);
                    if( val ) {
                        g_key_file_set_value(file, groups[i], keys[j], val);
                        log_debug("%s [%s] %s = %s\n", path, groups[i], keys[j],
                                  strip(val));
                        g_free(val);
                    }
                }
                g_strfreev(keys);
            }
        }
        g_strfreev(groups);
    }
EXIT:
    if( tmp )
        g_key_file_unref(tmp);
    return ack;
}

bool
keyfile_get_boolean(GKeyFile *file, const gchar *sec, const gchar *key,
                    bool def)
{
    GError *err = 0;
    bool    val = g_key_file_get_boolean(file, sec, key, &err);
    if( err )
        val = def;
    g_clear_error(&err);
    return val;
}

gint
keyfile_get_integer(GKeyFile *file, const gchar *sec, const gchar *key,
                    gint def)
{
    GError *err = 0;
    gint    val = g_key_file_get_integer(file, sec, key, &err);
    if( err )
        val = def;
    g_clear_error(&err);
    return val;
}

gchar *
keyfile_get_string(GKeyFile *file, const gchar *sec, const gchar *key,
                   const gchar *def)
{
    gchar *val = g_key_file_get_string(file, sec, key, NULL);
    if( !val )
        val = g_strdup(def);
    return val;
}

stringset_t *
keyfile_get_stringset(GKeyFile *file, const gchar *sec, const gchar *key)
{
    stringset_t *val = stringset_create();

    gchar **vec = g_key_file_get_string_list(file, sec, key, NULL, NULL);
    if( vec ) {
        for( size_t i = 0; vec[i]; ++i )
            stringset_add_item(val, vec[i]);
        g_strfreev(vec);
    }
    return val;
}

void
keyfile_set_boolean(GKeyFile *file, const gchar *sec, const gchar *key,
                    bool val)
{
    g_key_file_set_boolean(file, sec, key, val);
}

void
keyfile_set_integer(GKeyFile *file, const gchar *sec, const gchar *key,
                    gint val)
{
    g_key_file_set_integer(file, sec, key, val);
}

void
keyfile_set_string(GKeyFile *file, const gchar *sec, const gchar *key,
                   const gchar *val)
{
    g_key_file_set_string(file, sec, key, val ?: "");
}

void
keyfile_set_stringset(GKeyFile *file, const gchar *sec, const gchar *key,
                      const stringset_t *val)
{
    /* An interesting twist: g_key_file_set_string_list()
     * does not support zero length lists...
     */

    gchar **vec = stringset_to_strv(val);
    gsize len = 0;
    if( vec )
        while( vec[len] )
            ++len;
    if( len > 0 )
        g_key_file_set_string_list(file, sec, key, (const gchar * const *)vec, len);
    else
        g_key_file_set_string(file, sec, key, "");
    g_strfreev(vec);
}
