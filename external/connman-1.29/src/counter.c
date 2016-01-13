/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
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

static GHashTable *counter_table;
static GHashTable *owner_mapping;

struct connman_counter {
	char *owner;
	char *path;
	unsigned int interval;
	guint watch;
};

static void remove_counter(gpointer user_data)
{
	struct connman_counter *counter = user_data;

	DBG("owner %s path %s", counter->owner, counter->path);

	__connman_rtnl_update_interval_remove(counter->interval);

	__connman_service_counter_unregister(counter->path);

	g_free(counter->owner);
	g_free(counter->path);
	g_free(counter);
}

static void owner_disconnect(DBusConnection *conn, void *user_data)
{
	struct connman_counter *counter = user_data;

	DBG("owner %s path %s", counter->owner, counter->path);

	g_hash_table_remove(owner_mapping, counter->owner);
	g_hash_table_remove(counter_table, counter->path);
}

int __connman_counter_register(const char *owner, const char *path,
						unsigned int interval)
{
	struct connman_counter *counter;
	int err;

	DBG("owner %s path %s interval %u", owner, path, interval);

	counter = g_hash_table_lookup(counter_table, path);
	if (counter)
		return -EEXIST;

	counter = g_try_new0(struct connman_counter, 1);
	if (!counter)
		return -ENOMEM;

	counter->owner = g_strdup(owner);
	counter->path = g_strdup(path);

	err = __connman_service_counter_register(counter->path);
	if (err < 0) {
		g_free(counter->owner);
		g_free(counter->path);
		g_free(counter);
		return err;
	}

	g_hash_table_replace(counter_table, counter->path, counter);
	g_hash_table_replace(owner_mapping, counter->owner, counter);

	counter->interval = interval;
	__connman_rtnl_update_interval_add(counter->interval);

	counter->watch = g_dbus_add_disconnect_watch(connection, owner,
					owner_disconnect, counter, NULL);

	return 0;
}

int __connman_counter_unregister(const char *owner, const char *path)
{
	struct connman_counter *counter;

	DBG("owner %s path %s", owner, path);

	counter = g_hash_table_lookup(counter_table, path);
	if (!counter)
		return -ESRCH;

	if (g_strcmp0(owner, counter->owner) != 0)
		return -EACCES;

	if (counter->watch > 0)
		g_dbus_remove_watch(connection, counter->watch);

	g_hash_table_remove(owner_mapping, counter->owner);
	g_hash_table_remove(counter_table, counter->path);

	return 0;
}

void __connman_counter_send_usage(const char *path,
					DBusMessage *message)
{
	struct connman_counter *counter;

	counter = g_hash_table_lookup(counter_table, path);
	if (!counter) {
		if (message)
			dbus_message_unref(message);
		return;
	}

	dbus_message_set_destination(message, counter->owner);
	dbus_message_set_path(message, counter->path);
	dbus_message_set_interface(message, CONNMAN_COUNTER_INTERFACE);
	dbus_message_set_member(message, "Usage");
	dbus_message_set_no_reply(message, TRUE);

	g_dbus_send_message(connection, message);
}

static void release_counter(gpointer key, gpointer value, gpointer user_data)
{
	struct connman_counter *counter = value;
	DBusMessage *message;

	DBG("owner %s path %s", counter->owner, counter->path);

	if (counter->watch > 0)
		g_dbus_remove_watch(connection, counter->watch);

	message = dbus_message_new_method_call(counter->owner, counter->path,
					CONNMAN_COUNTER_INTERFACE, "Release");
	if (!message)
		return;

	dbus_message_set_no_reply(message, TRUE);

	g_dbus_send_message(connection, message);
}

int __connman_counter_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();
	if (!connection)
		return -1;

	counter_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							NULL, remove_counter);
	owner_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);

	return 0;
}

void __connman_counter_cleanup(void)
{
	DBG("");

	if (!connection)
		return;

	g_hash_table_foreach(counter_table, release_counter, NULL);

	g_hash_table_destroy(owner_mapping);
	g_hash_table_destroy(counter_table);

	dbus_connection_unref(connection);
}
