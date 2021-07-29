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

# include "appservices.h"

# include "appinfo.h"
# include "applications.h"
# include "control.h"
# include "session.h"
# include "stringset.h"
# include "util.h"
# include "logging.h"

# include <glob.h>
# include <linux/limits.h>
# include <pwd.h>
# include <stdio.h>
# include <sys/stat.h>
# include <unistd.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct serviceinfo_t
{
    char *name;
    char *exec;
} serviceinfo_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * APPSERVICES
 * ------------------------------------------------------------------------- */

static void    appservices_ctor                (appservices_t *self, control_t *control);
static void    appservices_dtor                (appservices_t *self);
appservices_t *appservices_create              (control_t *control);
void           appservices_delete              (appservices_t *self);
void           appservices_delete_at           (appservices_t **pself);
void           appservices_delete_cb           (void *self);
void           appservices_rethink             (appservices_t *self);
void           appservices_application_changed (appservices_t *self, const char *appname, appinfo_t *appinfo);
void           appservices_application_added   (appservices_t *self, const char *appname, appinfo_t *appinfo);
void           appservices_application_removed (appservices_t *self, const char *appname);

static void  appservices_update_user          (appservices_t *self);
static bool  appservices_ensure_run_directory(appservices_t *self, const char *directory);
static char *appservices_service_filename     (appservices_t *self, const char *service);
static void  appservices_write_service_file   (appservices_t *self, const char *appname, appinfo_t *appinfo);
static void  appservices_remove_service_file  (appservices_t *self, const char *appname);


/* ------------------------------------------------------------------------- *
 * SERVICEINFO
 * ------------------------------------------------------------------------- */

static serviceinfo_t *serviceinfo_create(const char *name, const char *exec);
static void           serviceinfo_delete(serviceinfo_t *self);
static void           serviceinfo_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * APPSERVICES
 * ------------------------------------------------------------------------- */

struct appservices_t
{
    control_t       *asv_control;
    GHashTable      *asv_service_lut;
    char            *asv_run_dir;
    uid_t            asv_uid;
    gid_t            asv_gid;
};

static void
appservices_ctor(appservices_t *self, control_t *control)
{
    log_info("appservices() create");

    self->asv_control = control;
    self->asv_uid = SESSION_UID_UNDEFINED;
    self->asv_gid = SESSION_GID_UNDEFINED;

    self->asv_service_lut = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  serviceinfo_delete_cb);
    appservices_rethink(self);
}

static void
appservices_dtor(appservices_t *self)
{
    log_info("appservices() delete");

    g_free(self->asv_run_dir);
    self->asv_run_dir = NULL;

    if( self->asv_service_lut ) {
        g_hash_table_unref(self->asv_service_lut),
            self->asv_service_lut = NULL;
    }
}

appservices_t *
appservices_create(control_t *control)
{
    appservices_t *self = g_malloc0(sizeof *self);
    appservices_ctor(self, control);
    return self;
}

void
appservices_delete(appservices_t *self)
{
    if( self ) {
        appservices_dtor(self);
        g_free(self);
    }
}

void
appservices_delete_at(appservices_t **pself)
{
    appservices_delete(*pself), *pself = NULL;
}

void
appservices_delete_cb(void *self)
{
    appservices_delete(self);
}

void
appservices_rethink(appservices_t *self)
{
    appservices_update_user(self);

    if( !self->asv_run_dir ) {
        return;
    }

    /* Repopulate the services table with the services from the current users run directory */
    g_hash_table_remove_all(self->asv_service_lut);

    stringset_t *appnames_to_remove = stringset_create();
    char *pattern = g_strdup_printf("%s" DBUS_SERVICES_DIRECTORY
                                    "/" DBUS_SERVICES_PATTERN,
                                    self->asv_run_dir);
    glob_t gl  = {};

    if( glob(pattern, 0, 0, &gl) == 0 ) {
        for( int i = 0; i < gl.gl_pathc; ++i ) {
            GKeyFile *keyfile = g_key_file_new();
            if( keyfile_load(keyfile, gl.gl_pathv[i]) ) {
                char *name = g_key_file_get_string(keyfile,
                                                   DBUS_SERVICE_SECTION,
                                                   DBUS_KEY_NAME,
                                                   NULL);
                char *exec = g_key_file_get_string(keyfile,
                                                   DBUS_SERVICE_SECTION,
                                                   DBUS_KEY_EXEC,
                                                   NULL);
                char *appname = g_key_file_get_string(keyfile,
                                                      DBUS_SERVICE_SECTION,
                                                      DBUS_KEY_APPLICATION,
                                                      NULL);
                if( name && exec && appname ) {
                    stringset_add_item(appnames_to_remove, appname);
                    g_hash_table_replace(self->asv_service_lut, appname,
                                         serviceinfo_create(name, exec));
                }
                else {
                    g_free(appname);
                }

                g_free(name);
                g_free(exec);
            }
            g_key_file_unref(keyfile);
        }
    }

    globfree(&gl);
    g_free(pattern);

    /* Add the current set of applications */
    applications_t *applications = control_applications(self->asv_control);

    gchar **appnames = stringset_to_strv(applications_available(applications));

    for( guint i = 0; appnames[i]; ++i ) {
        char *appname = appnames[i];

        appinfo_t *appinfo = applications_appinfo(applications, appname);

        if( appinfo && appinfo_dbus_auto_start(appinfo) ) {
            stringset_remove_item(appnames_to_remove, appname);

            appservices_write_service_file(self, appname, appinfo);
        }
    }

    g_strfreev(appnames);

    /* Remove any service files that weren't matched to an application. */
    appnames = stringset_to_strv(appnames_to_remove);
    for( guint i = 0; appnames[i]; ++i ) {
        appservices_remove_service_file(self, appnames[i]);
    }
    g_strfreev(appnames);
    stringset_delete(appnames_to_remove);
}

void
appservices_application_changed(appservices_t *self, const char *appname, appinfo_t *appinfo)
{
    if( appinfo_dbus_auto_start(appinfo) ) {
        appservices_write_service_file(self, appname, appinfo);
    }
    else {
        appservices_remove_service_file(self, appname);
    }
}

void
appservices_application_added(appservices_t *self, const char *appname, appinfo_t *appinfo)
{
    if( appinfo_dbus_auto_start(appinfo) ) {
        appservices_write_service_file(self, appname, appinfo);
    }
}

void
appservices_application_removed(appservices_t *self, const char *appname)
{
    appservices_remove_service_file(self, appname);
}

void
appservices_update_user(appservices_t *self)
{
    uid_t uid = control_current_user(self->asv_control);

    if( self->asv_uid != uid ) {
        self->asv_uid = uid;
        self->asv_gid = SESSION_GID_UNDEFINED;

        g_free(self->asv_run_dir);
        self->asv_run_dir = NULL;

        /* Get the gid and run directory of the current user */
        if( self->asv_uid != SESSION_UID_UNDEFINED ) {
            long initlen = sysconf(_SC_GETPW_R_SIZE_MAX);
            size_t len = 131072; // ARG_MAX;
            if( initlen != -1 ) {
                len = (size_t) initlen;
            }

            struct passwd result;
            struct passwd *resultp;
            char *buffer = malloc(len);

            if( !getpwuid_r(self->asv_uid, &result, buffer, len, &resultp) ) {
                self->asv_gid = result.pw_gid;
                self->asv_run_dir = g_strdup_printf(RUNTIME_DATADIR "/%lld",
                                                    (long long)uid);
            }

            free(buffer);
        }

        /* Verify the dbus services directory exists under run and create it if necessary
           Note the default directory ownership is incorrect in this instance and needs to
           be corrected for each subdirectory created */
        if( self->asv_run_dir && (
                    !appservices_ensure_run_directory(self, DBUS_DIRECTORY) ||
                    !appservices_ensure_run_directory(self, DBUS_SERVICES_DIRECTORY))) {
            g_free(self->asv_run_dir);
            self->asv_run_dir = NULL;
        }
    }
}

bool
appservices_ensure_run_directory(appservices_t *self, const char *directory)
{
    char *path = g_strdup_printf("%s%s", self->asv_run_dir, directory);

    bool exists = !access(path, F_OK);

    if( !exists ) {
        if( mkdir(path, 0700) ) {
            log_warning("appservices() could not create directory %s: %m", path);
        }
        else if( chown(path, self->asv_uid, self->asv_gid) ) {
            log_warning("appservices() could not change ownership of directory %s: %m", path);

            unlink(path);
        }
        else {
            exists = true;
        }
    }

    g_free(path);

    return exists;
}

char *
appservices_service_filename (appservices_t *self, const char *service)
{
    return g_strdup_printf("%s" DBUS_SERVICES_DIRECTORY
                           "/%s" DBUS_SERVICES_EXTENSION,
                           self->asv_run_dir, service);
}

void
appservices_write_service_file(appservices_t *self, const char *appname, appinfo_t *appinfo)
{
    if( !self->asv_run_dir ) {
        return;
    }

    bool changed = false;
    char *service_name = g_strdup_printf("%s.%s",
                                         appinfo_get_organization_name(appinfo),
                                         appinfo_get_application_name(appinfo));
    const char *exec = appinfo_get_exec_dbus(appinfo);

    /* If the service name for application has changed remove any existing service file */
    const serviceinfo_t *current_service = g_hash_table_lookup(self->asv_service_lut, appname);
    if( current_service && strcmp(service_name, current_service->name) ) {
        char *service_filename = appservices_service_filename(self, current_service->name);
        log_info("appservices(%s) remove service file %s", appname, service_filename);
        unlink(service_filename);
        g_free(service_filename);

        changed = true;
    }
    /* If the existing name and executable are unchanged do nothing */
    else if( current_service && !strcmp(exec, current_service->exec)) {
        g_free(service_name);

        return;
    }

    /* Populate a new service file */
    GKeyFile *keyfile = g_key_file_new();

    keyfile_set_string(keyfile, DBUS_SERVICE_SECTION, DBUS_KEY_NAME, service_name);
    keyfile_set_string(keyfile, DBUS_SERVICE_SECTION, DBUS_KEY_EXEC, exec);
    keyfile_set_string(keyfile, DBUS_SERVICE_SECTION, DBUS_KEY_APPLICATION, appname);

    char *service_filename = appservices_service_filename(self, service_name);

    log_info("appservices(%s) write service file %s", appname, service_filename);

    char *tmp_service_filename = g_strdup_printf("%s.tmp", service_filename);

    /* Write the service file to disk making sure to fixup the ownership and permissions
       as this is not running as the target user and a umask is set to protect settings files */
    if( keyfile_save(keyfile, tmp_service_filename) ) {
        if( chown(tmp_service_filename, self->asv_uid, self->asv_gid) ||
                chmod(tmp_service_filename, 0644) ) {
            log_warning("appservices() could not change ownership or permissions of file %s: %m", tmp_service_filename);

            unlink(tmp_service_filename);
        }
        else {
            rename(tmp_service_filename, service_filename);

            changed = true;
        }
    }

    g_key_file_unref(keyfile);

    g_hash_table_replace(self->asv_service_lut, g_strdup(appname),
                         serviceinfo_create(service_name, exec));

    g_free(service_name);
    g_free(service_filename);
    g_free(tmp_service_filename);

    if( changed ) {
        control_on_appservices_change(self->asv_control);
    }
}

void
appservices_remove_service_file(appservices_t *self, const char *appname)
{
    if( !self->asv_run_dir ) {
        return;
    }

    const serviceinfo_t *service = g_hash_table_lookup(self->asv_service_lut, appname);

    if( service ) {
        char *service_filename = appservices_service_filename(self, service->name);

        log_info("appservices(%s) remove service file %s", appname, service_filename);

        unlink(service_filename);
        g_free(service_filename);

        g_hash_table_remove(self->asv_service_lut, appname);

        control_on_appservices_change(self->asv_control);
    }
}

/* ------------------------------------------------------------------------- *
 * SERVICEINFO
 * ------------------------------------------------------------------------- */

serviceinfo_t *
serviceinfo_create(const char *name, const char *exec)
{
    serviceinfo_t *self = g_malloc0(sizeof *self);
    self->name = g_strdup(name);
    self->exec = g_strdup(exec);
    return self;
}

void
serviceinfo_delete(serviceinfo_t *self)
{
    g_free(self->name);
    g_free(self->exec);
    g_free(self);
}

void
serviceinfo_delete_cb(void *self)
{
    serviceinfo_delete(self);
}
