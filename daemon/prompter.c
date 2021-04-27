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

#include "prompter.h"

#include "control.h"
#include "service.h"
#include "session.h"
#include "stringset.h"
#include "appinfo.h"
#include "settings.h"
#include "util.h"
#include "logging.h"

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct service_t service_t;
typedef struct appinfo_t appinfo_t;

typedef enum prompter_state_t {
    PROMPTER_STATE_UNDEFINED,
    PROMPTER_STATE_IDLE,
    PROMPTER_STATE_CONNECT,
    PROMPTER_STATE_PROMPT,
    PROMPTER_STATE_DISCONNECT,
    PROMPTER_STATE_CONNECTION_FAILURE,
    PROMPTER_STATE_PROMPTING_FAILURE,
    PROMPTER_STATE_FINAL
} prompter_state_t;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * CHANGE
 * ------------------------------------------------------------------------- */

static bool change_timer(guint *pmember, guint value);

/* ------------------------------------------------------------------------- *
 * PROMPTER_STATE
 * ------------------------------------------------------------------------- */

static const char *prompter_state_repr(prompter_state_t state);

/* ------------------------------------------------------------------------- *
 * PROMPTER
 * ------------------------------------------------------------------------- */

static void  prompter_ctor     (prompter_t *self, service_t *service);
static void  prompter_dtor     (prompter_t *self);
prompter_t  *prompter_create   (service_t *service);
void         prompter_delete   (prompter_t *self);
void         prompter_delete_at(prompter_t **pself);
void         prompter_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * PROMPTER_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static service_t     *prompter_service     (const prompter_t *self);
static control_t     *prompter_control     (const prompter_t *self);
static appsettings_t *prompter_appsettings (const prompter_t *self, uid_t uid, const char *app);
static uid_t          prompter_current_user(const prompter_t *self);
static gchar         *prompter_bus_address (const prompter_t *self);

/* ------------------------------------------------------------------------- *
 * PROMPTER_STM
 * ------------------------------------------------------------------------- */

static prompter_state_t prompter_get_state       (const prompter_t *self);
static void             prompter_enter_state     (prompter_t *self);
static void             prompter_leave_state     (prompter_t *self);
static void             prompter_eval_state_now  (prompter_t *self);
static void             prompter_eval_state_later(prompter_t *self);
static gboolean         prompter_eval_state_cb   (gpointer aptr);
static bool             prompter_state_transfer_p(prompter_state_t prev, prompter_state_t next);
static void             prompter_set_state       (prompter_t *self, prompter_state_t state);
static bool             prompter_step_state      (prompter_t *self);
static void             prompter_exec_state      (prompter_t *self);

/* ------------------------------------------------------------------------- *
 * PROMPTER_TIMER
 * ------------------------------------------------------------------------- */

static bool     prompter_timer_running(const prompter_t *self);
static void     prompter_start_timer  (prompter_t *self, int ms);
static void     prompter_stop_timer   (prompter_t *self);
static gboolean prompter_timer_cb     (gpointer aptr);

/* ------------------------------------------------------------------------- *
 * PROMPTER_INVOCATION
 * ------------------------------------------------------------------------- */

static GDBusMethodInvocation *prompter_current_invocation  (prompter_t *self);
static void                   prompter_finish_invocation   (prompter_t *self);
static void                   prompter_fail_invocation     (prompter_t *self);
static void                   prompter_reply_invocation    (prompter_t *self);
static GDBusMethodInvocation *prompter_next_invocation     (prompter_t *self);
static void                   prompter_prompt_invocation_cb(GObject *obj, GAsyncResult *res, gpointer aptr);
static GVariant              *prompter_invocation_args     (const prompter_t *self, appinfo_t *appinfo);
static bool                   prompter_prompt_invocation   (prompter_t *self);
void                          prompter_handle_invocation   (prompter_t *self, GDBusMethodInvocation *invocation);

/* ------------------------------------------------------------------------- *
 * PROMPTER_RETURN
 * ------------------------------------------------------------------------- */

static void prompter_return_value(GDBusMethodInvocation *invocation, GVariant *val);
static void prompter_return_error(GDBusMethodInvocation *invocation, int code, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/* ------------------------------------------------------------------------- *
 * PROMPTER_QUEUE
 * ------------------------------------------------------------------------- */

static void                   prompter_enqueue    (prompter_t *self, GDBusMethodInvocation *invocation);
static guint                  prompter_queued     (prompter_t *self);
static GDBusMethodInvocation *prompter_dequeue    (prompter_t *self);
static void                   prompter_dequeue_all(prompter_t *self);

/* ------------------------------------------------------------------------- *
 * PROMPTER_CONNECTION
 * ------------------------------------------------------------------------- */

static GDBusConnection *prompter_connection  (const prompter_t *self);
static bool             prompter_is_connected(const prompter_t *self);
static bool             prompter_connect     (prompter_t *self);
static void             prompter_disconnect  (prompter_t *self);

/* ========================================================================= *
 * UTILITY
 * ========================================================================= */

static bool
change_timer(guint *pmember, guint value)
{
    bool changed = false;
    if( *pmember != value ) {
        g_source_remove(*pmember);
        *pmember = value;
        changed = true;
    }
    return changed;
}

/* ========================================================================= *
 * PROMPTER_STATE
 * ========================================================================= */

static const char *
prompter_state_repr(prompter_state_t state)
{
    static const char * const lut[] =  {
        [PROMPTER_STATE_UNDEFINED]          = "UNDEFINED",
        [PROMPTER_STATE_IDLE]               = "IDLE",
        [PROMPTER_STATE_CONNECT]            = "CONNECT",
        [PROMPTER_STATE_PROMPT]             = "PROMPT",
        [PROMPTER_STATE_DISCONNECT]         = "DISCONNECT",
        [PROMPTER_STATE_CONNECTION_FAILURE] = "CONNECTION_FAILURE",
        [PROMPTER_STATE_PROMPTING_FAILURE]  = "PROMPTING_FAILURE",
        [PROMPTER_STATE_FINAL]              = "FINAL",
    };
    return lut[state];
}

/* ========================================================================= *
 * PROMPTER
 * ========================================================================= */

struct prompter_t
{
    service_t             *prm_service;
    prompter_state_t       prm_state;
    guint                  prm_timer_id;
    guint                  prm_later_id;
    GQueue                *prm_queue;           // GDBusMethodInvocation *
    GDBusConnection       *prm_connection;
    GDBusMethodInvocation *prm_invocation;
};

static void
prompter_ctor(prompter_t *self, service_t *service)
{
    log_info("prompter() create");
    self->prm_service    = service;
    self->prm_state      = PROMPTER_STATE_UNDEFINED;
    self->prm_timer_id   = 0;
    self->prm_later_id   = 0;
    self->prm_queue      = g_queue_new();
    self->prm_connection = NULL;
    self->prm_invocation = NULL;
    prompter_set_state(self, PROMPTER_STATE_IDLE);
}

static void
prompter_dtor(prompter_t *self)
{
    log_info("prompter() delete");

    /* Transition to FINAL state should flush the
     * queue, stop transition timers, etc */
    prompter_set_state(self, PROMPTER_STATE_FINAL);

    change_timer(&self->prm_later_id, 0);

    g_queue_free(self->prm_queue),
        self->prm_queue = NULL;

    self->prm_service = NULL;
}

prompter_t *
prompter_create(service_t *service)
{
    prompter_t *self = g_malloc0(sizeof *self);
    prompter_ctor(self, service);
    return self;
}

void
prompter_delete(prompter_t *self)
{
    if( self ) {
        prompter_dtor(self);
        g_free(self);
    }
}

void
prompter_delete_at(prompter_t **pself)
{
    prompter_delete(*pself), *pself = NULL;
}

void
prompter_delete_cb(void *self)
{
    prompter_delete(self);
}

/* ------------------------------------------------------------------------- *
 * PROMPTER_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static service_t *
prompter_service(const prompter_t *self)
{
    return self->prm_service;
}

static control_t *
prompter_control(const prompter_t *self)
{
    return service_control(prompter_service(self));
}

static appsettings_t *
prompter_appsettings(const prompter_t *self, uid_t uid, const char *app)
{
    return control_appsettings(prompter_control(self), uid, app);
}

static uid_t
prompter_current_user(const prompter_t *self)
{
    // FIXME: this should be cached internally for error
    //        error reply handling on session change...
    return control_current_user(prompter_control(self));
}

static gchar *
prompter_bus_address(const prompter_t *self)
{
    gchar *addr = NULL;
    uid_t  uid  = control_current_user(prompter_control(self));
    if( uid != SESSION_UID_UNDEFINED )
        addr = g_strdup_printf("unix:path=/run/user/%lld/dbus/user_bus_socket",
                               (long long)uid);
    return addr;
}

#ifdef DEAD_CODE
static applications_t *
prompter_applications(const prompter_t *self)
{
    return control_applications(prompter_control(self));
}
#endif

/* ------------------------------------------------------------------------- *
 * PROMPTER_STM
 * ------------------------------------------------------------------------- */

static prompter_state_t
prompter_get_state(const prompter_t *self)
{
    return self->prm_state;
}

static void
prompter_enter_state(prompter_t *self)
{
    switch( prompter_get_state(self) ) {
    case PROMPTER_STATE_UNDEFINED:
        abort();
        break;

    case PROMPTER_STATE_IDLE:
        break;

    case PROMPTER_STATE_CONNECT:
        // FIXME: this should be done asynchronously
        if( !prompter_connect(self) )
            prompter_set_state(self, PROMPTER_STATE_CONNECTION_FAILURE);
        break;

    case PROMPTER_STATE_PROMPT:
        break;

    case PROMPTER_STATE_DISCONNECT:
        prompter_disconnect(self);
        break;

    case PROMPTER_STATE_CONNECTION_FAILURE:
        /* Rate limit connection attempts */
        prompter_start_timer(self, 5000);
        break;

    case PROMPTER_STATE_PROMPTING_FAILURE:
        /* Rate limit prompting attempts */
        prompter_start_timer(self, 1000);
        break;

    case PROMPTER_STATE_FINAL:
        /* Drop user session connection */
        prompter_disconnect(self);
        /* Fail all queued requests */
        prompter_dequeue_all(self);
        /* Fail pending queued request */
        prompter_fail_invocation(self);
        break;
    }
}

static void
prompter_leave_state(prompter_t *self)
{
    switch( prompter_get_state(self) ) {
    case PROMPTER_STATE_UNDEFINED:
        break;

    case PROMPTER_STATE_IDLE:
        break;

    case PROMPTER_STATE_CONNECT:
        break;

    case PROMPTER_STATE_PROMPT:
        prompter_fail_invocation(self);
        break;

    case PROMPTER_STATE_DISCONNECT:
        break;

    case PROMPTER_STATE_CONNECTION_FAILURE:
        prompter_stop_timer(self);
        break;

    case PROMPTER_STATE_PROMPTING_FAILURE:
        prompter_stop_timer(self);
        break;

    case PROMPTER_STATE_FINAL:
        abort();
        break;
    }
}

static void
prompter_eval_state_now(prompter_t *self)
{
    change_timer(&self->prm_later_id, 0);

    switch( prompter_get_state(self) ) {
    case PROMPTER_STATE_UNDEFINED:
        break;

    case PROMPTER_STATE_IDLE:
        if( prompter_queued(self) )
            prompter_set_state(self, PROMPTER_STATE_CONNECT);
        break;

    case PROMPTER_STATE_CONNECT:
        if( prompter_is_connected(self) )
            prompter_set_state(self, PROMPTER_STATE_PROMPT);
        break;

    case PROMPTER_STATE_PROMPT:
        if( prompter_current_invocation(self) ) {
            /* We have a pending call */
        }
        else if( !prompter_next_invocation(self) ) {
            prompter_set_state(self, PROMPTER_STATE_DISCONNECT);
        }
        else if( !prompter_prompt_invocation(self) ) {
            prompter_fail_invocation(self);
        }
        break;

    case PROMPTER_STATE_DISCONNECT:
        if( !prompter_is_connected(self) )
            prompter_set_state(self, PROMPTER_STATE_IDLE);
        break;

    case PROMPTER_STATE_CONNECTION_FAILURE:
        if( !prompter_timer_running(self) )
            prompter_set_state(self, PROMPTER_STATE_IDLE);
        break;

    case PROMPTER_STATE_PROMPTING_FAILURE:
        if( !prompter_timer_running(self) )
            prompter_set_state(self, PROMPTER_STATE_DISCONNECT);
        break;

    case PROMPTER_STATE_FINAL:
        break;
    }
}

static void
prompter_eval_state_later(prompter_t *self)
{
    if( !self->prm_later_id )
        self->prm_later_id = g_idle_add(prompter_eval_state_cb, self);
}

static gboolean
prompter_eval_state_cb(gpointer aptr)
{
    prompter_t *self = aptr;
    self->prm_later_id = 0;
    prompter_eval_state_now(self);
    return G_SOURCE_REMOVE;
}

static bool
prompter_state_transfer_p(prompter_state_t prev, prompter_state_t next)
{
    if( prev == PROMPTER_STATE_FINAL )
        return false;
    if( next == PROMPTER_STATE_UNDEFINED )
        return false;
    return true;
}

static void
prompter_set_state(prompter_t *self, prompter_state_t state)
{
    if( self->prm_state != state ) {
        if( !prompter_state_transfer_p(self->prm_state, state) ) {
            log_err("rejected transition: %s -> %s",
                    prompter_state_repr(self->prm_state),
                    prompter_state_repr(state));
        }
        else {
            log_info("state transition: %s -> %s",
                     prompter_state_repr(self->prm_state),
                     prompter_state_repr(state));

            prompter_leave_state(self);
            self->prm_state = state;
            prompter_enter_state(self);

            prompter_eval_state_later(self);
        }
    }
}

static bool
prompter_step_state(prompter_t *self)
{
    prompter_state_t state = prompter_get_state(self);
    prompter_eval_state_now(self);
    return prompter_get_state(self) != state;
}

static void
prompter_exec_state(prompter_t *self)
{
    while( prompter_step_state(self) )
        ;
}

/* ------------------------------------------------------------------------- *
 * PROMPTER_TIMER
 * ------------------------------------------------------------------------- */

static bool
prompter_timer_running(const prompter_t *self)
{
    return self->prm_timer_id != 0;
}

static void
prompter_start_timer(prompter_t *self, int ms)
{
    change_timer(&self->prm_timer_id,
                 g_timeout_add(ms, prompter_timer_cb, self));
}

static void
prompter_stop_timer(prompter_t *self)
{
    change_timer(&self->prm_timer_id, 0);
}

static gboolean
prompter_timer_cb(gpointer aptr)
{
    prompter_t *self = aptr;
    self->prm_timer_id = 0;
    prompter_exec_state(self);
    return G_SOURCE_REMOVE;
}

/* ------------------------------------------------------------------------- *
 * PROMPTER_INVOCATION
 * ------------------------------------------------------------------------- */

static GDBusMethodInvocation *
prompter_current_invocation(prompter_t *self)
{
    return self->prm_invocation;
}

static void
prompter_finish_invocation(prompter_t *self)
{
    GDBusMethodInvocation *invocation = self->prm_invocation;
    if( invocation ) {
        self->prm_invocation = NULL;

        uid_t uid = prompter_current_user(self);

        appsettings_t *appsettings = NULL;
        GVariant *parameters =
            g_dbus_method_invocation_get_parameters(invocation);
        const gchar *app = NULL;
        g_variant_get(parameters, "(&s)", &app);
        if( !app ) {
            prompter_return_error(invocation, G_DBUS_ERROR_INVALID_ARGS,
                                  SERVICE_MESSAGE_INVALID_APPLICATION, app);
        }
        else if( !(appsettings = prompter_appsettings(self, uid, app)) ) {
            prompter_return_error(invocation, G_DBUS_ERROR_INVALID_ARGS,
                                  SERVICE_MESSAGE_INVALID_USER, uid);
        }
        else {
            app_allowed_t allowed = appsettings_get_allowed(appsettings);
            if( allowed == APP_ALLOWED_NEVER ) {
                prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                                      SERVICE_MESSAGE_DENIED_PERMANENTLY);
            }
            else if( allowed == APP_ALLOWED_ALWAYS ) {
                const stringset_t *granted =
                    appsettings_get_granted(appsettings);
                gchar **vector = stringset_to_strv(granted);
                GVariant *variant =
                    g_variant_new_strv((const gchar * const *)vector, -1);
                prompter_return_value(invocation, variant);
                g_strfreev(vector);
            }
            else {
                prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                                      SERVICE_MESSAGE_NOT_ALLOWED);
            }
        }

        prompter_eval_state_later(self);
    }
}

static void
prompter_fail_invocation(prompter_t *self)
{
    if( self->prm_invocation ) {
        /* Return existing permissions */
        prompter_finish_invocation(self);
    }
}

static void
prompter_reply_invocation(prompter_t *self)
{
    if( self->prm_invocation ) {
        GVariant *parameters =
            g_dbus_method_invocation_get_parameters(self->prm_invocation);
        if( parameters ) {
            uid_t        uid = prompter_current_user(self);
            const gchar *app = NULL;
            g_variant_get(parameters, "(&s)", &app);
            if( app ) {
                appsettings_t *appsettings =
                    prompter_appsettings(self, uid, app);
                if( appsettings ) {
                    /* Allowing sets also granted before returning */
                    appsettings_set_allowed(appsettings, APP_ALLOWED_ALWAYS);
                }
            }
        }

        /* Return the possibly just granted permissions */
        prompter_finish_invocation(self);
    }
}

static GDBusMethodInvocation *
prompter_next_invocation(prompter_t *self)
{
    for( ;; ) {
        prompter_fail_invocation(self);

        if( !(self->prm_invocation = prompter_dequeue(self)) )
            break;

        log_debug("consider %p", self->prm_invocation);

        GVariant *parameters =
            g_dbus_method_invocation_get_parameters(self->prm_invocation);
        if( !parameters ) {
            log_debug("no parameters");
            continue;
        }

        const gchar *app = NULL;
        g_variant_get(parameters, "(&s)", &app);
        if( !app ) {
            log_debug("no app");
            continue;
        }

        uid_t uid = prompter_current_user(self);
        appsettings_t *appsettings = prompter_appsettings(self, uid, app);
        if( !appsettings ) {
            log_debug("no appsettings");
            continue;
        }

        app_allowed_t allowed = appsettings_get_allowed(appsettings);
        if( allowed == APP_ALLOWED_UNSET ) {
            log_debug("prompting ...");
            break;
        }
        if( allowed == APP_ALLOWED_ALWAYS ) {
            log_debug("already allowed");
            prompter_reply_invocation(self);
        }
        else {
            log_debug("already denied");
        }
    }

    log_debug("process %p", self->prm_invocation);
    return self->prm_invocation;
}

static void
prompter_prompt_invocation_cb(GObject *obj, GAsyncResult *res, gpointer aptr)
{
    GDBusConnection *con  = G_DBUS_CONNECTION(obj);
    prompter_t      *self = aptr;
    GError          *err  = NULL;
    GVariant        *rsp  = NULL;

    if( !(rsp = g_dbus_connection_call_finish(con, res, &err)) ) {
        if( err )
            log_err("error reply: domain=%u code=%d message='%s'",
                    (unsigned)err->domain, err->code, err->message);
        else
            log_err("null reply");
        prompter_fail_invocation(self);
    }
    else {
        prompter_reply_invocation(self);
    }

    g_clear_error(&err);
}

static GVariant *
prompter_invocation_args(const prompter_t *self, appinfo_t *appinfo)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sas}"));

    const stringset_t *permissions = appinfo_get_permissions(appinfo);
    stringset_t *filtered = service_filter_permissions(prompter_service(self),
                                                       permissions);
    gchar **vector = stringset_to_strv(filtered);
    for( guint i = 0; vector[i]; ++i ) {
        gchar *perm = vector[i];
        vector[i] = path_from_permission_name(perm);
        g_free(perm);
    }
    g_variant_builder_add(&builder, "{s^as}", "required", vector);
    g_strfreev(vector);

    gchar *desktop = path_from_desktop_name(appinfo_id(appinfo));
    GVariant *args = g_variant_new("(s@a{sas})", desktop,
                                   g_variant_builder_end(&builder));
    g_free(desktop);
    stringset_delete(filtered);
    return args;
}

static bool
prompter_prompt_invocation(prompter_t *self)
{
    bool                ack     = false;
    appinfo_t          *appinfo = NULL;
    const gchar        *app     = NULL;
    GCancellable       *cancellable  = NULL; // <-- FIXME: make this cancellable

    GVariant *parameters =
        g_dbus_method_invocation_get_parameters(prompter_current_invocation(self));

    g_variant_get(parameters, "(&s)", &app);
    if( !app ) {
        log_err("could not parse application parameter");
        goto EXIT;
    }

    if( !(appinfo = control_appinfo(prompter_control(self), app)) ) {
        log_err("unknown app: %s", app);
        goto EXIT;
    }

    g_dbus_connection_call(prompter_connection(self),
                           WINDOWPROMT_SERVICE,
                           WINDOWPROMT_OBJECT,
                           WINDOWPROMT_INTERFACE,
                           WINDOWPROMT_METHOD_PROMPT,
                           prompter_invocation_args(self, appinfo),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           G_MAXINT,
                           cancellable,
                           prompter_prompt_invocation_cb,
                           self);

    ack = true;

EXIT:
    return ack;
}

void
prompter_handle_invocation(prompter_t *self, GDBusMethodInvocation *invocation)
{
    prompter_enqueue(self, invocation);
    prompter_eval_state_later(self);
}

/* ------------------------------------------------------------------------- *
 * PROMPTER_RETURN
 * ------------------------------------------------------------------------- */

static void
prompter_return_value(GDBusMethodInvocation *invocation, GVariant *val)
{
    g_dbus_method_invocation_return_value(invocation,
                                          val
                                          ? g_variant_new_tuple(&val, 1)
                                          : NULL);
}

static void __attribute__((format(printf, 3, 4)))
prompter_return_error(GDBusMethodInvocation *invocation, int code,
                      const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    g_dbus_method_invocation_return_error_valist(invocation, G_DBUS_ERROR,
                                                 code, fmt, va);
    va_end(va);
}

/* ------------------------------------------------------------------------- *
 * PROMPTER_QUEUE
 * ------------------------------------------------------------------------- */

static void
prompter_enqueue(prompter_t *self, GDBusMethodInvocation *invocation)
{
    log_info("enqueue %p", invocation);
    g_queue_push_tail(self->prm_queue, invocation);
}

static guint
prompter_queued(prompter_t *self)
{
    return self->prm_queue->length;
}

static GDBusMethodInvocation *
prompter_dequeue(prompter_t *self)
{
    GDBusMethodInvocation *invocation = g_queue_pop_head(self->prm_queue);
    log_info("dequeue %p", invocation);
    return invocation;
}

static void
prompter_dequeue_all(prompter_t *self)
{
    for( GList *iter = self->prm_queue->head; iter; iter = iter->next ) {
        GDBusMethodInvocation *invocation = iter->data;
        iter->data = NULL;
        prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                              "Dismissed");
    }
    g_queue_remove_all(self->prm_queue, NULL);
}

// FIXME: on session changed -> dequeue_all
// FIXME: on app removed -> dequeue_app (rejected due to invalid app)
// FIXME: on app changed -> dequeue_app (accepted/rejected based on settings)
// FIXME: and cancel the pending call too when appropriate

/* ------------------------------------------------------------------------- *
 * PROMPTER_CONNECTION
 * ------------------------------------------------------------------------- */

static GDBusConnection *
prompter_connection(const prompter_t *self)
{
    return self->prm_connection;
}

static bool
prompter_is_connected(const prompter_t *self)
{
    return prompter_connection(self) != 0;
}

static bool
prompter_connect(prompter_t *self)
{
    gchar  *address = NULL;
    GError *error   = NULL;

    if( self->prm_connection )
        goto EXIT;

    if( !(address = prompter_bus_address(self)) )
        goto EXIT;

    GDBusConnectionFlags flags =
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
        G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION;

    self->prm_connection =
        g_dbus_connection_new_for_address_sync(address, flags,
                                               NULL, NULL, &error);

    if( error )
        log_err("connecting to %s failed: %s", address, error->message);
EXIT:
    g_clear_error(&error);
    g_free(address);
    return self->prm_connection != NULL;
}

static void
prompter_disconnect(prompter_t *self)
{
    if( self->prm_connection ) {
        g_object_unref(self->prm_connection),
            self->prm_connection = NULL;
    }
}
