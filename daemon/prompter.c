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
typedef struct watcher_t watcher_t;

typedef enum prompter_state_t {
    PROMPTER_STATE_UNDEFINED,
    PROMPTER_STATE_IDLE,
    PROMPTER_STATE_CONNECT,
    PROMPTER_STATE_PROMPT,
    PROMPTER_STATE_WAIT,
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

static bool change_cancellable_steal(GCancellable **pmember, GCancellable *value);

/* ------------------------------------------------------------------------- *
 * PROMPTER_STATE
 * ------------------------------------------------------------------------- */

static const char *prompter_state_repr(prompter_state_t state);

/* ------------------------------------------------------------------------- *
 * PROMPTER
 * ------------------------------------------------------------------------- */

static void  prompter_ctor                (prompter_t *self, service_t *service);
static void  prompter_dtor                (prompter_t *self);
prompter_t  *prompter_create              (service_t *service);
void         prompter_delete              (prompter_t *self);
void         prompter_delete_at           (prompter_t **pself);
void         prompter_delete_cb           (void *self);
void         prompter_applications_changed(prompter_t *self, const stringset_t *changed);
void         prompter_session_changed     (prompter_t *self);

/* ------------------------------------------------------------------------- *
 * PROMPTER_ATTRIBUTES
 * ------------------------------------------------------------------------- */

static service_t     *prompter_service     (const prompter_t *self);
static control_t     *prompter_control     (const prompter_t *self);
static appsettings_t *prompter_appsettings (const prompter_t *self, uid_t uid, const char *app);
static uid_t          prompter_current_user(const prompter_t *self);
static gchar         *prompter_bus_address (const prompter_t *self);

/* ------------------------------------------------------------------------- *
 * PROMPTER_PROPERTIES
 * ------------------------------------------------------------------------- */
static bool prompter_get_prompt_canceled(prompter_t *self);
static void prompter_set_prompt_canceled(prompter_t *self, bool canceled);

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

static GDBusMethodInvocation *prompter_current_invocation   (prompter_t *self);
static void                   prompter_finish_invocation    (prompter_t *self);
static bool                   prompter_try_finish_invocation(prompter_t *self, GDBusMethodInvocation *invocation, const stringset_t *changed);
static bool                   prompter_check_invocation     (prompter_t *self, GDBusMethodInvocation *invocation);
static void                   prompter_fail_invocation      (prompter_t *self);
static void                   prompter_reply_invocation     (prompter_t *self);
static GDBusMethodInvocation *prompter_next_invocation      (prompter_t *self);
static GVariant              *prompter_invocation_args      (const prompter_t *self, appinfo_t *appinfo);
static bool                   prompter_prompt_invocation    (prompter_t *self);
static void                   prompter_prompt_invocation_cb (GObject *obj, GAsyncResult *res, gpointer aptr);
static bool                   prompter_wait_invocation      (prompter_t *self);
static void                   prompter_wait_invocation_cb   (GObject *obj, GAsyncResult *res, gpointer aptr);
void                          prompter_handle_invocation    (prompter_t *self, GDBusMethodInvocation *invocation);
static void                   prompter_cancel_invocation    (prompter_t *self);
static void                   prompter_cancel_prompt        (prompter_t *self);
static void                   prompter_watch_name           (prompter_t *self, GDBusConnection *connection, const gchar *name);
static void                   prompter_unwatch_name         (prompter_t *self, const gchar *name);
static void                   prompter_handle_name_lost     (prompter_t *self, const gchar *name);
void                          prompter_dbus_reload_config   (prompter_t *self);

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
static GList                 *prompter_iter       (prompter_t *self);
static GList                 *prompter_drop       (prompter_t *self, GList *iter);

/* ------------------------------------------------------------------------- *
 * PROMPTER_CONNECTION
 * ------------------------------------------------------------------------- */

static GDBusConnection *prompter_connection         (const prompter_t *self);
static bool             prompter_is_connected       (const prompter_t *self);
static bool             prompter_connect            (prompter_t *self);
static void             prompter_disconnect         (prompter_t *self);
static void             prompter_disconnect_flush_cb(GObject *obj, GAsyncResult *res, gpointer aptr);

/* ------------------------------------------------------------------------- *
 * WATCHER
 * ------------------------------------------------------------------------- */

static void       watcher_ctor               (watcher_t *self, prompter_t *prompter, GDBusConnection *connection, const gchar *name);
static void       watcher_dtor               (watcher_t *self);
static watcher_t *watcher_create             (prompter_t *prompter, GDBusConnection *connection, const gchar *name);
static void       watcher_delete             (watcher_t *self);
static void       watcher_delete_cb          (void *self);
static void       watcher_watch              (watcher_t *self);
static void       watcher_unwatch            (watcher_t *self);
static void       watcher_handle_name_lost_cb(GDBusConnection *connection, const gchar *name, gpointer aptr);
static void       watcher_name_has_owner     (watcher_t *self);
static void       watcher_name_has_owner_cb  (GObject *obj, GAsyncResult *res, gpointer aptr);
static void       watcher_notify_name_lost   (watcher_t *self);

/* ========================================================================= *
 * UTILITY
 * ========================================================================= */

static bool
change_cancellable_steal(GCancellable **pmember, GCancellable *value)
{
    bool changed = false;
    if( *pmember != value ) {
        if( *pmember ) {
            g_cancellable_cancel(*pmember);
            g_object_unref(*pmember);
            *pmember = NULL;
        }
        if( value )
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
        [PROMPTER_STATE_WAIT]               = "WAIT",
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
    uid_t                  prm_cached_user;
    GQueue                *prm_queue;           // GDBusMethodInvocation *
    GDBusConnection       *prm_connection;
    GDBusMethodInvocation *prm_invocation;
    GCancellable          *prm_cancellable;
    gchar                 *prm_prompt;
    GHashTable            *prm_watchers;        // watched busnames -> watcher_t*
    bool                   prm_canceled;
};

static void
prompter_ctor(prompter_t *self, service_t *service)
{
    log_info("prompter() create");
    self->prm_service     = service;
    self->prm_state       = PROMPTER_STATE_UNDEFINED;
    self->prm_timer_id    = 0;
    self->prm_later_id    = 0;
    self->prm_cached_user = control_current_user(service_control(service));
    self->prm_queue       = g_queue_new();
    self->prm_connection  = NULL;
    self->prm_invocation  = NULL;
    self->prm_cancellable = NULL;
    self->prm_prompt      = NULL;
    self->prm_watchers    = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, watcher_delete_cb);
    self->prm_canceled    = false;
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

    g_hash_table_destroy(self->prm_watchers),
        self->prm_watchers = NULL;

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

void
prompter_applications_changed(prompter_t *self, const stringset_t *changed)
{
    /* First check the current invocation */
    if( prompter_try_finish_invocation(self, prompter_current_invocation(self),
                                       changed) ) {
        prompter_cancel_invocation(self);
        prompter_eval_state_later(self);
    }

    /* Then check the rest of the invocations */
    GList *iter = prompter_iter(self);
    while( iter ) {
        if( prompter_try_finish_invocation(self, iter->data, changed) )
            iter = prompter_drop(self, iter);
        else
            iter = iter->next;
    }
}

void
prompter_session_changed(prompter_t *self)
{
    /* All invocations are invalid when user changes. By this time all
     * invocations probably have failed and we have disconnected from
     * session bus but in case that's not true, fail remaining requests
     * and disconnect from bus.
     */
    if( self->prm_cached_user != SESSION_UID_UNDEFINED ) {
        if( prompter_current_user(self) != self->prm_cached_user ) {
            /* Fail all queued requests */
            prompter_dequeue_all(self);
            /* Fail pending queued request */
            prompter_fail_invocation(self);
            /* Disconnect from old bus */
            prompter_set_state(self, PROMPTER_STATE_DISCONNECT);
        }
    }
    self->prm_cached_user = prompter_current_user(self);
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
    return control_current_user(prompter_control(self));
}

static gchar *
prompter_bus_address(const prompter_t *self)
{
    gchar *addr = NULL;
    uid_t  uid  = prompter_current_user(self);
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
 * PROMPTER_PROPERTIES
 * ------------------------------------------------------------------------- */

static bool
prompter_get_prompt_canceled(prompter_t *self)
{
    return self->prm_canceled;
}

static void
prompter_set_prompt_canceled(prompter_t *self, bool canceled)
{
    if( canceled )
        log_debug("set prompt to be canceled");
    else if( self->prm_canceled )
        log_debug("prompt canceling cleared");
    self->prm_canceled = canceled;
}

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

    case PROMPTER_STATE_WAIT:
        if( !prompter_wait_invocation(self) )
            prompter_fail_invocation(self);
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
        break;

    case PROMPTER_STATE_WAIT:
        prompter_fail_invocation(self);
        prompter_set_prompt_canceled(self, false);
        change_cancellable_steal(&self->prm_cancellable, NULL);
        change_string(&self->prm_prompt, NULL);
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
        if( prompter_get_prompt_canceled(self) ||
            prompter_current_invocation(self) ) {
            /* We have a pending call or pending call was canceled */
            if( self->prm_prompt )
                prompter_set_state(self, PROMPTER_STATE_WAIT);
        }
        else if( !prompter_next_invocation(self) ) {
            prompter_set_state(self, PROMPTER_STATE_DISCONNECT);
        }
        else if( !prompter_prompt_invocation(self) ) {
            prompter_fail_invocation(self);
        }
        break;

    case PROMPTER_STATE_WAIT:
        if( prompter_get_prompt_canceled(self) ) {
            prompter_cancel_prompt(self);
            prompter_set_state(self, PROMPTER_STATE_PROMPT);
        }
        else if( !prompter_current_invocation(self) ) {
            /* We resolved the pending call */
            prompter_set_state(self, PROMPTER_STATE_PROMPT);
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
        if( !prompter_check_invocation(self, invocation) ) {
            /* If we get here, the prompt was canceled */
            prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                                  SERVICE_MESSAGE_NOT_ALLOWED);
        }

        prompter_eval_state_later(self);
    }
}

static bool
prompter_try_finish_invocation(prompter_t            *self,
                               GDBusMethodInvocation *invocation,
                               const stringset_t     *changed)
{
    bool handled = false;

    if( invocation ) {
        const gchar *app        = NULL;
        GVariant    *parameters =
            g_dbus_method_invocation_get_parameters(invocation);
        g_variant_get(parameters, "(&s)", &app);
        if( !app ) {
            /* Should not be reached but it's an error to get here anyway */
            prompter_return_error(invocation, G_DBUS_ERROR_INVALID_ARGS,
                                  SERVICE_MESSAGE_INVALID_APPLICATION, "<null>");
            handled = true;
        }
        else if( stringset_has_item(changed, app) ) {
            /* App was changed => check and handle if possible */
            handled = prompter_check_invocation(self, invocation);
        } /* else: the app was not changed */
    }

    return handled;
}

static bool
prompter_check_invocation(prompter_t *self, GDBusMethodInvocation *invocation) {
    /* Check if invocation can be handled already, i.e. if it's not
     * still undecided. Returns true if a reply was sent.
     */

    const gchar   *app         = NULL;
    appsettings_t *appsettings = NULL;
    bool           handled     = false;
    GVariant      *parameters  =
        g_dbus_method_invocation_get_parameters(invocation);
    uid_t          uid         = prompter_current_user(self);

    g_variant_get(parameters, "(&s)", &app);
    if( !app ) {
        prompter_return_error(invocation, G_DBUS_ERROR_INVALID_ARGS,
                              SERVICE_MESSAGE_INVALID_APPLICATION, "<null>");
        handled = true;
    }
    else if( !(appsettings = prompter_appsettings(self, uid, app)) ) {
        if( !control_valid_user(prompter_control(self), uid) )
            prompter_return_error(invocation, G_DBUS_ERROR_INVALID_ARGS,
                                  SERVICE_MESSAGE_INVALID_USER, uid);
        else
            prompter_return_error(invocation, G_DBUS_ERROR_INVALID_ARGS,
                                  SERVICE_MESSAGE_INVALID_APPLICATION, app);
        handled = true;
    }
    else {
        app_allowed_t allowed = appsettings_get_allowed(appsettings);
        if( allowed == APP_ALLOWED_NEVER ) {
            prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                                  SERVICE_MESSAGE_DENIED_PERMANENTLY);
            handled = true;
        }
        else if( allowed == APP_ALLOWED_ALWAYS ) {
            const stringset_t *granted =
                appsettings_get_granted(appsettings);
            gchar **vector = stringset_to_strv(granted);
            GVariant *variant =
                g_variant_new_strv((const gchar * const *)vector, -1);
            prompter_return_value(invocation, variant);
            handled = true;
            g_strfreev(vector);
        }
    }

    return handled;
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
    else if( !g_variant_check_format_string(rsp, "(o)", true) ) {
        log_err("Invalid signature in reply: %s",
                g_variant_get_type_string(rsp));
        prompter_fail_invocation(self);
    }
    else {
        gchar *object_path = NULL;
        g_variant_get(rsp, "(o)", &object_path);
        change_string_steal(&self->prm_prompt, object_path);
        prompter_eval_state_later(self);
    }

    if( rsp )
        g_variant_unref(rsp);
    g_clear_error(&err);
}

static bool
prompter_wait_invocation(prompter_t *self)
{
    if( !prompter_get_prompt_canceled(self) ) {
        change_cancellable_steal(&self->prm_cancellable, g_cancellable_new());
        g_dbus_connection_call(prompter_connection(self),
                               WINDOWPROMPT_SERVICE,
                               self->prm_prompt,
                               WINDOWPROMPT_PROMPT_INTERFACE,
                               WINDOWPROMPT_PROMPT_METHOD_WAIT,
                               NULL,
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE,
                               G_MAXINT,
                               self->prm_cancellable,
                               prompter_wait_invocation_cb,
                               self);
        return true;
    }
    return false;
}

static void
prompter_wait_invocation_cb(GObject *obj, GAsyncResult *res, gpointer aptr)
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
        g_variant_unref(rsp);
    }

    g_clear_error(&err);
}

static GVariant *
prompter_invocation_args(const prompter_t *self, appinfo_t *appinfo)
{
    gchar *desktop                 = NULL;
    const stringset_t *permissions = NULL;
    stringset_t *filtered          = NULL;
    GVariant *args                 = NULL;

    GVariantBuilder builder;

    /* Prompting is applicable only to applications that have desktop
     * file in standard directory.
     *
     * Applications that have desktop file only in sailjail specific
     * directory are either allowed by default (in which case we should
     * not end up here) or are denied without prompting.
     */
    desktop = path_from_desktop_name(appinfo_id(appinfo));
    if( access(desktop, R_OK) == -1 )
        goto EXIT;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sas}"));
    permissions = appinfo_get_permissions(appinfo);
    filtered = service_filter_permissions(prompter_service(self), permissions);
    gchar **vector = stringset_to_strv(filtered);
    for( guint i = 0; vector[i]; ++i ) {
        gchar *perm = vector[i];
        vector[i] = path_from_permission_name(perm);
        g_free(perm);
    }
    g_variant_builder_add(&builder, "{s^as}", "required", vector);
    g_strfreev(vector);

    args = g_variant_new("(s@a{sas})", desktop, g_variant_builder_end(&builder));

EXIT:
    stringset_delete(filtered);
    g_free(desktop);
    return args;
}

static bool
prompter_prompt_invocation(prompter_t *self)
{
    bool                ack     = false;
    appinfo_t          *appinfo = NULL;
    const gchar        *app     = NULL;
    GVariant           *invocation_args = NULL;

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

    if( !(invocation_args = prompter_invocation_args(self, appinfo)) ) {
        gchar *desktop = path_from_desktop_name(appinfo_id(appinfo));
        log_err("%s: does not exist - can't prompt", desktop);
        g_free(desktop);
        goto EXIT;
    }

    g_dbus_connection_call(prompter_connection(self),
                           WINDOWPROMPT_SERVICE,
                           WINDOWPROMPT_OBJECT,
                           WINDOWPROMPT_INTERFACE,
                           WINDOWPROMPT_METHOD_PROMPT,
                           invocation_args,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           G_MAXINT,
                           NULL,
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
    prompter_watch_name(self,
                        g_dbus_method_invocation_get_connection(invocation),
                        g_dbus_method_invocation_get_sender(invocation));
    prompter_eval_state_later(self);
}

static void
prompter_cancel_invocation(prompter_t *self)
{
    self->prm_invocation = NULL;
    prompter_set_prompt_canceled(self, true);
}

static void
prompter_cancel_prompt(prompter_t *self)
{
    if( !self->prm_prompt ) {
        /* You are only supposed to call this within WAIT state and only once */
        log_err("tried to cancel prompt without object path");
    } else {
        log_debug("canceling windowprompt");
        g_dbus_connection_call(prompter_connection(self),
                               WINDOWPROMPT_SERVICE,
                               self->prm_prompt,
                               WINDOWPROMPT_PROMPT_INTERFACE,
                               WINDOWPROMPT_PROMPT_METHOD_CANCEL,
                               NULL,
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               NULL,
                               self);
        change_string(&self->prm_prompt, NULL);
    }
}

static void
prompter_watch_name(prompter_t *self, GDBusConnection *connection, const gchar *name)
{
    if( !g_hash_table_contains(self->prm_watchers, name) ) {
        g_hash_table_insert(self->prm_watchers, g_strdup(name),
                            watcher_create(self, connection, name));
    }
}

static void
prompter_unwatch_name(prompter_t *self, const gchar *name)
{
    g_hash_table_remove(self->prm_watchers, name);
}

static void
prompter_handle_name_lost(prompter_t *self, const gchar *name)
{
    prompter_unwatch_name(self, name);

    /* First check the current invocation */
    GDBusMethodInvocation *invocation = self->prm_invocation;
    if( invocation && !g_strcmp0(g_dbus_method_invocation_get_sender(invocation), name) ) {
        log_debug("-> canceling %p", invocation);
        prompter_cancel_invocation(self);
        /* Must call g_dbus_method_invocation_return_* to destroy the invocation */
        prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                              SERVICE_MESSAGE_DISCONNECTED);
        prompter_eval_state_later(self);
    }

    /* Then check the rest of the invocations */
    GList *iter = prompter_iter(self);
    while( iter ) {
        invocation = iter->data;
        if( !g_strcmp0(g_dbus_method_invocation_get_sender(invocation), name) ) {
            log_debug("-> skipping %p", invocation);
            iter->data = NULL;
            /* Must call g_dbus_method_invocation_return_* to destroy the invocation */
            prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                                  SERVICE_MESSAGE_DISCONNECTED);
            iter = prompter_drop(self, iter);
        }
        else {
            iter = iter->next;
        }
    }
}

void
prompter_dbus_reload_config(prompter_t *self)
{
    log_info("reload dbus config");

    bool connected = prompter_is_connected(self);

    if( !connected ) {
        log_info("temporarily connecting to the user session");
        if( !prompter_connect(self) ) {
            log_err("unable to connect to the user session to reload dbus config");
            return;
        }
    }

    g_dbus_connection_call(prompter_connection(self),
                           DBUS_SERVICE,
                           DBUS_PATH,
                           DBUS_INTERFACE,
                           DBUS_METHOD_RELOAD_CONFIG,
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           self);

    if( !connected ) {
        log_info("disconnecting temporary user session connection");
        prompter_disconnect(self);
    }
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
    for( GList *iter = prompter_iter(self); iter; iter = iter->next ) {
        GDBusMethodInvocation *invocation = iter->data;
        iter->data = NULL;
        prompter_return_error(invocation, G_DBUS_ERROR_AUTH_FAILED,
                              SERVICE_MESSAGE_DISMISSED);
    }
    g_queue_remove_all(self->prm_queue, NULL);
}

static GList *
prompter_iter(prompter_t *self)
{
    return self->prm_queue->head;
}

static GList *
prompter_drop(prompter_t *self, GList *iter)
{
    GList *next = iter->next;
    g_queue_delete_link(self->prm_queue, iter);
    return next;
}

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

    if( error ) {
        log_err("connecting to %s failed: %s", address, error->message);
        goto EXIT;
    }

    /* This is not a user session service, set exit-on-close explicitly false */
    g_dbus_connection_set_exit_on_close(self->prm_connection, FALSE);

EXIT:
    g_clear_error(&error);
    g_free(address);
    return self->prm_connection != NULL;
}

static void
prompter_disconnect(prompter_t *self)
{
    if( self->prm_connection ) {
        g_dbus_connection_flush(self->prm_connection, NULL,
                                prompter_disconnect_flush_cb, NULL),
            self->prm_connection = NULL;
    }
}

static void
prompter_disconnect_flush_cb(GObject *obj, GAsyncResult *res, gpointer aptr)
{
    GDBusConnection *con  = G_DBUS_CONNECTION(obj);

    g_dbus_connection_flush_finish(con, res, NULL);

    g_object_unref(con);
}

/* ------------------------------------------------------------------------- *
 * WATCHER
 * ------------------------------------------------------------------------- */

struct watcher_t
{
    prompter_t      *wtc_prompter;
    GDBusConnection *wtc_connection;
    gchar           *wtc_name;
    guint            wtc_watcher;
    GCancellable    *wtc_cancellable;
};

static void
watcher_ctor(watcher_t *self, prompter_t *prompter, GDBusConnection *connection, const gchar *name)
{
    log_info("watcher() create");
    self->wtc_prompter          = prompter;
    self->wtc_connection        = g_object_ref(connection);
    self->wtc_name              = g_strdup(name);
    self->wtc_watcher           = 0;
    self->wtc_cancellable       = NULL;

    watcher_watch(self);
    watcher_name_has_owner(self);
}

static void
watcher_dtor(watcher_t *self)
{
    log_info("watcher() delete");

    change_cancellable_steal(&self->wtc_cancellable, NULL);

    watcher_unwatch(self);

    change_string(&self->wtc_name, NULL);

    g_object_unref(self->wtc_connection),
        self->wtc_connection = NULL;

    self->wtc_prompter = NULL;
}

static watcher_t *
watcher_create(prompter_t *prompter, GDBusConnection *connection, const gchar *name)
{
    watcher_t *self = g_malloc0(sizeof *self);
    watcher_ctor(self, prompter, connection, name);
    return self;
}

static void
watcher_delete(watcher_t *self)
{
    if( self ) {
        watcher_dtor(self);
        g_free(self);
    }
}

static void
watcher_delete_cb(void *self)
{
    watcher_delete(self);
}

static void
watcher_watch(watcher_t *self)
{
    self->wtc_watcher = g_bus_watch_name_on_connection(
            self->wtc_connection, self->wtc_name, G_BUS_NAME_WATCHER_FLAGS_NONE,
            NULL, watcher_handle_name_lost_cb, self, NULL);
    log_debug("watching for '%s' to leave bus", self->wtc_name);
}

static void
watcher_unwatch(watcher_t *self)
{
    if( self->wtc_watcher )
        g_bus_unwatch_name(self->wtc_watcher);
    self->wtc_watcher = 0;
}

static void
watcher_handle_name_lost_cb(GDBusConnection *connection, const gchar *name, gpointer aptr)
{
    (void)connection; // unused
    (void)name; // unused
    watcher_t *self = aptr;
    log_debug("'%s' left bus", self->wtc_name);
    watcher_notify_name_lost(self);
}

static void
watcher_name_has_owner(watcher_t *self)
{
    change_cancellable_steal(&self->wtc_cancellable, g_cancellable_new());
    g_dbus_connection_call(self->wtc_connection,
                           DBUS_SERVICE,
                           DBUS_PATH,
                           DBUS_INTERFACE,
                           DBUS_METHOD_NAME_HAS_OWNER,
                           g_variant_new("(s)", self->wtc_name),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           self->wtc_cancellable,
                           watcher_name_has_owner_cb,
                           self);
}

static void
watcher_name_has_owner_cb(GObject *obj, GAsyncResult *res, gpointer aptr)
{
    GDBusConnection *con  = G_DBUS_CONNECTION(obj);
    GError          *err  = NULL;
    GVariant        *rsp  = NULL;
    /* NB: aptr might be invalid pointer,
     *     check before use if the call was canceled */
    watcher_t       *self = aptr;
    gboolean    has_owner = true;

    if( !(rsp = g_dbus_connection_call_finish(con, res, &err)) ) {
        if( err ) {
            log_err("error reply from dbus server: domain=%u code=%d message='%s'",
                    (unsigned)err->domain, err->code, err->message);
            /* aptr is invalid if the call was cancelled */
            if( err->code == G_IO_ERROR_CANCELLED )
                self = NULL;
        }
        else {
            log_err("null reply from dbus server");
        }
    }
    else {
        g_variant_get(rsp, "(b)", &has_owner);
        log_debug("'%s' %s owner", self->wtc_name, (has_owner ? "has" :  "doesn't have"));
    }

    if( self ) {
        change_cancellable_steal(&self->wtc_cancellable, NULL);
        if( !has_owner )
            watcher_notify_name_lost(self);
    }
    if( rsp )
        g_variant_unref(rsp);
    g_clear_error(&err);
}

static void
watcher_notify_name_lost(watcher_t *self)
{
    gchar *name = g_strdup(self->wtc_name);
    prompter_handle_name_lost(self->wtc_prompter, name);
    g_free(name);
}
