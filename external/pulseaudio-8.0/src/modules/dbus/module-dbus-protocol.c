/***
  This file is part of PulseAudio.

  Copyright 2009 Tanu Kaskinen
  Copyright 2006 Lennart Poettering
  Copyright 2006 Shams E. King

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dbus/dbus.h>

#include <pulse/mainloop-api.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>

#include <pulsecore/client.h>
#include <pulsecore/core-util.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/idxset.h>
#include <pulsecore/macro.h>
#include <pulsecore/modargs.h>
#include <pulsecore/module.h>
#include <pulsecore/protocol-dbus.h>

#include "iface-client.h"
#include "iface-core.h"

#include "module-dbus-protocol-symdef.h"

PA_MODULE_DESCRIPTION("D-Bus interface");
PA_MODULE_USAGE(
        "access=local|remote|local,remote "
        "tcp_port=<port number> "
        "tcp_listen=<hostname>");
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_AUTHOR("Tanu Kaskinen");
PA_MODULE_VERSION(PACKAGE_VERSION);

enum server_type {
    SERVER_TYPE_LOCAL,
    SERVER_TYPE_TCP
};

struct server;
struct connection;

struct userdata {
    pa_module *module;
    bool local_access;
    bool remote_access;
    uint32_t tcp_port;
    char *tcp_listen;

    struct server *local_server;
    struct server *tcp_server;

    pa_idxset *connections;

    pa_defer_event *cleanup_event;

    pa_dbus_protocol *dbus_protocol;
    pa_dbusiface_core *core_iface;
};

struct server {
    struct userdata *userdata;
    enum server_type type;
    DBusServer *dbus_server;
};

struct connection {
    struct server *server;
    pa_dbus_wrap_connection *wrap_conn;
    pa_client *client;
};

static const char* const valid_modargs[] = {
    "access",
    "tcp_port",
    "tcp_listen",
    NULL
};

static void connection_free(struct connection *c) {
    pa_assert(c);

    pa_assert_se(pa_dbus_protocol_unregister_connection(c->server->userdata->dbus_protocol, pa_dbus_wrap_connection_get(c->wrap_conn)) >= 0);

    pa_client_free(c->client);
    pa_dbus_wrap_connection_free(c->wrap_conn);
    pa_xfree(c);
}

/* Called from pa_client_kill(). */
static void client_kill_cb(pa_client *c) {
    struct connection *conn;

    pa_assert(c);
    pa_assert(c->userdata);

    conn = c->userdata;
    pa_idxset_remove_by_data(conn->server->userdata->connections, conn, NULL);
    connection_free(conn);
    c->userdata = NULL;

    pa_log_info("Connection killed.");
}

/* Called from pa_client_send_event(). */
static void client_send_event_cb(pa_client *c, const char *name, pa_proplist *data) {
    struct connection *conn = NULL;
    DBusMessage *signal_msg = NULL;
    DBusMessageIter msg_iter;

    pa_assert(c);
    pa_assert(name);
    pa_assert(data);
    pa_assert(c->userdata);

    conn = c->userdata;

    pa_assert_se(signal_msg = dbus_message_new_signal(pa_dbusiface_core_get_client_path(conn->server->userdata->core_iface, c),
                                                      PA_DBUSIFACE_CLIENT_INTERFACE,
                                                      "ClientEvent"));
    dbus_message_iter_init_append(signal_msg, &msg_iter);
    pa_assert_se(dbus_message_iter_append_basic(&msg_iter, DBUS_TYPE_STRING, &name));
    pa_dbus_append_proplist(&msg_iter, data);

    pa_assert_se(dbus_connection_send(pa_dbus_wrap_connection_get(conn->wrap_conn), signal_msg, NULL));
    dbus_message_unref(signal_msg);
}

/* Called by D-Bus at the authentication phase. */
static dbus_bool_t user_check_cb(DBusConnection *connection, unsigned long uid, void *data) {
    pa_log_debug("Allowing connection by user %lu.", uid);

    return TRUE;
}

static DBusHandlerResult disconnection_filter_cb(DBusConnection *connection, DBusMessage *message, void *user_data) {
    struct connection *c = user_data;

    pa_assert(connection);
    pa_assert(message);
    pa_assert(c);

    if (dbus_message_is_signal(message, "org.freedesktop.DBus.Local", "Disconnected")) {
        /* The connection died. Now we want to free the connection object, but
         * let's wait until this message is fully processed, in case someone
         * else is interested in this signal too. */
        c->server->userdata->module->core->mainloop->defer_enable(c->server->userdata->cleanup_event, 1);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* Called by D-Bus when a new client connection is received. */
static void connection_new_cb(DBusServer *dbus_server, DBusConnection *new_connection, void *data) {
    struct server *s = data;
    struct connection *c;
    pa_client_new_data new_data;
    pa_client *client;

    pa_assert(new_connection);
    pa_assert(s);

    pa_client_new_data_init(&new_data);
    new_data.module = s->userdata->module;
    new_data.driver = __FILE__;
    pa_proplist_sets(new_data.proplist, PA_PROP_APPLICATION_NAME, "D-Bus client");
    client = pa_client_new(s->userdata->module->core, &new_data);
    pa_client_new_data_done(&new_data);

    if (!client) {
        dbus_connection_close(new_connection);
        return;
    }

    if (s->type == SERVER_TYPE_TCP || s->userdata->module->core->server_type == PA_SERVER_TYPE_SYSTEM) {
        /* FIXME: Here we allow anyone from anywhere to access the server,
         * anonymously. Access control should be configurable. */
        dbus_connection_set_unix_user_function(new_connection, user_check_cb, NULL, NULL);
        dbus_connection_set_allow_anonymous(new_connection, TRUE);
    }

    c = pa_xnew(struct connection, 1);
    c->server = s;
    c->wrap_conn = pa_dbus_wrap_connection_new_from_existing(s->userdata->module->core->mainloop, true, new_connection);
    c->client = client;

    c->client->kill = client_kill_cb;
    c->client->send_event = client_send_event_cb;
    c->client->userdata = c;

    pa_assert_se(dbus_connection_add_filter(new_connection, disconnection_filter_cb, c, NULL));

    pa_idxset_put(s->userdata->connections, c, NULL);

    pa_assert_se(pa_dbus_protocol_register_connection(s->userdata->dbus_protocol, new_connection, c->client) >= 0);
}

/* Called by PA mainloop when a D-Bus fd watch event needs handling. */
static void io_event_cb(pa_mainloop_api *mainloop, pa_io_event *e, int fd, pa_io_event_flags_t events, void *userdata) {
    unsigned int flags = 0;
    DBusWatch *watch = userdata;

#if HAVE_DBUS_WATCH_GET_UNIX_FD
    pa_assert(fd == dbus_watch_get_unix_fd(watch));
#else
    pa_assert(fd == dbus_watch_get_fd(watch));
#endif

    if (!dbus_watch_get_enabled(watch)) {
        pa_log_warn("Asked to handle disabled watch: %p %i", (void*) watch, fd);
        return;
    }

    if (events & PA_IO_EVENT_INPUT)
        flags |= DBUS_WATCH_READABLE;
    if (events & PA_IO_EVENT_OUTPUT)
        flags |= DBUS_WATCH_WRITABLE;
    if (events & PA_IO_EVENT_HANGUP)
        flags |= DBUS_WATCH_HANGUP;
    if (events & PA_IO_EVENT_ERROR)
        flags |= DBUS_WATCH_ERROR;

    dbus_watch_handle(watch, flags);
}

/* Called by PA mainloop when a D-Bus timer event needs handling. */
static void time_event_cb(pa_mainloop_api *mainloop, pa_time_event* e, const struct timeval *tv, void *userdata) {
    DBusTimeout *timeout = userdata;

    if (dbus_timeout_get_enabled(timeout)) {
        struct timeval next = *tv;
        dbus_timeout_handle(timeout);

        /* restart it for the next scheduled time */
        pa_timeval_add(&next, (pa_usec_t) dbus_timeout_get_interval(timeout) * 1000);
        mainloop->time_restart(e, &next);
    }
}

/* Translates D-Bus fd watch event flags to PA IO event flags. */
static pa_io_event_flags_t get_watch_flags(DBusWatch *watch) {
    unsigned int flags;
    pa_io_event_flags_t events = 0;

    pa_assert(watch);

    flags = dbus_watch_get_flags(watch);

    /* no watch flags for disabled watches */
    if (!dbus_watch_get_enabled(watch))
        return PA_IO_EVENT_NULL;

    if (flags & DBUS_WATCH_READABLE)
        events |= PA_IO_EVENT_INPUT;
    if (flags & DBUS_WATCH_WRITABLE)
        events |= PA_IO_EVENT_OUTPUT;

    return events | PA_IO_EVENT_HANGUP | PA_IO_EVENT_ERROR;
}

/* Called by D-Bus when a D-Bus fd watch event is added. */
static dbus_bool_t watch_add_cb(DBusWatch *watch, void *data) {
    struct server *s = data;
    pa_mainloop_api *mainloop;
    pa_io_event *ev;

    pa_assert(watch);
    pa_assert(s);

    mainloop = s->userdata->module->core->mainloop;

    ev = mainloop->io_new(
            mainloop,
#if HAVE_DBUS_WATCH_GET_UNIX_FD
            dbus_watch_get_unix_fd(watch),
#else
            dbus_watch_get_fd(watch),
#endif
            get_watch_flags(watch), io_event_cb, watch);

    dbus_watch_set_data(watch, ev, NULL);

    return TRUE;
}

/* Called by D-Bus when a D-Bus fd watch event is removed. */
static void watch_remove_cb(DBusWatch *watch, void *data) {
    struct server *s = data;
    pa_io_event *ev;

    pa_assert(watch);
    pa_assert(s);

    if ((ev = dbus_watch_get_data(watch)))
        s->userdata->module->core->mainloop->io_free(ev);
}

/* Called by D-Bus when a D-Bus fd watch event is toggled. */
static void watch_toggled_cb(DBusWatch *watch, void *data) {
    struct server *s = data;
    pa_io_event *ev;

    pa_assert(watch);
    pa_assert(s);

    pa_assert_se(ev = dbus_watch_get_data(watch));

    /* get_watch_flags() checks if the watch is enabled */
    s->userdata->module->core->mainloop->io_enable(ev, get_watch_flags(watch));
}

/* Called by D-Bus when a D-Bus timer event is added. */
static dbus_bool_t timeout_add_cb(DBusTimeout *timeout, void *data) {
    struct server *s = data;
    pa_mainloop_api *mainloop;
    pa_time_event *ev;
    struct timeval tv;

    pa_assert(timeout);
    pa_assert(s);

    if (!dbus_timeout_get_enabled(timeout))
        return FALSE;

    mainloop = s->userdata->module->core->mainloop;

    pa_gettimeofday(&tv);
    pa_timeval_add(&tv, (pa_usec_t) dbus_timeout_get_interval(timeout) * 1000);

    ev = mainloop->time_new(mainloop, &tv, time_event_cb, timeout);

    dbus_timeout_set_data(timeout, ev, NULL);

    return TRUE;
}

/* Called by D-Bus when a D-Bus timer event is removed. */
static void timeout_remove_cb(DBusTimeout *timeout, void *data) {
    struct server *s = data;
    pa_time_event *ev;

    pa_assert(timeout);
    pa_assert(s);

    if ((ev = dbus_timeout_get_data(timeout)))
        s->userdata->module->core->mainloop->time_free(ev);
}

/* Called by D-Bus when a D-Bus timer event is toggled. */
static void timeout_toggled_cb(DBusTimeout *timeout, void *data) {
    struct server *s = data;
    pa_mainloop_api *mainloop;
    pa_time_event *ev;

    pa_assert(timeout);
    pa_assert(s);

    mainloop = s->userdata->module->core->mainloop;

    pa_assert_se(ev = dbus_timeout_get_data(timeout));

    if (dbus_timeout_get_enabled(timeout)) {
        struct timeval tv;

        pa_gettimeofday(&tv);
        pa_timeval_add(&tv, (pa_usec_t) dbus_timeout_get_interval(timeout) * 1000);

        mainloop->time_restart(ev, &tv);
    } else
        mainloop->time_restart(ev, NULL);
}

static void server_free(struct server *s) {
    pa_assert(s);

    if (s->dbus_server) {
        dbus_server_disconnect(s->dbus_server);
        dbus_server_unref(s->dbus_server);
    }

    pa_xfree(s);
}

static struct server *start_server(struct userdata *u, const char *address, enum server_type type) {
    /* XXX: We assume that when we unref the DBusServer instance at module
     * shutdown, nobody else holds any references to it. If we stop assuming
     * that someday, dbus_server_set_new_connection_function,
     * dbus_server_set_watch_functions and dbus_server_set_timeout_functions
     * calls should probably register free callbacks, instead of providing NULL
     * as they do now. */

    struct server *s = NULL;
    DBusError error;

    pa_assert(u);
    pa_assert(address);

    dbus_error_init(&error);

    s = pa_xnew0(struct server, 1);
    s->userdata = u;
    s->type = type;
    s->dbus_server = dbus_server_listen(address, &error);

    if (dbus_error_is_set(&error)) {
        pa_log("dbus_server_listen() failed: %s: %s", error.name, error.message);
        goto fail;
    }

    dbus_server_set_new_connection_function(s->dbus_server, connection_new_cb, s, NULL);

    if (!dbus_server_set_watch_functions(s->dbus_server, watch_add_cb, watch_remove_cb, watch_toggled_cb, s, NULL)) {
        pa_log("dbus_server_set_watch_functions() ran out of memory.");
        goto fail;
    }

    if (!dbus_server_set_timeout_functions(s->dbus_server, timeout_add_cb, timeout_remove_cb, timeout_toggled_cb, s, NULL)) {
        pa_log("dbus_server_set_timeout_functions() ran out of memory.");
        goto fail;
    }

    return s;

fail:
    if (s)
        server_free(s);

    dbus_error_free(&error);

    return NULL;
}

static struct server *start_local_server(struct userdata *u) {
    struct server *s = NULL;
    char *address = NULL;

    pa_assert(u);

    address = pa_get_dbus_address_from_server_type(u->module->core->server_type);

    s = start_server(u, address, SERVER_TYPE_LOCAL); /* May return NULL */

    pa_xfree(address);

    return s;
}

static struct server *start_tcp_server(struct userdata *u) {
    struct server *s = NULL;
    char *address = NULL;

    pa_assert(u);

    address = pa_sprintf_malloc("tcp:host=%s,port=%u", u->tcp_listen, u->tcp_port);

    s = start_server(u, address, SERVER_TYPE_TCP); /* May return NULL */

    pa_xfree(address);

    return s;
}

static int get_access_arg(pa_modargs *ma, bool *local_access, bool *remote_access) {
    const char *value = NULL;

    pa_assert(ma);
    pa_assert(local_access);
    pa_assert(remote_access);

    if (!(value = pa_modargs_get_value(ma, "access", NULL)))
        return 0;

    if (pa_streq(value, "local")) {
        *local_access = true;
        *remote_access = false;
    } else if (pa_streq(value, "remote")) {
        *local_access = false;
        *remote_access = true;
    } else if (pa_streq(value, "local,remote")) {
        *local_access = true;
        *remote_access = true;
    } else
        return -1;

    return 0;
}

/* Frees dead client connections. */
static void cleanup_cb(pa_mainloop_api *a, pa_defer_event *e, void *userdata) {
    struct userdata *u = userdata;
    struct connection *conn = NULL;
    uint32_t idx;

    PA_IDXSET_FOREACH(conn, u->connections, idx) {
        if (!dbus_connection_get_is_connected(pa_dbus_wrap_connection_get(conn->wrap_conn))) {
            pa_idxset_remove_by_data(u->connections, conn, NULL);
            connection_free(conn);
        }
    }

    u->module->core->mainloop->defer_enable(e, 0);
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_modargs *ma = NULL;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->module = m;
    u->local_access = true;
    u->remote_access = false;
    u->tcp_port = PA_DBUS_DEFAULT_PORT;

    if (get_access_arg(ma, &u->local_access, &u->remote_access) < 0) {
        pa_log("Invalid access argument: '%s'", pa_modargs_get_value(ma, "access", NULL));
        goto fail;
    }

    if (pa_modargs_get_value_u32(ma, "tcp_port", &u->tcp_port) < 0 || u->tcp_port < 1 || u->tcp_port > 49150) {
        pa_log("Invalid tcp_port argument: '%s'", pa_modargs_get_value(ma, "tcp_port", NULL));
        goto fail;
    }

    u->tcp_listen = pa_xstrdup(pa_modargs_get_value(ma, "tcp_listen", "0.0.0.0"));

    if (u->local_access && !(u->local_server = start_local_server(u))) {
        pa_log("Starting the local D-Bus server failed.");
        goto fail;
    }

    if (u->remote_access && !(u->tcp_server = start_tcp_server(u))) {
        pa_log("Starting the D-Bus server for remote connections failed.");
        goto fail;
    }

    u->connections = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    u->cleanup_event = m->core->mainloop->defer_new(m->core->mainloop, cleanup_cb, u);
    m->core->mainloop->defer_enable(u->cleanup_event, 0);

    u->dbus_protocol = pa_dbus_protocol_get(m->core);
    u->core_iface = pa_dbusiface_core_new(m->core);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->core_iface)
        pa_dbusiface_core_free(u->core_iface);

    if (u->connections)
        pa_idxset_free(u->connections, (pa_free_cb_t) connection_free);

    /* This must not be called before the connections are freed, because if
     * there are any connections left, they will emit the
     * org.freedesktop.DBus.Local.Disconnected signal, and
     * disconnection_filter_cb() will be called. disconnection_filter_cb() then
     * tries to enable the defer event, and if it's already freed, an assertion
     * will be hit in mainloop.c. */
    if (u->cleanup_event)
        m->core->mainloop->defer_free(u->cleanup_event);

    if (u->tcp_server)
        server_free(u->tcp_server);

    if (u->local_server)
        server_free(u->local_server);

    if (u->dbus_protocol)
        pa_dbus_protocol_unref(u->dbus_protocol);

    pa_xfree(u->tcp_listen);
    pa_xfree(u);
    m->userdata = NULL;
}
