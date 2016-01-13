/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2014  BMW Car IT GmbH.
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

#include <gdbus.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/device.h>
#include <connman/network.h>
#include <connman/service.h>
#include <connman/inet.h>
#include <connman/dbus.h>

#define DUNDEE_SERVICE			"org.ofono.dundee"
#define DUNDEE_MANAGER_INTERFACE	DUNDEE_SERVICE ".Manager"
#define DUNDEE_DEVICE_INTERFACE		DUNDEE_SERVICE ".Device"

#define DEVICE_ADDED			"DeviceAdded"
#define DEVICE_REMOVED			"DeviceRemoved"
#define PROPERTY_CHANGED		"PropertyChanged"

#define GET_PROPERTIES			"GetProperties"
#define SET_PROPERTY			"SetProperty"
#define GET_DEVICES			"GetDevices"

#define TIMEOUT 60000

static DBusConnection *connection;

static GHashTable *dundee_devices = NULL;

struct dundee_data {
	char *path;
	char *name;

	struct connman_device *device;
	struct connman_network *network;

	bool active;

	int index;

	/* IPv4 Settings */
	enum connman_ipconfig_method method;
	struct connman_ipaddress *address;
	char *nameservers;

	DBusPendingCall *call;
};

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

static int create_device(struct dundee_data *info)
{
	struct connman_device *device;
	char *ident;
	int err;

	DBG("%s", info->path);

	ident = g_strdup(get_ident(info->path));
	device = connman_device_create("dundee", CONNMAN_DEVICE_TYPE_BLUETOOTH);
	if (!device) {
		err = -ENOMEM;
		goto out;
	}

	DBG("device %p", device);

	connman_device_set_ident(device, ident);

	connman_device_set_string(device, "Path", info->path);

	connman_device_set_data(device, info);

	err = connman_device_register(device);
	if (err < 0) {
		connman_error("Failed to register DUN device");
		connman_device_unref(device);
		goto out;
	}

	info->device = device;

out:
	g_free(ident);
	return err;
}

static void destroy_device(struct dundee_data *info)
{
	connman_device_set_powered(info->device, false);

	if (info->call)
		dbus_pending_call_cancel(info->call);

	if (info->network) {
		connman_device_remove_network(info->device, info->network);
		connman_network_unref(info->network);
		info->network = NULL;
	}

	connman_device_unregister(info->device);
	connman_device_unref(info->device);

	info->device = NULL;
}

static void device_destroy(gpointer data)
{
	struct dundee_data *info = data;

	if (info->device)
		destroy_device(info);

	g_free(info->path);
	g_free(info->name);

	g_free(info);
}

static int create_network(struct dundee_data *info)
{
	struct connman_network *network;
	const char *group;
	int err;

	DBG("%s", info->path);

	network = connman_network_create(info->path,
				CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN);
	if (!network)
		return -ENOMEM;

	DBG("network %p", network);

	connman_network_set_data(network, info);

	connman_network_set_string(network, "Path",
				info->path);

	connman_network_set_name(network, info->name);

	group = get_ident(info->path);
	connman_network_set_group(network, group);

	err = connman_device_add_network(info->device, network);
	if (err < 0) {
		connman_network_unref(network);
		return err;
	}

	info->network = network;

	return 0;
}

static void set_connected(struct dundee_data *info)
{
	struct connman_service *service;

	DBG("%s", info->path);

	connman_inet_ifup(info->index);

	service = connman_service_lookup_from_network(info->network);
	if (!service)
		return;

	connman_service_create_ip4config(service, info->index);
	connman_network_set_index(info->network, info->index);
	connman_network_set_ipv4_method(info->network,
					CONNMAN_IPCONFIG_METHOD_FIXED);
	connman_network_set_ipaddress(info->network, info->address);
	connman_network_set_nameservers(info->network, info->nameservers);

	connman_network_set_connected(info->network, true);
}

static void set_disconnected(struct dundee_data *info)
{
	DBG("%s", info->path);

	connman_network_set_connected(info->network, false);
	connman_inet_ifdown(info->index);
}

static void set_property_reply(DBusPendingCall *call, void *user_data)
{
	struct dundee_data *info = user_data;
	DBusMessage *reply;
	DBusError error;

	DBG("%s", info->path);

	info->call = NULL;

	dbus_error_init(&error);

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("Failed to change property: %s %s %s",
				info->path, error.name, error.message);
		dbus_error_free(&error);

		connman_network_set_error(info->network,
					CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL);
	}

	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int set_property(struct dundee_data *info,
			const char *property, int type, void *value)
{
	DBusMessage *message;
	DBusMessageIter iter;

	DBG("%s %s", info->path, property);

	message = dbus_message_new_method_call(DUNDEE_SERVICE, info->path,
					DUNDEE_DEVICE_INTERFACE, SET_PROPERTY);
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);
	connman_dbus_property_append_basic(&iter, property, type, value);

	if (!dbus_connection_send_with_reply(connection, message,
						&info->call, TIMEOUT)) {
		connman_error("Failed to change property: %s %s",
				info->path, property);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!info->call) {
		connman_error("D-Bus connection not available");
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(info->call, set_property_reply,
					info, NULL);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static int device_set_active(struct dundee_data *info)
{
	dbus_bool_t active = TRUE;

	DBG("%s", info->path);

	return set_property(info, "Active", DBUS_TYPE_BOOLEAN,
				&active);
}

static int device_set_inactive(struct dundee_data *info)
{
	dbus_bool_t active = FALSE;
	int err;

	DBG("%s", info->path);

	err = set_property(info, "Active", DBUS_TYPE_BOOLEAN,
				&active);
	if (err == -EINPROGRESS)
		return 0;

	return err;
}

static int network_probe(struct connman_network *network)
{
	DBG("network %p", network);

	return 0;
}

static void network_remove(struct connman_network *network)
{
	DBG("network %p", network);
}

static int network_connect(struct connman_network *network)
{
	struct dundee_data *info = connman_network_get_data(network);

	DBG("network %p", network);

	return device_set_active(info);
}

static int network_disconnect(struct connman_network *network)
{
	struct dundee_data *info = connman_network_get_data(network);

	DBG("network %p", network);

	return device_set_inactive(info);
}

static struct connman_network_driver network_driver = {
	.name		= "network",
	.type		= CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN,
	.probe		= network_probe,
	.remove		= network_remove,
	.connect	= network_connect,
	.disconnect	= network_disconnect,
};

static int dundee_probe(struct connman_device *device)
{
	GHashTableIter iter;
	gpointer key, value;

	DBG("device %p", device);

	if (!dundee_devices)
		return -ENOTSUP;

	g_hash_table_iter_init(&iter, dundee_devices);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct dundee_data *info = value;

		if (device == info->device)
			return 0;
	}

	return -ENOTSUP;
}

static void dundee_remove(struct connman_device *device)
{
	DBG("device %p", device);
}

static int dundee_enable(struct connman_device *device)
{
	DBG("device %p", device);

	return 0;
}

static int dundee_disable(struct connman_device *device)
{
	DBG("device %p", device);

	return 0;
}

static struct connman_device_driver dundee_driver = {
	.name		= "dundee",
	.type		= CONNMAN_DEVICE_TYPE_BLUETOOTH,
	.probe		= dundee_probe,
	.remove		= dundee_remove,
	.enable		= dundee_enable,
	.disable	= dundee_disable,
};

static char *extract_nameservers(DBusMessageIter *array)
{
	DBusMessageIter entry;
	char *nameservers = NULL;
	char *tmp;

	dbus_message_iter_recurse(array, &entry);

	while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
		const char *nameserver;

		dbus_message_iter_get_basic(&entry, &nameserver);

		if (!nameservers) {
			nameservers = g_strdup(nameserver);
		} else {
			tmp = nameservers;
			nameservers = g_strdup_printf("%s %s", tmp, nameserver);
			g_free(tmp);
		}

		dbus_message_iter_next(&entry);
	}

	return nameservers;
}

static void extract_settings(DBusMessageIter *array,
				struct dundee_data *info)
{
	DBusMessageIter dict;
	char *address = NULL, *gateway = NULL;
	char *nameservers = NULL;
	const char *interface = NULL;
	int index = -1;

	if (dbus_message_iter_get_arg_type(array) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key, *val;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Interface")) {
			dbus_message_iter_get_basic(&value, &interface);

			DBG("Interface %s", interface);

			index = connman_inet_ifindex(interface);

			DBG("index %d", index);

			if (index < 0)
				break;
		} else if (g_str_equal(key, "Address")) {
			dbus_message_iter_get_basic(&value, &val);

			address = g_strdup(val);

			DBG("Address %s", address);
		} else if (g_str_equal(key, "DomainNameServers")) {
			nameservers = extract_nameservers(&value);

			DBG("Nameservers %s", nameservers);
		} else if (g_str_equal(key, "Gateway")) {
			dbus_message_iter_get_basic(&value, &val);

			gateway = g_strdup(val);

			DBG("Gateway %s", gateway);
		}

		dbus_message_iter_next(&dict);
	}

	if (index < 0)
		goto out;

	info->address = connman_ipaddress_alloc(CONNMAN_IPCONFIG_TYPE_IPV4);
	if (!info->address)
		goto out;

	info->index = index;
	connman_ipaddress_set_ipv4(info->address, address, NULL, gateway);

	info->nameservers = nameservers;

out:
	if (info->nameservers != nameservers)
		g_free(nameservers);

	g_free(address);
	g_free(gateway);
}

static gboolean device_changed(DBusConnection *conn,
				DBusMessage *message,
				void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct dundee_data *info = NULL;
	DBusMessageIter iter, value;
	const char *key;
	const char *signature =	DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_VARIANT_AS_STRING;

	if (!dbus_message_has_signature(message, signature)) {
		connman_error("dundee signature does not match");
		return TRUE;
	}

	info = g_hash_table_lookup(dundee_devices, path);
	if (!info)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	/*
	 * Dundee guarantees the ordering of Settings and
	 * Active. Settings will always be send before Active = True.
	 * That means we don't have to order here.
	 */
	if (g_str_equal(key, "Active")) {
		dbus_bool_t active;

		dbus_message_iter_get_basic(&value, &active);
		info->active = active;

		DBG("%s Active %d", info->path, info->active);

		if (info->active)
			set_connected(info);
		else
			set_disconnected(info);
	} else if (g_str_equal(key, "Settings")) {
		DBG("%s Settings", info->path);

		extract_settings(&value, info);
	} else if (g_str_equal(key, "Name")) {
		char *name;

		dbus_message_iter_get_basic(&value, &name);

		g_free(info->name);
		info->name = g_strdup(name);

		DBG("%s Name %s", info->path, info->name);

		connman_network_set_name(info->network, info->name);
		connman_network_update(info->network);
	}

	return TRUE;
}

static void add_device(const char *path, DBusMessageIter *properties)
{
	struct dundee_data *info;
	int err;

	info = g_hash_table_lookup(dundee_devices, path);
	if (info)
		return;

	info = g_try_new0(struct dundee_data, 1);
	if (!info)
		return;

	info->path = g_strdup(path);

	while (dbus_message_iter_get_arg_type(properties) ==
			DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(properties, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Active")) {
			dbus_bool_t active;

			dbus_message_iter_get_basic(&value, &active);
			info->active = active;

			DBG("%s Active %d", info->path, info->active);
		} else if (g_str_equal(key, "Settings")) {
			DBG("%s Settings", info->path);

			extract_settings(&value, info);
		} else if (g_str_equal(key, "Name")) {
			char *name;

			dbus_message_iter_get_basic(&value, &name);

			info->name = g_strdup(name);

			DBG("%s Name %s", info->path, info->name);
		}

		dbus_message_iter_next(properties);
	}

	g_hash_table_insert(dundee_devices, g_strdup(path), info);

	err = create_device(info);
	if (err < 0)
		goto out;

	err = create_network(info);
	if (err < 0) {
		destroy_device(info);
		goto out;
	}

	if (info->active)
		set_connected(info);

	return;

out:
	g_hash_table_remove(dundee_devices, path);
}

static gboolean device_added(DBusConnection *conn, DBusMessage *message,
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
		connman_error("dundee signature does not match");
		return TRUE;
	}

	DBG("");

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &path);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &properties);

	add_device(path, &properties);

	return TRUE;
}

static void remove_device(DBusConnection *conn, const char *path)
{
	DBG("path %s", path);

	g_hash_table_remove(dundee_devices, path);
}

static gboolean device_removed(DBusConnection *conn, DBusMessage *message,
				void *user_data)
{
	const char *path;
	const char *signature = DBUS_TYPE_OBJECT_PATH_AS_STRING;

	if (!dbus_message_has_signature(message, signature)) {
		connman_error("dundee signature does not match");
		return TRUE;
	}

	dbus_message_get_args(message, NULL, DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_INVALID);
	remove_device(conn, path);
	return TRUE;
}

static void manager_get_devices_reply(DBusPendingCall *call, void *user_data)
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

	DBG("");

	reply = dbus_pending_call_steal_reply(call);

	if (!dbus_message_has_signature(reply, signature)) {
		connman_error("dundee signature does not match");
		goto done;
	}

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("%s", error.message);
		dbus_error_free(&error);
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

		add_device(path, &properties);

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int manager_get_devices(void)
{
	DBusMessage *message;
	DBusPendingCall *call;

	DBG("");

	message = dbus_message_new_method_call(DUNDEE_SERVICE, "/",
					DUNDEE_MANAGER_INTERFACE, GET_DEVICES);
	if (!message)
		return -ENOMEM;

	if (!dbus_connection_send_with_reply(connection, message,
						&call, TIMEOUT)) {
		connman_error("Failed to call GetDevices()");
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!call) {
		connman_error("D-Bus connection not available");
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(call, manager_get_devices_reply,
					NULL, NULL);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static void dundee_connect(DBusConnection *conn, void *user_data)
{
	DBG("connection %p", conn);

	dundee_devices = g_hash_table_new_full(g_str_hash, g_str_equal,
					g_free, device_destroy);

	manager_get_devices();
}

static void dundee_disconnect(DBusConnection *conn, void *user_data)
{
	DBG("connection %p", conn);

	g_hash_table_destroy(dundee_devices);
	dundee_devices = NULL;
}

static guint watch;
static guint added_watch;
static guint removed_watch;
static guint device_watch;

static int dundee_init(void)
{
	int err;

	connection = connman_dbus_get_connection();
	if (!connection)
		return -EIO;

	watch = g_dbus_add_service_watch(connection, DUNDEE_SERVICE,
			dundee_connect, dundee_disconnect, NULL, NULL);

	added_watch = g_dbus_add_signal_watch(connection, DUNDEE_SERVICE, NULL,
						DUNDEE_MANAGER_INTERFACE,
						DEVICE_ADDED, device_added,
						NULL, NULL);

	removed_watch = g_dbus_add_signal_watch(connection, DUNDEE_SERVICE,
						NULL, DUNDEE_MANAGER_INTERFACE,
						DEVICE_REMOVED, device_removed,
						NULL, NULL);

	device_watch = g_dbus_add_signal_watch(connection, DUNDEE_SERVICE,
						NULL, DUNDEE_DEVICE_INTERFACE,
						PROPERTY_CHANGED,
						device_changed,
						NULL, NULL);


	if (watch == 0 || added_watch == 0 || removed_watch == 0 ||
			device_watch == 0) {
		err = -EIO;
		goto remove;
	}

	err = connman_network_driver_register(&network_driver);
	if (err < 0)
		goto remove;

	err = connman_device_driver_register(&dundee_driver);
	if (err < 0) {
		connman_network_driver_unregister(&network_driver);
		goto remove;
	}

	return 0;

remove:
	g_dbus_remove_watch(connection, watch);
	g_dbus_remove_watch(connection, added_watch);
	g_dbus_remove_watch(connection, removed_watch);
	g_dbus_remove_watch(connection, device_watch);

	dbus_connection_unref(connection);

	return err;
}

static void dundee_exit(void)
{
	g_dbus_remove_watch(connection, watch);
	g_dbus_remove_watch(connection, added_watch);
	g_dbus_remove_watch(connection, removed_watch);
	g_dbus_remove_watch(connection, device_watch);

	connman_device_driver_unregister(&dundee_driver);
	connman_network_driver_unregister(&network_driver);

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(dundee, "Dundee plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, dundee_init, dundee_exit)
