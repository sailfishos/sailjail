/*
 * Copyright (c) 2020 Open Mobile Platform LLC.
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

#include "jail_creds.h"

#include <gutil_intarray.h>
#include <gutil_ints.h>
#include <gutil_misc.h>

#include <sys/types.h>
#include <grp.h>

static
GUtilInts*
jail_parse_ids(
    const char* data,
    gsize len,
    const char* prefix)
{
    /* Find the prefix */
    const char* ptr = g_strstr_len(data, len, prefix);
    GUtilInts* result = NULL;

    if (ptr) {
        gboolean ok = TRUE;
        GString* buf = g_string_sized_new(7);
        GUtilIntArray* parsed = gutil_int_array_new();
        const char* end;

        /* Skip the prefix */
        ptr += strlen(prefix);

        /* Find the end of line */
        len -= ptr - data;
        end = g_strstr_len(ptr, len, "\n");
        if (!end) end = ptr + len;

        /* Skip the spaces */
        while (ptr < end && g_ascii_isspace(*ptr)) ptr++;

        /* Read the numbers */
        while (ptr < end) {
            int val;

            /* Read and parse the number (only expecting positives) */
            g_string_set_size(buf, 0);
            while (ptr < end && g_ascii_isdigit(*ptr)) {
                g_string_append_c(buf, *ptr++);
            }
            /* g_ascii_isdigit() has filtered out minus too, val must be > 0 */
            if (gutil_parse_int(buf->str, 0, &val)) {
                /* Store the value, skip the spaces and continue */
                gutil_int_array_append(parsed, val);
                while (ptr < end && g_ascii_isspace(*ptr)) ptr++;
                continue;
            }
            /* Bummer */
            ok = FALSE;
            break;
        }

        /* Expecting at least one number */
        if (ok && parsed->count > 0) {
            result = gutil_int_array_free_to_ints(parsed);
        } else {
            gutil_int_array_free(parsed, TRUE);
        }
        g_string_free(buf, TRUE);
    }
    return result;
}

static
JailCreds*
jail_creds_from_data(
    const char* data,
    gsize len,
    GError** error)
{
    JailCreds* creds = NULL;
    guint nuids, ngids, ngroups;
    GUtilInts* puids = jail_parse_ids(data, len, "\nUid:");
    GUtilInts* pgids = jail_parse_ids(data, len, "\nGid:");
    GUtilInts* pgroups = jail_parse_ids(data, len, "\nGroups:");
    const int* uids = gutil_ints_get_data(puids, &nuids);
    const int* gids = gutil_ints_get_data(pgids, &ngids);
    const int* groups = gutil_ints_get_data(pgroups, &ngroups);

    if (nuids == 4 && ngids == 4 && ngroups > 0) {
        gid_t* cred_groups;
        guint i;

        creds = g_malloc(sizeof(*creds) + ngroups * sizeof(gid_t));
        cred_groups = (gid_t*)(creds + 1);
        for (i = 0; i < ngroups; i++) {
            cred_groups[i] = groups[i];
        }

        creds->ruid = uids[0];
        creds->euid = uids[1];
        creds->suid = uids[2];
        creds->fsuid = uids[3];
        creds->rgid = gids[0];
        creds->egid = gids[1];
        creds->sgid = gids[2];
        creds->fsgid = gids[3];
        creds->groups = cred_groups;
        creds->ngroups = ngroups;
    } else if (error) {
        g_propagate_error(error, g_error_new(G_FILE_ERROR,
            G_FILE_ERROR_INVAL, "Invalid credentials data"));
    }
    gutil_ints_unref(puids);
    gutil_ints_unref(pgids);
    gutil_ints_unref(pgroups);
    return creds;
}

JailCreds*
jail_creds_from_file(
    const char* fname,
    GError** error)
{
    JailCreds* creds = NULL;
    char* data;
    gsize len;

    if (g_file_get_contents(fname, &data, &len, error)) {
        creds = jail_creds_from_data(data, len, error);
        g_free(data);
    }
    return creds;
}

JailCreds*
jail_creds_for_pid(
    pid_t pid,
    GError** error)
{
    char* fname = g_strdup_printf("/proc/%u/status", (guint)pid);
    JailCreds* creds = jail_creds_from_file(fname, error);

    g_free(fname);
    return creds;
}

void
jail_creds_free(
    JailCreds* creds)
{
    g_free(creds);
}

gboolean
jail_creds_check_egid(
    pid_t pid,
    const char* group)
{
    gboolean match = FALSE;

    if (group) {
        JailCreds* creds = jail_creds_for_pid(pid, NULL);

        if (creds) {
            struct group* gr = getgrgid(creds->egid);

            match = (gr && !g_strcmp0(gr->gr_name, group));
            jail_creds_free(creds);
        }
    }
    return match;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
