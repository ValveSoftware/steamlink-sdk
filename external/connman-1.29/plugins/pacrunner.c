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
#include <string.h>

#include <gdbus.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/notifier.h>
#include <connman/dbus.h>
#include <connman/log.h>
#include <connman/proxy.h>

#define PACRUNNER_SERVICE	"org.pacrunner"
#define PACRUNNER_INTERFACE	"org.pacrunner.Manager"
#define PACRUNNER_PATH		"/org/pacrunner/manager"

#define PACRUNNER_CLIENT_INTERFACE	"org.pacrunner.Client"
#define PACRUNNER_CLIENT_PATH		"/org/pacrunner/client"

#define DBUS_TIMEOUT	5000

struct proxy_data {
	struct connman_service *service;
	char *url;
};

static DBusConnection *connection;
static dbus_bool_t daemon_running = FALSE;

static struct connman_service *default_service = NULL;
static char *current_config = NULL;

static void create_config_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	const char *path;

	DBG("");

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		connman_error("Failed to create proxy configuration");
		goto done;
	}

	if (!dbus_message_get_args(reply, NULL, DBUS_TYPE_OBJECT_PATH,
					&path, DBUS_TYPE_INVALID))
		goto done;

	g_free(current_config);
	current_config = g_strdup(path);

done:
	dbus_message_unref(reply);
}

static void append_string(DBusMessageIter *iter, void *user_data)
{
	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, user_data);
}

static void append_string_list(DBusMessageIter *iter, void *user_data)
{
	char **list = user_data;
	int i;

	for (i = 0; list[i]; i++)
		dbus_message_iter_append_basic(iter,
					DBUS_TYPE_STRING, &list[i]);
}

static void create_proxy_configuration(void)
{
	DBusMessage *msg;
	DBusMessageIter iter, dict;
	DBusPendingCall *call;
	dbus_bool_t result;
	char *interface;
	const char *method;
	const char *str;
	char **str_list;

	if (!default_service)
		return;

	DBG("");

	msg = dbus_message_new_method_call(PACRUNNER_SERVICE, PACRUNNER_PATH,
			PACRUNNER_INTERFACE, "CreateProxyConfiguration");
	if (!msg)
		return;

	dbus_message_set_auto_start(msg, FALSE);

	dbus_message_iter_init_append(msg, &iter);
	connman_dbus_dict_open(&iter, &dict);

	switch(connman_service_get_proxy_method(default_service)) {
	case CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN:
		connman_dbus_dict_close(&iter, &dict);
		goto done;
	case CONNMAN_SERVICE_PROXY_METHOD_DIRECT:
		method= "direct";
		break;
	case CONNMAN_SERVICE_PROXY_METHOD_MANUAL:
		method = "manual";

		str_list = connman_service_get_proxy_servers(default_service);
		if (!str_list) {
			connman_dbus_dict_close(&iter, &dict);
			goto done;
		}

		connman_dbus_dict_append_array(&dict, "Servers",
					DBUS_TYPE_STRING, append_string_list,
					str_list);
		g_strfreev(str_list);

		str_list = connman_service_get_proxy_excludes(default_service);
		if (!str_list)
			break;

		connman_dbus_dict_append_array(&dict, "Excludes",
					DBUS_TYPE_STRING, append_string_list,
					str_list);
		g_strfreev(str_list);

		break;
	case CONNMAN_SERVICE_PROXY_METHOD_AUTO:
		method = "auto";

		str = connman_service_get_proxy_url(default_service);
		if (!str) {
			str = connman_service_get_proxy_autoconfig(
							default_service);
			if (!str) {
				connman_dbus_dict_close(&iter, &dict);
				goto done;
			}
		}

		connman_dbus_dict_append_basic(&dict, "URL",
					DBUS_TYPE_STRING, &str);
		break;
	}

	connman_dbus_dict_append_basic(&dict, "Method",
				DBUS_TYPE_STRING, &method);

	interface = connman_service_get_interface(default_service);
	if (interface) {
		connman_dbus_dict_append_basic(&dict, "Interface",
						DBUS_TYPE_STRING, &interface);
		g_free(interface);
	}

	str = connman_service_get_domainname(default_service);
	if (str)
		connman_dbus_dict_append_array(&dict, "Domains",
					DBUS_TYPE_STRING, append_string, &str);

	str_list = connman_service_get_nameservers(default_service);
	if (str_list)
		connman_dbus_dict_append_array(&dict, "Nameservers",
					DBUS_TYPE_STRING, append_string_list,
					str_list);
	g_strfreev(str_list);

	connman_dbus_dict_close(&iter, &dict);

	result = dbus_connection_send_with_reply(connection, msg,
							&call, DBUS_TIMEOUT);

	if (!result || !call)
		goto done;

	dbus_pending_call_set_notify(call, create_config_reply, NULL, NULL);

	dbus_pending_call_unref(call);

done:
	dbus_message_unref(msg);
}

static void destroy_config_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);

	DBG("");

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
		connman_error("Failed to destoy proxy configuration");

	dbus_message_unref(reply);
}

static void destroy_proxy_configuration(void)
{
	DBusMessage *msg;
	DBusPendingCall *call;
	dbus_bool_t result;

	if (!current_config)
		return;

	DBG("");

	msg = dbus_message_new_method_call(PACRUNNER_SERVICE, PACRUNNER_PATH,
			PACRUNNER_INTERFACE, "DestroyProxyConfiguration");
	if (!msg)
		return;

	dbus_message_set_auto_start(msg, FALSE);

	dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &current_config,
							DBUS_TYPE_INVALID);

	result = dbus_connection_send_with_reply(connection, msg,
							&call, DBUS_TIMEOUT);

	dbus_message_unref(msg);

	if (!result || !call)
		return;

	dbus_pending_call_set_notify(call, destroy_config_reply, NULL, NULL);

	dbus_pending_call_unref(call);

	g_free(current_config);
	current_config = NULL;
}

static void default_service_changed(struct connman_service *service)
{
	DBG("service %p", service);

	if (service == default_service)
		return;

	default_service = service;

	if (!daemon_running)
		return;

	destroy_proxy_configuration();

	create_proxy_configuration();
}

static void proxy_changed(struct connman_service *service)
{
	DBG("service %p", service);

	if (service != default_service)
		return;

	if (!daemon_running)
		return;

	destroy_proxy_configuration();

	create_proxy_configuration();
}

static struct connman_notifier pacrunner_notifier = {
	.name			= "pacrunner",
	.default_changed	= default_service_changed,
	.proxy_changed		= proxy_changed,
};

static void pacrunner_connect(DBusConnection *conn, void *user_data)
{
	DBG("");

	daemon_running = TRUE;

	create_proxy_configuration();
}

static void pacrunner_disconnect(DBusConnection *conn, void *user_data)
{
	DBG("");

	daemon_running = FALSE;

	g_free(current_config);
	current_config = NULL;
}

static char * parse_url(const char *url)
{
	char *scheme, *host, *path, *host_ret;

	scheme = g_strdup(url);
	if (!scheme)
		return NULL;

	host = strstr(scheme, "://");
	if (host) {
		*host = '\0';
		host += 3;
	} else
		host = scheme;

	path = strchr(host, '/');
	if (path)
		*(path++) = '\0';

	host_ret = g_strdup(host);

	g_free(scheme);

	return host_ret;
}

static void request_lookup_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	struct proxy_data *data = user_data;
	const char *proxy;

	DBG("");

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		connman_error("Failed to find URL:%s", data->url);
		proxy = NULL;
		goto done;
	}

	if (!dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING,
					&proxy, DBUS_TYPE_INVALID))
		proxy = NULL;

done:
	connman_proxy_driver_lookup_notify(data->service, data->url, proxy);

	connman_service_unref(data->service);

	g_free(data->url);
	g_free(data);

	dbus_message_unref(reply);
}

static int request_lookup(struct connman_service *service, const char *url)
{
	DBusMessage *msg;
	DBusPendingCall *call;
	dbus_bool_t result;
	char *host;
	struct proxy_data *data;

	DBG("");

	msg = dbus_message_new_method_call(PACRUNNER_SERVICE,
						PACRUNNER_CLIENT_PATH,
						PACRUNNER_CLIENT_INTERFACE,
						"FindProxyForURL");
	if (!msg)
		return -1;

	host = parse_url(url);
	if (!host) {
		dbus_message_unref(msg);
		return -EINVAL;
	}

	data = g_try_new0(struct proxy_data, 1);
	if (!data) {
		dbus_message_unref(msg);
		g_free(host);
		return -ENOMEM;
	}

	data->url = g_strdup(url);
	data->service = connman_service_ref(service);

	dbus_message_append_args(msg, DBUS_TYPE_STRING, &url,
					DBUS_TYPE_STRING, &host,
					DBUS_TYPE_INVALID);

	result = dbus_connection_send_with_reply(connection, msg,
							&call, DBUS_TIMEOUT);

	dbus_message_unref(msg);

	if (!result || !call) {
		g_free(host);
		g_free(data->url);
		g_free(data);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(call, request_lookup_reply,
							data, NULL);

	dbus_pending_call_unref(call);
	g_free(host);

	return 0;
}

static void cancel_lookup(struct connman_service *service, const char *url)
{
	DBG("");
}

static struct connman_proxy_driver pacrunner_proxy = {
	.name		= "pacrunnerproxy",
	.priority	= CONNMAN_PROXY_PRIORITY_HIGH,
	.request_lookup	= request_lookup,
	.cancel_lookup	= cancel_lookup,
};

static guint pacrunner_watch;

static int pacrunner_init(void)
{
	connection = connman_dbus_get_connection();
	if (!connection)
		return -EIO;

	pacrunner_watch = g_dbus_add_service_watch(connection,
					PACRUNNER_SERVICE, pacrunner_connect,
					pacrunner_disconnect, NULL, NULL);
	if (pacrunner_watch == 0) {
		dbus_connection_unref(connection);
		return -EIO;
	}

	connman_notifier_register(&pacrunner_notifier);

	connman_proxy_driver_register(&pacrunner_proxy);

	return 0;
}

static void pacrunner_exit(void)
{
	connman_proxy_driver_unregister(&pacrunner_proxy);

	connman_notifier_unregister(&pacrunner_notifier);

	g_dbus_remove_watch(connection, pacrunner_watch);

	destroy_proxy_configuration();

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(pacrunner, "PAC runner proxy plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, pacrunner_init, pacrunner_exit)
