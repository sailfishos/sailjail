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

#include "config.h"

#include "util.h"
#include "stringset.h"
#include "logging.h"

#include <glob.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct config_t config_t;
typedef struct stringset_t stringset_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * CONFIG
 * ------------------------------------------------------------------------- */

static void  config_ctor     (config_t *self);
static void  config_dtor     (config_t *self);
config_t    *config_create   (void);
void         config_delete   (config_t *self);
void         config_delete_at(config_t **pself);
void         config_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * CONFIG_LOAD
 * ------------------------------------------------------------------------- */

static void config_unload(config_t *self);
static void config_load  (config_t *self);

/* ------------------------------------------------------------------------- *
 * CONFIG_VALUE
 * ------------------------------------------------------------------------- */

bool         config_boolean  (const config_t *self, const gchar *sec, const gchar *key, bool def);
gint         config_integer  (const config_t *self, const gchar *sec, const gchar *key, gint def);
gchar       *config_string   (const config_t *self, const gchar *sec, const gchar *key, const gchar *def);
stringset_t *config_stringset(const config_t *self, const gchar *sec, const gchar *key);

/* ========================================================================= *
 * CONFIG
 * ========================================================================= */

struct config_t
{
    GKeyFile *cfg_keyfile;
};

static void
config_ctor(config_t *self)
{
    log_info("config() created");
    self->cfg_keyfile = NULL;
    config_load(self);
}

static void
config_dtor(config_t *self)
{
    log_info("config() deleted");
    config_unload(self);
}

config_t *
config_create(void)
{
    config_t *self = g_malloc0(sizeof *self);
    config_ctor(self);
    return self;
}

void
config_delete(config_t *self)
{
    if( self ) {
        config_dtor(self);
        g_free(self);
    }
}

void
config_delete_at(config_t **pself)
{
    config_delete(*pself), *pself = NULL;
}

void
config_delete_cb(void *self)
{
    config_delete(self);
}

static void
config_unload(config_t *self)
{
    if( self->cfg_keyfile ) {
        g_key_file_unref(self->cfg_keyfile),
            self->cfg_keyfile = NULL;
    }
}

static void
config_load(config_t *self)
{
    glob_t gl = {};

    config_unload(self);

    self->cfg_keyfile = g_key_file_new();

    if( glob(CONFIG_DIRECTORY "/" CONFIG_PATTERN, 0, 0, &gl) != 0 )
        goto EXIT;

    for( int i = 0; i < gl.gl_pathc; ++i )
        keyfile_merge(self->cfg_keyfile, gl.gl_pathv[i]);

EXIT:
    globfree(&gl);
}

bool
config_boolean(const config_t *self, const gchar *sec, const gchar *key,
               bool def)
{
    return keyfile_get_boolean(self->cfg_keyfile, sec, key, def);
}

gint
config_integer(const config_t *self, const gchar *sec, const gchar *key,
               gint def)
{
    return keyfile_get_integer(self->cfg_keyfile, sec, key, def);
}

gchar *
config_string(const config_t *self, const gchar *sec, const gchar *key,
              const gchar *def)
{
    return keyfile_get_string(self->cfg_keyfile, sec, key, def);
}

stringset_t *
config_stringset(const config_t *self, const gchar *sec, const gchar *key)
{
    return keyfile_get_stringset(self->cfg_keyfile, sec, key);
}
