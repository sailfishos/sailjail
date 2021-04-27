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

#include "session.h"

#include "control.h"
#include "logging.h"
#include "util.h"

#include <systemd/sd-login.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SESSION
 * ------------------------------------------------------------------------- */

session_t *session_create   (control_t *control);
void       session_delete   (session_t *self);
void       session_delete_at(session_t **pself);
void       session_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * SESSION_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static control_t *session_control(const session_t *self);

/* ------------------------------------------------------------------------- *
 * SESSION_USER
 * ------------------------------------------------------------------------- */

uid_t session_current_user(const session_t *self);

/* ------------------------------------------------------------------------- *
 * SESSION_NOTIFY
 * ------------------------------------------------------------------------- */

static void session_notify_changed(const session_t *self);

/* ------------------------------------------------------------------------- *
 * SESSION_MONITOR
 * ------------------------------------------------------------------------- */

static gboolean session_monitor_cb    (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static void     session_start_monitor (session_t *self);
static void     session_stop_monitor  (session_t *self);
static void     session_update_monitor(session_t *self);

/* ------------------------------------------------------------------------- *
 * SESSION_SEAT
 * ------------------------------------------------------------------------- */

static uid_t session_get_seat0_uid(void);

/* ========================================================================= *
 * SESSION
 * ========================================================================= */

struct session_t
{
    bool               ssn_initialized;
    control_t         *ssn_control;

    /** Currently active user session */
    uid_t             ssn_active_uid;

    /** Systemd login monitoring object */
    sd_login_monitor *ssn_monitor_obj;

    /** I/O watch id for ssn_monitor_obj */
    guint             ssn_monitor_id;
};

static inline void
session_ctor(session_t *self, control_t *control)
{
    log_info("session() create");
    self->ssn_initialized = false;
    self->ssn_control     = control;
    self->ssn_active_uid  = SESSION_UID_UNDEFINED;
    self->ssn_monitor_obj = 0;
    self->ssn_monitor_id  = 0;

    /* Get initial state */
    session_start_monitor(self);
    session_update_monitor(self);

    /* Enable notifications */
    self->ssn_initialized = true;
}

static inline void
session_dtor(session_t *self)
{
    log_info("session() delete");
    self->ssn_initialized = false;
    session_stop_monitor(self);
}

session_t *
session_create(control_t *control)
{
    session_t *self = g_malloc0(sizeof *self);
    session_ctor(self, control);
    return self;
}

void
session_delete(session_t *self)
{
    if( self ) {
        session_dtor(self);
        g_free(self);
    }
}

void
session_delete_at(session_t **pself)
{
    session_delete(*pself), *pself = NULL;
}

void
session_delete_cb(void *self)
{
    session_delete(self);
}

static control_t *
session_control(const session_t *self)
{
    return self->ssn_control;
}

/* ========================================================================= *
 * SESSION_USER
 * ========================================================================= */

uid_t
session_current_user(const session_t *self)
{
    return self->ssn_active_uid;
}

static void
session_notify_changed(const session_t *self)
{
    if( self->ssn_initialized ) {
        log_info("SESSION MONITOR: notify");
        control_on_session_changed(session_control(self));
    }
}

/* ========================================================================= *
 * SESSION_MONITOR
 * ========================================================================= */

/** Handle systemd login monitoring wakeups
 */
static gboolean
session_monitor_cb(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    session_t *self = aptr;
    gboolean result = G_SOURCE_REMOVE;

    if( !self->ssn_monitor_id || !self->ssn_monitor_obj )
        goto EXIT;

    if( cnd & ~G_IO_IN )
        goto EXIT;

    session_update_monitor(self);

    if( sd_login_monitor_flush(self->ssn_monitor_obj) >= 0 )
        result = G_SOURCE_CONTINUE;

EXIT:
    if( result == G_SOURCE_REMOVE && self->ssn_monitor_id ) {
        log_crit("SESSION MONITOR: disabled");
        self->ssn_monitor_id = 0;
        session_stop_monitor(self);
    }

    return result;
}

/** Start user session monitoring
 */
static void
session_start_monitor(session_t *self)
{
    bool ack = false;

    session_stop_monitor(self);

    if( sd_login_monitor_new("session", &self->ssn_monitor_obj) < 0 )
        goto EXIT;

    int fd = sd_login_monitor_get_fd(self->ssn_monitor_obj);
    if( fd < 0 )
        goto EXIT;

    self->ssn_monitor_id = gutil_add_watch(fd, G_IO_IN,
                                           session_monitor_cb, self);
    if( !self->ssn_monitor_id )
        goto EXIT;

    ack = true;

    log_info("SESSION MONITOR: started");

EXIT:
    if( !ack )
        session_stop_monitor(self);

    return;
}

/** Stop user session monitoring
 */
static void
session_stop_monitor(session_t *self)
{
    if( self->ssn_monitor_id ) {
        log_info("SESSION MONITOR: stopped");

        g_source_remove(self->ssn_monitor_id),
            self->ssn_monitor_id = 0;
    }

    if( self->ssn_monitor_obj ) {
        sd_login_monitor_unref(self->ssn_monitor_obj),
            self->ssn_monitor_obj = 0;
    }
}

static void
session_update_monitor(session_t *self)
{
    uid_t uid = session_get_seat0_uid();
    if( self->ssn_active_uid != uid ) {
        log_info("SESSION MONITOR: uid: %d -> %d",
                 (int)self->ssn_active_uid, (int)uid);
        self->ssn_active_uid = uid;
        session_notify_changed(self);
    }
}

/* ========================================================================= *
 * SESSION_SEAT
 * ========================================================================= */

/** Find uid for user active session at seat0
 */
static uid_t
session_get_seat0_uid(void)
{
    uid_t  active_uid = SESSION_UID_UNDEFINED;
    char **sessions   = 0;

    int rc;

    if( (rc = sd_get_sessions(&sessions)) < 0 ) {
        log_warning("sd_get_sessions: %s", strerror(-rc));
        goto EXIT;
    }

    if( rc < 1 || !sessions )
        goto EXIT;

    for( size_t i = 0; active_uid == SESSION_UID_UNDEFINED && sessions[i]; ++i ) {
        uid_t  uid    = SESSION_UID_UNDEFINED;
        char  *seat   = 0;
        char  *state  = 0;

        if( (rc = sd_session_get_uid(sessions[i], &uid)) < 0 ) {
            log_warning("sd_session_get_uid(%s): %s",
                        sessions[i], strerror(-rc));
        }
        else if( (rc = sd_session_get_state(sessions[i], &state)) < 0 ) {
            log_warning("sd_session_get_state(%s): %s",
                        sessions[i], strerror(-rc));
        }
        else if( (rc = sd_session_get_seat(sessions[i], &seat)) < 0 ) {
            /* NB: It is normal to have sessions without a seat, but
             *     sd_session_get_seat() reports error on such cases
             *     and we do not want that to cause logging noise.
             */
        }
        else if( state && seat && !strcmp(seat, "seat0") ) {
            if( !strcmp(state, "active") || !strcmp(state, "online") )
                active_uid = uid;
        }

        free(state);
        free(seat);
    }

EXIT:
    if( sessions ) {
        for( size_t i = 0; sessions[i]; ++i )
            free(sessions[i]);
        free(sessions);
    }

    return active_uid;
}
