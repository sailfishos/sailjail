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

#include "later.h"

#include "logging.h"

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

later_t         *later_create    (const char *label, gint priority, guint delay, later_func_t func, gpointer aptr);
void             later_delete    (later_t *self);
void             later_delete_at (later_t **pself);
void             later_schedule  (later_t *self);
void             later_cancel    (later_t *self);
void             later_execute   (later_t *self);
static gboolean  later_trigger_cb(gpointer aptr);

/* ========================================================================= *
 * LATER
 * ========================================================================= */

struct later_t
{
    const char  *label;
    gint         priority;
    guint        delay;
    later_func_t func;
    gpointer     aptr;
    guint        id;
};

later_t *
later_create(const char *label, gint priority, guint delay,
             later_func_t func, gpointer aptr)
{
    later_t *self = g_malloc0(sizeof *self);

    self->label    = label;
    self->priority = priority;
    self->delay    = delay;
    self->func     = func;
    self->aptr     = aptr;
    self->id       = 0;

    return self;
}

void
later_delete(later_t *self)
{
    if( self ) {
        later_cancel(self);
        g_free(self);
    }
}

void
later_delete_at(later_t **pself)
{
    later_delete(*pself), *pself = NULL;
}

void
later_schedule(later_t *self)
{
    if( !self->id ) {
        log_debug("later(%s) scheduled", self->label);
        if( self->delay )
            self->id = g_timeout_add_full(self->priority,
                                          self->delay,
                                          later_trigger_cb,
                                          self, NULL);
        else
            self->id = g_idle_add_full(self->priority,
                                       later_trigger_cb,
                                       self, NULL);
    }
}

void
later_cancel(later_t *self)
{
    if( self->id ) {
        log_debug("later(%s) cancelled", self->label);
        g_source_remove(self->id),
            self->id = 0;
    }
}

void
later_execute(later_t *self)
{
    later_cancel(self);
    log_debug("later(%s) execute", self->label);
    self->func(self->aptr);
}

static gboolean
later_trigger_cb(gpointer aptr)
{
    later_t *self = aptr;
    log_debug("later(%s) triggered", self->label);
    self->id = 0;
    later_execute(self);
    return G_SOURCE_REMOVE;
}
