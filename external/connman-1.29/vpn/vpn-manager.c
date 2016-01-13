/*
 *
 *  ConnMan VPN daemon
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

#include <errno.h>

#include <gdbus.h>
#include <connman/log.h>
#include <connman/agent.h>

#include "../src/connman.h"

#include "vpn.h"
#include "connman/vpn-dbus.h"

static int vpn_connect_count;
static DBusConnection *connection;

static DBusMessage *create(DBusConnection *conn, DBusMessage *msg, void *data)
{
	int err;

	DBG("conn %p", conn);

	err = __vpn_provider_create(msg);
	if (err < 0) {
		if (err == -EINPROGRESS) {
			connman_error("Invalid return code (%d) "
					"from connect", err);
			err = -EINVAL;
		}

		return __connman_error_failed(msg, -err);
	}

	return NULL;
}

static DBusMessage *remove(DBusConnection *conn, DBusMessage *msg, void *data)
{
	const char *path;
	int err;

	dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
							DBUS_TYPE_INVALID);

	DBG("conn %p path %s", conn, path);

	err = __vpn_provider_remove(path);
	if (err < 0)
		return __connman_error_failed(msg, -err);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static DBusMessage *get_connections(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	DBusMessage *reply;

	DBG("conn %p", conn);

	reply = __vpn_provider_get_connections(msg);
	if (!reply)
		return __connman_error_failed(msg, EINVAL);

	return reply;
}

static DBusMessage *register_agent(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	const char *sender, *path;
	int err;

	DBG("conn %p", conn);

	sender = dbus_message_get_sender(msg);

	dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
							DBUS_TYPE_INVALID);

	err = connman_agent_register(sender, path);
	if (err < 0)
		return __connman_error_failed(msg, -err);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static DBusMessage *unregister_agent(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	const char *sender, *path;
	int err;

	DBG("conn %p", conn);

	sender = dbus_message_get_sender(msg);

	dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
							DBUS_TYPE_INVALID);

	err = connman_agent_unregister(sender, path);
	if (err < 0)
		return __connman_error_failed(msg, -err);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static const GDBusMethodTable manager_methods[] = {
	{ GDBUS_ASYNC_METHOD("Create",
			GDBUS_ARGS({ "properties", "a{sv}" }),
			GDBUS_ARGS({ "path", "o" }),
			create) },
	{ GDBUS_ASYNC_METHOD("Remove",
			GDBUS_ARGS({ "identifier", "o" }), NULL,
			remove) },
	{ GDBUS_METHOD("GetConnections", NULL,
			GDBUS_ARGS({ "connections", "a(oa{sv})" }),
			get_connections) },
	{ GDBUS_METHOD("RegisterAgent",
			GDBUS_ARGS({ "path", "o" }), NULL,
			register_agent) },
	{ GDBUS_METHOD("UnregisterAgent",
			GDBUS_ARGS({ "path", "o" }), NULL,
			unregister_agent) },
	{ },
};

static const GDBusSignalTable manager_signals[] = {
	{ GDBUS_SIGNAL("ConnectionAdded",
			GDBUS_ARGS({ "identifier", "o" },
				{ "properties", "a{sv}" })) },
	{ GDBUS_SIGNAL("ConnectionRemoved",
			GDBUS_ARGS({ "identifier", "o" })) },
	{ },
};

int __vpn_manager_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();
	if (!connection)
		return -1;

	g_dbus_register_interface(connection, VPN_MANAGER_PATH,
					VPN_MANAGER_INTERFACE,
					manager_methods,
					manager_signals, NULL, NULL, NULL);

	vpn_connect_count = 0;

	return 0;
}

void __vpn_manager_cleanup(void)
{
	DBG("");

	if (!connection)
		return;

	g_dbus_unregister_interface(connection, VPN_MANAGER_PATH,
						VPN_MANAGER_INTERFACE);

	dbus_connection_unref(connection);
}
