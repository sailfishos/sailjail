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

#include "applications.h"

#include "logging.h"
#include "control.h"
#include "stringset.h"
#include "appinfo.h"
#include "util.h"

#include <glob.h>
#include <fnmatch.h>

#include <gio/gio.h>

/* ========================================================================= *
 * CONSTANTS
 * ========================================================================= */

#define APPLICATIONS_RESCAN_DELAY            1000 // [ms]

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef enum
{
    APPLICATIONS_DIRECTORY_MONITOR,
    SAILJAIL_APP_DIRECTORY_MONITOR,
    DIRECTORY_MONITOR_COUNT
} applications_monitor_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * APPLICATIONS
 * ------------------------------------------------------------------------- */

static void     applications_ctor     (applications_t *self, control_t *control);
static void     applications_dtor     (applications_t *self);
applications_t *applications_create   (control_t *control);
void            applications_delete   (applications_t *self);
void            applications_delete_at(applications_t **pself);
void            applications_delete_cb(void *self);
void            applications_rethink  (applications_t *self);

/* ------------------------------------------------------------------------- *
 * APPLICATIONS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

control_t         *applications_control  (const applications_t *self);
const stringset_t *applications_available(applications_t *self);
appinfo_t         *applications_appinfo  (applications_t *self, const char *appname);
const config_t    *applications_config   (const applications_t *self);

/* ------------------------------------------------------------------------- *
 * APPLICATIONS_NOTIFY
 * ------------------------------------------------------------------------- */

static void applications_notify_changed(applications_t *self, GHashTable *changed);

/* ------------------------------------------------------------------------- *
 * APPLICATIONS_MONITOR
 * ------------------------------------------------------------------------- */

static applications_monitor_t  applications_get_monitor      (applications_t *self, GFileMonitor *mon);
static const char             *applications_monitor_dir_path (applications_monitor_t monitor);
static const char             *applications_monitor_name     (applications_monitor_t monitor);
static void                    applications_start_monitor_dir(applications_t *self, applications_monitor_t monitor);
static void                    applications_start_monitor    (applications_t *self);
static void                    applications_stop_monitor_dir (applications_t *self, applications_monitor_t monitor);
static void                    applications_stop_monitor     (applications_t *self);
static bool                    applications_monitor_p        (const gchar *path);
static void                    applications_monitor_cb       (GFileMonitor *mon, GFile *file1, GFile *file2, GFileMonitorEvent event, gpointer aptr);

/* ------------------------------------------------------------------------- *
 * APPLICATIONS_SCAN
 * ------------------------------------------------------------------------- */

static void     applications_scan_pattern (GHashTable *scanned, const char *pattern);
static void     applications_scan_now     (applications_t *self);
static gboolean applications_rescan_cb    (gpointer aptr);
static void     applications_rescan_later (applications_t *self);
static bool     applications_cancel_rescan(applications_t *self);

/* ------------------------------------------------------------------------- *
 * APPLICATIONS_APPINFO
 * ------------------------------------------------------------------------- */

static appinfo_t *applications_get_appinfo   (const applications_t *self, const char *appname);
static appinfo_t *applications_add_appinfo   (applications_t *self, const char *appname);
static bool       applications_remove_appinfo(applications_t *self, const char *appname);

/* ========================================================================= *
 * APPLICATIONS
 * ========================================================================= */

struct applications_t
{
    bool                   aps_initialized;
    control_t             *aps_control;
    stringset_t           *aps_available;
    guint                  aps_rescan_id;
    GFileMonitor          *aps_monitor_objs[DIRECTORY_MONITOR_COUNT];
    GHashTable            *aps_appinfo_lut;
};

static void
applications_ctor(applications_t *self, control_t *control)
{
    log_info("applications() create");

    self->aps_initialized = false;
    self->aps_control     = control;
    self->aps_available   = stringset_create();
    self->aps_rescan_id   = 0;

    self->aps_monitor_objs[APPLICATIONS_DIRECTORY_MONITOR] = NULL;
    self->aps_monitor_objs[SAILJAIL_APP_DIRECTORY_MONITOR] = NULL;

    self->aps_appinfo_lut = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  appinfo_delete_cb);

    /* Fetch initial state */
    applications_start_monitor(self);
    applications_scan_now(self);

    self->aps_initialized = true;
}

static  void
applications_dtor(applications_t *self)
{
    log_info("applications() delete");

    self->aps_initialized = false;

    applications_stop_monitor(self);
    applications_cancel_rescan(self);

    if( self->aps_appinfo_lut ) {
        g_hash_table_unref(self->aps_appinfo_lut),
            self->aps_appinfo_lut = NULL;
    }

    stringset_delete_at(&self->aps_available);
}

applications_t *
applications_create(control_t *control)
{
    applications_t *self = g_malloc0(sizeof *self);
    applications_ctor(self, control);
    return self;
}

void
applications_delete(applications_t *self)
{
    if( self ) {
        applications_dtor(self);
        g_free(self);
    }
}

void
applications_delete_at(applications_t **pself)
{
    applications_delete(*pself), *pself = NULL;
}

void
applications_delete_cb(void *self)
{
    applications_delete(self);
}

/* ------------------------------------------------------------------------- *
 * APPLICATIONS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

control_t *
applications_control(const applications_t *self)
{
    return self->aps_control;
}

const stringset_t *
applications_available(applications_t *self)
{
    if( applications_cancel_rescan(self) )
        applications_scan_now(self);
    return self->aps_available;
}

appinfo_t *
applications_appinfo(applications_t *self, const char *appname)
{
    appinfo_t *appinfo = applications_get_appinfo(self, appname);
    /* Do not expose removed/invalid desktop file
     * placeholders outside applications module.
     */
    return appinfo_valid(appinfo) ? appinfo : NULL;
}

const config_t *
applications_config(const applications_t *self)
{
    return control_config(applications_control(self));
}

/* ========================================================================= *
 * APPLICATIONS_NOTIFY
 * ========================================================================= */

static void
applications_notify_changed(applications_t *self, GHashTable *changed)
{
    if( self->aps_initialized )
        control_on_application_change(applications_control(self), changed);
}

/* ========================================================================= *
 * APPLICATIONS_MONITOR
 * ========================================================================= */

static applications_monitor_t
applications_get_monitor(applications_t *self, GFileMonitor *mon)
{
    for( applications_monitor_t monitor = 0; monitor < DIRECTORY_MONITOR_COUNT; ++monitor )
        if( self->aps_monitor_objs[monitor] == mon )
            return monitor;
    return DIRECTORY_MONITOR_COUNT;
}

static const char *
applications_monitor_dir_path(applications_monitor_t monitor)
{
    static const char * const lut[] = {
        [APPLICATIONS_DIRECTORY_MONITOR] = APPLICATIONS_DIRECTORY,
        [SAILJAIL_APP_DIRECTORY_MONITOR] = SAILJAIL_APP_DIRECTORY,
        [DIRECTORY_MONITOR_COUNT] = NULL
    };
    return lut[monitor];
}

static const char *
applications_monitor_name(applications_monitor_t monitor)
{
    static const char * const lut[] = {
        [APPLICATIONS_DIRECTORY_MONITOR] = "APPLICATIONS MONITOR",
        [SAILJAIL_APP_DIRECTORY_MONITOR] = "SAILJAIL APP MONITOR",
        [DIRECTORY_MONITOR_COUNT] = NULL
    };
    return lut[monitor];
}

static void
applications_start_monitor_dir(applications_t *self, applications_monitor_t monitor)
{
    applications_stop_monitor_dir(self, monitor);

    GFileMonitorFlags flags = G_FILE_MONITOR_WATCH_MOVES;
    GFile *file = g_file_new_for_path(applications_monitor_dir_path(monitor));
    if( file ) {
        if( (self->aps_monitor_objs[monitor] = g_file_monitor_directory(file, flags, 0, 0)) ) {
            g_signal_connect(self->aps_monitor_objs[monitor], "changed",
                             G_CALLBACK(applications_monitor_cb), self);
            log_info("%s: started", applications_monitor_name(monitor));
        }
        g_object_unref(file);
    }
}

static void
applications_start_monitor(applications_t *self)
{
    for( applications_monitor_t monitor = 0; monitor < DIRECTORY_MONITOR_COUNT; ++monitor )
        applications_start_monitor_dir(self, monitor);
}

static void
applications_stop_monitor_dir(applications_t *self, applications_monitor_t monitor)
{
    if( self->aps_monitor_objs[monitor] ) {
        log_info("%s: stopped", applications_monitor_name(monitor));
        g_object_unref(self->aps_monitor_objs[monitor]),
            self->aps_monitor_objs[monitor] = NULL;
    }
}

static void
applications_stop_monitor(applications_t *self)
{
    for( applications_monitor_t monitor = 0; monitor < DIRECTORY_MONITOR_COUNT; ++monitor )
        applications_stop_monitor_dir(self, monitor);
}

static bool
applications_monitor_p(const gchar *path)
{
    path = path_basename(path);
    return path && fnmatch(APPLICATIONS_PATTERN, path, FNM_PATHNAME) == 0;
}

static void
applications_monitor_cb(GFileMonitor      *mon,
                        GFile             *file1,
                        GFile             *file2,
                        GFileMonitorEvent  event,
                        gpointer           aptr)
{
    applications_t *self = aptr;

    const char *path1 = file1 ? g_file_peek_path(file1) : NULL;
    const char *path2 = file2 ? g_file_peek_path(file2) : NULL;
    bool trigger = (applications_monitor_p(path1) ||
                    applications_monitor_p(path2));

    if( trigger ) {
        log_info("%s: trigger @ %s %s", applications_monitor_name(applications_get_monitor(self, mon)),
                 path1, path2);
        applications_rescan_later(self);
    }
}

/* ========================================================================= *
 * APPLICATIONS_SCAN
 * ========================================================================= */

void
applications_rethink(applications_t *self)
{
    GHashTable *changed = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                g_free, NULL);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, self->aps_appinfo_lut);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        if( appinfo_evaluate_permissions(value) )
            g_hash_table_add(changed, g_strdup(key));
    }

    if( g_hash_table_size(changed) > 0 )
        applications_notify_changed(self, changed);

    g_hash_table_unref(changed);
}

static void
applications_scan_pattern(GHashTable *scanned, const char *pattern)
{
    glob_t gl  = {};

    if( glob(pattern, 0, 0, &gl) == 0 ) {
        for( int i = 0; i < gl.gl_pathc; ++i ) {
            gchar *appname = path_to_desktop_name(gl.gl_pathv[i]);
            g_hash_table_add(scanned, appname);
        }
    }

    globfree(&gl);
}

static void
applications_scan_now(applications_t *self)
{
    GHashTable *scanned = NULL;
    GHashTable *changed = NULL;

    applications_cancel_rescan(self);

    log_info("APPLICATIONS RESCAN: executing");

    scanned = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    applications_scan_pattern(scanned, APPLICATIONS_DIRECTORY "/" APPLICATIONS_PATTERN);
    applications_scan_pattern(scanned, SAILJAIL_APP_DIRECTORY "/" APPLICATIONS_PATTERN);

    changed = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    GHashTableIter iter;
    gpointer key, value;

    /* Find out entries that no longer exist */
    g_hash_table_iter_init(&iter, self->aps_appinfo_lut);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        if( !g_hash_table_lookup(scanned, key) )
            g_hash_table_add(changed, g_strdup(key));
    }

    /* Flush removed entries from bookkeeping */
    g_hash_table_iter_init(&iter, changed);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        log_debug("APPLICATIONS RESCAN: remove: %s", (char *)key);
        applications_remove_appinfo(self, key);
    }

    /* Update existing / new entries */
    g_hash_table_iter_init(&iter, scanned);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        //log_debug("APPLICATIONS RESCAN: update: %s", (char *)key);
        appinfo_t *appinfo = applications_add_appinfo(self, key);
        if( appinfo_parse_desktop(appinfo) )
            g_hash_table_add(changed, g_strdup(key));
    }

    /* Update available list */
    stringset_clear(self->aps_available);
    g_hash_table_iter_init(&iter, self->aps_appinfo_lut);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        if( appinfo_valid(value) )
            stringset_add_item(self->aps_available, key);
    }

    /* Notify upwards, receiver can ref() if needed */
    if( g_hash_table_size(changed) > 0 )
        applications_notify_changed(self, changed);

    if( changed )
        g_hash_table_unref(changed);
    if( scanned )
        g_hash_table_unref(scanned);
}

static gboolean
applications_rescan_cb(gpointer aptr)
{
    applications_t *self = aptr;

    self->aps_rescan_id = 0;

    log_info("APPLICATIONS RESCAN: triggered");
    applications_scan_now(self);

    return G_SOURCE_REMOVE;
}

static void
applications_rescan_later(applications_t *self)
{
    if( !self->aps_rescan_id )
        log_info("APPLICATIONS RESCAN: scheduled");
    else
        g_source_remove(self->aps_rescan_id);
    self->aps_rescan_id = g_timeout_add(APPLICATIONS_RESCAN_DELAY,
                                        applications_rescan_cb, self);
}

static bool
applications_cancel_rescan(applications_t *self)
{
    bool canceled = false;
    if( self->aps_rescan_id ) {
        log_info("APPLICATIONS RESCAN: canceled");
        g_source_remove(self->aps_rescan_id),
            self->aps_rescan_id = 0;
        canceled = true;
    }
    return canceled;
}

/* ------------------------------------------------------------------------- *
 * APPLICATIONS_APPINFO
 * ------------------------------------------------------------------------- */

static appinfo_t *
applications_get_appinfo(const applications_t *self, const char *appname)
{
    return g_hash_table_lookup(self->aps_appinfo_lut, appname);
}

static appinfo_t *
applications_add_appinfo(applications_t *self, const char *appname)
{
    appinfo_t *appinfo = applications_get_appinfo(self, appname);
    if( !appinfo ) {
        appinfo = appinfo_create(self, appname);
        g_hash_table_insert(self->aps_appinfo_lut, g_strdup(appname), appinfo);
    }
    return appinfo;
}

static bool
applications_remove_appinfo(applications_t *self, const char *appname)
{
    return g_hash_table_remove(self->aps_appinfo_lut, appname);
}
