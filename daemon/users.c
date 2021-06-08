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

#include "users.h"

#include "logging.h"
#include "util.h"
#include "control.h"

#include <pwd.h>
#include <fnmatch.h>

#include <gio/gio.h>

/* ========================================================================= *
 * CONSTANTS
 * ========================================================================= */

#define USERS_UID_MIN 100000
#define USERS_UID_MAX 100007
#define USERS_UID_GUEST 105000

/* ========================================================================= *
 * Types
 * ========================================================================= */

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * USERS
 * ------------------------------------------------------------------------- */

static void  users_ctor     (users_t *self, control_t *control);
static void  users_dtor     (users_t *self);
users_t     *users_create   (control_t *control);
void         users_delete   (users_t *self);
void         users_delete_at(users_t **pself);
void         users_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * USERS_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static control_t *users_control(const users_t *self);

/* ------------------------------------------------------------------------- *
 * USERS_USER
 * ------------------------------------------------------------------------- */

uid_t users_first_user   (const users_t *self);
uid_t users_last_user    (const users_t *self);
bool  users_user_exists  (users_t *self, uid_t uid);
bool  users_user_is_guest(const users_t *self, uid_t uid);

/* ------------------------------------------------------------------------- *
 * USERS_NOTIFY
 * ------------------------------------------------------------------------- */

static void users_notify_changed(users_t *self);

/* ------------------------------------------------------------------------- *
 * USERS_SCAN
 * ------------------------------------------------------------------------- */

static bool     users_scan_now     (users_t *self);
static gboolean users_rescan_cb    (gpointer aptr);
static void     users_rescan_later (users_t *self);
static bool     users_cancel_rescan(users_t *self);

/* ------------------------------------------------------------------------- *
 * USERS_MONITOR
 * ------------------------------------------------------------------------- */

static void users_start_monitor(users_t *self);
static void users_stop_monitor (users_t *self);
static bool users_monitor_p    (const gchar *path);
static void users_monitor_cb   (GFileMonitor *mon, GFile *file1, GFile *file2, GFileMonitorEvent event, gpointer aptr);

/* ========================================================================= *
 * USERS
 * ========================================================================= */

struct users_t
{
    bool              usr_initialized;
    control_t        *usr_control;
    GHashTable       *usr_current;
    guint             usr_rescan_id;
    GFileMonitor     *usr_monitor_obj;
};

static void
users_ctor(users_t *self, control_t *control)
{
    log_info("users() create");

    self->usr_initialized = false;
    self->usr_control     = control;
    self->usr_current     = g_hash_table_new (g_direct_hash, g_direct_equal);
    self->usr_rescan_id   = 0;
    self->usr_monitor_obj = NULL;

    /* Get initial state */
    users_start_monitor(self);
    users_scan_now(self);

    /* Enable notifications */
    self->usr_initialized = true;
}

static  void
users_dtor(users_t *self)
{
    log_info("users() delete");

    /* Disable notifications */
    self->usr_initialized = true;

    if( self->usr_current ) {
        g_hash_table_unref(self->usr_current),
            self->usr_current = NULL;
    }

    users_stop_monitor(self);
    users_cancel_rescan(self);
}

users_t *
users_create(control_t *control)
{
    users_t *self = g_malloc0(sizeof *self);
    users_ctor(self, control);
    return self;
}

void
users_delete(users_t *self)
{
    if( self ) {
        users_dtor(self);
        g_free(self);
    }
}

void
users_delete_at(users_t **pself)
{
    users_delete(*pself), *pself = NULL;
}

void
users_delete_cb(void *self)
{
    users_delete(self);
}

/* ========================================================================= *
 * USERS_ATTRIBUTES
 * ========================================================================= */

static control_t *
users_control(const users_t *self)
{
    return self->usr_control;
}

/* ========================================================================= *
 * USERS_USER
 * ========================================================================= */

uid_t
users_first_user(const users_t *self)
{
    return USERS_UID_MIN;
}

uid_t
users_last_user(const users_t *self)
{
    return USERS_UID_MAX;
}

bool
users_user_exists(users_t *self, uid_t uid)
{
    if( users_cancel_rescan(self) )
        users_scan_now(self);

    return g_hash_table_contains(self->usr_current, GINT_TO_POINTER(uid));
}

bool
users_user_is_guest(const users_t *self, uid_t uid)
{
    (void)self; // unused
    return uid == USERS_UID_GUEST;
}

/* ========================================================================= *
 * USERS_NOTIFY
 * ========================================================================= */

static void
users_notify_changed(users_t *self)
{
    if( self->usr_initialized ) {
        log_info("USERS NOTIFY");
        control_on_users_changed(users_control(self));
    }
}

/* ========================================================================= *
 * USERS_SCAN
 * ========================================================================= */

static bool
users_scan_now(users_t *self)
{
    users_cancel_rescan(self);
    log_info("USERS RESCAN: executing");

    bool changed = false;
    struct passwd *pw;

    GHashTable *scanned = g_hash_table_new (g_direct_hash, g_direct_equal);

    setpwent();
    while( (pw = getpwent()) ) {
        if( (pw->pw_uid >= USERS_UID_MIN && pw->pw_uid <= USERS_UID_MAX) ||
            pw->pw_uid == USERS_UID_GUEST ) {
            g_hash_table_add(scanned, GINT_TO_POINTER(pw->pw_uid));
        }
    }
    endpwent();

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, scanned);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        if( !g_hash_table_contains(self->usr_current, key) ) {
            log_info("UID(%u) added", GPOINTER_TO_UINT(key));
            changed = true;
        }
    }
    g_hash_table_iter_init(&iter, self->usr_current);
    while( g_hash_table_iter_next(&iter, &key, &value) ) {
        if( !g_hash_table_contains(scanned, key) ) {
            log_info("UID(%u) removed", GPOINTER_TO_UINT(key));
            changed = true;
        }
    }

    g_hash_table_unref(self->usr_current),
        self->usr_current = scanned;

    return changed;
}

static gboolean
users_rescan_cb(gpointer aptr)
{
    users_t *self = aptr;

    self->usr_rescan_id = 0;

    log_info("USERS RESCAN: triggered");
    if( users_scan_now(self) )
        users_notify_changed(self);

    return G_SOURCE_REMOVE;
}

static void
users_rescan_later(users_t *self)
{
    if( self->usr_rescan_id )
        g_source_remove(self->usr_rescan_id);
    else
        log_info("USERS RESCAN: scheduled");
    self->usr_rescan_id = g_timeout_add(2500, users_rescan_cb, self);
}

static bool
users_cancel_rescan(users_t *self)
{
    bool canceled = false;
    if( self->usr_rescan_id ) {
        log_info("USERS RESCAN: canceled");
        g_source_remove(self->usr_rescan_id),
            self->usr_rescan_id = 0;
        canceled = true;
    }
    return canceled;
}

/* ========================================================================= *
 * USERS_MONITOR
 * ========================================================================= */

static void
users_start_monitor(users_t *self)
{
    users_stop_monitor(self);

    GFileMonitorFlags flags = G_FILE_MONITOR_WATCH_MOVES;
    GFile *file = g_file_new_for_path(USERS_DIRECTORY);
    if( file ) {
        if( (self->usr_monitor_obj = g_file_monitor_directory(file, flags, 0, 0)) ) {
            g_signal_connect(self->usr_monitor_obj, "changed",
                             G_CALLBACK(users_monitor_cb), self);
            log_info("USERS MONITOR: started");
        }
        g_object_unref(file);
    }
}

static void
users_stop_monitor(users_t *self)
{
    if( self->usr_monitor_obj ) {
        log_info("USERS MONITOR: stopped");
        g_object_unref(self->usr_monitor_obj),
            self->usr_monitor_obj = NULL;
    }
}

static bool
users_monitor_p(const gchar *path)
{
    path = path_basename(path);
    return path && fnmatch(USERS_PATTERN, path, FNM_PATHNAME) == 0;
}

static void
users_monitor_cb(GFileMonitor      *mon,
                 GFile             *file1,
                 GFile             *file2,
                 GFileMonitorEvent  event,
                 gpointer           aptr)
{
    users_t *self = aptr;

    const char *path1 = file1 ? g_file_peek_path(file1) : NULL;
    const char *path2 = file2 ? g_file_peek_path(file2) : NULL;
    bool trigger = (users_monitor_p(path1) ||
                    users_monitor_p(path2));

    if( trigger ) {
        log_info("USERS MONITOR: triggers @ %s %s", path1, path2);
        users_rescan_later(self);
    }
}
