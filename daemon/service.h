/*
 * Copyright (c) 2021 Open Mobile Platform LLC.
 * Copyright (c) 2021 Jolla Ltd.
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

#ifndef  SERVICE_H_
# define SERVICE_H_

# include <stdbool.h>
# include <glib.h>

G_BEGIN_DECLS;

/* ========================================================================= *
 * DBUS
 * ========================================================================= */

#define DBUS_SERVICE                           "org.freedesktop.DBus"
#define DBUS_PATH                              "/"
#define DBUS_INTERFACE                         "org.freedesktop.DBus"
#define DBUS_METHOD_RELOAD_CONFIG              "ReloadConfig"
#define DBUS_METHOD_NAME_HAS_OWNER             "NameHasOwner"

# define WINDOWPROMPT_SERVICE                  "com.jolla.windowprompt"
# define WINDOWPROMPT_OBJECT                   "/com/jolla/windowprompt"
# define WINDOWPROMPT_INTERFACE                "com.jolla.windowprompt"
# define WINDOWPROMPT_METHOD_PROMPT            "newPermissionPrompt"
# define WINDOWPROMPT_PROMPT_INTERFACE         "com.jolla.windowprompt.Prompt"
# define WINDOWPROMPT_PROMPT_METHOD_WAIT       "wait"
# define WINDOWPROMPT_PROMPT_METHOD_CANCEL     "cancel"

# define PERMISSIONMGR_BUS                     G_BUS_TYPE_SYSTEM

# define PERMISSIONMGR_SERVICE                 "org.sailfishos.sailjaild1"
# define PERMISSIONMGR_INTERFACE               "org.sailfishos.sailjaild1"
# define PERMISSIONMGR_OBJECT                  "/org/sailfishos/sailjaild1"
# define PERMISSIONMGR_METHOD_PROMPT           "PromptLaunchPermissions"
# define PERMISSIONMGR_METHOD_QUERY            "QueryLaunchPermissions"
# define PERMISSIONMGR_METHOD_GET_APPLICATIONS "GetApplications"
# define PERMISSIONMGR_METHOD_GET_APPINFO      "GetAppInfo"
# define PERMISSIONMGR_METHOD_GET_LICENSE      "GetLicenseAgreed"
# define PERMISSIONMGR_METHOD_SET_LICENSE      "SetLicenseAgreed"
# define PERMISSIONMGR_METHOD_GET_LAUNCHABLE   "GetLaunchAllowed"
# define PERMISSIONMGR_METHOD_SET_LAUNCHABLE   "SetLaunchAllowed"
# define PERMISSIONMGR_METHOD_GET_GRANTED      "GetGrantedPermissions"
# define PERMISSIONMGR_METHOD_SET_GRANTED      "SetGrantedPermissions"
# define PERMISSIONMGR_METHOD_SET_X_GRANTED    "SetGrantedXPermissions"
# define PERMISSIONMGR_SIGNAL_APP_ADDED        "ApplicationAdded"
# define PERMISSIONMGR_SIGNAL_APP_CHANGED      "ApplicationChanged"
# define PERMISSIONMGR_SIGNAL_APP_REMOVED      "ApplicationRemoved"

/* Message templates used for error reporting */
# define SERVICE_MESSAGE_INVALID_APPLICATION   "Invalid application name: %s"
# define SERVICE_MESSAGE_INVALID_USER          "Invalid user id: %u"
# define SERVICE_MESSAGE_INVALID_PERMISSIONS   "Invalid permissions list"
# define SERVICE_MESSAGE_DENIED_PERMANENTLY    "Denied permanently"
# define SERVICE_MESSAGE_NOT_ALLOWED           "Not allowed"
# define SERVICE_MESSAGE_RESTRICTED_METHOD     "%s is not allowed to access %s"
# define SERVICE_MESSAGE_GUEST_NOT_LOGGED_IN   "Guest user is not logged in"
# define SERVICE_MESSAGE_DISMISSED             "Dismissed"
# define SERVICE_MESSAGE_DISCONNECTED          "Disconnected"

# define PERMISSIONMGR_NOTIFY_DELAY            0 // [ms]

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct config_t       config_t;
typedef struct users_t        users_t;
typedef struct session_t      session_t;
typedef struct permissions_t  permissions_t;
typedef struct applications_t applications_t;
typedef struct control_t      control_t;
typedef struct service_t      service_t;
typedef struct appinfo_t      appinfo_t;
typedef struct stringset_t    stringset_t;
typedef struct prompter_t     prompter_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SERVICE
 * ------------------------------------------------------------------------- */

service_t *service_create              (control_t *control);
void       service_delete              (service_t *self);
void       service_delete_at           (service_t **pself);
void       service_delete_cb           (void *self);
void       service_applications_changed(service_t *self, const stringset_t *changed);

/* ------------------------------------------------------------------------- *
 * SERVICE_PERMISSIONS
 * ------------------------------------------------------------------------- */

stringset_t *service_filter_permissions(const service_t *self, const stringset_t *permissions);

/* ------------------------------------------------------------------------- *
 * SERVICE_ATTRIBUTES
 * ------------------------------------------------------------------------- */

control_t  *service_control (const service_t *self);
prompter_t *service_prompter(service_t *self);

/* ------------------------------------------------------------------------- *
 * SERVICE_NAMEOWNER
 * ------------------------------------------------------------------------- */

bool service_is_nameowner(const service_t *self);

G_END_DECLS

#endif /* SERVICE_H_ */
