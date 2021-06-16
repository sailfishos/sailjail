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

#ifndef  STRINGSET_H_
# define STRINGSET_H_

# include <stdbool.h>
# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct stringset_t stringset_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

stringset_t  *stringset_create        (void);
void          stringset_delete        (stringset_t *self);
void          stringset_delete_at     (stringset_t **pself);
void          stringset_delete_cb     (void *self);
guint         stringset_size          (const stringset_t *self);
bool          stringset_empty         (const stringset_t *self);
const GList  *stringset_list          (const stringset_t *self);
bool          stringset_has_item      (const stringset_t *self, const gchar *item);
bool          stringset_add_item      (stringset_t *self, const gchar *item);
bool          stringset_add_item_steal(stringset_t *self, gchar *item);
bool          stringset_add_item_fmt  (stringset_t *self, const char *fmt, ...);
bool          stringset_remove_item   (stringset_t *self, const gchar *item);
bool          stringset_clear         (stringset_t *self);
GVariant     *stringset_to_variant    (const stringset_t *self);
gchar        *stringset_to_string     (const stringset_t *self);
gchar       **stringset_to_strv       (const stringset_t *self);
stringset_t  *stringset_from_strv     (char **vector);
stringset_t  *stringset_copy          (const stringset_t *self);
void          stringset_swap          (stringset_t *self, stringset_t *that);
stringset_t  *stringset_filter_out    (const stringset_t *self, const stringset_t *mask);
stringset_t  *stringset_filter_in     (const stringset_t *self, const stringset_t *mask);
bool          stringset_equal         (const stringset_t *self, const stringset_t *that);
bool          stringset_extend        (stringset_t *self, const stringset_t *that);
bool          stringset_assign        (stringset_t *self, const stringset_t *that);

G_END_DECLS

#endif /* STRINGSET_H_ */
