/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2013  Intel Corporation. All rights reserved.
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

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/dbus.h>
#include <connman/technology.h>
#include <connman/device.h>
#include <connman/inet.h>
#include <gdbus.h>

#define BLUEZ_SERVICE                   "org.bluez"
#define BLUEZ_PATH                      "/org/bluez"
#define BLUETOOTH_PAN_PANU              "00001115-0000-1000-8000-00805f9b34fb"
#define BLUETOOTH_PAN_NAP               "00001116-0000-1000-8000-00805f9b34fb"
#define BLUETOOTH_PAN_GN                "00001117-0000-1000-8000-00805f9b34fb"

#define BLUETOOTH_ADDR_LEN              6

static DBusConnection *connection;
static GDBusClient *client;
static GHashTable *devices;
static GHashTable *networks;
static bool bluetooth_tethering;

struct bluetooth_pan {
	struct connman_network *network;
	GDBusProxy *btdevice_proxy;
	GDBusProxy *btnetwork_proxy;
	const char *pan_role;
};

static void address2ident(const char *address, char *ident)
{
	int i;

	for (i = 0; i < BLUETOOTH_ADDR_LEN; i++) {
		ident[i * 2] = address[i * 3];
		ident[i * 2 + 1] = address[i * 3 + 1];
	}
	ident[BLUETOOTH_ADDR_LEN * 2] = '\0';
}

static const char *proxy_get_string(GDBusProxy *proxy, const char *property)
{
	DBusMessageIter iter;
	const char *str;

	if (!g_dbus_proxy_get_property(proxy, property, &iter))
		return NULL;
	dbus_message_iter_get_basic(&iter, &str);
	return str;
}

static bool proxy_get_bool(GDBusProxy *proxy, const char *property)
{
	DBusMessageIter iter;
	dbus_bool_t value;

	if (!g_dbus_proxy_get_property(proxy, property, &iter))
		return false;
	dbus_message_iter_get_basic(&iter, &value);
	return value;
}

static const char *proxy_get_role(GDBusProxy *proxy)
{
	DBusMessageIter iter, value;
	const char *pref = NULL;

	if (!proxy)
		return NULL;

        if (!g_dbus_proxy_get_property(proxy, "UUIDs", &iter))
		return NULL;

        dbus_message_iter_recurse(&iter, &value);
        while (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING) {
                const char *uuid;

		dbus_message_iter_get_basic(&value, &uuid);
		/*
		 * If a device offers more than one role, we prefer NAP,
		 * then GN, then PANU.
		 */
		if (!strcmp(uuid, BLUETOOTH_PAN_NAP))
			return "nap";
		if (!strcmp(uuid, BLUETOOTH_PAN_GN))
			pref = "gn";
		if (!strcmp(uuid, BLUETOOTH_PAN_PANU) && !pref)
			pref = "panu";

                dbus_message_iter_next(&value);
        }

	return pref;
}

static int bluetooth_pan_probe(struct connman_network *network)
{
	GHashTableIter iter;
	gpointer key, value;

	DBG("network %p", network);

	g_hash_table_iter_init(&iter, networks);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct bluetooth_pan *pan = value;

		if (network == pan->network)
			return 0;
	}

	return -EOPNOTSUPP;
}

static void pan_remove_nap(struct bluetooth_pan *pan)
{
	struct connman_device *device;
	struct connman_network *network = pan->network;

	DBG("network %p pan %p", pan->network, pan);

	if (!network)
		return;

	pan->network = NULL;
	connman_network_set_data(network, NULL);

	device = connman_network_get_device(network);
	if (device)
		connman_device_remove_network(device, network);

	connman_network_unref(network);
}

static void bluetooth_pan_remove(struct connman_network *network)
{
	struct bluetooth_pan *pan = connman_network_get_data(network);

	DBG("network %p pan %p", network, pan);

	connman_network_set_data(network, NULL);

	if (pan)
		pan_remove_nap(pan);
}

static bool pan_connect(struct bluetooth_pan *pan,
		const char *iface)
{
	int index;

	if (!iface) {
		if (!proxy_get_bool(pan->btnetwork_proxy, "Connected"))
			return false;
		iface = proxy_get_string(pan->btnetwork_proxy, "Interface");
	}

	if (!iface)
		return false;

	index = connman_inet_ifindex(iface);
	if (index < 0) {
		DBG("network %p invalid index %d", pan->network, index);
		return false;
	}

	connman_network_set_index(pan->network, index);
	connman_network_set_connected(pan->network, true);

	return true;
}

static void pan_connect_cb(DBusMessage *message, void *user_data)
{
	const char *path = user_data;
	const char *iface = NULL;
	struct bluetooth_pan *pan;
	DBusMessageIter iter;

	pan = g_hash_table_lookup(networks, path);
	if (!pan) {
		DBG("network already removed");
		return;
	}

	if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_ERROR) {
		const char *dbus_error = dbus_message_get_error_name(message);

		DBG("network %p %s", pan->network, dbus_error);

		if (strcmp(dbus_error,
				"org.bluez.Error.AlreadyConnected") != 0) {
			connman_network_set_error(pan->network,
				CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL);
			return;
		}
	} else {
		if (dbus_message_iter_init(message, &iter) &&
				dbus_message_iter_get_arg_type(&iter) ==
				DBUS_TYPE_STRING)
			dbus_message_iter_get_basic(&iter, &iface);
	}

	DBG("network %p interface %s", pan->network, iface);

	pan_connect(pan, iface);
}

static void pan_connect_append(DBusMessageIter *iter,
		void *user_data)
{
	const char *path = user_data;
	struct bluetooth_pan *pan;

	pan = g_hash_table_lookup(networks, path);
	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &pan->pan_role);
}

static int bluetooth_pan_connect(struct connman_network *network)
{
	struct bluetooth_pan *pan = connman_network_get_data(network);
	const char *path;

	DBG("network %p", network);

	if (!pan)
		return -EINVAL;

	path = g_dbus_proxy_get_path(pan->btnetwork_proxy);

	if (!g_dbus_proxy_method_call(pan->btnetwork_proxy, "Connect",
			pan_connect_append, pan_connect_cb,
			g_strdup(path), g_free))
		return -EIO;

	connman_network_set_associating(pan->network, true);

	return -EINPROGRESS;
}

static void pan_disconnect_cb(DBusMessage *message, void *user_data)
{
	const char *path = user_data;
	struct bluetooth_pan *pan;

	pan = g_hash_table_lookup(networks, path);
	if (!pan) {
		DBG("network already removed");
		return;
	}

	if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_ERROR) {
		const char *dbus_error = dbus_message_get_error_name(message);

		DBG("network %p %s", pan->network, dbus_error);
	}

	DBG("network %p", pan->network);

	connman_network_set_connected(pan->network, false);
}

static int bluetooth_pan_disconnect(struct connman_network *network)
{
	struct bluetooth_pan *pan = connman_network_get_data(network);
	const char *path;

	DBG("network %p", network);

	if (!pan)
		return -EINVAL;

	path = g_dbus_proxy_get_path(pan->btnetwork_proxy);

	if (!g_dbus_proxy_method_call(pan->btnetwork_proxy, "Disconnect",
			NULL, pan_disconnect_cb, g_strdup(path), g_free))
		return -EIO;

       return -EINPROGRESS;
}

static void btnetwork_property_change(GDBusProxy *proxy, const char *name,
		DBusMessageIter *iter, void *user_data)
{
	struct bluetooth_pan *pan;
	dbus_bool_t connected;
	bool proxy_connected, network_connected;

	if (strcmp(name, "Connected") != 0)
		return;

	pan = g_hash_table_lookup(networks, g_dbus_proxy_get_path(proxy));
	if (!pan || !pan->network)
		return;

	dbus_message_iter_get_basic(iter, &connected);
	proxy_connected = connected;

	network_connected = connman_network_get_connected(pan->network);

	DBG("network %p network connected %d proxy connected %d",
			pan->network, network_connected, proxy_connected);

       if (network_connected != proxy_connected)
               connman_network_set_connected(pan->network, proxy_connected);
}

static void pan_create_nap(struct bluetooth_pan *pan)
{
	struct connman_device *device;
	const char* role;

	role = proxy_get_role(pan->btdevice_proxy);
	if (!role) {
		pan_remove_nap(pan);
		return;
	}

	device = g_hash_table_lookup(devices,
			proxy_get_string(pan->btdevice_proxy, "Adapter"));

	if (!device || !connman_device_get_powered(device))
		return;

	if (!pan->network) {
		const char *address;
		char ident[BLUETOOTH_ADDR_LEN * 2 + 1];
		const char *name, *path;

		address = proxy_get_string(pan->btdevice_proxy, "Address");
		if (!address) {
			connman_warn("Bluetooth device address missing");
			return;
		}

		address2ident(address, ident);

		pan->network = connman_network_create(ident,
				CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN);

		name = proxy_get_string(pan->btdevice_proxy, "Alias");
		path = g_dbus_proxy_get_path(pan->btnetwork_proxy);

		DBG("network %p %s %s", pan->network, path, name);

		if (!pan->network) {
			connman_warn("Bluetooth network %s creation failed",
					path);
			return;
		}

		connman_network_set_data(pan->network, pan);
		connman_network_set_name(pan->network, name);
		connman_network_set_group(pan->network, ident);
	}

	pan->pan_role = role;
	connman_device_add_network(device, pan->network);

	if (pan_connect(pan, NULL))
		DBG("network %p already connected", pan->network);
}

static void btdevice_property_change(GDBusProxy *proxy, const char *name,
		DBusMessageIter *iter, void *user_data)
{
	struct bluetooth_pan *pan;
	const char *old_role = NULL;
	const char *new_role;

	if (strcmp(name, "UUIDs"))
		return;

	pan = g_hash_table_lookup(networks, g_dbus_proxy_get_path(proxy));
	if (!pan)
		return;

	if (pan->network &&
			connman_network_get_device(pan->network))
		old_role = pan->pan_role;
	new_role = proxy_get_role(pan->btdevice_proxy);

	DBG("network %p network role %s proxy role %s", pan->network, old_role,
			new_role);

	if (old_role && new_role && !strcmp(old_role, new_role))
		return;

	pan_create_nap(pan);
}

static void pan_free(gpointer data)
{
	struct bluetooth_pan *pan = data;

	if (pan->btnetwork_proxy) {
		g_dbus_proxy_unref(pan->btnetwork_proxy);
		pan->btnetwork_proxy = NULL;
	}

	if (pan->btdevice_proxy) {
		g_dbus_proxy_unref(pan->btdevice_proxy);
		pan->btdevice_proxy = NULL;
	}

	pan_remove_nap(pan);

	g_free(pan);
}

static void pan_create(GDBusProxy *network_proxy)
{
	const char *path = g_dbus_proxy_get_path(network_proxy);
	struct bluetooth_pan *pan;

	pan = g_try_new0(struct bluetooth_pan, 1);

	if (!pan) {
		connman_error("Out of memory creating PAN NAP");
		return;
	}

	g_hash_table_replace(networks, g_strdup(path), pan);

	pan->btnetwork_proxy = g_dbus_proxy_ref(network_proxy);
	pan->btdevice_proxy = g_dbus_proxy_new(client, path,
			"org.bluez.Device1");

	if (!pan->btdevice_proxy) {
		connman_error("Cannot create BT PAN watcher %s", path);
		g_hash_table_remove(networks, path);
		return;
	}

	g_dbus_proxy_set_property_watch(pan->btnetwork_proxy,
			btnetwork_property_change, NULL);

	g_dbus_proxy_set_property_watch(pan->btdevice_proxy,
			btdevice_property_change, NULL);

	DBG("pan %p %s role %s", pan, path, proxy_get_role(pan->btdevice_proxy));

	pan_create_nap(pan);
}

static struct connman_network_driver network_driver = {
	.name		= "bluetooth",
	.type           = CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN,
	.probe          = bluetooth_pan_probe,
	.remove         = bluetooth_pan_remove,
	.connect        = bluetooth_pan_connect,
	.disconnect     = bluetooth_pan_disconnect,
};

static void enable_device(struct connman_device *device, const char *path)
{
	GHashTableIter iter;
	gpointer key, value;

	DBG("device %p %s", device, path);
	connman_device_set_powered(device, true);

	g_hash_table_iter_init(&iter, networks);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct bluetooth_pan *pan = value;

		if (g_strcmp0(proxy_get_string(pan->btdevice_proxy, "Adapter"),
						path) == 0) {

			DBG("enable network %p", pan->network);
			pan_create_nap(pan);
		}
	}
}

static void device_enable_cb(const DBusError *error, void *user_data)
{
	char *path = user_data;
	struct connman_device *device;

	device = g_hash_table_lookup(devices, path);
	if (!device) {
		DBG("device already removed");
		goto out;
	}

	if (dbus_error_is_set(error)) {
		connman_warn("Bluetooth device %s not enabled %s",
				path, error->message);
		goto out;
	}

	enable_device(device, path);
out:
	g_free(path);
}

static int bluetooth_device_enable(struct connman_device *device)
{
	GDBusProxy *proxy = connman_device_get_data(device);
	dbus_bool_t device_powered = TRUE;
	const char *path;

	if (!proxy)
		return 0;

	path = g_dbus_proxy_get_path(proxy);

	if (proxy_get_bool(proxy, "Powered")) {
		DBG("already enabled %p %s", device, path);
		return -EALREADY;
	}

	DBG("device %p %s", device, path);

	g_dbus_proxy_set_property_basic(proxy, "Powered",
			DBUS_TYPE_BOOLEAN, &device_powered,
			device_enable_cb, g_strdup(path), NULL);

	return -EINPROGRESS;
}

static void disable_device(struct connman_device *device, const char *path)
{
	GHashTableIter iter;
	gpointer key, value;

	DBG("device %p %s", device, path);
	connman_device_set_powered(device, false);

	g_hash_table_iter_init(&iter, networks);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct bluetooth_pan *pan = value;

		if (pan->network && connman_network_get_device(pan->network)
				== device) {
			DBG("disable network %p", pan->network);
			connman_device_remove_network(device, pan->network);
		}
	}
}

static void device_disable_cb(const DBusError *error, void *user_data)
{
	char *path = user_data;
	struct connman_device *device;

	device = g_hash_table_lookup(devices, path);
	if (!device) {
		DBG("device already removed");
		goto out;
	}

	if (dbus_error_is_set(error)) {
		connman_warn("Bluetooth device %s not disabled: %s",
				path, error->message);
		goto out;
	}

	disable_device(device, path);

out:
	g_free(path);
}

static int bluetooth_device_disable(struct connman_device *device)
{
	GDBusProxy *proxy = connman_device_get_data(device);
	dbus_bool_t device_powered = FALSE;
	const char *path;

	if (!proxy)
		return 0;

	path = g_dbus_proxy_get_path(proxy);

	if (!proxy_get_bool(proxy, "Powered")) {
		DBG("already disabled %p %s", device, path);
		return -EALREADY;
	}

	DBG("device %p %s", device, path);

	g_dbus_proxy_set_property_basic(proxy, "Powered",
			DBUS_TYPE_BOOLEAN, &device_powered,
			device_disable_cb, g_strdup(path), NULL);

	return -EINPROGRESS;
}

static void adapter_property_change(GDBusProxy *proxy, const char *name,
		DBusMessageIter *iter, void *user_data)
{
	struct connman_device *device;
	const char *path;
	bool adapter_powered, device_powered;

	if (strcmp(name, "Powered") != 0)
		return;

	path = g_dbus_proxy_get_path(proxy);
	device = g_hash_table_lookup(devices, path);

	adapter_powered = proxy_get_bool(proxy, "Powered");
	device_powered = connman_device_get_powered(device);

	DBG("device %p %s device powered %d adapter powered %d", device, path,
			device_powered, adapter_powered);

	if (device_powered != adapter_powered) {
		if (adapter_powered)
			enable_device(device, path);
		else
			disable_device(device, path);
	}
}

static void device_free(gpointer data)
{
	struct connman_device *device = data;
	GDBusProxy *proxy = connman_device_get_data(device);

	connman_device_set_data(device, NULL);
	if (proxy)
		g_dbus_proxy_unref(proxy);

	connman_device_unregister(device);
	connman_device_unref(device);
}

struct tethering_info {
	struct connman_technology *technology;
	char *bridge;
	bool enable;
};

static void tethering_free(void *user_data)
{
	struct tethering_info *tethering = user_data;

	g_free(tethering->bridge);
	g_free(tethering);
}

static void tethering_create_cb(DBusMessage *message, void *user_data)
{
	struct tethering_info *tethering = user_data;

	if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_ERROR) {
		const char *dbus_error = dbus_message_get_error_name(message);

		DBG("%s tethering failed: %s",
				tethering->enable ? "enable" : "disable",
				dbus_error);
		return;
	}

	DBG("bridge %s %s", tethering->bridge, tethering->enable ?
			"enabled": "disabled");

	if (tethering->technology)
		connman_technology_tethering_notify(tethering->technology,
				tethering->enable);
}

static void tethering_append(DBusMessageIter *iter, void *user_data)
{
	struct tethering_info *tethering = user_data;
	const char *nap = "nap";

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &nap);
	if (tethering->enable)
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
				&tethering->bridge);
}

static bool tethering_create(const char *path,
		struct connman_technology *technology, const char *bridge,
		bool enabled)
{
	struct tethering_info *tethering = g_new0(struct tethering_info, 1);
	GDBusProxy *proxy;
	const char *method;
	bool result;

	DBG("path %s bridge %s", path, bridge);

	if (!bridge)
		return -EINVAL;

	proxy = g_dbus_proxy_new(client, path, "org.bluez.NetworkServer1");
	if (!proxy)
		return false;

	tethering->technology = technology;
	tethering->bridge = g_strdup(bridge);
	tethering->enable = enabled;

	if (tethering->enable)
		method = "Register";
	else
		method = "Unregister";

	result = g_dbus_proxy_method_call(proxy, method, tethering_append,
			tethering_create_cb, tethering, tethering_free);

	g_dbus_proxy_unref(proxy);

	return result;
}

static void device_create(GDBusProxy *proxy)
{
	struct connman_device *device = NULL;
	const char *path = g_dbus_proxy_get_path(proxy);
	const char *address;
	char ident[BLUETOOTH_ADDR_LEN * 2 + 1];
	bool powered;

	address = proxy_get_string(proxy, "Address");
	if (!address)
		return;

	address2ident(address, ident);

	device = connman_device_create("bluetooth",
			CONNMAN_DEVICE_TYPE_BLUETOOTH);
	if (!device)
		return;

	connman_device_set_data(device, g_dbus_proxy_ref(proxy));
	connman_device_set_ident(device, ident);

	g_hash_table_replace(devices, g_strdup(path), device);

	DBG("device %p %s device powered %d adapter powered %d", device,
			path, connman_device_get_powered(device),
			proxy_get_bool(proxy, "Powered"));

	if (connman_device_register(device) < 0) {
		g_hash_table_remove(devices, device);
		return;
	}

	g_dbus_proxy_set_property_watch(proxy, adapter_property_change, NULL);

	powered = proxy_get_bool(proxy, "Powered");
	connman_device_set_powered(device, powered);

	if (proxy_get_role(proxy) && !bluetooth_tethering)
		tethering_create(path, NULL, NULL, false);
}

static void object_added(GDBusProxy *proxy, void *user_data)
{
	const char *interface;

	interface = g_dbus_proxy_get_interface(proxy);
	if (!interface) {
		connman_warn("Interface or proxy missing when adding "
							"bluetooth object");
		return;
	}

	if (strcmp(interface, "org.bluez.Adapter1") == 0) {
		DBG("%s %s", interface, g_dbus_proxy_get_path(proxy));
		device_create(proxy);
		return;
	}

	if (strcmp(interface, "org.bluez.Network1") == 0) {
		DBG("%s %s", interface, g_dbus_proxy_get_path(proxy));
		pan_create(proxy);
		return;
	}
}

static void object_removed(GDBusProxy *proxy, void *user_data)
{
	const char *interface, *path;

	interface = g_dbus_proxy_get_interface(proxy);
	if (!interface) {
		connman_warn("Interface or proxy missing when removing "
							"bluetooth object");
		return;
	}

	if (strcmp(interface, "org.bluez.Adapter1") == 0) {
		path = g_dbus_proxy_get_path(proxy);
		DBG("%s %s", interface, path);

		g_hash_table_remove(devices, path);
	}

	if (strcmp(interface, "org.bluez.Network1") == 0) {
		path = g_dbus_proxy_get_path(proxy);
		DBG("%s %s", interface, path);

		g_hash_table_remove(networks, path);
	}

}

static int bluetooth_device_probe(struct connman_device *device)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, devices);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct connman_device *known = value;

		if (device == known)
			return 0;
	}

	return -EOPNOTSUPP;
}

static void bluetooth_device_remove(struct connman_device *device)
{
	DBG("%p", device);
}

static struct connman_device_driver device_driver = {
	.name		= "bluetooth",
	.type		= CONNMAN_DEVICE_TYPE_BLUETOOTH,
	.probe          = bluetooth_device_probe,
	.remove         = bluetooth_device_remove,
	.enable         = bluetooth_device_enable,
	.disable        = bluetooth_device_disable,
};

static int bluetooth_tech_probe(struct connman_technology *technology)
{
	return 0;
}

static void bluetooth_tech_remove(struct connman_technology *technology)
{

}

static int bluetooth_tech_set_tethering(struct connman_technology *technology,
		const char *identifier, const char *passphrase,
		const char *bridge, bool enabled)
{
	GHashTableIter hash_iter;
	gpointer key, value;
	int i = 0;

	bluetooth_tethering = enabled;

	g_hash_table_iter_init(&hash_iter, devices);

	while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
		const char *path = key;
		struct connman_device *device = value;

		DBG("device %p", device);

		if (tethering_create(path, technology, bridge, enabled)
				)
			i++;
	}

	DBG("%s %d device(s)", enabled ? "enabled" : "disabled", i);

	if (i == 0)
		return -ENODEV;

	return 0;
}

static struct connman_technology_driver tech_driver = {
	.name		= "bluetooth",
	.type		= CONNMAN_SERVICE_TYPE_BLUETOOTH,
	.probe          = bluetooth_tech_probe,
	.remove         = bluetooth_tech_remove,
	.set_tethering  = bluetooth_tech_set_tethering,
};

static int bluetooth_init(void)
{
	connection = connman_dbus_get_connection();
	if (!connection)
		goto out;

	if (connman_technology_driver_register(&tech_driver) < 0) {
		connman_warn("Failed to initialize technology for Bluez 5");
		goto out;
	}

	devices = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			device_free);

	if (connman_device_driver_register(&device_driver) < 0) {
		connman_warn("Failed to initialize device driver for "
				BLUEZ_SERVICE);
		connman_technology_driver_unregister(&tech_driver);
		goto out;
	}

	if (connman_network_driver_register(&network_driver) < 0) {
		connman_technology_driver_unregister(&tech_driver);
		connman_device_driver_unregister(&device_driver);
		goto out;
	}

	networks = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			pan_free);

	client = g_dbus_client_new(connection, BLUEZ_SERVICE, BLUEZ_PATH);
	if (!client) {
		connman_warn("Failed to initialize D-Bus client for "
				BLUEZ_SERVICE);
		goto out;
	}

	g_dbus_client_set_proxy_handlers(client, object_added, object_removed,
			NULL, NULL);

	return 0;

out:
	if (networks)
		g_hash_table_destroy(networks);

	if (devices)
		g_hash_table_destroy(devices);

	if (client)
		g_dbus_client_unref(client);

	if (connection)
		dbus_connection_unref(connection);

	return -EIO;
}

static void bluetooth_exit(void)
{
	/*
	 * We unset the disabling of the Bluetooth device when shutting down
	 * so that non-PAN BT connections are not affected.
	 */
	device_driver.disable = NULL;

	g_dbus_client_unref(client);

	connman_network_driver_unregister(&network_driver);
	g_hash_table_destroy(networks);

	connman_device_driver_unregister(&device_driver);
	g_hash_table_destroy(devices);

	connman_technology_driver_unregister(&tech_driver);
	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(bluetooth, "Bluetooth technology plugin", VERSION,
                CONNMAN_PLUGIN_PRIORITY_DEFAULT, bluetooth_init, bluetooth_exit)
