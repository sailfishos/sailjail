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

#ifndef  UTIL_H_
# define UTIL_H_

# include <stdbool.h>
# include <glib.h>

G_BEGIN_DECLS

/* ========================================================================= *
 * Constants
 * ========================================================================= */

/* Expected: We get these from Makefile, or spec during rpm build */
# ifndef  VERSION
#  define VERSION                       "0.0.0"
# endif

# ifndef  BINDIR
#  define BINDIR                        "/usr/bin"
# endif

# ifndef  SYSCONFDIR
#  define SYSCONFDIR                    "/etc"
# endif

# ifndef  SHAREDSTATEDIR
#  define SHAREDSTATEDIR                "/var/lib"
# endif

# ifndef  DATADIR
#  define DATADIR                       "/usr/share"
# endif

# define RUNTIME_DATADIR                "/run/user"

# define HOME_LOCALDIR                  "/.local"
# define HOME_DATADIR                   HOME_LOCALDIR "/share"

/* Config from: *.conf */
# define CONFIG_DIRECTORY               SYSCONFDIR "/sailjail/config"
# define CONFIG_EXTENSION               ".conf"
# define CONFIG_PATTERN                 "[0-9][0-9]*" CONFIG_EXTENSION

/* Users from: passwd */
# define USERS_DIRECTORY                SYSCONFDIR
# define USERS_EXTENSION                ""
# define USERS_PATTERN                  "passwd" USERS_EXTENSION

/* Permissions from: *.permission */
# define PERMISSIONS_DIRECTORY          SYSCONFDIR "/sailjail/permissions"
# define PERMISSIONS_EXTENSION          ".permission"
# define PERMISSIONS_PATTERN            "[A-Z]*" PERMISSIONS_EXTENSION
# define PROFILES_EXTENSION             ".profile"

/* Applications from: *.desktop */
# define APPLICATIONS_DIRECTORY         DATADIR "/applications"
# define APPLICATIONS_EXTENSION         ".desktop"
# define APPLICATIONS_PATTERN           "*" APPLICATIONS_EXTENSION

/* Sailjail overrides from: *.desktop */
# define SAILJAIL_APP_DIRECTORY         SYSCONFDIR "/sailjail/applications"

/* Writable DBus services */
# define DBUS_DIRECTORY                 "/dbus-1"
# define DBUS_SERVICES_DIRECTORY        DBUS_DIRECTORY "/services"
# define DBUS_SERVICES_EXTENSION        ".service"
# define DBUS_SERVICES_PATTERN          "*" DBUS_SERVICES_EXTENSION

/* Settings from: *.settings */
# define SETTINGS_DIRECTORY             SHAREDSTATEDIR "/sailjail/settings"
# define SETTINGS_EXTENSION             ".settings"
# define SETTINGS_PATTERN               "*" SETTINGS_EXTENSION

/* Booster binaries in: /usr/libexec/mapplauncherd/ */
# define BOOSTER_DIRECTORY              "/usr/libexec/mapplauncherd"
# define BOOSTER_EXTENSION              ""
# define BOOSTER_PATTERN                "booster-*"

/* Standard desktop properties */
# define DESKTOP_SECTION                "Desktop Entry"
# define DESKTOP_KEY_NAME               "Name"
# define DESKTOP_KEY_TYPE               "Type"
# define DESKTOP_KEY_ICON               "Icon"
# define DESKTOP_KEY_EXEC               "Exec"
# define DESKTOP_KEY_NO_DISPLAY         "NoDisplay"

/* Maemo desktop properties */
# define MAEMO_SECTION                  "Desktop Entry"
# define MAEMO_KEY_SERVICE              "X-Maemo-Service"
# define MAEMO_KEY_OBJECT               "X-Maemo-Object-Path"
# define MAEMO_KEY_METHOD               "X-Maemo-Method"

/* Sailjail desktop properties */
# define SAILJAIL_SECTION_PRIMARY       "X-Sailjail"
# define SAILJAIL_SECTION_SECONDARY     "Sailjail"
# define SAILJAIL_KEY_ORGANIZATION_NAME "OrganizationName"
# define SAILJAIL_KEY_APPLICATION_NAME  "ApplicationName"
# define SAILJAIL_KEY_DATA_DIRECTORY    "DataDirectory"
# define SAILJAIL_KEY_PERMISSIONS       "Permissions"
# define SAILJAIL_KEY_SANDBOXING        "Sandboxing"
# define SAILJAIL_KEY_EXEC_DBUS         "ExecDBus"

# define NEMO_KEY_APPLICATION_TYPE      "X-Nemo-Application-Type"
# define NEMO_KEY_SINGLE_INSTANCE       "X-Nemo-Single-Instance"

# define MAEMO_KEY_FIXED_ARGS           "X-Maemo-Fixed-Args"

# define OSSO_KEY_SERVICE               "X-Osso-Service"

/* DBus service file properties */
# define DBUS_SERVICE_SECTION            "D-BUS Service"
# define DBUS_KEY_NAME                   "Name"
# define DBUS_KEY_EXEC                   "Exec"
# define DBUS_KEY_APPLICATION            "X-Sailjail-Application"

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct stringset_t    stringset_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

char *strip(char *str);

/* ------------------------------------------------------------------------- *
 * PATH
 * ------------------------------------------------------------------------- */

const gchar *path_basename             (const gchar *path);
gchar       *path_construct            (const gchar *dir, const gchar *file, const gchar *ext);
const gchar *path_extension            (const gchar *path);
gchar       *path_dirname              (const gchar *path);
gchar       *path_to_desktop_name      (const gchar *path);
gchar       *path_from_desktop_name    (const gchar *stem);
gchar       *alt_path_from_desktop_name(const gchar *stem);
gchar       *path_to_permission_name   (const gchar *path);
gchar       *path_from_permission_name (const gchar *stem);
gchar       *path_from_profile_name    (const gchar *stem);

/* ------------------------------------------------------------------------- *
 * GUTIL
 * ------------------------------------------------------------------------- */

guint gutil_add_watch(int fd, GIOCondition cnd, GIOFunc cb, gpointer aptr);

/* ------------------------------------------------------------------------- *
 * CHANGE
 * ------------------------------------------------------------------------- */

bool change_uid         (uid_t *where, uid_t val);
bool change_boolean     (bool *where, bool val);
bool change_string      (gchar **pstr, const gchar *val);
bool change_string_steal(gchar **pstr, gchar *val);
bool change_timer       (guint *where, guint val);

/* ------------------------------------------------------------------------- *
 * KEYFILE
 * ------------------------------------------------------------------------- */

bool         keyfile_save         (GKeyFile *file, const gchar *path);
bool         keyfile_load         (GKeyFile *file, const gchar *path);
bool         keyfile_merge        (GKeyFile *file, const gchar *path);
bool         keyfile_get_boolean  (GKeyFile *file, const gchar *sec, const gchar *key, bool def);
gint         keyfile_get_integer  (GKeyFile *file, const gchar *sec, const gchar *key, gint def);
gchar       *keyfile_get_string   (GKeyFile *file, const gchar *sec, const gchar *key, const gchar *def);
stringset_t *keyfile_get_stringset(GKeyFile *file, const gchar *sec, const gchar *key);
void         keyfile_set_boolean  (GKeyFile *file, const gchar *sec, const gchar *key, bool val);
void         keyfile_set_integer  (GKeyFile *file, const gchar *sec, const gchar *key, gint val);
void         keyfile_set_string   (GKeyFile *file, const gchar *sec, const gchar *key, const gchar *val);
void         keyfile_set_stringset(GKeyFile *file, const gchar *sec, const gchar *key, const stringset_t *val);

G_END_DECLS

#endif /* UTIL_H_ */
