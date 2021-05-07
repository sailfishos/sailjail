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

#ifndef  APPLICATIONS_H_
# define APPLICATIONS_H_

# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct config_t       config_t;
typedef struct stringset_t    stringset_t;
typedef struct applications_t applications_t;
typedef struct appinfo_t      appinfo_t;
typedef struct control_t      control_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * APPLICATIONS
 * ------------------------------------------------------------------------- */

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

G_END_DECLS

#endif /* APPLICATIONS_H_ */
