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

#include <gdbus.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/log.h>
#include <connman/notifier.h>
#include <connman/dbus.h>

enum {
	NM_STATE_UNKNOWN          = 0,
	NM_STATE_ASLEEP           = 10,
	NM_STATE_DISCONNECTED     = 20,
	NM_STATE_DISCONNECTING    = 30,
	NM_STATE_CONNECTING       = 40,
	NM_STATE_CONNECTED_LOCAL  = 50,
	NM_STATE_CONNECTED_SITE   = 60,
	NM_STATE_CONNECTED_GLOBAL = 70
};

#define NM_SERVICE    "org.freedesktop.NetworkManager"
#define NM_PATH       "/org/freedesktop/NetworkManager"
#define NM_INTERFACE  NM_SERVICE

#define DBUS_PROPERTIES_INTERFACE	"org.freedesktop.DBus.Properties"

static DBusConnection *connection = NULL;
static struct connman_service *current_service = NULL;
static dbus_uint32_t nm_state = NM_STATE_UNKNOWN;

static void state_changed(dbus_uint32_t state)
{
	DBusMessage *signal;

	DBG("state %d", state);

	signal = dbus_message_new_signal(NM_PATH, NM_INTERFACE,
						"StateChanged");
	if (!signal)
		return;

	dbus_message_append_args(signal, DBUS_TYPE_UINT32, &state,
						DBUS_TYPE_INVALID);

	g_dbus_send_message(connection, signal);
}

static void properties_changed(dbus_uint32_t state)
{
	const char *key = "State";
	DBusMessageIter iter, dict, dict_entry, dict_val;
	DBusMessage *signal;

	DBG("state %d", state);

	signal = dbus_message_new_signal(NM_PATH, NM_INTERFACE,
						"PropertiesChanged");
	if (!signal)
		return;

	dbus_message_iter_init_append(signal, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_STRING_AS_STRING
					DBUS_TYPE_VARIANT_AS_STRING
					DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
					&dict);

	dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY,
							NULL, &dict_entry);

	dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(&dict_entry, DBUS_TYPE_VARIANT,
					DBUS_TYPE_UINT32_AS_STRING, &dict_val);

	dbus_message_iter_append_basic(&dict_val, DBUS_TYPE_UINT32, &state);

	dbus_message_iter_close_container(&dict_entry, &dict_val);
	dbus_message_iter_close_container(&dict, &dict_entry);
	dbus_message_iter_close_container(&iter, &dict);

	g_dbus_send_message(connection, signal);
}

static void default_changed(struct connman_service *service)
{
	DBG("service %p", service);

	if (!service)
		nm_state = NM_STATE_DISCONNECTED;
	else
		nm_state = NM_STATE_CONNECTED_LOCAL;

	state_changed(nm_state);
	properties_changed(nm_state);

	current_service = service;
}

static void service_state_changed(struct connman_service *service,
					enum connman_service_state state)
{
	DBG("service %p state %d", service, state);

	if (!current_service || current_service != service)
		return;

	switch (state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
		nm_state = NM_STATE_UNKNOWN;
		break;
	case CONNMAN_SERVICE_STATE_FAILURE:
	case CONNMAN_SERVICE_STATE_IDLE:
		nm_state = NM_STATE_DISCONNECTED;
		break;
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
		nm_state = NM_STATE_CONNECTING;
		break;
	case CONNMAN_SERVICE_STATE_READY:
		nm_state = NM_STATE_CONNECTED_LOCAL;
		break;
	case CONNMAN_SERVICE_STATE_ONLINE:
		nm_state = NM_STATE_CONNECTED_GLOBAL;
		break;
	case CONNMAN_SERVICE_STATE_DISCONNECT:
		nm_state = NM_STATE_DISCONNECTING;
		break;
	}

	state_changed(nm_state);
	properties_changed(nm_state);
}

static void offline_mode(bool enabled)
{
	DBG("enabled %d", enabled);

	if (enabled)
		nm_state = NM_STATE_ASLEEP;
	else
		nm_state = NM_STATE_DISCONNECTED;

	state_changed(nm_state);
	properties_changed(nm_state);

	current_service = NULL;
}

static struct connman_notifier notifier = {
	.name			= "nmcompat",
	.priority		= CONNMAN_NOTIFIER_PRIORITY_DEFAULT,
	.default_changed	= default_changed,
	.service_state_changed	= service_state_changed,
	.offline_mode		= offline_mode,
};

static DBusMessage *property_get(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	const char *interface, *key;

	dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &interface,
				DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID);

	DBG("interface %s property %s", interface, key);

	if (!g_str_equal(interface, NM_INTERFACE))
		return dbus_message_new_error(msg, DBUS_ERROR_FAILED,
						"Unsupported interface");

	if (g_str_equal(key, "State")) {
		DBusMessage *reply;
		DBusMessageIter iter, value;

		DBG("state %d", nm_state);

		reply = dbus_message_new_method_return(msg);
		if (!reply)
			return NULL;

		dbus_message_iter_init_append(reply, &iter);

		dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
					DBUS_TYPE_UINT32_AS_STRING, &value);
		dbus_message_iter_append_basic(&value,
						DBUS_TYPE_UINT32, &nm_state);
		dbus_message_iter_close_container(&iter, &value);

		return reply;
	}

	return dbus_message_new_error(msg, DBUS_ERROR_FAILED,
						"Unsupported property");
}

static const GDBusMethodTable methods[] = {
	{ GDBUS_METHOD("Get",
			GDBUS_ARGS({ "interface", "s" }, { "key", "s" }),
			GDBUS_ARGS({ "property", "v" }), property_get) },
	{ },
};

static const GDBusSignalTable signals[] = {
	{ GDBUS_SIGNAL("PropertiesChanged",
			GDBUS_ARGS({ "properties", "a{sv}" })) },
	{ GDBUS_SIGNAL("StateChanged",
			GDBUS_ARGS({ "state", "u" })) },
	{ },
};

static int nmcompat_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();
	if (!connection)
		return -1;

	if (!g_dbus_request_name(connection, NM_SERVICE, NULL)) {
		connman_error("nmcompat: failed to register service");
		return -1;
	}

	if (connman_notifier_register(&notifier) < 0) {
		connman_error("nmcompat: failed to register notifier");
		return -1;
	}

	if (!g_dbus_register_interface(connection, NM_PATH,
					DBUS_PROPERTIES_INTERFACE, methods,
					signals, NULL, NULL, NULL)) {
		connman_error("nmcompat: failed to register "
						DBUS_PROPERTIES_INTERFACE);
		return -1;
	}

	return 0;
}

static void nmcompat_exit(void)
{
	DBG("");

	connman_notifier_unregister(&notifier);

	if (!connection)
		return;

	g_dbus_unregister_interface(connection, NM_PATH,
					DBUS_PROPERTIES_INTERFACE);

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(nmcompat, "NetworkManager compatibility interfaces",
			VERSION, CONNMAN_PLUGIN_PRIORITY_DEFAULT,
			nmcompat_init, nmcompat_exit)
