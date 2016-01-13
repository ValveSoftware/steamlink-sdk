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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gdbus.h>
#include <gweb/gresolv.h>

#include "connman.h"

static DBusConnection *connection = NULL;

static GHashTable *provider_hash = NULL;

static GSList *driver_list = NULL;

struct connman_provider {
	int refcount;
	bool immutable;
	struct connman_service *vpn_service;
	int index;
	char *identifier;
	int family;
	struct connman_provider_driver *driver;
	void *driver_data;
};

void __connman_provider_append_properties(struct connman_provider *provider,
							DBusMessageIter *iter)
{
	const char *host, *domain, *type;

	if (!provider->driver || !provider->driver->get_property)
		return;

	host = provider->driver->get_property(provider, "Host");
	domain = provider->driver->get_property(provider, "Domain");
	type = provider->driver->get_property(provider, "Type");

	if (host)
		connman_dbus_dict_append_basic(iter, "Host",
					DBUS_TYPE_STRING, &host);

	if (domain)
		connman_dbus_dict_append_basic(iter, "Domain",
					DBUS_TYPE_STRING, &domain);

	if (type)
		connman_dbus_dict_append_basic(iter, "Type", DBUS_TYPE_STRING,
						 &type);
}

struct connman_provider *
connman_provider_ref_debug(struct connman_provider *provider,
			const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", provider, provider->refcount + 1,
		file, line, caller);

	__sync_fetch_and_add(&provider->refcount, 1);

	return provider;
}

static void provider_remove(struct connman_provider *provider)
{
	if (provider->driver) {
		provider->driver->remove(provider);
		provider->driver = NULL;
	}
}

static void provider_destruct(struct connman_provider *provider)
{
	DBG("provider %p", provider);

	g_free(provider->identifier);
	g_free(provider);
}

void connman_provider_unref_debug(struct connman_provider *provider,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", provider, provider->refcount - 1,
		file, line, caller);

	if (__sync_fetch_and_sub(&provider->refcount, 1) != 1)
		return;

	provider_destruct(provider);
}

static int provider_indicate_state(struct connman_provider *provider,
					enum connman_service_state state)
{
	DBG("state %d", state);

	__connman_service_ipconfig_indicate_state(provider->vpn_service, state,
					CONNMAN_IPCONFIG_TYPE_IPV4);

	return __connman_service_ipconfig_indicate_state(provider->vpn_service,
					state, CONNMAN_IPCONFIG_TYPE_IPV6);
}

int connman_provider_disconnect(struct connman_provider *provider)
{
	int err;

	DBG("provider %p", provider);

	if (provider->driver && provider->driver->disconnect)
		err = provider->driver->disconnect(provider);
	else
		return -EOPNOTSUPP;

	if (provider->vpn_service)
		provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_DISCONNECT);

	if (err < 0) {
		if (err != -EINPROGRESS)
			return err;

		return -EINPROGRESS;
	}

	return 0;
}

int connman_provider_remove(struct connman_provider *provider)
{
	DBG("Removing VPN %s", provider->identifier);

	provider_remove(provider);

	connman_provider_set_state(provider, CONNMAN_PROVIDER_STATE_IDLE);

	g_hash_table_remove(provider_hash, provider->identifier);

	return 0;
}

int __connman_provider_connect(struct connman_provider *provider)
{
	int err;

	DBG("provider %p", provider);

	if (provider->driver && provider->driver->connect)
		err = provider->driver->connect(provider);
	else
		return -EOPNOTSUPP;

	if (err < 0) {
		if (err != -EINPROGRESS)
			return err;

		provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_ASSOCIATION);

		return -EINPROGRESS;
	}

	return 0;
}

int __connman_provider_remove_by_path(const char *path)
{
	struct connman_provider *provider;
	GHashTableIter iter;
	gpointer value, key;

	DBG("path %s", path);

	g_hash_table_iter_init(&iter, provider_hash);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		const char *srv_path;
		provider = value;

		if (!provider->vpn_service)
			continue;

		srv_path = __connman_service_get_path(provider->vpn_service);

		if (g_strcmp0(srv_path, path) == 0) {
			DBG("Removing VPN %s", provider->identifier);

			provider_remove(provider);

			connman_provider_set_state(provider,
						CONNMAN_PROVIDER_STATE_IDLE);

			g_hash_table_remove(provider_hash,
						provider->identifier);
			return 0;
		}
	}

	return -ENXIO;
}

static int set_connected(struct connman_provider *provider,
					bool connected)
{
	struct connman_service *service = provider->vpn_service;
	struct connman_ipconfig *ipconfig;

	if (!service)
		return -ENODEV;

	ipconfig = __connman_service_get_ipconfig(service, provider->family);

	if (connected) {
		if (!ipconfig) {
			provider_indicate_state(provider,
						CONNMAN_SERVICE_STATE_FAILURE);
			return -EIO;
		}

		__connman_ipconfig_address_add(ipconfig);
		__connman_ipconfig_gateway_add(ipconfig);

		provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_READY);

		if (provider->driver && provider->driver->set_routes)
			provider->driver->set_routes(provider,
						CONNMAN_PROVIDER_ROUTE_ALL);

	} else {
		if (ipconfig) {
			provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_DISCONNECT);
			__connman_ipconfig_gateway_remove(ipconfig);
		}

		provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_IDLE);
	}

	return 0;
}

int connman_provider_set_state(struct connman_provider *provider,
					enum connman_provider_state state)
{
	if (!provider || !provider->vpn_service)
		return -EINVAL;

	switch (state) {
	case CONNMAN_PROVIDER_STATE_UNKNOWN:
		return -EINVAL;
	case CONNMAN_PROVIDER_STATE_IDLE:
		return set_connected(provider, false);
	case CONNMAN_PROVIDER_STATE_CONNECT:
		return provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_ASSOCIATION);
	case CONNMAN_PROVIDER_STATE_READY:
		return set_connected(provider, true);
	case CONNMAN_PROVIDER_STATE_DISCONNECT:
		return provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_DISCONNECT);
	case CONNMAN_PROVIDER_STATE_FAILURE:
		return provider_indicate_state(provider,
					CONNMAN_SERVICE_STATE_FAILURE);
	}

	return -EINVAL;
}

int connman_provider_indicate_error(struct connman_provider *provider,
					enum connman_provider_error error)
{
	enum connman_service_error service_error;

	switch (error) {
	case CONNMAN_PROVIDER_ERROR_LOGIN_FAILED:
		service_error = CONNMAN_SERVICE_ERROR_LOGIN_FAILED;
		break;
	case CONNMAN_PROVIDER_ERROR_AUTH_FAILED:
		service_error = CONNMAN_SERVICE_ERROR_AUTH_FAILED;
		break;
	case CONNMAN_PROVIDER_ERROR_CONNECT_FAILED:
		service_error = CONNMAN_SERVICE_ERROR_CONNECT_FAILED;
		break;
	default:
		service_error = CONNMAN_SERVICE_ERROR_UNKNOWN;
		break;
	}

	return __connman_service_indicate_error(provider->vpn_service,
							service_error);
}

int connman_provider_create_service(struct connman_provider *provider)
{
	if (provider->vpn_service) {
		bool connected;

		connected = __connman_service_is_connected_state(
			provider->vpn_service, CONNMAN_IPCONFIG_TYPE_IPV4);
		if (connected)
			return -EALREADY;

		connected = __connman_service_is_connected_state(
			provider->vpn_service, CONNMAN_IPCONFIG_TYPE_IPV6);
		if (connected)
			return -EALREADY;

		return 0;
	}

	provider->vpn_service =
		__connman_service_create_from_provider(provider);

	if (!provider->vpn_service) {
		connman_warn("service creation failed for provider %s",
			provider->identifier);

		g_hash_table_remove(provider_hash, provider->identifier);
		return -EOPNOTSUPP;
	}

	return 0;
}

bool __connman_provider_is_immutable(struct connman_provider *provider)

{
	if (provider)
		return provider->immutable;

	return false;
}

int connman_provider_set_immutable(struct connman_provider *provider,
						bool immutable)
{
	if (!provider)
		return -EINVAL;

	provider->immutable = immutable;

	return 0;
}

static struct connman_provider *provider_lookup(const char *identifier)
{
	return g_hash_table_lookup(provider_hash, identifier);
}

static void connection_ready(DBusMessage *msg, int error_code, void *user_data)
{
	DBusMessage *reply;
	const char *identifier = user_data;

	DBG("msg %p error %d", msg, error_code);

	if (error_code != 0) {
		reply = __connman_error_failed(msg, -error_code);
		if (!g_dbus_send_message(connection, reply))
			DBG("reply %p send failed", reply);
	} else {
		const char *path;
		struct connman_provider *provider;

		provider = provider_lookup(identifier);
		if (!provider) {
			reply = __connman_error_failed(msg, EINVAL);
			g_dbus_send_message(connection, reply);
			return;
		}

		path = __connman_service_get_path(provider->vpn_service);

		g_dbus_send_reply(connection, msg,
				DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_INVALID);
	}
}

int __connman_provider_create_and_connect(DBusMessage *msg)
{
	struct connman_provider_driver *driver;

	if (!driver_list)
		return -EINVAL;

	driver = driver_list->data;
	if (!driver || !driver->create)
		return -EINVAL;

	DBG("msg %p", msg);

	return driver->create(msg, connection_ready);
}

const char *__connman_provider_get_ident(struct connman_provider *provider)
{
	if (!provider)
		return NULL;

	return provider->identifier;
}

int connman_provider_set_string(struct connman_provider *provider,
					const char *key, const char *value)
{
	if (provider->driver && provider->driver->set_property)
		return provider->driver->set_property(provider, key, value);

	return 0;
}

const char *connman_provider_get_string(struct connman_provider *provider,
							const char *key)
{
	if (provider->driver && provider->driver->get_property)
		return provider->driver->get_property(provider, key);

	return NULL;
}

bool
__connman_provider_check_routes(struct connman_provider *provider)
{
	if (!provider)
		return false;

	if (provider->driver && provider->driver->check_routes)
		return provider->driver->check_routes(provider);

	return false;
}

void *connman_provider_get_data(struct connman_provider *provider)
{
	return provider->driver_data;
}

void connman_provider_set_data(struct connman_provider *provider, void *data)
{
	provider->driver_data = data;
}

void connman_provider_set_index(struct connman_provider *provider, int index)
{
	struct connman_service *service = provider->vpn_service;
	struct connman_ipconfig *ipconfig;

	DBG("");

	if (!service)
		return;

	ipconfig = __connman_service_get_ip4config(service);

	if (!ipconfig) {
		connman_service_create_ip4config(service, index);

		ipconfig = __connman_service_get_ip4config(service);
		if (!ipconfig) {
			DBG("Couldnt create ipconfig");
			goto done;
		}
	}

	__connman_ipconfig_set_method(ipconfig, CONNMAN_IPCONFIG_METHOD_OFF);
	__connman_ipconfig_set_index(ipconfig, index);

	ipconfig = __connman_service_get_ip6config(service);

	if (!ipconfig) {
		connman_service_create_ip6config(service, index);

		ipconfig = __connman_service_get_ip6config(service);
		if (!ipconfig) {
			DBG("Couldnt create ipconfig for IPv6");
			goto done;
		}
	}

	__connman_ipconfig_set_method(ipconfig, CONNMAN_IPCONFIG_METHOD_OFF);
	__connman_ipconfig_set_index(ipconfig, index);

done:
	provider->index = index;
}

int connman_provider_get_index(struct connman_provider *provider)
{
	return provider->index;
}

int connman_provider_set_ipaddress(struct connman_provider *provider,
					struct connman_ipaddress *ipaddress)
{
	struct connman_ipconfig *ipconfig = NULL;

	ipconfig = __connman_service_get_ipconfig(provider->vpn_service,
							ipaddress->family);
	if (!ipconfig)
		return -EINVAL;

	provider->family = ipaddress->family;

	__connman_ipconfig_set_method(ipconfig, CONNMAN_IPCONFIG_METHOD_FIXED);

	__connman_ipconfig_set_local(ipconfig, ipaddress->local);
	__connman_ipconfig_set_peer(ipconfig, ipaddress->peer);
	__connman_ipconfig_set_broadcast(ipconfig, ipaddress->broadcast);
	__connman_ipconfig_set_gateway(ipconfig, ipaddress->gateway);
	__connman_ipconfig_set_prefixlen(ipconfig, ipaddress->prefixlen);

	return 0;
}

int connman_provider_set_pac(struct connman_provider *provider, const char *pac)
{
	DBG("provider %p pac %s", provider, pac);

	__connman_service_set_pac(provider->vpn_service, pac);

	return 0;
}


int connman_provider_set_domain(struct connman_provider *provider,
					const char *domain)
{
	DBG("provider %p domain %s", provider, domain);

	__connman_service_set_domainname(provider->vpn_service, domain);

	return 0;
}

int connman_provider_set_nameservers(struct connman_provider *provider,
					char * const *nameservers)
{
	int i;

	DBG("provider %p nameservers %p", provider, nameservers);

	__connman_service_nameserver_clear(provider->vpn_service);

	if (!nameservers)
		return 0;

	for (i = 0; nameservers[i]; i++)
		__connman_service_nameserver_append(provider->vpn_service,
						nameservers[i], false);

	return 0;
}

static void unregister_provider(gpointer data)
{
	struct connman_provider *provider = data;

	DBG("provider %p service %p", provider, provider->vpn_service);

	if (provider->vpn_service) {
		connman_service_unref(provider->vpn_service);
		provider->vpn_service = NULL;
	}

	connman_provider_unref(provider);
}

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	return 0;
}

int connman_provider_driver_register(struct connman_provider_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_insert_sorted(driver_list, driver,
							compare_priority);
	return 0;
}

void connman_provider_driver_unregister(struct connman_provider_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_remove(driver_list, driver);
}

void connman_provider_set_driver(struct connman_provider *provider,
				struct connman_provider_driver *driver)
{
	provider->driver = driver;
}

static void provider_disconnect_all(gpointer key, gpointer value,
						gpointer user_data)
{
	struct connman_provider *provider = value;

	connman_provider_disconnect(provider);
}

static void provider_offline_mode(bool enabled)
{
	DBG("enabled %d", enabled);

	if (enabled)
		g_hash_table_foreach(provider_hash, provider_disconnect_all,
									NULL);

}

static void provider_initialize(struct connman_provider *provider)
{
	DBG("provider %p", provider);

	provider->index = 0;
	provider->identifier = NULL;
}

static struct connman_provider *provider_new(void)
{
	struct connman_provider *provider;

	provider = g_try_new0(struct connman_provider, 1);
	if (!provider)
		return NULL;

	provider->refcount = 1;

	DBG("provider %p", provider);
	provider_initialize(provider);

	return provider;
}

struct connman_provider *connman_provider_get(const char *identifier)
{
	struct connman_provider *provider;

	provider = g_hash_table_lookup(provider_hash, identifier);
	if (provider)
		return provider;

	provider = provider_new();
	if (!provider)
		return NULL;

	DBG("provider %p", provider);

	provider->identifier = g_strdup(identifier);

	g_hash_table_insert(provider_hash, provider->identifier, provider);

	return provider;
}

void connman_provider_put(struct connman_provider *provider)
{
	g_hash_table_remove(provider_hash, provider->identifier);
}

static struct connman_provider *provider_get(int index)
{
	GHashTableIter iter;
	gpointer value, key;

	g_hash_table_iter_init(&iter, provider_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct connman_provider *provider = value;

		if (provider->index == index)
			return provider;
	}

	return NULL;
}

static void provider_service_changed(struct connman_service *service,
				enum connman_service_state state)
{
	struct connman_provider *provider;
	int vpn_index, service_index;

	if (!service)
		return;

	switch (state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_IDLE:
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
	case CONNMAN_SERVICE_STATE_READY:
	case CONNMAN_SERVICE_STATE_ONLINE:
		return;
	case CONNMAN_SERVICE_STATE_DISCONNECT:
	case CONNMAN_SERVICE_STATE_FAILURE:
		break;
	}

	service_index = __connman_service_get_index(service);

	vpn_index = __connman_connection_get_vpn_index(service_index);

	DBG("service %p %s state %d index %d/%d", service,
		__connman_service_get_ident(service),
		state, service_index, vpn_index);

	if (vpn_index < 0)
		return;

	provider = provider_get(vpn_index);
	if (!provider)
		return;

	DBG("disconnect %p index %d", provider, vpn_index);

	connman_provider_disconnect(provider);

	return;
}

static struct connman_notifier provider_notifier = {
	.name			= "provider",
	.offline_mode		= provider_offline_mode,
	.service_state_changed	= provider_service_changed,
};

int __connman_provider_init(void)
{
	int err;

	DBG("");

	connection = connman_dbus_get_connection();

	provider_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, unregister_provider);

	err = connman_notifier_register(&provider_notifier);
	if (err < 0) {
		g_hash_table_destroy(provider_hash);
		dbus_connection_unref(connection);
	}

	return err;
}

void __connman_provider_cleanup(void)
{
	DBG("");

	connman_notifier_unregister(&provider_notifier);

	g_hash_table_destroy(provider_hash);
	provider_hash = NULL;

	dbus_connection_unref(connection);
}
