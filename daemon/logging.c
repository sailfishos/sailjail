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

#include "logging.h"

#include "util.h"

#include <stdio.h>
#include <errno.h>

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * LOG
 * ------------------------------------------------------------------------- */

void               log_set_target     (log_target_t target);
static int         log_normalize_level(int lev);
static const char *log_tag            (int lev);
int                log_get_level      (void);
void               log_set_level      (int lev);
bool               log_p              (int lev);
void               log_emit_real      (const char *file, int line, const char *func, int lev, const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/* ========================================================================= *
 * LOG
 * ========================================================================= */

static int log_level = LOG_WARNING;

static log_target_t log_target = LOG_TO_STDERR;

void
log_set_target(log_target_t target)
{
    log_target = target;
}

static int
log_normalize_level(int lev)
{
    return (lev < LOG_CRIT) ? LOG_CRIT : (lev > LOG_TRACE) ? LOG_TRACE : lev;
}

static const char *
log_tag(int lev)
{
    static const char * const tag[] = {
        [LOG_EMERG]   = "X: ",
        [LOG_ALERT]   = "A: ",
        [LOG_CRIT]    = "C: ",
        [LOG_ERR]     = "E: ",
        [LOG_WARNING] = "W: ",
        [LOG_NOTICE]  = "N: ",
        [LOG_INFO]    = "I: ",
        [LOG_DEBUG]   = "D: ",
        [LOG_TRACE]   = "T: ",
    };
    return tag[lev];
}

int
log_get_level(void)
{
    return log_level;
}

void
log_set_level(int lev)
{
    log_level = log_normalize_level(lev);
}

bool
log_p(int lev)
{
    return log_normalize_level(lev) <= log_level;
}

void
log_emit_real(const char *file, int line, const char *func, int lev, const char *fmt, ...)
{
    (void)file;
    (void)line;
    (void)func;

    lev = log_normalize_level(lev);
    if( lev <= log_level ) {
        int saved = errno;

        va_list va;
        va_start(va, fmt);
        if( log_target == LOG_TO_SYSLOG ) {
            vsyslog(lev, fmt, va);
        }
        else {
            char *msg = 0;
            if( vasprintf(&msg, fmt, va) < 0 )
                msg = 0;

#if LOGGING_SHOW_FUNCTION
            fprintf(stderr, "%s:%d: %s(): %s%s\n",
                    file, line, func, log_tag(lev), msg ? strip(msg) : fmt);
#else
            fprintf(stderr, "%s%s\n", log_tag(lev), msg ? strip(msg) : fmt);
#endif
            fflush(stderr);
            free(msg);
        }
        va_end(va);

        errno = saved;
    }
}
