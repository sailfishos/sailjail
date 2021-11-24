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

#ifndef  PERMISSIONS_H_
# define PERMISSIONS_H_

# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Constants
 * ========================================================================= */

# define PERMISSION_BASE          "Base"
# define PERMISSION_PRIVILEGED    "Privileged"
# define PERMISSION_COMPATIBILITY "Compatibility"

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct control_t     control_t;
typedef struct config_t      config_t;
typedef struct stringset_t   stringset_t;
typedef struct permissions_t permissions_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * PERMISSIONS
 * ------------------------------------------------------------------------- */

permissions_t *permissions_create   (control_t *control);
void           permissions_delete   (permissions_t *self);
void           permissions_delete_at(permissions_t **pself);
void           permissions_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * PERMISSIONS_AVAILABLE
 * ------------------------------------------------------------------------- */

const stringset_t *permissions_available(permissions_t *self);

G_END_DECLS

#endif /* PERMISSIONS_H_ */
