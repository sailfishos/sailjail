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

#define _GNU_SOURCE /* setresuid() and setresgid() */

#include "jail_free.h"
#include "jail_creds.h"

#include <glib-unix.h>

#include <grp.h>
#include <unistd.h>
#include <sys/fsuid.h>

void
jail_free(
    int argc,
    char* argv[],
    const JailCreds* creds,
    GError** error)
{
    const gboolean root = (!geteuid() || !getegid());

    setfsuid(creds->fsuid);
    setfsgid(creds->fsgid);
    if (setgroups(creds->ngroups, creds->groups) && root) {
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "setgroups error: %s", strerror(errno)));
    } else if (setresgid(creds->rgid, creds->egid, creds->sgid) && root) {
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "setresgid(%u,%u,%u) error: %s", (guint)creds->rgid,
            (guint)creds->egid, (guint)creds->sgid, strerror(errno)));
    } else if (setresuid(creds->ruid, creds->euid, creds->suid) && root) {
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "setresuid(%u,%u,%u) error: %s", (guint)creds->ruid,
            (guint)creds->euid, (guint)creds->suid, strerror(errno)));
    } else {
        fflush(NULL);
        execvp(argv[0], (char**) argv);
        g_propagate_error(error, g_error_new(G_UNIX_ERROR, errno,
            "exec(%s) error: %s", argv[0], strerror(errno)));
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
