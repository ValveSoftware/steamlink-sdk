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

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/notifier.h>
#include <connman/dbus.h>
#include <connman/log.h>

#define IOSPM_SERVICE		"com.intel.mid.ospm"
#define IOSPM_INTERFACE		IOSPM_SERVICE ".Comms"

#define IOSPM_BLUETOOTH		"/com/intel/mid/ospm/bluetooth"
#define IOSPM_FLIGHT_MODE	"/com/intel/mid/ospm/flight_mode"

static DBusConnection *connection;

static void send_indication(const char *path, bool enabled)
{
	DBusMessage *message;
	const char *method;

	DBG("path %s enabled %d", path, enabled);

	if (enabled)
		method = "IndicateStart";
	else
		method = "IndicateStop";

	message = dbus_message_new_method_call(IOSPM_SERVICE, path,
						IOSPM_INTERFACE, method);
	if (!message)
		return;

	dbus_message_set_no_reply(message, TRUE);

	dbus_connection_send(connection, message, NULL);

	dbus_message_unref(message);
}

static void iospm_service_enabled(enum connman_service_type type,
						bool enabled)
{
	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_VPN:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
		send_indication(IOSPM_BLUETOOTH, enabled);
		break;
	}
}

static void iospm_offline_mode(bool enabled)
{
	send_indication(IOSPM_FLIGHT_MODE, enabled);
}

static struct connman_notifier iospm_notifier = {
	.name		= "iospm",
	.priority	= CONNMAN_NOTIFIER_PRIORITY_DEFAULT,
	.service_enabled= iospm_service_enabled,
	.offline_mode	= iospm_offline_mode,
};

static int iospm_init(void)
{
	connection = connman_dbus_get_connection();

	return connman_notifier_register(&iospm_notifier);
}

static void iospm_exit(void)
{
	connman_notifier_unregister(&iospm_notifier);

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(ospm, "Intel OSPM notification plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, iospm_init, iospm_exit)
