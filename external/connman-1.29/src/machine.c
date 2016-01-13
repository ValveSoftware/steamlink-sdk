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

#define DEFAULT_MACHINE_TYPE	"laptop"

#define HOSTNAMED_SERVICE	"org.freedesktop.hostname1"
#define HOSTNAMED_INTERFACE	HOSTNAMED_SERVICE
#define HOSTNAMED_PATH		"/org/freedesktop/hostname1"

static GDBusClient *hostnamed_client = NULL;
static GDBusProxy *hostnamed_proxy = NULL;
static char *machine_type = NULL;

const char *connman_machine_get_type(void)
{
	if (machine_type)
		return machine_type;

	return DEFAULT_MACHINE_TYPE;
}

static void machine_property_changed(GDBusProxy *proxy, const char *name,
					DBusMessageIter *iter, void *user_data)
{
	DBG("Property %s", name);

	if (g_str_equal(name, "Chassis")) {
		const char *str;

		if (!iter) {
			g_dbus_proxy_refresh_property(proxy, name);
			return;
		}

		if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRING)
			return;

		dbus_message_iter_get_basic(iter, &str);
		g_free(machine_type);
		machine_type = g_strdup(str);

		DBG("Machine type set to %s", machine_type);
	}
}

int __connman_machine_init(void)
{
	DBusConnection *connection;
	int err = -EIO;

	DBG("");

	connection = connman_dbus_get_connection();

	hostnamed_client = g_dbus_client_new(connection, HOSTNAMED_SERVICE,
							HOSTNAMED_PATH);
	if (!hostnamed_client)
		goto error;

	hostnamed_proxy = g_dbus_proxy_new(hostnamed_client, HOSTNAMED_PATH,
							HOSTNAMED_INTERFACE);
	if (!hostnamed_proxy)
		goto error;

	g_dbus_proxy_set_property_watch(hostnamed_proxy,
					machine_property_changed, NULL);

	dbus_connection_unref(connection);

	return 0;
error:
	if (hostnamed_client) {
		g_dbus_client_unref(hostnamed_client);
		hostnamed_client = NULL;
	}

	dbus_connection_unref(connection);

	return err;
}

void __connman_machine_cleanup(void)
{
	DBG("");

	if (hostnamed_proxy) {
		g_dbus_proxy_unref(hostnamed_proxy);
		hostnamed_proxy = NULL;
	}

	if (hostnamed_client) {
		g_dbus_client_unref(hostnamed_client);
		hostnamed_client = NULL;
	}

	g_free(machine_type);
	machine_type = NULL;
}
