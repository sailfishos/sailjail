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
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
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

#include "test_common.h"

#include <gutil_log.h>

void
test_init(
    TestOpt* opt,
    int argc,
    char* argv[])
{
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    const char* base = g_basename(argv[0]);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    int i;

    memset(opt, 0, sizeof(*opt));
    for (i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-d") || !strcmp(arg, "--debug")) {
            opt->flags |= TEST_FLAG_DEBUG;
        } else if (!strcmp(arg, "-v")) {
            GTestConfig* config = (GTestConfig*)g_test_config_vars;
            config->test_verbose = TRUE;
        } else {
            GWARN("Unsupported command line option %s", arg);
        }
    }

    /* Setup logging */
    gutil_log_default.name = base;
    gutil_log_default.level = g_test_verbose() ?
        GLOG_LEVEL_VERBOSE : GLOG_LEVEL_NONE;
    gutil_log_timestamp = FALSE;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
