/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2014  Intel Corporation. All rights reserved.
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

#include <gdbus.h>

#include "connman.h"

static DBusConnection *connection;

struct _peer_service {
	bool registered;
	const char *owner;
	DBusMessage *pending;

	GBytes *specification;
	GBytes *query;
	int version;

	bool master;
};

struct _peer_service_owner {
	char *owner;
	guint watch;
	GList *services;
};

static struct connman_peer_driver *peer_driver;

static GHashTable *owners_map;
static GHashTable *services_map;
static int peer_master;

static void reply_pending(struct _peer_service *service, int error)
{
	if (!service->pending)
		return;

	connman_dbus_reply_pending(service->pending, error, NULL);
	service->pending = NULL;
}

static struct _peer_service *find_peer_service(GBytes *specification,
					GBytes *query, int version,
					const char *owner, bool remove)
{
	struct _peer_service *service = NULL;
	struct _peer_service_owner *ps_owner;
	GList *list;

	ps_owner = g_hash_table_lookup(services_map, specification);
	if (!ps_owner)
		return NULL;

	if (owner && g_strcmp0(owner, ps_owner->owner) != 0)
		return NULL;

	for (list = ps_owner->services; list; list = list->next) {
		service = list->data;

		if (service->specification == specification)
			break;

		if (version) {
			if (!service->version)
				continue;
			if (version != service->version)
				continue;
		}

		if (query) {
			if (!service->query)
				continue;
			if (g_bytes_equal(service->query, query))
				continue;
		}

		if (g_bytes_equal(service->specification, specification))
			break;
	}

	if (!service)
		return NULL;

	if (owner && remove)
		ps_owner->services = g_list_delete_link(ps_owner->services,
									list);

	return service;
}

static void unregister_peer_service(struct _peer_service *service)
{
	gsize spec_length, query_length = 0;
	const void *spec, *query = NULL;

	if (!peer_driver || !service->specification)
		return;

	spec = g_bytes_get_data(service->specification, &spec_length);
	if (service->query)
		query = g_bytes_get_data(service->query, &query_length);

	peer_driver->unregister_service(spec, spec_length, query,
					query_length, service->version);
}

static void remove_peer_service(gpointer user_data)
{
	struct _peer_service *service = user_data;

	reply_pending(service, ECONNABORTED);

	if (service->registered)
		unregister_peer_service(service);

	if (service->specification) {
		if (service->owner) {
			find_peer_service(service->specification,
					service->query, service->version,
					service->owner, true);
		}

		g_hash_table_remove(services_map, service->specification);
		g_bytes_unref(service->specification);
	}

	if (service->query)
		g_bytes_unref(service->query);

	if (service->master)
		peer_master--;

	g_free(service);
}

static void apply_peer_service_removal(gpointer user_data)
{
	struct _peer_service *service = user_data;

	service->owner = NULL;
	remove_peer_service(user_data);
}

static void remove_peer_service_owner(gpointer user_data)
{
	struct _peer_service_owner *ps_owner = user_data;

	DBG("owner %s", ps_owner->owner);

	if (ps_owner->watch > 0)
		g_dbus_remove_watch(connection, ps_owner->watch);

	if (ps_owner->services) {
		g_list_free_full(ps_owner->services,
					apply_peer_service_removal);
	}

	g_free(ps_owner->owner);
	g_free(ps_owner);
}

static void owner_disconnect(DBusConnection *conn, void *user_data)
{
	struct _peer_service_owner *ps_owner = user_data;

	ps_owner->watch = 0;
	g_hash_table_remove(owners_map, ps_owner->owner);
}

static void service_registration_result(int result, void *user_data)
{
	struct _peer_service *service = user_data;

	reply_pending(service, -result);

	if (service->registered)
		return;

	if (result == 0) {
		service->registered = true;
		if (service->master)
			peer_master++;
		return;
	}

	remove_peer_service(service);
}

static int register_peer_service(struct _peer_service *service)
{
	gsize spec_length, query_length = 0;
	const void *spec, *query = NULL;

	if (!peer_driver)
		return 0;

	spec = g_bytes_get_data(service->specification, &spec_length);
	if (service->query)
		query = g_bytes_get_data(service->query, &query_length);

	return peer_driver->register_service(spec, spec_length, query,
					query_length, service->version,
					service_registration_result, service);
}

static void register_all_services(gpointer key, gpointer value,
						gpointer user_data)
{
	struct _peer_service_owner *ps_owner = value;
	GList *list;

	for (list = ps_owner->services; list; list = list->next) {
		struct _peer_service *service = list->data;

		if (service->registered)
			register_peer_service(service);
	}
}

void __connman_peer_service_set_driver(struct connman_peer_driver *driver)
{
	peer_driver = driver;
	if (!peer_driver)
		return;

	g_hash_table_foreach(owners_map, register_all_services, NULL);
}

int __connman_peer_service_register(const char *owner, DBusMessage *msg,
					const unsigned char *specification,
					int specification_length,
					const unsigned char *query,
					int query_length, int version,
					bool master)
{
	struct _peer_service_owner *ps_owner;
	GBytes *spec, *query_spec = NULL;
	struct _peer_service *service;
	bool new = false;
	int ret = 0;

	DBG("owner %s - spec %p/length %d - query %p/length %d - version %d",
				owner,specification, specification_length,
				query, query_length, version);

	if (!specification || specification_length == 0)
		return -EINVAL;

	ps_owner = g_hash_table_lookup(owners_map, owner);
	if (!ps_owner) {
		ps_owner = g_try_new0(struct _peer_service_owner, 1);
		if (!ps_owner)
			return -ENOMEM;

		ps_owner->owner = g_strdup(owner);
		ps_owner->watch = g_dbus_add_disconnect_watch(connection,
						owner, owner_disconnect,
						ps_owner, NULL);
		g_hash_table_insert(owners_map, ps_owner->owner, ps_owner);
		new = true;
	}

	spec = g_bytes_new(specification, specification_length);
	if (query)
		query_spec = g_bytes_new(query, query_length);

	service = find_peer_service(spec, query_spec, version, NULL, false);
	if (service) {
		DBG("Found one existing service %p", service);

		if (g_strcmp0(service->owner, owner))
			ret = -EBUSY;

		if (service->pending)
			ret = -EINPROGRESS;
		else
			ret = -EEXIST;

		service = NULL;
		goto error;
	}

	service = g_try_new0(struct _peer_service, 1);
	if (!service) {
		ret = -ENOMEM;
		goto error;
	}

	service->owner = ps_owner->owner;
	service->specification = spec;
	service->query = query_spec;
	service->version = version;
	service->master = master;

	g_hash_table_insert(services_map, spec, ps_owner);
	spec = query_spec = NULL;

	ret = register_peer_service(service);
	if (ret != 0 && ret != -EINPROGRESS)
		goto error;
	else if (ret == -EINPROGRESS)
		service->pending = dbus_message_ref(msg);
	else {
		service->registered = true;
		if (master)
			peer_master++;
	}

	ps_owner->services = g_list_prepend(ps_owner->services, service);

	return ret;
error:
	if (spec)
		g_bytes_unref(spec);
	if (query_spec)
		g_bytes_unref(query_spec);

	if (service)
		remove_peer_service(service);

	if (new)
		g_hash_table_remove(owners_map, ps_owner->owner);

	return ret;
}

int __connman_peer_service_unregister(const char *owner,
					const unsigned char *specification,
					int specification_length,
					const unsigned char *query,
					int query_length, int version)
{
	struct _peer_service_owner *ps_owner;
	GBytes *spec, *query_spec = NULL;
	struct _peer_service *service;

	DBG("owner %s - spec %p/length %d - query %p/length %d - version %d",
				owner,specification, specification_length,
				query, query_length, version);

	ps_owner = g_hash_table_lookup(owners_map, owner);
	if (!ps_owner)
		return -ESRCH;

	spec = g_bytes_new(specification, specification_length);
	if (query)
		query_spec = g_bytes_new(query, query_length);

	service = find_peer_service(spec, query_spec, version, owner, true);

	g_bytes_unref(spec);
	g_bytes_unref(query_spec);

	if (!service)
		return -ESRCH;

	remove_peer_service(service);

	if (!ps_owner->services)
		g_hash_table_remove(owners_map, ps_owner->owner);

	return 0;
}

bool connman_peer_service_is_master(void)
{
	if (!peer_master || !peer_driver)
		return false;

	return true;
}

int __connman_peer_service_init(void)
{
	DBG("");
	connection = connman_dbus_get_connection();
	if (!connection)
		return -1;

	peer_driver = NULL;

	owners_map = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
						remove_peer_service_owner);
	services_map = g_hash_table_new_full(g_bytes_hash, g_bytes_equal,
								NULL, NULL);
	peer_master = 0;

	return 0;
}

void __connman_peer_service_cleanup(void)
{
	DBG("");

	if (!connection)
		return;

	g_hash_table_destroy(owners_map);
	g_hash_table_destroy(services_map);
	peer_master = 0;

	dbus_connection_unref(connection);
	connection = NULL;
}
