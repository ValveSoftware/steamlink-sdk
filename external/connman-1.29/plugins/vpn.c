/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2013  Intel Corporation. All rights reserved.
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

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <glib.h>

#include <gdbus.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/technology.h>
#include <connman/plugin.h>
#include <connman/log.h>
#include <connman/dbus.h>
#include <connman/provider.h>
#include <connman/ipaddress.h>
#include <connman/vpn-dbus.h>
#include <connman/inet.h>
#include <gweb/gresolv.h>

#define DBUS_TIMEOUT 10000

static DBusConnection *connection;

static GHashTable *vpn_connections = NULL;
static guint watch;
static guint added_watch;
static guint removed_watch;
static guint property_watch;

struct vpn_route {
	int family;
	char *network;
	char *netmask;
	char *gateway;
};

struct config_create_data {
	connection_ready_cb callback;
	DBusMessage *message;
	char *path;
};

struct connection_data {
	char *path;
	char *ident;
	struct connman_provider *provider;
	int index;
	DBusPendingCall *call;
	bool connect_pending;
	struct config_create_data *cb_data;

	char *state;
	char *type;
	char *name;
	char *host;
	char **host_ip;
	char *domain;
	char **nameservers;
	bool immutable;

	GHashTable *server_routes;
	GHashTable *user_routes;
	GHashTable *setting_strings;

	struct connman_ipaddress *ip;

	GResolv *resolv;
	guint resolv_id;
};

static int set_string(struct connman_provider *provider,
					const char *key, const char *value)
{
	struct connection_data *data;

	data = connman_provider_get_data(provider);
	if (!data)
		return -EINVAL;

	DBG("data %p provider %p key %s value %s", data, provider, key, value);

	if (g_str_equal(key, "Type")) {
		g_free(data->type);
		data->type = g_strdup(value);
	} else if (g_str_equal(key, "Name")) {
		g_free(data->name);
		data->name = g_strdup(value);
	} else if (g_str_equal(key, "Host")) {
		g_free(data->host);
		data->host = g_strdup(value);
	} else if (g_str_equal(key, "VPN.Domain") ||
				g_str_equal(key, "Domain")) {
		g_free(data->domain);
		data->domain = g_strdup(value);
	} else
		g_hash_table_replace(data->setting_strings,
				g_strdup(key), g_strdup(value));
	return 0;
}

static const char *get_string(struct connman_provider *provider,
							const char *key)
{
	struct connection_data *data;

	data = connman_provider_get_data(provider);
	if (!data)
		return NULL;

	DBG("data %p provider %p key %s", data, provider, key);

	if (g_str_equal(key, "Type"))
		return data->type;
	else if (g_str_equal(key, "Name"))
		return data->name;
	else if (g_str_equal(key, "Host"))
		return data->host;
	else if (g_str_equal(key, "HostIP")) {
		if (!data->host_ip ||
				!data->host_ip[0])
			return data->host;
		else
			return data->host_ip[0];
	} else if (g_str_equal(key, "VPN.Domain"))
		return data->domain;

	return g_hash_table_lookup(data->setting_strings, key);
}

static char *get_ident(const char *path)
{
	char *pos;

	if (*path != '/')
		return NULL;

	pos = strrchr(path, '/');
	if (!pos)
		return NULL;

	return pos + 1;
}

static void cancel_host_resolv(struct connection_data *data)
{
	if (data->resolv_id != 0)
		g_resolv_cancel_lookup(data->resolv, data->resolv_id);

	data->resolv_id = 0;

	g_resolv_unref(data->resolv);
	data->resolv = NULL;
}

static gboolean remove_resolv(gpointer user_data)
{
	struct connection_data *data = user_data;

	cancel_host_resolv(data);

	return FALSE;
}

static void resolv_result(GResolvResultStatus status,
					char **results, gpointer user_data)
{
	struct connection_data *data = user_data;

	DBG("status %d", status);

	if (status == G_RESOLV_RESULT_STATUS_SUCCESS && results &&
						g_strv_length(results) > 0) {
		g_strfreev(data->host_ip);
		data->host_ip = g_strdupv(results);
	}

	/*
	 * We cannot unref the resolver here as resolv struct is manipulated
	 * by gresolv.c after we return from this callback.
	 */
	g_timeout_add_seconds(0, remove_resolv, data);

	data->resolv_id = 0;
}

static void resolv_host_addr(struct connection_data *data)
{
	if (!data->host)
		return;

	if (connman_inet_check_ipaddress(data->host) > 0)
		return;

	if (data->host_ip)
		return;

	data->resolv = g_resolv_new(0);
	if (!data->resolv) {
		DBG("Cannot resolv %s", data->host);
		return;
	}

	DBG("Trying to resolv %s", data->host);

	data->resolv_id = g_resolv_lookup_hostname(data->resolv, data->host,
						resolv_result, data);
}

static void free_config_cb_data(struct config_create_data *cb_data)
{
	if (!cb_data)
		return;

	g_free(cb_data->path);
	cb_data->path = NULL;

	if (cb_data->message) {
		dbus_message_unref(cb_data->message);
		cb_data->message = NULL;
	}

	cb_data->callback = NULL;

	g_free(cb_data);
}

static void set_provider_state(struct connection_data *data)
{
	enum connman_provider_state state = CONNMAN_PROVIDER_STATE_UNKNOWN;
	int err = 0;

	DBG("provider %p new state %s", data->provider, data->state);

	if (g_str_equal(data->state, "ready")) {
		state = CONNMAN_PROVIDER_STATE_READY;
		goto set;
	} else if (g_str_equal(data->state, "configuration")) {
		state = CONNMAN_PROVIDER_STATE_CONNECT;
	} else if (g_str_equal(data->state, "idle")) {
		state = CONNMAN_PROVIDER_STATE_IDLE;
	} else if (g_str_equal(data->state, "disconnect")) {
		err = ECONNREFUSED;
		state = CONNMAN_PROVIDER_STATE_DISCONNECT;
		goto set;
	} else if (g_str_equal(data->state, "failure")) {
		err = ECONNREFUSED;
		state = CONNMAN_PROVIDER_STATE_FAILURE;
		goto set;
	}

	connman_provider_set_state(data->provider, state);
	return;

set:
	if (data->cb_data)
		data->cb_data->callback(data->cb_data->message,
					err, data->ident);

	connman_provider_set_state(data->provider, state);

	free_config_cb_data(data->cb_data);
	data->cb_data = NULL;
}

static int create_provider(struct connection_data *data, void *user_data)
{
	struct connman_provider_driver *driver = user_data;
	int err;

	DBG("%s", data->path);

	data->provider = connman_provider_get(data->ident);
	if (!data->provider)
		return -ENOMEM;

	DBG("provider %p name %s", data->provider, data->name);

	connman_provider_set_data(data->provider, data);
	connman_provider_set_driver(data->provider, driver);

	err = connman_provider_create_service(data->provider);
	if (err == 0) {
		connman_provider_set_immutable(data->provider, data->immutable);
		if (g_str_equal(data->state, "ready")) {
			connman_provider_set_index(data->provider,
							data->index);
			if (data->ip)
				connman_provider_set_ipaddress(data->provider,
								data->ip);
		}

		set_provider_state(data);
	}

	return 0;
}

static void destroy_route(gpointer user_data)
{
	struct vpn_route *route = user_data;

	g_free(route->network);
	g_free(route->netmask);
	g_free(route->gateway);
	g_free(route);
}

static struct connection_data *create_connection_data(const char *path)
{
	struct connection_data *data;

	data = g_try_new0(struct connection_data, 1);
	if (!data)
		return NULL;

	DBG("path %s", path);

	data->path = g_strdup(path);
	data->ident = g_strdup(get_ident(path));
	data->index = -1;

	data->setting_strings = g_hash_table_new_full(g_str_hash,
						g_str_equal, g_free, g_free);

	data->server_routes = g_hash_table_new_full(g_direct_hash,
					g_str_equal, g_free, destroy_route);
	data->user_routes = g_hash_table_new_full(g_str_hash,
					g_str_equal, g_free, destroy_route);

	return data;
}

static int extract_ip(DBusMessageIter *array, int family,
						struct connection_data *data)
{
	DBusMessageIter dict;
	char *address = NULL, *gateway = NULL, *netmask = NULL, *peer = NULL;
	unsigned char prefix_len;

	if (dbus_message_iter_get_arg_type(array) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Address")) {
			dbus_message_iter_get_basic(&value, &address);
			DBG("address %s", address);
		} else if (g_str_equal(key, "Netmask")) {
			dbus_message_iter_get_basic(&value, &netmask);
			DBG("netmask %s", netmask);
		} else if (g_str_equal(key, "PrefixLength")) {
			dbus_message_iter_get_basic(&value, &netmask);
			DBG("prefix length %s", netmask);
		} else if (g_str_equal(key, "Peer")) {
			dbus_message_iter_get_basic(&value, &peer);
			DBG("peer %s", peer);
		} else if (g_str_equal(key, "Gateway")) {
			dbus_message_iter_get_basic(&value, &gateway);
			DBG("gateway %s", gateway);
		}

		dbus_message_iter_next(&dict);
	}

	connman_ipaddress_free(data->ip);
	data->ip = connman_ipaddress_alloc(family);
	if (!data->ip)
		return -ENOMEM;

	switch (family) {
	case AF_INET:
		connman_ipaddress_set_ipv4(data->ip, address, netmask,
								gateway);
		break;
	case AF_INET6:
		prefix_len = atoi(netmask);
		connman_ipaddress_set_ipv6(data->ip, address, prefix_len,
								gateway);
		break;
	default:
		return -EINVAL;
	}

	connman_ipaddress_set_peer(data->ip, peer);

	return 0;
}

static int extract_nameservers(DBusMessageIter *array,
						struct connection_data *data)
{
	DBusMessageIter entry;
	char **nameservers = NULL;
	int i = 0;

	dbus_message_iter_recurse(array, &entry);

	while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
		const char *nameserver;

		dbus_message_iter_get_basic(&entry, &nameserver);

		nameservers = g_try_renew(char *, nameservers, i + 2);
		if (!nameservers)
			return -ENOMEM;

		DBG("[%d] %s", i, nameserver);

		nameservers[i] = g_strdup(nameserver);
		if (!nameservers[i])
			return -ENOMEM;

		nameservers[++i] = NULL;

		dbus_message_iter_next(&entry);
	}

	g_strfreev(data->nameservers);
	data->nameservers = nameservers;

	return 0;
}

static int errorstr2val(const char *error) {
	if (g_strcmp0(error, CONNMAN_ERROR_INTERFACE ".InProgress") == 0)
		return -EINPROGRESS;

	if (g_strcmp0(error, CONNMAN_ERROR_INTERFACE ".AlreadyConnected") == 0)
		return -EISCONN;

	return -ECONNREFUSED;
}

static void connect_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;
	struct connection_data *data = user_data;
	struct config_create_data *cb_data = data->cb_data;

	if (!dbus_pending_call_get_completed(call))
		return;

	DBG("user_data %p path %s", user_data, cb_data ? cb_data->path : NULL);

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		int err = errorstr2val(error.name);
		if (err != -EINPROGRESS) {
			connman_error("Connect reply: %s (%s)", error.message,
								error.name);
			dbus_error_free(&error);

			DBG("data %p cb_data %p", data, cb_data);
			if (cb_data) {
				cb_data->callback(cb_data->message, err, NULL);
				free_config_cb_data(cb_data);
				data->cb_data = NULL;
			}
			goto done;
		}
		dbus_error_free(&error);
	}

	/*
	 * The vpn connection is up when we get a "ready" state
	 * property so at this point we do nothing for the provider
	 * state.
	 */

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int connect_provider(struct connection_data *data, void *user_data)
{
	DBusPendingCall *call;
	DBusMessage *message;
	struct config_create_data *cb_data = user_data;

	DBG("data %p user %p path %s", data, cb_data, data->path);

	data->connect_pending = false;

	message = dbus_message_new_method_call(VPN_SERVICE, data->path,
					VPN_CONNECTION_INTERFACE,
					VPN_CONNECT);
	if (!message)
		return -ENOMEM;

	if (!dbus_connection_send_with_reply(connection, message,
						&call, DBUS_TIMEOUT)) {
		connman_error("Unable to call %s.%s()",
			VPN_CONNECTION_INTERFACE, VPN_CONNECT);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!call) {
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (cb_data) {
		g_free(cb_data->path);
		cb_data->path = g_strdup(data->path);
	}

	dbus_pending_call_set_notify(call, connect_reply, data, NULL);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static void add_connection(const char *path, DBusMessageIter *properties,
			void *user_data)
{
	struct connection_data *data;
	int err;
	char *ident = get_ident(path);
	bool found = false;

	data = g_hash_table_lookup(vpn_connections, ident);
	if (data) {
		/*
		 * We might have a dummy connection struct here that
		 * was created by configuration_create_reply() so in
		 * that case just continue.
		 */
		if (!data->connect_pending)
			return;

		found = true;
	} else {
		data = create_connection_data(path);
		if (!data)
			return;
	}

	DBG("data %p path %s", data, path);

	while (dbus_message_iter_get_arg_type(properties) ==
			DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;
		char *str;

		dbus_message_iter_recurse(properties, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "State")) {
			dbus_message_iter_get_basic(&value, &str);
			DBG("state %s -> %s", data->state, str);
			data->state = g_strdup(str);
		} else if (g_str_equal(key, "IPv4")) {
			extract_ip(&value, AF_INET, data);
		} else if (g_str_equal(key, "IPv6")) {
			extract_ip(&value, AF_INET6, data);
		} else if (g_str_equal(key, "Name")) {
			dbus_message_iter_get_basic(&value, &str);
			data->name = g_strdup(str);
		} else if (g_str_equal(key, "Type")) {
			dbus_message_iter_get_basic(&value, &str);
			data->type = g_strdup(str);
		} else if (g_str_equal(key, "Immutable")) {
			dbus_bool_t immutable;

			dbus_message_iter_get_basic(&value, &immutable);
			data->immutable = immutable;
		} else if (g_str_equal(key, "Host")) {
			dbus_message_iter_get_basic(&value, &str);
			data->host = g_strdup(str);
		} else if (g_str_equal(key, "Domain")) {
			dbus_message_iter_get_basic(&value, &str);
			g_free(data->domain);
			data->domain = g_strdup(str);
		} else if (g_str_equal(key, "Nameservers")) {
			extract_nameservers(&value, data);
		} else if (g_str_equal(key, "Index")) {
			dbus_message_iter_get_basic(&value, &data->index);
		} else if (g_str_equal(key, "ServerRoutes")) {
			/* Ignored */
		} else if (g_str_equal(key, "UserRoutes")) {
			/* Ignored */
		} else {
			if (dbus_message_iter_get_arg_type(&value) ==
							DBUS_TYPE_STRING) {
				dbus_message_iter_get_basic(&value, &str);
				g_hash_table_replace(data->setting_strings,
						g_strdup(key), g_strdup(str));
			} else {
				DBG("unknown key %s", key);
			}
		}

		dbus_message_iter_next(properties);
	}

	if (!found)
		g_hash_table_insert(vpn_connections, g_strdup(data->ident),
									data);

	err = create_provider(data, user_data);
	if (err < 0)
		goto out;

	resolv_host_addr(data);

	if (data->nameservers)
		connman_provider_set_nameservers(data->provider,
						data->nameservers);

	if (data->domain)
		connman_provider_set_domain(data->provider,
						data->domain);

	if (data->connect_pending)
		connect_provider(data, data->cb_data);

	return;

out:
	DBG("removing %s", data->ident);
	g_hash_table_remove(vpn_connections, data->ident);
}

static void get_connections_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;
	DBusMessageIter array, dict;
	const char *signature = DBUS_TYPE_ARRAY_AS_STRING
		DBUS_STRUCT_BEGIN_CHAR_AS_STRING
		DBUS_TYPE_OBJECT_PATH_AS_STRING
		DBUS_TYPE_ARRAY_AS_STRING
		DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
		DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_VARIANT_AS_STRING
		DBUS_DICT_ENTRY_END_CHAR_AS_STRING
		DBUS_STRUCT_END_CHAR_AS_STRING;

	if (!dbus_pending_call_get_completed(call))
		return;

	DBG("");

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("%s", error.message);
		dbus_error_free(&error);
		goto done;
	}

	if (!dbus_message_has_signature(reply, signature)) {
		connman_error("vpnd signature \"%s\" does not match "
							"expected \"%s\"",
			dbus_message_get_signature(reply), signature);
		goto done;
	}

	if (!dbus_message_iter_init(reply, &array))
		goto done;

	dbus_message_iter_recurse(&array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_STRUCT) {
		DBusMessageIter value, properties;
		const char *path;

		dbus_message_iter_recurse(&dict, &value);
		dbus_message_iter_get_basic(&value, &path);

		dbus_message_iter_next(&value);
		dbus_message_iter_recurse(&value, &properties);

		add_connection(path, &properties, user_data);

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int get_connections(void *user_data)
{
	DBusPendingCall *call;
	DBusMessage *message;

	DBG("");

	message = dbus_message_new_method_call(VPN_SERVICE, "/",
					VPN_MANAGER_INTERFACE,
					GET_CONNECTIONS);
	if (!message)
		return -ENOMEM;

	if (!dbus_connection_send_with_reply(connection, message,
						&call, DBUS_TIMEOUT)) {
		connman_error("Unable to call %s.%s()", VPN_MANAGER_INTERFACE,
							GET_CONNECTIONS);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!call) {
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(call, get_connections_reply,
							user_data, NULL);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static int provider_probe(struct connman_provider *provider)
{
	return 0;
}

static void remove_connection_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;

	if (!dbus_pending_call_get_completed(call))
		return;

	DBG("");

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		/*
		 * If the returned error is NotFound, it means that we
		 * have actually removed the provider in vpnd already.
		 */
		if (!dbus_error_has_name(&error,
				CONNMAN_ERROR_INTERFACE".NotFound"))
			connman_error("%s", error.message);

		dbus_error_free(&error);
	}

	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int provider_remove(struct connman_provider *provider)
{
	DBusPendingCall *call;
	DBusMessage *message;
	struct connection_data *data;

	data = connman_provider_get_data(provider);

	DBG("provider %p data %p", provider, data);

	if (!data) {
		/*
		 * This means the provider is already removed,
		 * just ignore the dbus in this case.
		 */
		return -EALREADY;
	}

	/*
	 * When provider.c:provider_remove() calls this function,
	 * it will remove the provider itself after the call.
	 * This means that we cannot use the provider pointer later
	 * as it is no longer valid.
	 */
	data->provider = NULL;

	message = dbus_message_new_method_call(VPN_SERVICE, "/",
					VPN_MANAGER_INTERFACE,
					VPN_REMOVE);
	if (!message)
		return -ENOMEM;

	dbus_message_append_args(message, DBUS_TYPE_OBJECT_PATH, &data->path,
				NULL);

	if (!dbus_connection_send_with_reply(connection, message,
						&call, DBUS_TIMEOUT)) {
		connman_error("Unable to call %s.%s()", VPN_MANAGER_INTERFACE,
							VPN_REMOVE);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!call) {
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(call, remove_connection_reply,
							NULL, NULL);

	dbus_message_unref(message);

	return 0;
}

static int provider_connect(struct connman_provider *provider)
{
	struct connection_data *data;

	data = connman_provider_get_data(provider);
	if (!data)
		return -EINVAL;

	return connect_provider(data, NULL);
}

static void disconnect_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;

	if (!dbus_pending_call_get_completed(call))
		return;

	DBG("user %p", user_data);

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("%s", error.message);
		dbus_error_free(&error);
		goto done;
	}

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int disconnect_provider(struct connection_data *data)
{
	DBusPendingCall *call;
	DBusMessage *message;

	DBG("data %p path %s", data, data->path);

	message = dbus_message_new_method_call(VPN_SERVICE, data->path,
					VPN_CONNECTION_INTERFACE,
					VPN_DISCONNECT);
	if (!message)
		return -ENOMEM;

	if (!dbus_connection_send_with_reply(connection, message,
						&call, DBUS_TIMEOUT)) {
		connman_error("Unable to call %s.%s()",
			VPN_CONNECTION_INTERFACE, VPN_DISCONNECT);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!call) {
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(call, disconnect_reply, NULL, NULL);

	dbus_message_unref(message);

	connman_provider_set_state(data->provider,
					CONNMAN_PROVIDER_STATE_DISCONNECT);
	/*
	 * We return 0 here instead of -EINPROGRESS because
	 * __connman_service_disconnect() needs to return something
	 * to gdbus so that gdbus will not call Disconnect() more
	 * than once. This way we do not need to pass the dbus reply
	 * message around the code.
	 */
	return 0;
}

static int provider_disconnect(struct connman_provider *provider)
{
	struct connection_data *data;

	DBG("provider %p", provider);

	data = connman_provider_get_data(provider);
	if (!data)
		return -EINVAL;

	if (g_str_equal(data->state, "ready") ||
			g_str_equal(data->state, "configuration"))
		return disconnect_provider(data);

	return 0;
}

static void configuration_create_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;
	DBusMessageIter iter;
	const char *signature = DBUS_TYPE_OBJECT_PATH_AS_STRING;
	const char *path;
	char *ident;
	struct connection_data *data;
	struct config_create_data *cb_data = user_data;

	if (!dbus_pending_call_get_completed(call))
		return;

	DBG("user %p", cb_data);

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("dbus error: %s", error.message);
		dbus_error_free(&error);
		goto done;
	}

	if (!dbus_message_has_signature(reply, signature)) {
		connman_error("vpn configuration signature \"%s\" does not "
						"match expected \"%s\"",
			dbus_message_get_signature(reply), signature);
		goto done;
	}

	if (!dbus_message_iter_init(reply, &iter))
		goto done;

	dbus_message_iter_get_basic(&iter, &path);

	/*
	 * Then try to connect the VPN as expected by ConnectProvider API
	 */
	ident = get_ident(path);

	data = g_hash_table_lookup(vpn_connections, ident);
	if (!data) {
		/*
		 * Someone removed the data. We cannot really continue.
		 */
		DBG("Pending data not found for %s, cannot continue!", ident);
	} else {
		data->call = NULL;
		data->connect_pending = true;

		if (!data->cb_data)
			data->cb_data = cb_data;
		else
			DBG("Connection callback data already in use!");

		/*
		 * Connection is created in add_connections() after
		 * we have received the ConnectionAdded signal.
		 */

		DBG("cb %p msg %p", data->cb_data,
			data->cb_data ? data->cb_data->message : NULL);
	}

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static void set_dbus_ident(char *ident)
{
	int i, len = strlen(ident);

	for (i = 0; i < len; i++) {
		if (ident[i] >= '0' && ident[i] <= '9')
			continue;
		if (ident[i] >= 'a' && ident[i] <= 'z')
			continue;
		if (ident[i] >= 'A' && ident[i] <= 'Z')
			continue;
		ident[i] = '_';
	}
}

static struct vpn_route *parse_user_route(const char *user_route)
{
	char *network, *netmask;
	struct vpn_route *route = NULL;
	int family = PF_UNSPEC;
	char **elems = g_strsplit(user_route, "/", 0);

	if (!elems)
		return NULL;

	network = elems[0];
	if (!network || *network == '\0') {
		DBG("no network/netmask set");
		goto out;
	}

	netmask = elems[1];
	if (netmask && *netmask == '\0') {
		DBG("no netmask set");
		goto out;
	}

	if (g_strrstr(network, ":"))
		family = AF_INET6;
	else if (g_strrstr(network, ".")) {
		family = AF_INET;

		if (!g_strrstr(netmask, ".")) {
			/* We have netmask length */
			in_addr_t addr;
			struct in_addr netmask_in;
			unsigned char prefix_len = 32;

			if (netmask) {
				char *ptr;
				long int value = strtol(netmask, &ptr, 10);
				if (ptr != netmask && *ptr == '\0' &&
						value && value <= 32)
					prefix_len = value;
			}

			addr = 0xffffffff << (32 - prefix_len);
			netmask_in.s_addr = htonl(addr);
			netmask = inet_ntoa(netmask_in);

			DBG("network %s netmask %s", network, netmask);
		}
	}

	route = g_try_new(struct vpn_route, 1);
	if (!route)
		goto out;

	route->network = g_strdup(network);
	route->netmask = g_strdup(netmask);
	route->gateway = NULL;
	route->family = family;

out:
	g_strfreev(elems);
	return route;
}

static GSList *get_user_networks(DBusMessageIter *array)
{
	DBusMessageIter entry;
	GSList *list = NULL;

	dbus_message_iter_recurse(array, &entry);

	while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
		const char *val;
		struct vpn_route *route;

		dbus_message_iter_get_basic(&entry, &val);

		route = parse_user_route(val);
		if (route)
			list = g_slist_prepend(list, route);

		dbus_message_iter_next(&entry);
	}

	return list;
}

static void append_route(DBusMessageIter *iter, void *user_data)
{
	struct vpn_route *route = user_data;
	DBusMessageIter item;
	int family = 0;

	connman_dbus_dict_open(iter, &item);

	if (!route)
		goto empty_dict;

	if (route->family == AF_INET)
		family = 4;
	else if (route->family == AF_INET6)
		family = 6;

	if (family != 0)
		connman_dbus_dict_append_basic(&item, "ProtocolFamily",
					DBUS_TYPE_INT32, &family);

	if (route->network)
		connman_dbus_dict_append_basic(&item, "Network",
					DBUS_TYPE_STRING, &route->network);

	if (route->netmask)
		connman_dbus_dict_append_basic(&item, "Netmask",
					DBUS_TYPE_STRING, &route->netmask);

	if (route->gateway)
		connman_dbus_dict_append_basic(&item, "Gateway",
					DBUS_TYPE_STRING, &route->gateway);

empty_dict:
	connman_dbus_dict_close(iter, &item);
}

static void append_routes(DBusMessageIter *iter, void *user_data)
{
	GSList *list, *routes = user_data;

	DBG("routes %p", routes);

	for (list = routes; list; list = g_slist_next(list)) {
		DBusMessageIter dict;
		struct vpn_route *route = list->data;

		dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL,
						&dict);
		append_route(&dict, route);
		dbus_message_iter_close_container(iter, &dict);
	}
}

static int create_configuration(DBusMessage *msg, connection_ready_cb callback)
{
	DBusMessage *new_msg = NULL;
	DBusPendingCall *call;
	DBusMessageIter iter, array, new_iter, new_dict;
	const char *type = NULL, *name = NULL;
	const char *host = NULL, *domain = NULL;
	char *ident, *me = NULL;
	int err = 0;
	dbus_bool_t result;
	struct connection_data *data;
	struct config_create_data *user_data = NULL;
	GSList *networks = NULL;

	/*
	 * We copy the old message data into new message. We cannot
	 * just use the old message as is because the user route
	 * information is not in the same format in vpnd.
	 */
	new_msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
	dbus_message_iter_init_append(new_msg, &new_iter);
	connman_dbus_dict_open(&new_iter, &new_dict);

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_recurse(&iter, &array);

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		void *item_value;
		const char *key;
		int value_type;

		dbus_message_iter_recurse(&array, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		value_type = dbus_message_iter_get_arg_type(&value);
		item_value = NULL;

		switch (value_type) {
		case DBUS_TYPE_STRING:
			dbus_message_iter_get_basic(&value, &item_value);

			if (g_str_equal(key, "Type"))
				type = (const char *)item_value;
			else if (g_str_equal(key, "Name"))
				name = (const char *)item_value;
			else if (g_str_equal(key, "Host"))
				host = (const char *)item_value;
			else if (g_str_equal(key, "VPN.Domain"))
				domain = (const char *)item_value;

			DBG("%s %s", key, (char *)item_value);

			if (item_value)
				connman_dbus_dict_append_basic(&new_dict, key,
						value_type, &item_value);
			break;
		case DBUS_TYPE_ARRAY:
			if (g_str_equal(key, "Networks")) {
				networks = get_user_networks(&value);
				connman_dbus_dict_append_array(&new_dict,
							"UserRoutes",
							DBUS_TYPE_DICT_ENTRY,
							append_routes,
							networks);
			}
			break;
		}

		dbus_message_iter_next(&array);
	}

	connman_dbus_dict_close(&new_iter, &new_dict);

	DBG("VPN type %s name %s host %s domain %s networks %p",
		type, name, host, domain, networks);

	if (!host || !domain) {
		err = -EINVAL;
		goto done;
	}

	if (!type || !name) {
		err = -EOPNOTSUPP;
		goto done;
	}

	ident = g_strdup_printf("%s_%s", host, domain);
	set_dbus_ident(ident);

	DBG("ident %s", ident);

	data = g_hash_table_lookup(vpn_connections, ident);
	if (data) {
		if (data->call || data->cb_data) {
			DBG("create configuration call already pending");
			err = -EINPROGRESS;
			goto done;
		}
	} else {
		char *path = g_strdup_printf("%s/connection/%s", VPN_PATH,
								ident);
		data = create_connection_data(path);
		g_free(path);

		if (!data) {
			err = -ENOMEM;
			goto done;
		}

		g_hash_table_insert(vpn_connections, g_strdup(ident), data);
	}

	/*
	 * User called net.connman.Manager.ConnectProvider if we are here.
	 * So use the data from original message in the new msg.
	 */
	me = g_strdup(dbus_message_get_destination(msg));

	dbus_message_set_interface(new_msg, VPN_MANAGER_INTERFACE);
	dbus_message_set_path(new_msg, "/");
	dbus_message_set_destination(new_msg, VPN_SERVICE);
	dbus_message_set_sender(new_msg, me);
	dbus_message_set_member(new_msg, "Create");

	user_data = g_try_new0(struct config_create_data, 1);
	if (!user_data) {
		err = -ENOMEM;
		goto done;
	}

	user_data->callback = callback;
	user_data->message = dbus_message_ref(msg);
	user_data->path = NULL;

	DBG("cb %p msg %p", user_data, msg);

	result = dbus_connection_send_with_reply(connection, new_msg,
						&call, DBUS_TIMEOUT);
	if (!result || !call) {
		err = -EIO;
		goto done;
	}

	dbus_pending_call_set_notify(call, configuration_create_reply,
							user_data, NULL);
	data->call = call;

done:
	if (new_msg)
		dbus_message_unref(new_msg);

	if (networks)
		g_slist_free_full(networks, destroy_route);

	g_free(me);
	return err;
}

static bool check_host(char **hosts, char *host)
{
	int i;

	if (!hosts)
		return false;

	for (i = 0; hosts[i]; i++) {
		if (g_strcmp0(hosts[i], host) == 0)
			return true;
	}

	return false;
}

static void set_route(struct connection_data *data, struct vpn_route *route)
{
	/*
	 * If the VPN administrator/user has given a route to
	 * VPN server, then we must discard that because the
	 * server cannot be contacted via VPN tunnel.
	 */
	if (check_host(data->host_ip, route->network)) {
		DBG("Discarding VPN route to %s via %s at index %d",
			route->network, route->gateway, data->index);
		return;
	}

	if (route->family == AF_INET6) {
		unsigned char prefix_len = atoi(route->netmask);

		connman_inet_add_ipv6_network_route(data->index,
							route->network,
							route->gateway,
							prefix_len);
	} else {
		connman_inet_add_network_route(data->index, route->network,
						route->gateway,
						route->netmask);
	}
}

static int set_routes(struct connman_provider *provider,
				enum connman_provider_route_type type)
{
	struct connection_data *data;
	GHashTableIter iter;
	gpointer value, key;

	DBG("provider %p", provider);

	data = connman_provider_get_data(provider);
	if (!data)
		return -EINVAL;

	if (type == CONNMAN_PROVIDER_ROUTE_ALL ||
					type == CONNMAN_PROVIDER_ROUTE_USER) {
		g_hash_table_iter_init(&iter, data->user_routes);

		while (g_hash_table_iter_next(&iter, &key, &value))
			set_route(data, value);
	}

	if (type == CONNMAN_PROVIDER_ROUTE_ALL ||
				type == CONNMAN_PROVIDER_ROUTE_SERVER) {
		g_hash_table_iter_init(&iter, data->server_routes);

		while (g_hash_table_iter_next(&iter, &key, &value))
			set_route(data, value);
	}

	return 0;
}

static bool check_routes(struct connman_provider *provider)
{
	struct connection_data *data;

	DBG("provider %p", provider);

	data = connman_provider_get_data(provider);
	if (!data)
		return false;

	if (data->user_routes &&
			g_hash_table_size(data->user_routes) > 0)
		return true;

	if (data->server_routes &&
			g_hash_table_size(data->server_routes) > 0)
		return true;

	return false;
}

static struct connman_provider_driver provider_driver = {
	.name = "VPN",
	.type = CONNMAN_PROVIDER_TYPE_VPN,
	.probe = provider_probe,
	.remove = provider_remove,
	.connect = provider_connect,
	.disconnect = provider_disconnect,
	.set_property = set_string,
	.get_property = get_string,
	.create = create_configuration,
	.set_routes = set_routes,
	.check_routes = check_routes,
};

static void destroy_provider(struct connection_data *data)
{
	DBG("data %p", data);

	if (g_str_equal(data->state, "ready") ||
			g_str_equal(data->state, "configuration"))
		connman_provider_disconnect(data->provider);

	if (data->call)
		dbus_pending_call_cancel(data->call);

	connman_provider_set_data(data->provider, NULL);

	connman_provider_remove(data->provider);

	data->provider = NULL;
}

static void connection_destroy(gpointer hash_data)
{
	struct connection_data *data = hash_data;

	DBG("data %p", data);

	if (data->provider)
		destroy_provider(data);

	g_free(data->path);
	g_free(data->ident);
	g_free(data->state);
	g_free(data->type);
	g_free(data->name);
	g_free(data->host);
	g_free(data->host_ip);
	g_free(data->domain);
	g_hash_table_destroy(data->server_routes);
	g_hash_table_destroy(data->user_routes);
	g_strfreev(data->nameservers);
	g_hash_table_destroy(data->setting_strings);
	connman_ipaddress_free(data->ip);

	cancel_host_resolv(data);

	g_free(data);
}

static void vpnd_created(DBusConnection *conn, void *user_data)
{
	DBG("connection %p", conn);

	get_connections(user_data);
}

static void vpnd_removed(DBusConnection *conn, void *user_data)
{
	DBG("connection %p", conn);

	g_hash_table_remove_all(vpn_connections);
}

static void remove_connection(DBusConnection *conn, const char *path)
{
	DBG("path %s", path);

	g_hash_table_remove(vpn_connections, get_ident(path));
}

static gboolean connection_removed(DBusConnection *conn, DBusMessage *message,
				void *user_data)
{
	const char *path;
	const char *signature = DBUS_TYPE_OBJECT_PATH_AS_STRING;
	struct connection_data *data;

	if (!dbus_message_has_signature(message, signature)) {
		connman_error("vpn removed signature \"%s\" does not match "
							"expected \"%s\"",
			dbus_message_get_signature(message), signature);
		return TRUE;
	}

	dbus_message_get_args(message, NULL, DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_INVALID);

	data = g_hash_table_lookup(vpn_connections, get_ident(path));
	if (data)
		remove_connection(conn, path);

	return TRUE;
}

static gboolean connection_added(DBusConnection *conn, DBusMessage *message,
				void *user_data)
{
	DBusMessageIter iter, properties;
	const char *path;
	const char *signature = DBUS_TYPE_OBJECT_PATH_AS_STRING
		DBUS_TYPE_ARRAY_AS_STRING
		DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
		DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_VARIANT_AS_STRING
		DBUS_DICT_ENTRY_END_CHAR_AS_STRING;

	if (!dbus_message_has_signature(message, signature)) {
		connman_error("vpn ConnectionAdded signature \"%s\" does not "
						"match expected \"%s\"",
			dbus_message_get_signature(message), signature);
		return TRUE;
	}

	DBG("");

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &path);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &properties);

	add_connection(path, &properties, user_data);

	return TRUE;
}

static int save_route(GHashTable *routes, int family, const char *network,
			const char *netmask, const char *gateway)
{
	struct vpn_route *route;
	char *key = g_strdup_printf("%d/%s/%s", family, network, netmask);

	DBG("family %d network %s netmask %s", family, network, netmask);

	route = g_hash_table_lookup(routes, key);
	if (!route) {
		route = g_try_new0(struct vpn_route, 1);
		if (!route) {
			connman_error("out of memory");
			return -ENOMEM;
		}

		route->family = family;
		route->network = g_strdup(network);
		route->netmask = g_strdup(netmask);
		route->gateway = g_strdup(gateway);

		g_hash_table_replace(routes, key, route);
	} else
		g_free(key);

	return 0;
}

static int read_route_dict(GHashTable *routes, DBusMessageIter *dicts)
{
	DBusMessageIter dict;
	const char *network, *netmask, *gateway;
	int family;

	dbus_message_iter_recurse(dicts, &dict);

	network = netmask = gateway = NULL;
	family = PF_UNSPEC;

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {

		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "ProtocolFamily")) {
			int pf;
			dbus_message_iter_get_basic(&value, &pf);
			switch (pf) {
			case 4:
				family = AF_INET;
				break;
			case 6:
				family = AF_INET6;
				break;
			}
			DBG("family %d", family);
		} else if (g_str_equal(key, "Netmask")) {
			dbus_message_iter_get_basic(&value, &netmask);
			DBG("netmask %s", netmask);
		} else if (g_str_equal(key, "Network")) {
			dbus_message_iter_get_basic(&value, &network);
			DBG("host %s", network);
		} else if (g_str_equal(key, "Gateway")) {
			dbus_message_iter_get_basic(&value, &gateway);
			DBG("gateway %s", gateway);
		}

		dbus_message_iter_next(&dict);
	}

	if (!netmask || !network || !gateway) {
		DBG("Value missing.");
		return -EINVAL;
	}

	return save_route(routes, family, network, netmask, gateway);
}

static int routes_changed(DBusMessageIter *array, GHashTable *routes)
{
	DBusMessageIter entry;
	int ret = -EINVAL;

	if (dbus_message_iter_get_arg_type(array) != DBUS_TYPE_ARRAY) {
		DBG("Expecting array, ignoring routes.");
		return -EINVAL;
	}

	while (dbus_message_iter_get_arg_type(array) == DBUS_TYPE_ARRAY) {

		dbus_message_iter_recurse(array, &entry);

		while (dbus_message_iter_get_arg_type(&entry) ==
							DBUS_TYPE_STRUCT) {
			DBusMessageIter dicts;

			dbus_message_iter_recurse(&entry, &dicts);

			while (dbus_message_iter_get_arg_type(&dicts) ==
							DBUS_TYPE_ARRAY) {
				int err = read_route_dict(routes, &dicts);
				if (ret != 0)
					ret = err;
				dbus_message_iter_next(&dicts);
			}

			dbus_message_iter_next(&entry);
		}

		dbus_message_iter_next(array);
	}

	return ret;
}

static gboolean property_changed(DBusConnection *conn,
				DBusMessage *message,
				void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct connection_data *data = NULL;
	DBusMessageIter iter, value;
	bool ip_set = false;
	int err;
	char *str;
	const char *key;
	const char *signature =	DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_VARIANT_AS_STRING;

	if (!dbus_message_has_signature(message, signature)) {
		connman_error("vpn property signature \"%s\" does not match "
							"expected \"%s\"",
			dbus_message_get_signature(message), signature);
		return TRUE;
	}

	data = g_hash_table_lookup(vpn_connections, get_ident(path));
	if (!data)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	DBG("key %s", key);

	if (g_str_equal(key, "State")) {
		dbus_message_iter_get_basic(&value, &str);

		DBG("%s %s -> %s", data->path, data->state, str);

		if (g_str_equal(data->state, str))
			return TRUE;

		g_free(data->state);
		data->state = g_strdup(str);

		set_provider_state(data);
	} else if (g_str_equal(key, "Index")) {
		dbus_message_iter_get_basic(&value, &data->index);
		connman_provider_set_index(data->provider, data->index);
	} else if (g_str_equal(key, "IPv4")) {
		err = extract_ip(&value, AF_INET, data);
		ip_set = true;
	} else if (g_str_equal(key, "IPv6")) {
		err = extract_ip(&value, AF_INET6, data);
		ip_set = true;
	} else if (g_str_equal(key, "ServerRoutes")) {
		err = routes_changed(&value, data->server_routes);
		/*
		 * Note that the vpnd will delay the route sending a bit
		 * (in order to collect the routes from VPN client),
		 * so we might have got the State changed property before
		 * we got ServerRoutes. This means that we must try to set
		 * the routes here because they would be left unset otherwise.
		 */
		if (err == 0)
			set_routes(data->provider,
						CONNMAN_PROVIDER_ROUTE_SERVER);
	} else if (g_str_equal(key, "UserRoutes")) {
		err = routes_changed(&value, data->user_routes);
		if (err == 0)
			set_routes(data->provider,
						CONNMAN_PROVIDER_ROUTE_USER);
	} else if (g_str_equal(key, "Nameservers")) {
		if (extract_nameservers(&value, data) == 0 &&
						data->nameservers)
			connman_provider_set_nameservers(data->provider,
							data->nameservers);
	} else if (g_str_equal(key, "Domain")) {
		dbus_message_iter_get_basic(&value, &str);
		g_free(data->domain);
		data->domain = g_strdup(str);
		connman_provider_set_domain(data->provider, data->domain);
	}

	if (ip_set && err == 0) {
		err = connman_provider_set_ipaddress(data->provider, data->ip);
		if (err < 0)
			DBG("setting provider IP address failed (%s/%d)",
				strerror(-err), -err);
	}

	return TRUE;
}

static int vpn_init(void)
{
	int err;

	connection = connman_dbus_get_connection();
	if (!connection)
		return -EIO;

	watch = g_dbus_add_service_watch(connection, VPN_SERVICE,
			vpnd_created, vpnd_removed, &provider_driver, NULL);

	added_watch = g_dbus_add_signal_watch(connection, VPN_SERVICE, NULL,
					VPN_MANAGER_INTERFACE,
					CONNECTION_ADDED, connection_added,
					&provider_driver, NULL);

	removed_watch = g_dbus_add_signal_watch(connection, VPN_SERVICE, NULL,
					VPN_MANAGER_INTERFACE,
					CONNECTION_REMOVED, connection_removed,
					NULL, NULL);

	property_watch = g_dbus_add_signal_watch(connection, VPN_SERVICE, NULL,
					VPN_CONNECTION_INTERFACE,
					PROPERTY_CHANGED, property_changed,
					NULL, NULL);

	if (added_watch == 0 || removed_watch == 0 || property_watch == 0) {
		err = -EIO;
		goto remove;
	}

	err = connman_provider_driver_register(&provider_driver);
	if (err == 0) {
		vpn_connections = g_hash_table_new_full(g_str_hash,
						g_str_equal,
						g_free, connection_destroy);

		vpnd_created(connection, &provider_driver);
	}

	return err;

remove:
	g_dbus_remove_watch(connection, watch);
	g_dbus_remove_watch(connection, added_watch);
	g_dbus_remove_watch(connection, removed_watch);
	g_dbus_remove_watch(connection, property_watch);

	dbus_connection_unref(connection);

	return err;
}

static void vpn_exit(void)
{
	g_dbus_remove_watch(connection, watch);
	g_dbus_remove_watch(connection, added_watch);
	g_dbus_remove_watch(connection, removed_watch);
	g_dbus_remove_watch(connection, property_watch);

	connman_provider_driver_unregister(&provider_driver);

	if (vpn_connections)
		g_hash_table_destroy(vpn_connections);

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(vpn, "VPN plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, vpn_init, vpn_exit)
