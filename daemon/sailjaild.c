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

#include "config.h"
#include "control.h"
#include "mainloop.h"
#include "logging.h"
#include "util.h"

#include <getopt.h>

#include <glib/gstdio.h>

#include <systemd/sd-daemon.h>

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SAILJAILD
 * ------------------------------------------------------------------------- */

static void sailjaild_filesystem_setup(void);
static int  sailjaild_main            (int argc, char **argv);

/* ------------------------------------------------------------------------- *
 * MAIN
 * ------------------------------------------------------------------------- */

int main(int argc, char **argv);

/* ========================================================================= *
 * SAILJAILD
 * ========================================================================= */

static const struct option long_options[] = {
    {"help",         no_argument,       NULL, 'h'},
    {"verbose",      no_argument,       NULL, 'v'},
    {"quiet",        no_argument,       NULL, 'q'},
    {"version",      no_argument,       NULL, 'V'},
    {"systemd",      no_argument,       NULL, 'S'},
    {"force-stderr", no_argument,       NULL, 'T'},
    {"force-syslog", no_argument,       NULL, 's'},
    {0, 0, 0, 0}
};
static const char short_options[] = "hvqVSTs";

static void
sailjaild_filesystem_setup(void)
{
    /* Note: Settings are stored in encrypted home partition. As
     *       home might not be available/mounted at rpm install time,
     *       data directories must be created during runtime.
     */

    /* Create directories using standard mode bits */
    if( !g_file_test(SETTINGS_DIRECTORY, G_FILE_TEST_IS_DIR) ) {
        if( g_mkdir_with_parents(SETTINGS_DIRECTORY, 0755) == -1 ) {
            log_err("%s: could not create directory: %m", SETTINGS_DIRECTORY);

            /* Limp onwards instead of possibly breaking everything */
            log_warning("permissions can't be stored persistently");
        }
    }

    /* Make settings directory itself accessible by root only */
    if( g_chmod(SETTINGS_DIRECTORY, 0750) == -1 ) {
        log_err("%s: could not update permissions: %m",
                SETTINGS_DIRECTORY);
    }

    /* Settings files must not be world readable.
     * As sailjaild writes only to settings files,
     * we can just set umask here to accomplish it.
     */
    umask(0027);
}

static int
sailjaild_main(int argc, char **argv)
{
    int       exit_code = EXIT_FAILURE;
    config_t  *config   = config_create();
    control_t *control  = NULL;
    bool       systemd  = false;

    /* Handle options */
    for( ;; ) {
        int opt = getopt_long(argc, argv, short_options, long_options, 0);

        if( opt == -1 )
            break;

        switch( opt ) {
        case 'h':
            printf("usage: TBD\n");
            exit_code = EXIT_SUCCESS;
            goto EXIT;
        case 'v':
            log_set_level(log_get_level() + 1);
            break;
        case 'q':
            log_set_level(log_get_level() - 1);
            break;
        case 'T':
            log_set_target(LOG_TO_STDERR);
            break;
        case 's':
            log_set_target(LOG_TO_SYSLOG);
            break;
        case 'V':
            printf("%s\n", VERSION);
            exit_code = EXIT_SUCCESS;
            goto EXIT;
        case 'S':
            systemd = true;
            log_set_target(LOG_TO_SYSLOG);
            break;
        case '?':
            fprintf(stderr, "(use --help for instructions)\n");
            goto EXIT;
        }
    }

    sailjaild_filesystem_setup();

    control = control_create(config);

    if( systemd )
        sd_notify(0, "READY=1");

    exit_code = app_run();

EXIT:
    control_delete_at(&control);
    config_delete_at(&config);

    log_debug("exit %d", exit_code);
    return exit_code;
}

/* ========================================================================= *
 * MAIN
 * ========================================================================= */

int
main(int argc, char **argv)
{
    return sailjaild_main(argc, argv);
}
