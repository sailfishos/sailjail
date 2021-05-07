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

#ifndef LOGGING_H_
# define LOGGING_H_

# include <stdbool.h>
# include <syslog.h>

# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Macros
 * ========================================================================= */

# ifndef LOG_DEBUG
#  error foobar
# else
# define LOG_TRACE 8
#endif

# if 01
#  define LOGGING_SHOW_FUNCTION 1
#  define LOGGING_LEVEL LOG_DEBUG
# else
#  define LOGGING_SHOW_FUNCTION 0
#  define LOGGING_LEVEL LOG_INFO
# endif

# define log_emit(LEV, FMT, ARGS...) do {\
    if( log_p(LEV) ) {\
        log_emit_real(__FILE__, __LINE__, __func__, LEV, FMT, ##ARGS);\
    }\
} while(0)

# define log_crit(    FMT, ARGS...) log_emit(LOG_CRIT,    FMT, ##ARGS)
# define log_err(     FMT, ARGS...) log_emit(LOG_ERR,     FMT, ##ARGS)
# define log_warning( FMT, ARGS...) log_emit(LOG_WARNING, FMT, ##ARGS)

# if LOGGING_LEVEL >= LOG_NOTICE
#  define log_notice( FMT, ARGS...) log_emit(LOG_NOTICE,  FMT, ##ARGS)
# else
#  define log_notice( FMT, ARGS...) do{}while(0)
# endif

# if LOGGING_LEVEL >= LOG_INFO
#  define log_info(   FMT, ARGS...) log_emit(LOG_INFO,    FMT, ##ARGS)
# else
#  define log_info(   FMT, ARGS...) do{}while(0)
# endif

# if LOGGING_LEVEL >= LOG_DEBUG
#  define log_debug(  FMT, ARGS...) log_emit(LOG_DEBUG,   FMT, ##ARGS)
# else
#  define log_debug(  FMT, ARGS...) do{}while(0)
# endif

# if LOGGING_LEVEL >= LOG_TRACE
#  define log_trace(  FMT, ARGS...) log_emit(LOG_TRACE,   FMT, ##ARGS)
# else
#  define log_trace(  FMT, ARGS...) do{}while(0)
# endif

# if 01
#  define HERE do{}while(0)
# elif LOGGING_SHOW_FUNCTION
#  define HERE log_debug("...")
# else
#  define HERE log_debug("%s:%d: %s(): ...", __FILE__, __LINE__, __func__)
# endif

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef enum {
    LOG_TO_STDERR,
    LOG_TO_SYSLOG,
} log_target_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * LOG
 * ------------------------------------------------------------------------- */

void log_set_target(log_target_t target);
int  log_get_level (void);
void log_set_level (int lev);
bool log_p         (int lev);
void log_emit_real (const char *file, int line, const char *func, int lev, const char *fmt, ...) __attribute__((format(printf, 5, 6)));

G_END_DECLS

#endif /* LOGGING_H_ */
