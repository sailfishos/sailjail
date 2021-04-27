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

#include "permissions.h"

#include "logging.h"
#include "control.h"
#include "stringset.h"
#include "util.h"

#include <glob.h>
#include <fnmatch.h>

#include <gio/gio.h>

/* ========================================================================= *
 * CONSTANTS
 * ========================================================================= */

#define PERMISSIONS_RESCAN_DELAY            1000 // [ms]

/* ========================================================================= *
 * Types
 * ========================================================================= */

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * PERMISSIONS
 * ------------------------------------------------------------------------- */

static void    permissions_ctor     (permissions_t *self, control_t *control);
static void    permissions_dtor     (permissions_t *self);
permissions_t *permissions_create   (control_t *control);
void           permissions_delete   (permissions_t *self);
void           permissions_delete_at(permissions_t **pself);
void           permissions_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * PERMISSIONS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static control_t *permissions_control(const permissions_t *self);

/* ------------------------------------------------------------------------- *
 * PERMISSIONS_AVAILABLE
 * ------------------------------------------------------------------------- */

const stringset_t *permissions_available(permissions_t *self);

/* ------------------------------------------------------------------------- *
 * PERMISSIONS_NOTIFY
 * ------------------------------------------------------------------------- */

static void permissions_notify_changed(permissions_t *self);

/* ------------------------------------------------------------------------- *
 * PERMISSIONS_MONITOR
 * ------------------------------------------------------------------------- */

static void permissions_start_monitor(permissions_t *self);
static void permissions_stop_monitor (permissions_t *self);
static bool permissions_monitor_p    (const gchar *path);
static void permissions_monitor_cb   (GFileMonitor *mon, GFile *file1, GFile *file2, GFileMonitorEvent event, gpointer aptr);

/* ------------------------------------------------------------------------- *
 * PERMISSIONS_SCAN
 * ------------------------------------------------------------------------- */

static bool     permissions_scan_now     (permissions_t *self);
static gboolean permissions_rescan_cb    (gpointer aptr);
static void     permissions_rescan_later (permissions_t *self);
static bool     permissions_cancel_rescan(permissions_t *self);

/* ========================================================================= *
 * PERMISSIONS
 * ========================================================================= */

struct permissions_t
{
    bool                   prm_initialized;
    control_t             *prm_control;
    stringset_t           *prm_current;
    guint                  prm_rescan_id;
    GFileMonitor          *prm_monitor_obj;
};

static void
permissions_ctor(permissions_t *self, control_t *control)
{
    log_info("permissions() create");

    self->prm_initialized = false;
    self->prm_control     = control;
    self->prm_current     = stringset_create();
    self->prm_rescan_id   = 0;
    self->prm_monitor_obj = NULL;

    /* Fetch initial state */
    permissions_start_monitor(self);
    permissions_scan_now(self);

    /* Enable notifications */
    self->prm_initialized = true;
}

static  void
permissions_dtor(permissions_t *self)
{
    log_info("permissions() delete");

    /* Detach notification hooks */
    self->prm_initialized = false;

    permissions_stop_monitor(self);
    permissions_cancel_rescan(self);
    stringset_delete_at(&self->prm_current);
}

permissions_t *
permissions_create(control_t *control)
{
    permissions_t *self = g_malloc0(sizeof *self);
    permissions_ctor(self, control);
    return self;
}

void
permissions_delete(permissions_t *self)
{
    if( self ) {
        permissions_dtor(self);
        g_free(self);
    }
}

void
permissions_delete_at(permissions_t **pself)
{
    permissions_delete(*pself), *pself = NULL;
}

void
permissions_delete_cb(void *self)
{
    permissions_delete(self);
}

/* ========================================================================= *
 * PERMISSIONS_ATTRIBUTES
 * ========================================================================= */

static control_t *
permissions_control(const permissions_t *self)
{
    return self->prm_control;
}

/* ========================================================================= *
 * PERMISSIONS_AVAILABLE
 * ========================================================================= */

const stringset_t *
permissions_available(permissions_t *self)
{
    if( permissions_cancel_rescan(self) )
        permissions_scan_now(self);

    return self->prm_current;
}

/* ========================================================================= *
 * PERMISSIONS_NOTIFY
 * ========================================================================= */

static void
permissions_notify_changed(permissions_t *self)
{
    if( self->prm_initialized ) {
        log_info("PERMISSIONS NOTIFY");
        control_on_permissions_change(permissions_control(self));
    }
}

/* ========================================================================= *
 * PERMISSIONS_MONITOR
 * ========================================================================= */

static void
permissions_start_monitor(permissions_t *self)
{
    permissions_stop_monitor(self);

    GFileMonitorFlags flags = G_FILE_MONITOR_WATCH_MOVES;
    GFile *file = g_file_new_for_path(PERMISSIONS_DIRECTORY);
    if( file ) {
        if( (self->prm_monitor_obj = g_file_monitor_directory(file, flags, 0, 0)) ) {
            g_signal_connect(self->prm_monitor_obj, "changed",
                             G_CALLBACK(permissions_monitor_cb), self);
            log_info("PERMISSIONS MONITOR: started");
        }
        g_object_unref(file);
    }
}

static void
permissions_stop_monitor(permissions_t *self)
{
    if( self->prm_monitor_obj ) {
        log_info("PERMISSIONS MONITOR: stopped");
        g_object_unref(self->prm_monitor_obj),
            self->prm_monitor_obj = NULL;
    }
}

static bool
permissions_monitor_p(const gchar *path)
{
    path = path_basename(path);
    return path && fnmatch(PERMISSIONS_PATTERN, path, FNM_PATHNAME) == 0;
}

static void
permissions_monitor_cb(GFileMonitor      *mon,
                       GFile             *file1,
                       GFile             *file2,
                       GFileMonitorEvent  event,
                       gpointer           aptr)
{
    permissions_t *self = aptr;

    const char *path1 = file1 ? g_file_peek_path(file1) : NULL;
    const char *path2 = file2 ? g_file_peek_path(file2) : NULL;
    bool trigger = (permissions_monitor_p(path1) ||
                    permissions_monitor_p(path2));

    if( trigger ) {
        log_info("PERMISSIONS MONITOR: trigger @ %s %s", path1, path2);
        permissions_rescan_later(self);
    }
}

/* ========================================================================= *
 * PERMISSIONS_SCAN
 * ========================================================================= */

static bool
permissions_scan_now(permissions_t *self)
{
    permissions_cancel_rescan(self);
    log_info("PERMISSIONS RESCAN: executing");

    bool changed = false;

    stringset_t *scanned = stringset_create();
    glob_t       gl    = {};

    if( glob(PERMISSIONS_DIRECTORY "/" PERMISSIONS_PATTERN, 0, 0, &gl) != 0 ) {
        /* Keep current data */
        goto EXIT;
    }

    /* 'Privileged' exists even if there is no
     * matching permission file present.
     */
    stringset_add_item(scanned, PERMISSION_PRIVILEGED);

    for( int i = 0; i < gl.gl_pathc; ++i ) {
        gchar *name = path_to_permission_name(gl.gl_pathv[i]);
        stringset_add_item(scanned, name);
        g_free(name);
    }

    /* 'Base' does not play role in permission management
     * even if a matching file for it would be present.
     */
    stringset_remove_item(scanned, PERMISSION_BASE);

    stringset_t *addset = stringset_filter_out(scanned, self->prm_current);
    stringset_t *remset = stringset_filter_out(self->prm_current, scanned);

    if( !stringset_empty(addset) ) {
        gchar *addstr = stringset_to_string(addset);
        if( addstr && *addstr )
            log_notice("PERMISSIONS RESCAN: added: %s", addstr);
        g_free(addstr);
        changed = true;
    }

    if( !stringset_empty(remset) ) {
        gchar *remstr = stringset_to_string(remset);
        if( remstr && *remstr )
            log_notice("PERMISSIONS RESCAN: removed: %s", remstr);
        g_free(remstr);
        changed = true;
    }

    stringset_delete(addset);
    stringset_delete(remset);

    if( changed )
        stringset_swap(self->prm_current, scanned);

EXIT:
    stringset_delete(scanned);
    globfree(&gl);
    return changed;
}

static gboolean
permissions_rescan_cb(gpointer aptr)
{
    permissions_t *self = aptr;

    self->prm_rescan_id = 0;

    log_info("PERMISSIONS RESCAN: triggered");
    if( permissions_scan_now(self) )
        permissions_notify_changed(self);

    return G_SOURCE_REMOVE;
}

static void
permissions_rescan_later(permissions_t *self)
{
    if( !self->prm_rescan_id )
        log_info("PERMISSIONS RESCAN: scheduled");
    else
        g_source_remove(self->prm_rescan_id);
    self->prm_rescan_id = g_timeout_add(PERMISSIONS_RESCAN_DELAY,
                                        permissions_rescan_cb, self);
}

static bool
permissions_cancel_rescan(permissions_t *self)
{
    bool canceled = false;
    if( self->prm_rescan_id ) {
        log_info("PERMISSIONS RESCAN: canceled");
        g_source_remove(self->prm_rescan_id),
            self->prm_rescan_id = 0;
        canceled = true;
    }
    return canceled;
}
