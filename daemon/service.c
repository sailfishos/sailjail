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

#include "service.h"

#include "logging.h"
#include "mainloop.h"
#include "prompter.h"
#include "control.h"
#include "appinfo.h"
#include "applications.h"
#include "permissions.h"
#include "session.h"
#include "stringset.h"
#include "settings.h"

#include <dbusaccess_peer.h>
#include <dbusaccess_policy.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct prompter_t prompter_t;

typedef struct service_t  service_t;

/* ========================================================================= *
 * Policies
 * ========================================================================= */

#define SERVICE_PRIVILEGED_POLICY "1;user(root)|group(privileged) = allow"
#define SERVICE_MDM_POLICY "1;user(sailfish-mdm)|group(sailfish-mdm) = allow"

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SERVICE
 * ------------------------------------------------------------------------- */

static void  service_ctor                (service_t *self, control_t *control);
static void  service_dtor                (service_t *self);
service_t   *service_create              (control_t *control);
void         service_delete              (service_t *self);
void         service_delete_at           (service_t **pself);
void         service_delete_cb           (void *self);
void         service_applications_changed(service_t *self, const stringset_t *changed);

/* ------------------------------------------------------------------------- *
 * SERVICE_PERMISSIONS
 * ------------------------------------------------------------------------- */

stringset_t *service_filter_permissions(const service_t *self, const stringset_t *permissions);

/* ------------------------------------------------------------------------- *
 * SERVICE_ATTRIBUTES
 * ------------------------------------------------------------------------- */

control_t             *service_control     (const service_t *self);
static applications_t *service_applications(const service_t *self);
static appsettings_t  *service_appsettings (const service_t *self, uid_t uid, const char *app);
static appinfo_t      *service_appinfo     (const service_t *self, const char *appname);
prompter_t            *service_prompter    (service_t *self);

/* ------------------------------------------------------------------------- *
 * SERVICE_CONNECTION
 * ------------------------------------------------------------------------- */

static GDBusConnection *service_get_connection(const service_t *self);
static bool             service_has_connection(const service_t *self);
static void             service_set_connection(service_t *self, GDBusConnection *connection);

/* ------------------------------------------------------------------------- *
 * SERVICE_NOTIFY
 * ------------------------------------------------------------------------- */

static void     service_schedule_notify(service_t *self);
static void     service_cancel_notify  (service_t *self);
static gboolean service_notify_cb      (gpointer aptr);
static void     service_notify         (service_t *self);

/* ------------------------------------------------------------------------- *
 * SERVICE_NAMEOWNER
 * ------------------------------------------------------------------------- */

static void service_set_nameowner(service_t *self, bool nameowner);
bool        service_is_nameowner (const service_t *self);

/* ------------------------------------------------------------------------- *
 * SERVICE_ACCESS_CONTROL
 * ------------------------------------------------------------------------- */

static bool service_may_administrate(const gchar *sender);
static bool service_is_privileged   (const gchar *sender);
static bool service_is_mdm          (const gchar *sender);
static bool service_test_policy     (const gchar *sender, DAPolicy *policy);

#define G_BUS_TO_DA_BUS(bus) ((bus) == G_BUS_TYPE_SYSTEM ? DA_BUS_SYSTEM : DA_BUS_SESSION)

/* ------------------------------------------------------------------------- *
 * SERVICE_DBUS
 * ------------------------------------------------------------------------- */

static GDBusInterfaceInfo *service_dbus_interface_info  (const gchar *interface);
static bool                service_dbus_name_p          (const gchar *name);
static void                service_dbus_bus_acquired_cb (GDBusConnection *connection, const gchar *name, gpointer user_data);
static void                service_dbus_name_acquired_cb(GDBusConnection *connection, const gchar *name, gpointer user_data);
static void                service_dbus_name_lost_cb    (GDBusConnection *connection, const gchar *name, gpointer user_data);
static void                service_dbus_call_cb         (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data);
static void                service_dbus_emit_signal     (service_t *self, const char *member, const char *value);

/* ========================================================================= *
 * SERVICE
 * ========================================================================= */

struct service_t
{
    // uplink
    control_t       *srv_control;

    // data
    GDBusConnection *srv_dbus_connection;   // service_dbus_bus_acquired_cb()
    bool             srv_dbus_nameowner;    // service_dbus_name_acquired_cb()
    guint            srv_dbus_object_id;    // g_dbus_connection_register_object()
    guint            srv_notify_id;         // service_schedule_notify()
    stringset_t     *srv_dbus_applications; // signaled applications
    stringset_t     *srv_permission_filter; // masking: Base,Privileged

    // downlink
    prompter_t      *srv_prompter;

    // dbus service
    guint            srv_dbus_name_own_id; // g_bus_own_name()
};

static void
service_ctor(service_t *self, control_t *control)
{
    log_info("service() create");
    // uplink
    self->srv_control = control;

    // data
    self->srv_dbus_connection   = NULL;
    self->srv_dbus_nameowner    = false;
    self->srv_dbus_object_id    = 0;
    self->srv_notify_id         = 0;
    self->srv_dbus_applications = stringset_create();

    /* Some permissions must be omitted from prompting */
    self->srv_permission_filter = stringset_create();
    stringset_add_item(self->srv_permission_filter, PERMISSION_BASE);
    stringset_add_item(self->srv_permission_filter, PERMISSION_PRIVILEGED);

    // downlink
    self->srv_prompter         = prompter_create(self);

    // dbus service
    self->srv_dbus_name_own_id = g_bus_own_name(PERMISSIONMGR_BUS,
                                                PERMISSIONMGR_SERVICE,
                                                G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
                                                service_dbus_bus_acquired_cb,
                                                service_dbus_name_acquired_cb,
                                                service_dbus_name_lost_cb,
                                                self,
                                                NULL);
}

static void
service_dtor(service_t *self)
{
    log_info("service() delete");

    // dbus service
    if( self->srv_dbus_name_own_id ) {
        g_bus_unown_name(self->srv_dbus_name_own_id),
            self->srv_dbus_name_own_id = 0;
    }

    // downlink
    prompter_delete_at(&self->srv_prompter);

    // connection ref
    service_set_connection(self, NULL);

    // data
    service_cancel_notify(self);
    stringset_delete_at(&self->srv_dbus_applications);
    stringset_delete_at(&self->srv_permission_filter);

}

service_t *
service_create(control_t *control)
{
    service_t *self = g_malloc0(sizeof *self);
    service_ctor(self, control);
    return self;
}

void
service_delete(service_t *self)
{
    if( self ) {
        service_dtor(self);
        g_free(self);
    }
}

void
service_delete_at(service_t **pself)
{
    service_delete(*pself), *pself = NULL;
}

void
service_delete_cb(void *self)
{
    service_delete(self);
}

/* ------------------------------------------------------------------------- *
 * SERVICE_PERMISSIONS
 * ------------------------------------------------------------------------- */

stringset_t *
service_filter_permissions(const service_t *self, const stringset_t *permissions)
{
    /* Mask out permissions that we do not want to show in prompt */
    stringset_t *filtered = stringset_filter_out(permissions,
                                                 self->srv_permission_filter);

    /* But masking must not lead into auto-allow Privileged */
    if( stringset_empty(filtered) ) {
        if( stringset_has_item(permissions, PERMISSION_PRIVILEGED) )
            stringset_add_item(filtered, PERMISSION_PRIVILEGED);
    }

    return filtered;
}

/* ------------------------------------------------------------------------- *
 * SERVICE_ATTRIBUTES
 * ------------------------------------------------------------------------- */

control_t *
service_control(const service_t *self)
{
    return self->srv_control;
}

static applications_t *
service_applications(const service_t *self)
{
    return control_applications(service_control(self));
}

static appsettings_t *
service_appsettings(const service_t *self, uid_t uid, const char *app)
{
    return control_appsettings(service_control(self), uid, app);
}

static appinfo_t *
service_appinfo(const service_t *self, const char *appname)
{
    return control_appinfo(service_control(self), appname);
}

prompter_t *
service_prompter(service_t *self)
{
    return self->srv_prompter;
}

#ifdef DEAD_CODE
static session_t *
service_session(const service_t *self)
{
    return control_session(service_control(self));
}

static const config_t *
service_config(const service_t *self)
{
    return control_config(service_control(self));
}

static users_t *
service_users(const service_t *self)
{
    return control_users(service_control(self));
}

static permissions_t *
service_permissions(const service_t *self)
{
    return control_permissions(service_control(self));
}

static uid_t
service_current_user(service_t *self)
{
    return session_current_user(service_session(self));
}
#endif

/* ------------------------------------------------------------------------- *
 * SERVICE_CONNECTION
 * ------------------------------------------------------------------------- */

static GDBusConnection *
service_get_connection(const service_t *self)
{
    return self->srv_dbus_connection;
}

static bool
service_has_connection(const service_t *self)
{
    return service_get_connection(self) != 0;
}

static void
service_set_connection(service_t *self, GDBusConnection *connection)
{
    static const GDBusInterfaceVTable interface_vtable =
    {
        .method_call = service_dbus_call_cb,
    };

    if( self->srv_dbus_connection != connection ) {
        log_debug("connection: %p -> %p", self->srv_dbus_connection, connection);

        service_set_nameowner(self, false);

        if( self->srv_dbus_object_id ) {
            log_debug("obj unregister: %u", self->srv_dbus_object_id);
            g_dbus_connection_unregister_object(self->srv_dbus_connection, self->srv_dbus_object_id),
                self->srv_dbus_object_id = 0;
        }

        if( self->srv_dbus_connection ) {
            g_object_unref(self->srv_dbus_connection),
                self->srv_dbus_connection = 0;
        }

        if( connection ) {
            self->srv_dbus_connection = g_object_ref(connection);

            self->srv_dbus_object_id =
                g_dbus_connection_register_object(self->srv_dbus_connection,
                                                  PERMISSIONMGR_OBJECT,
                                                  service_dbus_interface_info(PERMISSIONMGR_INTERFACE),
                                                  &interface_vtable,
                                                  self,
                                                  NULL,
                                                  NULL);
            log_debug("obj register: %u", self->srv_dbus_object_id);
        }
    }
}

/* ------------------------------------------------------------------------- *
 * SERVICE_NOTIFY
 * ------------------------------------------------------------------------- */

static void
service_schedule_notify(service_t *self)
{
#if PERMISSIONMGR_NOTIFY_DELAY <= 0
    if( !self->srv_notify_id ) {
        self->srv_notify_id = g_idle_add(service_notify_cb, self);
    }
#else
    if( self->srv_notify_id )
        g_source_remove(self->srv_notify_id);
    self->srv_notify_id = g_timeout_add(PERMISSIONMGR_NOTIFY_DELAY, service_notify_cb, self);
#endif
}

static void
service_cancel_notify(service_t *self)
{
    if( self->srv_notify_id ) {
        g_source_remove(self->srv_notify_id),
            self->srv_notify_id = 0;
    }
}

static gboolean
service_notify_cb(gpointer aptr)
{
    service_t *self = aptr;
    self->srv_notify_id = 0;
    service_notify(self);
    return G_SOURCE_REMOVE;
}

static void
service_notify(service_t *self)
{
    service_cancel_notify(self);

    if( service_is_nameowner(self) && service_has_connection(self) ) {
        // FIXME: do we want initial state broadcast?
    }
}

/* ------------------------------------------------------------------------- *
 * SERVICE_NAMEOWNER
 * ------------------------------------------------------------------------- */

static void
service_set_nameowner(service_t *self, bool nameowner)
{
    if( self->srv_dbus_nameowner != nameowner ) {
        log_info("nameowner: %d -> %d", self->srv_dbus_nameowner, nameowner);
        self->srv_dbus_nameowner = nameowner;

        if( self->srv_dbus_nameowner )
            service_schedule_notify(self);
    }
}

bool
service_is_nameowner(const service_t *self)
{
    return self->srv_dbus_nameowner;
}

/* ------------------------------------------------------------------------- *
 * SERVICE_ACCESS_CONTROL
 * ------------------------------------------------------------------------- */

static bool
service_may_administrate(const gchar *sender)
{
    return service_is_privileged(sender) || service_is_mdm(sender);
}

static bool
service_is_privileged(const gchar *sender)
{
    static DAPolicy *policy = NULL;
    if (!policy)
        policy = da_policy_new(SERVICE_PRIVILEGED_POLICY);
    return service_test_policy(sender, policy);
}

static bool
service_is_mdm(const gchar *sender)
{
    static DAPolicy *policy = NULL;
    if (!policy)
        policy = da_policy_new(SERVICE_MDM_POLICY);
    return service_test_policy(sender, policy);
}

static bool
service_test_policy(const gchar *sender, DAPolicy *policy)
{
    DA_ACCESS access = DA_ACCESS_DENY;
    DAPeer *peer     = da_peer_get(G_BUS_TO_DA_BUS(PERMISSIONMGR_BUS), sender);

    if (peer)
        access = da_policy_check(policy, &peer->cred, 0, NULL, DA_ACCESS_DENY);

    return access == DA_ACCESS_ALLOW;
}

/* ------------------------------------------------------------------------- *
 * SERVICE_DBUS
 * ------------------------------------------------------------------------- */

static const gchar introspect_xml[] =\
"<node>"
"  <interface name='" PERMISSIONMGR_INTERFACE "'>"
"    <method name='" PERMISSIONMGR_METHOD_GET_APPLICATIONS "'>"
"      <arg type='as' name='applications' direction='out'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_GET_APPINFO "'>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='a{sv}' name='appinfo' direction='out'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_GET_LICENSE "'>"
"      <arg type='u' name='uid' direction='in'/>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='i' name='agreed' direction='out'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_SET_LICENSE "'>"
"      <arg type='u' name='uid' direction='in'/>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='i' name='agreed' direction='in'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_GET_LAUNCHABLE "'>"
"      <arg type='u' name='uid' direction='in'/>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='i' name='allowed' direction='out'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_SET_LAUNCHABLE "'>"
"      <arg type='u' name='uid' direction='in'/>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='i' name='allowed' direction='in'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_GET_GRANTED "'>"
"      <arg type='u' name='uid' direction='in'/>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='as' name='permissions' direction='out'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_SET_GRANTED "'>"
"      <arg type='u' name='uid' direction='in'/>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='as' name='permissions' direction='in'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_PROMPT "'>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='as' name='granted' direction='out'/>"
"    </method>"

"    <method name='" PERMISSIONMGR_METHOD_QUERY "'>"
"      <arg type='s' name='application' direction='in'/>"
"      <arg type='as' name='granted' direction='out'/>"
"    </method>"

"    <signal name='" PERMISSIONMGR_SIGNAL_APP_ADDED "'>"
"      <arg type='s' name='application'/>"
"    </signal>"

"    <signal name='" PERMISSIONMGR_SIGNAL_APP_CHANGED "'>"
"      <arg type='s' name='application'/>"
"    </signal>"

"    <signal name='" PERMISSIONMGR_SIGNAL_APP_REMOVED "'>"
"      <arg type='s' name='application'/>"
"    </signal>"
"  </interface>"
"</node>";

static GDBusInterfaceInfo *
service_dbus_interface_info(const gchar *interface)
{
    static GDBusNodeInfo *introspect_data = NULL;
    if( !introspect_data )
        introspect_data = g_dbus_node_info_new_for_xml(introspect_xml, NULL);
    return g_dbus_node_info_lookup_interface(introspect_data, interface);
}

static bool
service_dbus_name_p(const gchar *name)
{
    return !g_strcmp0(name, PERMISSIONMGR_SERVICE);
}

static void
service_dbus_bus_acquired_cb(GDBusConnection *connection,
                             const gchar *name,
                             gpointer user_data)
{
    service_t *self = user_data;

    if( service_dbus_name_p(name) ) {
        log_notice("dbus connection acquired");
        service_set_connection(self, connection);
    }
}

static void
service_dbus_name_acquired_cb(GDBusConnection *connection,
                              const gchar *name,
                              gpointer user_data)
{
    service_t *self = user_data;

    if( service_dbus_name_p(name) ) {
        log_notice("dbus name acquired");
        service_set_nameowner(self, true);
    }
}

static void
service_dbus_name_lost_cb(GDBusConnection *connection,
                          const gchar *name,
                          gpointer user_data)
{
    service_t *self = user_data;
    if( !connection ) {
        log_err("dbus connect failed");
        app_quit();
    }
    else if( service_dbus_name_p(name) ) {
        log_err("dbus name lost");
        service_set_nameowner(self, false);
        app_quit();
    }
}

static void
service_dbus_call_cb(GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
    service_t *self = user_data;

    log_debug("from=%s object=%s method=%s.%s",
              sender, object_path, interface_name, method_name);

    auto void value_reply(GVariant *val) {
        log_debug("reply(%p)", val);
        g_dbus_method_invocation_return_value(invocation,
                                              val
                                              ? g_variant_new_tuple(&val, 1)
                                              : NULL);
    }

    auto void __attribute__((format(printf, 2, 3))) error_reply(int code, const gchar *fmt, ...) {
        va_list va;
        va_start(va, fmt);
        g_dbus_method_invocation_return_error_valist(invocation, G_DBUS_ERROR,
                                                     code, fmt, va);
        va_end(va);
    }

    if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_GET_APPLICATIONS) ) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        const stringset_t *apps = applications_available(service_applications(self));
        for( const GList *iter = stringset_list(apps); iter; iter = iter->next ) {
            const char *appname = iter->data;
            g_variant_builder_add(builder, "s", appname);
        }
        GVariant *variant = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
        value_reply(variant);
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_GET_APPINFO) ) {
        const gchar *app = NULL;
        g_variant_get(parameters, "(&s)", &app);
        appinfo_t *appinfo = service_appinfo(self, app);
        if( !appinfo ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
        }
        else {
            GVariant *variant = appinfo_to_variant(appinfo);
            value_reply(variant);
        }
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_GET_LICENSE) ) {
        guint32      uid = SESSION_UID_UNDEFINED;
        const gchar *app = NULL;
        g_variant_get(parameters, "(u&s)", &uid, &app);
        appsettings_t *appsettings = NULL;
        if( !control_valid_user(service_control(self), uid) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_USER, uid);
        }
        else if( !(appsettings = control_appsettings(service_control(self), uid, app)) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
        }
        else {
            app_agreed_t agreed = appsettings_get_agreed(appsettings);
            GVariant *variant = g_variant_new_int32(agreed);
            value_reply(variant);
        }
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_SET_LICENSE) ) {
        if( !service_may_administrate(sender) ) {
            error_reply(G_DBUS_ERROR_ACCESS_DENIED, SERVICE_MESSAGE_RESTRICTED_METHOD, sender,
                        method_name);
        } else {
            guint32      uid   = SESSION_UID_UNDEFINED;
            const gchar *app   = NULL;
            gint        agreed = APP_AGREED_UNSET;
            g_variant_get(parameters, "(u&si)", &uid, &app, &agreed);
            appsettings_t *appsettings = NULL;
            if( !control_valid_user(service_control(self), uid) ) {
                error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_USER, uid);
            }
            else if( !(appsettings = control_appsettings(service_control(self), uid, app)) ) {
                error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
            }
            else {
                appsettings_set_agreed(appsettings, agreed);
                value_reply(NULL);
            }
        }
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_GET_LAUNCHABLE) ) {
        guint32      uid = SESSION_UID_UNDEFINED;
        const gchar *app = NULL;
        g_variant_get(parameters, "(u&s)", &uid, &app);
        appsettings_t *appsettings = NULL;
        if( !control_valid_user(service_control(self), uid) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_USER, uid);
        }
        else if( !(appsettings = control_appsettings(service_control(self), uid, app)) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
        }
        else {
            app_allowed_t allowed = appsettings_get_allowed(appsettings);
            GVariant *variant = g_variant_new_int32(allowed);
            value_reply(variant);
        }
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_SET_LAUNCHABLE) ) {
        if( !service_may_administrate(sender) ) {
            error_reply(G_DBUS_ERROR_ACCESS_DENIED, SERVICE_MESSAGE_RESTRICTED_METHOD, sender,
                        method_name);
        } else {
            guint32      uid    = SESSION_UID_UNDEFINED;
            const gchar *app    = NULL;
            gint        allowed = APP_ALLOWED_UNSET;
            g_variant_get(parameters, "(u&si)", &uid, &app, &allowed);
            appsettings_t *appsettings = NULL;
            if( !control_valid_user(service_control(self), uid) ) {
                error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_USER, uid);
            }
            else if( !(appsettings = control_appsettings(service_control(self), uid, app)) ) {
                error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
            }
            else {
                appsettings_set_allowed(appsettings, allowed);
                value_reply(NULL);
            }
        }
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_GET_GRANTED) ) {
        guint32      uid = SESSION_UID_UNDEFINED;
        const gchar *app = NULL;
        g_variant_get(parameters, "(u&s)", &uid, &app);
        appsettings_t *appsettings = NULL;
        if( !control_valid_user(service_control(self), uid) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_USER, uid);
        }
        else if( !(appsettings = control_appsettings(service_control(self), uid, app)) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
        }
        else {
            const stringset_t *granted = appsettings_get_granted(appsettings);
            gchar **vector = stringset_to_strv(granted);
            GVariant *variant = g_variant_new_strv((const gchar * const *)vector, -1);
            value_reply(variant);
            g_strfreev(vector);
        }
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_SET_GRANTED) ) {
        if( !service_may_administrate(sender) ) {
            error_reply(G_DBUS_ERROR_ACCESS_DENIED, SERVICE_MESSAGE_RESTRICTED_METHOD, sender,
                        method_name);
        } else {
            guint32      uid = SESSION_UID_UNDEFINED;
            const gchar *app = NULL;
            gchar **vector   = NULL;
            g_variant_get(parameters, "(u&s^as)", &uid, &app, &vector);
            appsettings_t *appsettings = NULL;
            if( !control_valid_user(service_control(self), uid) ) {
                error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_USER, uid);
            }
            else if( !(appsettings = control_appsettings(service_control(self), uid, app)) ) {
                error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
            }
            else if( !vector ) {
                error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_PERMISSIONS);
            }
            else {
                stringset_t *granted = stringset_from_strv(vector);
                appsettings_set_granted(appsettings, granted);
                stringset_delete(granted);
                value_reply(NULL);
            }
            g_strfreev(vector);
        }
    }
    else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_PROMPT) ||
             !g_strcmp0(method_name, PERMISSIONMGR_METHOD_QUERY) ) {
        /* Use session user */
        guint32        uid         = control_current_user(service_control(self));
        const gchar   *app         = NULL;
        appinfo_t     *appinfo     = NULL;
        appsettings_t *appsettings = NULL;
        g_variant_get(parameters, "(&s)", &app);

        if( !(appinfo = service_appinfo(self, app)) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_APPLICATION, app);
        }
        else if( !(appsettings = service_appsettings(self, uid, app)) ) {
            error_reply(G_DBUS_ERROR_INVALID_ARGS, SERVICE_MESSAGE_INVALID_USER, uid);
        }
        else {
            /* Automatically allow apps that do not require any permissions.
             * Unless they have been explicitly denied earlier.
             *
             * Applications defined outside /usr/share/applications can not
             * be prompted for permissions.
             */
            gchar *desktop = path_from_desktop_name(appinfo_id(appinfo));
            const stringset_t *permissions = appinfo_get_permissions(appinfo);
            stringset_t *filtered = service_filter_permissions(self,
                                                               permissions);
            if( stringset_empty(filtered) ) {
                if( appsettings_get_allowed(appsettings) == APP_ALLOWED_UNSET )
                    appsettings_set_allowed(appsettings, APP_ALLOWED_ALWAYS);
            }
            /* Check current status and reply immediately / queue for prompting.
             */
            app_allowed_t allowed = appsettings_get_allowed(appsettings);
            if( allowed == APP_ALLOWED_NEVER ) {
                error_reply(G_DBUS_ERROR_AUTH_FAILED, SERVICE_MESSAGE_DENIED_PERMANENTLY);
            }
            else if( allowed == APP_ALLOWED_ALWAYS ) {
                const stringset_t *granted = appsettings_get_granted(appsettings);
                gchar **vector = stringset_to_strv(granted);
                GVariant *variant = g_variant_new_strv((const gchar * const *)vector, -1);
                value_reply(variant);
                g_strfreev(vector);
            }
            else if( !g_strcmp0(method_name, PERMISSIONMGR_METHOD_QUERY) ||
                      access(desktop, R_OK) == -1 ) {
                error_reply(G_DBUS_ERROR_AUTH_FAILED, SERVICE_MESSAGE_NOT_ALLOWED);
            }
            else {
                prompter_handle_invocation(service_prompter(self), invocation);
            }
            stringset_delete(filtered);
            g_free(desktop);
        }
    }
    else {
        error_reply(G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method: %s",
                    method_name);
    }
    log_debug("done");
}

static void
service_dbus_emit_signal(service_t *self, const char *member, const char *value)
{
    GDBusConnection *connection = service_get_connection(self);
    if( !connection ) {
        log_warning("broadcast %s(%s):  skipped: not connected", member, value);
    }
    else{
        GError *error = 0;
        g_dbus_connection_emit_signal(connection, NULL,
                                      PERMISSIONMGR_OBJECT,
                                      PERMISSIONMGR_INTERFACE,
                                      member,
                                      g_variant_new("(s)", value),
                                      &error);
        if( error )
            log_warning("broadcast %s(%s):  failed: %s", member, value, error->message);
        else
            log_debug("broadcast %s(%s):  succeeded", member, value);
        g_clear_error(&error);
    }
}

void
service_applications_changed(service_t *self, const stringset_t *changed)
{
    log_notice("*** applications changed broadcast");

    for( const GList *iter = stringset_list(changed); iter; iter = iter->next ) {
        const char *app = iter->data;
        appinfo_t *appinfo = service_appinfo(self, app);
        const char *member = PERMISSIONMGR_SIGNAL_APP_CHANGED;
        if( !appinfo_valid(appinfo) ) {
            member = PERMISSIONMGR_SIGNAL_APP_REMOVED;
            stringset_remove_item(self->srv_dbus_applications, app);
        }
        else if( !stringset_has_item(self->srv_dbus_applications, app) ) {
            member = PERMISSIONMGR_SIGNAL_APP_ADDED;
            stringset_add_item(self->srv_dbus_applications, app);
        }
        service_dbus_emit_signal(self, member, app);
    }
}
