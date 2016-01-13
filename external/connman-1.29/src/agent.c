/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <gdbus.h>
#include <connman/agent.h>
#include <connman/setting.h>

#include "connman.h"

#define agent_ref(agent) \
	agent_ref_debug(agent, __FILE__, __LINE__, __func__)
#define agent_unref(agent) \
	agent_unref_debug(agent, __FILE__, __LINE__, __func__)

static DBusConnection *connection = NULL;
static GHashTable *agent_hash = NULL;
static struct connman_agent *default_agent = NULL;

struct connman_agent {
	int refcount;
	char *owner;
	char *path;
	struct connman_agent_request *pending;
	GList *queue;	/* queued requests for this agent */
	guint watch;
};

struct connman_agent_request {
	void *user_context;
	void *user_data;
	DBusMessage *msg;
	DBusPendingCall *call;
	int timeout;
	agent_queue_cb callback;
	struct connman_agent_driver *driver;
};

static GSList *driver_list = NULL;

void *connman_agent_get_info(const char *dbus_sender, const char **sender,
							const char **path)
{
	struct connman_agent *agent;

	if (!dbus_sender)
		agent = default_agent;
	else {
		agent = g_hash_table_lookup(agent_hash, dbus_sender);
		if (!agent)
			agent = default_agent;
	}

	if (agent) {
		if (sender)
			*sender = agent->owner;
		if (path)
			*path = agent->path;
	} else {
		if (sender)
			*sender = NULL;
		if (path)
			*path = NULL;
	}

	return agent;
}

static void agent_request_free(struct connman_agent_request *request)
{
	if (!request)
		return;

	if (request->user_context) {
		if (request->driver && request->driver->context_unref)
			request->driver->context_unref(request->user_context);
	}

	if (request->msg)
		dbus_message_unref(request->msg);

	if (request->call) {
		dbus_pending_call_cancel(request->call);
		dbus_pending_call_unref(request->call);
	}

	g_free(request);
}

static void agent_finalize_pending(struct connman_agent *agent,
				DBusMessage *reply)
{
	struct connman_agent_request *pending = agent->pending;
	if (pending) {
		agent->pending = NULL;
		pending->callback(reply, pending->user_data);
		agent_request_free(pending);
	}
}

static void agent_receive_message(DBusPendingCall *call, void *user_data);

static int agent_send_next_request(struct connman_agent *agent)
{
	if (agent->pending)
		return -EBUSY;

	if (!agent->queue)
		return 0;

	agent->pending = agent->queue->data;
	agent->queue = g_list_remove(agent->queue, agent->pending);

	if (!agent->pending->msg)
		goto fail;

	if (!dbus_connection_send_with_reply(connection, agent->pending->msg,
						&agent->pending->call,
						agent->pending->timeout))
		goto fail;

	if (!agent->pending->call)
		goto fail;

	if (!dbus_pending_call_set_notify(agent->pending->call,
						agent_receive_message,
						agent, NULL))
		goto fail;

	dbus_message_unref(agent->pending->msg);
	agent->pending->msg = NULL;
	return 0;

fail:
	agent_finalize_pending(agent, NULL);
	return -ESRCH;
}

static int send_cancel_request(struct connman_agent *agent,
			struct connman_agent_request *request)
{
	DBusMessage *message;

	DBG("send cancel req to %s %s", agent->owner, agent->path);

	message = dbus_message_new_method_call(agent->owner,
					agent->path,
					request->driver->interface,
					"Cancel");
	if (!message) {
		connman_error("Couldn't allocate D-Bus message");
		return -ENOMEM;
	}

	g_dbus_send_message(connection, message);
	return 0;
}

static void agent_receive_message(DBusPendingCall *call, void *user_data)
{
	struct connman_agent *agent = user_data;
	DBusMessage *reply;
	int err;

	DBG("agent %p req %p", agent, agent->pending);

	reply = dbus_pending_call_steal_reply(call);
	dbus_pending_call_unref(call);
	agent->pending->call = NULL;

	if (dbus_message_is_error(reply,
			"org.freedesktop.DBus.Error.Timeout") ||
			dbus_message_is_error(reply,
			"org.freedesktop.DBus.Error.TimedOut")) {
		send_cancel_request(agent, agent->pending);
	}

	agent_finalize_pending(agent, reply);
	dbus_message_unref(reply);

	err = agent_send_next_request(agent);
	if (err < 0 && err != -EBUSY)
		DBG("send next request failed (%s/%d)", strerror(-err), -err);
}

static struct connman_agent_driver *get_driver(void)
{
	return g_slist_nth_data(driver_list, 0);
}

int connman_agent_queue_message(void *user_context,
				DBusMessage *msg, int timeout,
				agent_queue_cb callback, void *user_data,
				void *agent_data)
{
	struct connman_agent_request *queue_data;
	struct connman_agent_driver *driver;
	struct connman_agent *agent = agent_data;
	int err;

	if (!user_context || !callback)
		return -EBADMSG;

	queue_data = g_new0(struct connman_agent_request, 1);
	if (!queue_data)
		return -ENOMEM;

	driver = get_driver();
	DBG("driver %p", driver);

	if (driver && driver->context_ref) {
		queue_data->user_context = driver->context_ref(user_context);
		queue_data->driver = driver;
	} else
		queue_data->user_context = user_context;

	queue_data->msg = dbus_message_ref(msg);
	queue_data->timeout = timeout;
	queue_data->callback = callback;
	queue_data->user_data = user_data;
	agent->queue = g_list_append(agent->queue, queue_data);

	err = agent_send_next_request(agent);
	if (err < 0 && err != -EBUSY)
		DBG("send next request failed (%s/%d)", strerror(-err), -err);

	return err;
}

static void set_default_agent(void)
{
	struct connman_agent *agent = NULL;
	GHashTableIter iter;
	gpointer key, value;

	if (default_agent)
		return;

	g_hash_table_iter_init(&iter, agent_hash);
	if (g_hash_table_iter_next(&iter, &key, &value))
		agent = value;

	if (agent)
		DBG("default agent set to %s %s", agent->owner, agent->path);
	else
		DBG("default agent cleared");

	default_agent = agent;
}

static void agent_disconnect(DBusConnection *conn, void *user_data)
{
	struct connman_agent *agent = user_data;

	DBG("agent %s disconnected", agent->owner);

	if (agent->watch > 0) {
		g_dbus_remove_watch(conn, agent->watch);
		agent->watch = 0;
	}

	g_hash_table_remove(agent_hash, agent->owner);
}

static struct connman_agent *agent_ref_debug(struct connman_agent *agent,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", agent, agent->refcount + 1,
		file, line, caller);

	__sync_fetch_and_add(&agent->refcount, 1);

	return agent;
}

static struct connman_agent *agent_create(const char *name, const char *path)
{
	struct connman_agent *agent;

	agent = g_new0(struct connman_agent, 1);

	agent->owner = g_strdup(name);
	agent->path = g_strdup(path);

	agent->watch = g_dbus_add_disconnect_watch(connection,
							name, agent_disconnect,
							agent, NULL);

	return agent_ref(agent);
}

int connman_agent_register(const char *sender, const char *path)
{
	struct connman_agent *agent;

	DBG("sender %s path %s", sender, path);

	agent = g_hash_table_lookup(agent_hash, sender);
	if (agent)
		return -EEXIST;

	agent = agent_create(sender, path);
	if (!agent)
		return -EINVAL;

	DBG("agent %s", agent->owner);

	g_hash_table_replace(agent_hash, agent->owner, agent);

	if (!default_agent)
		set_default_agent();

	return 0;
}

struct report_error_data {
	void *user_context;
	report_error_cb_t callback;
	void *user_data;
};

static void report_error_reply(DBusMessage *reply, void *user_data)
{
	struct report_error_data *report_error = user_data;
	bool retry = false;
	const char *dbus_err;

	if (!reply)
		goto out;

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		dbus_err = dbus_message_get_error_name(reply);
		if (dbus_err &&
			strcmp(dbus_err,
				CONNMAN_AGENT_INTERFACE ".Error.Retry") == 0)
			retry = true;
	}

	report_error->callback(report_error->user_context, retry,
			report_error->user_data);
out:
	g_free(report_error);
}

int connman_agent_report_error_full(void *user_context, const char *path,
				const char *method, const char *error,
				report_error_cb_t callback,
				const char *dbus_sender, void *user_data)
{
	DBusMessage *message;
	DBusMessageIter iter;
	struct report_error_data *report_error;
	struct connman_agent *agent;
	int err;

	agent = connman_agent_get_info(dbus_sender, NULL, NULL);

	DBG("agent %p sender %s context %p path %s", agent,
		dbus_sender, user_context, agent ? agent->path : "-");

	if (!user_context || !agent || !agent->path || !error || !callback)
		return -ESRCH;

	message = dbus_message_new_method_call(agent->owner, agent->path,
					CONNMAN_AGENT_INTERFACE, method);
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);

	dbus_message_iter_append_basic(&iter,
				DBUS_TYPE_OBJECT_PATH, &path);
	dbus_message_iter_append_basic(&iter,
				DBUS_TYPE_STRING, &error);

	report_error = g_try_new0(struct report_error_data, 1);
	if (!report_error) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	report_error->user_context = user_context;
	report_error->callback = callback;
	report_error->user_data = user_data;

	err = connman_agent_queue_message(user_context, message,
					connman_timeout_input_request(),
					report_error_reply, report_error,
					agent);
	if (err < 0 && err != -EBUSY) {
		DBG("error %d sending error request", err);
		g_free(report_error);
		dbus_message_unref(message);
		return -ESRCH;
	}

	dbus_message_unref(message);

	return -EINPROGRESS;
}

int connman_agent_report_error(void *user_context, const char *path,
				const char *error,
				report_error_cb_t callback,
				const char *dbus_sender, void *user_data)
{
	return connman_agent_report_error_full(user_context, path,
				"ReportError", error, callback, dbus_sender,
				user_data);
}

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_agent_driver *driver1 = a;
	const struct connman_agent_driver *driver2 = b;

	return driver2->priority - driver1->priority;
}

/**
 * connman_agent_driver_register:
 * @driver: Agent driver definition
 *
 * Register a new agent driver
 *
 * Returns: %0 on success
 */
int connman_agent_driver_register(struct connman_agent_driver *driver)
{
	DBG("Registering driver %p name %s", driver, driver->name);

	driver_list = g_slist_insert_sorted(driver_list, driver,
							compare_priority);

	return 0;
}

static void release_driver(void)
{
	connman_agent_driver_unregister(get_driver());
}

static void cancel_all_requests(struct connman_agent *agent)
{
	GList *list;

	DBG("request %p pending %p", agent->pending, agent->queue);

	if (agent->pending) {
		if (agent->pending->call)
			send_cancel_request(agent, agent->pending);

		agent_finalize_pending(agent, NULL);
	}

	for (list = agent->queue; list; list = list->next) {
		struct connman_agent_request *request = list->data;

		if (!request)
			continue;

		request->callback(NULL, request->user_data);
		agent_request_free(request);
	}

	g_list_free(agent->queue);
	agent->queue = NULL;
}

void connman_agent_cancel(void *user_context)
{
	GHashTableIter iter;
	gpointer key, value;
	int err;

	DBG("context %p", user_context);

	g_hash_table_iter_init(&iter, agent_hash);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		GList *list, *next;
		struct connman_agent *agent = value;

		/*
		 * Cancel all the pending requests to a given agent and service
		 */
		list = agent->queue;
		while (list) {
			struct connman_agent_request *request = list->data;

			next = list->next;

			if (request && request->user_context &&
						request->user_context ==
								user_context) {
				DBG("cancel pending %p", request);

				request->callback(NULL, request->user_data);

				agent_request_free(request);

				agent->queue = g_list_delete_link(agent->queue,
									list);
			}

			list = next;
		}

		/*
		 * If there is a request from client to a given service,
		 * we need to cancel it.
		 */
		if (agent->pending && agent->pending->user_context &&
				agent->pending->user_context == user_context) {
			DBG("cancel request %p", agent->pending);

			if (agent->pending->call)
				send_cancel_request(agent, agent->pending);

			agent_finalize_pending(agent, NULL);

			err = agent_send_next_request(agent);
			if (err < 0 && err != -EBUSY)
				DBG("send next request failed (%s/%d)",
						strerror(-err), -err);
		}
	}
}

static void agent_unref_debug(struct connman_agent *agent,
			const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", agent, agent->refcount - 1,
		file, line, caller);

	if (__sync_fetch_and_sub(&agent->refcount, 1) != 1)
		return;

	cancel_all_requests(agent);

	g_free(agent->owner);
	g_free(agent->path);

	if (agent == default_agent) {
		default_agent = NULL;
		set_default_agent();
	}

	g_free(agent);
}

static void agent_release(struct connman_agent *agent, const char *interface)
{
	DBusMessage *message;

	DBG("release agent %s %s", agent->owner, agent->path);

	message = dbus_message_new_method_call(agent->owner, agent->path,
						interface, "Release");
	if (message == NULL) {
		connman_error("Couldn't allocate D-Bus message");
		return;
	}

	dbus_message_set_no_reply(message, TRUE);
	g_dbus_send_message(connection, message);
}

static void release_agents(void)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, agent_hash);
	while (g_hash_table_iter_next(&iter, &key, &value))
		agent_release(value, get_driver()->interface);
}

/**
 * connman_agent_driver_unregister:
 * @driver: Agent driver definition
 *
 * Remove a previously registered agent driver
 */
void connman_agent_driver_unregister(struct connman_agent_driver *driver)
{
	GSList *list;

	if (!driver)
		return;

	DBG("Unregistering driver %p name %s", driver, driver->name);

	release_agents();

	for (list = driver_list; list; list = list->next) {
		if (driver != list->data)
			continue;

		g_hash_table_remove_all(agent_hash);
		break;
	}

	driver_list = g_slist_remove(driver_list, driver);
}

static void agent_destroy(gpointer data)
{
	struct connman_agent *agent = data;

	DBG("agent %s req %p", agent->owner, agent->pending);

	if (agent->watch > 0) {
		g_dbus_remove_watch(connection, agent->watch);
		agent->watch = 0;
	}

	agent_unref(agent);
}

int connman_agent_unregister(const char *sender, const char *path)
{
	DBG("sender %s path %s", sender, path);

	if (!g_hash_table_remove(agent_hash, sender))
		return -ESRCH;

	return 0;
}

int __connman_agent_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();
	if (!connection)
		return -EINVAL;

	agent_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, agent_destroy);
	if (!agent_hash)
		return -ENOMEM;

	return 0;
}

void __connman_agent_cleanup(void)
{
	DBG("");

	if (!connection)
		return;

	g_hash_table_destroy(agent_hash);

	release_driver();

	dbus_connection_unref(connection);
	connection = NULL;
}
