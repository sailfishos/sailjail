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

#include "stringset.h"

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static void   stringset_ctor       (stringset_t *self);
static void   stringset_dtor       (stringset_t *self);
stringset_t  *stringset_create     (void);
void          stringset_delete     (stringset_t *self);
void          stringset_delete_at  (stringset_t **pself);
void          stringset_delete_cb  (void *self);
guint         stringset_size       (const stringset_t *self);
bool          stringset_empty      (const stringset_t *self);
const GList  *stringset_list       (const stringset_t *self);
bool          stringset_has_item   (const stringset_t *self, const gchar *item);
bool          stringset_add_item   (stringset_t *self, const gchar *item);
bool          stringset_remove_item(stringset_t *self, const gchar *item);
bool          stringset_clear      (stringset_t *self);
GVariant     *stringset_to_variant (const stringset_t *self);
gchar        *stringset_to_string  (const stringset_t *self);
gchar       **stringset_to_strv    (const stringset_t *self);
stringset_t  *stringset_from_strv  (char **vector);
stringset_t  *stringset_copy       (const stringset_t *self);
void          stringset_swap       (stringset_t *self, stringset_t *that);
stringset_t  *stringset_filter_out (const stringset_t *self, const stringset_t *mask);
stringset_t  *stringset_filter_in  (const stringset_t *self, const stringset_t *mask);
bool          stringset_equal      (const stringset_t *self, const stringset_t *that);
bool          stringset_extend     (stringset_t *self, const stringset_t *that);
bool          stringset_assign     (stringset_t *self, const stringset_t *that);

/* ========================================================================= *
 * STRINGSET
 * ========================================================================= */

struct stringset_t
{
    /* GQueue keeps the set in deterministic order */
    GQueue      sst_list;

    /* GHashTable is used for lookups and memory management */
    GHashTable *sst_hash;
};

static void
stringset_ctor(stringset_t *self)
{
    //log_debug("stringset() create");
    g_queue_init(&self->sst_list);
    self->sst_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           g_free, NULL);
}

static void
stringset_dtor(stringset_t *self)
{
    //log_debug("stringset() delete");

    stringset_clear(self);

    if( self->sst_hash ) {
        g_hash_table_unref(self->sst_hash),
            self->sst_hash = NULL;
    }
}

stringset_t *
stringset_create(void)
{
    stringset_t *self = g_malloc0(sizeof *self);
    stringset_ctor(self);
    return self;
}

void
stringset_delete(stringset_t *self)
{
    if( self ) {
        stringset_dtor(self);
        g_free(self);
    }
}

void
stringset_delete_at(stringset_t **pself)
{
    stringset_delete(*pself), *pself = NULL;
}

void
stringset_delete_cb(void *self)
{
    stringset_delete(self);
}

guint
stringset_size(const stringset_t *self)
{
    return self->sst_list.length;
}

bool
stringset_empty(const stringset_t *self)
{
    return stringset_size(self) == 0;
}

const GList *
stringset_list(const stringset_t *self)
{
    return self->sst_list.head;
}

bool
stringset_has_item(const stringset_t *self, const gchar *item)
{
    return g_hash_table_lookup(self->sst_hash, item) != 0;
}

bool
stringset_add_item(stringset_t *self, const gchar *item)
{
    bool added_item = false;
    const gchar *have = g_hash_table_lookup(self->sst_hash, item);
    if( !have ) {
        gchar *add = g_strdup(item);
        g_hash_table_add(self->sst_hash, add);
        g_queue_push_tail(&self->sst_list, add);
        added_item = true;
    }
    return added_item;
}

bool
stringset_remove_item(stringset_t *self, const gchar *item)
{
    bool removed_item = false;

    const gchar *have = g_hash_table_lookup(self->sst_hash, item);
    if( have ) {
        g_queue_remove(&self->sst_list, have);
        g_hash_table_remove(self->sst_hash, have);
        removed_item = true;
    }
    return removed_item;
}

bool
stringset_clear(stringset_t *self)
{
    bool removed_items = false;

    if( !stringset_empty(self) ) {
        g_queue_clear(&self->sst_list);
        g_hash_table_remove_all(self->sst_hash);
        removed_items = true;
    }
    return removed_items;
}

GVariant *
stringset_to_variant(const stringset_t *self)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    for( const GList *iter = stringset_list(self); iter; iter = iter->next ) {
        gchar *item = iter->data;
        g_variant_builder_add(builder, "s", item);
    }
    GVariant *variant = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return variant;
}

gchar *
stringset_to_string(const stringset_t *self)
{
    GString *string = g_string_new(NULL);
    const gchar *sep = 0;
    for( const GList *iter = stringset_list(self); iter; iter = iter->next ) {
        gchar *item = iter->data;
        if( sep )
            g_string_append(string, sep);
        else
            sep = ",";
        g_string_append(string, item);
    }
    return g_string_free(string, false);
}

gchar **
stringset_to_strv(const stringset_t *self)
{
    guint   count  = stringset_size(self) + 1;
    gchar **vector = g_malloc0_n(count, sizeof *vector);
    guint index = 0;
    for( const GList *iter = stringset_list(self); iter; iter = iter->next )
        vector[index++] = g_strdup(iter->data);
    vector[index++] = NULL;
    return vector;
}

stringset_t *
stringset_from_strv(char **vector)
{
    stringset_t *self = stringset_create();
    if( vector ) {
        for( size_t i = 0; vector[i]; ++i ) {
            stringset_add_item(self, vector[i]);
        }
    }
    return self;
}

stringset_t *
stringset_copy(const stringset_t *self)
{
    stringset_t *copied = stringset_create();
    for( const GList *iter = stringset_list(self); iter; iter = iter->next ) {
        gchar *item = iter->data;
        stringset_add_item(copied, item);
    }
    return copied;
}

void
stringset_swap(stringset_t *self, stringset_t *that)
{
    stringset_t temp = *self; *self = *that; *that = temp;
}

stringset_t *
stringset_filter_out(const stringset_t *self, const stringset_t *mask)
{
    stringset_t *filtered = stringset_create();
    for( const GList *iter = stringset_list(self); iter; iter = iter->next ) {
        gchar *item = iter->data;
        if( !stringset_has_item(mask, item) )
            stringset_add_item(filtered, item);
    }
    return filtered;
}

stringset_t *
stringset_filter_in(const stringset_t *self, const stringset_t *mask)
{
    stringset_t *filtered = stringset_create();
    for( const GList *iter = stringset_list(self); iter; iter = iter->next ) {
        gchar *item = iter->data;
        if( stringset_has_item(mask, item) )
            stringset_add_item(filtered, item);
    }
    return filtered;
}

bool
stringset_equal(const stringset_t *self, const stringset_t *that)
{
    bool equal = false;
    if( stringset_size(self) == stringset_size(that) ) {
        const GList *iter1 = stringset_list(self);
        const GList *iter2 = stringset_list(that);
        while( iter1 && iter2 ) {
            if( g_strcmp0(iter1->data, iter2->data) ) {
                break;
            }
            iter1 = iter1->next;
            iter2 = iter2->next;
        }
        equal = !iter1 && !iter2;
    }
    return equal;
}

bool
stringset_extend(stringset_t *self, const stringset_t *that)
{
    bool changed = false;
    for( const GList *iter = stringset_list(that); iter; iter = iter->next ) {
        if( stringset_add_item(self, iter->data) )
            changed = true;
    }
    return changed;
}

bool
stringset_assign(stringset_t *self, const stringset_t *that)
{
    bool changed = false;
    if( !stringset_equal(self, that) ) {
        stringset_clear(self);
        stringset_extend(self, that);
        changed = true;
    }
    return changed;
}
