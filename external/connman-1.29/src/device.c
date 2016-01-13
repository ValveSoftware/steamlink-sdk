/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2014  Intel Corporation. All rights reserved.
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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <net/if.h>

#include "connman.h"

static GSList *device_list = NULL;
static gchar **device_filter = NULL;
static gchar **nodevice_filter = NULL;

enum connman_pending_type {
	PENDING_NONE	= 0,
	PENDING_ENABLE	= 1,
	PENDING_DISABLE = 2,
};

struct connman_device {
	int refcount;
	enum connman_device_type type;
	enum connman_pending_type powered_pending;	/* Indicates a pending
							 * enable/disable
							 * request
							 */
	bool powered;
	bool scanning;
	bool disconnected;
	char *name;
	char *node;
	char *address;
	char *interface;
	char *ident;
	char *path;
	int index;
	guint pending_timeout;

	struct connman_device_driver *driver;
	void *driver_data;

	char *last_network;
	struct connman_network *network;
	GHashTable *networks;
};

static void clear_pending_trigger(struct connman_device *device)
{
	if (device->pending_timeout > 0) {
		g_source_remove(device->pending_timeout);
		device->pending_timeout = 0;
	}
}

static const char *type2description(enum connman_device_type type)
{
	switch (type) {
	case CONNMAN_DEVICE_TYPE_UNKNOWN:
	case CONNMAN_DEVICE_TYPE_VENDOR:
		break;
	case CONNMAN_DEVICE_TYPE_ETHERNET:
		return "Ethernet";
	case CONNMAN_DEVICE_TYPE_WIFI:
		return "Wireless";
	case CONNMAN_DEVICE_TYPE_BLUETOOTH:
		return "Bluetooth";
	case CONNMAN_DEVICE_TYPE_GPS:
		return "GPS";
	case CONNMAN_DEVICE_TYPE_CELLULAR:
		return "Cellular";
	case CONNMAN_DEVICE_TYPE_GADGET:
		return "Gadget";

	}

	return NULL;
}

static const char *type2string(enum connman_device_type type)
{
	switch (type) {
	case CONNMAN_DEVICE_TYPE_UNKNOWN:
	case CONNMAN_DEVICE_TYPE_VENDOR:
		break;
	case CONNMAN_DEVICE_TYPE_ETHERNET:
		return "ethernet";
	case CONNMAN_DEVICE_TYPE_WIFI:
		return "wifi";
	case CONNMAN_DEVICE_TYPE_BLUETOOTH:
		return "bluetooth";
	case CONNMAN_DEVICE_TYPE_GPS:
		return "gps";
	case CONNMAN_DEVICE_TYPE_CELLULAR:
		return "cellular";
	case CONNMAN_DEVICE_TYPE_GADGET:
		return "gadget";

	}

	return NULL;
}

enum connman_service_type __connman_device_get_service_type(
				struct connman_device *device)
{
	enum connman_device_type type = connman_device_get_type(device);

	switch (type) {
	case CONNMAN_DEVICE_TYPE_UNKNOWN:
	case CONNMAN_DEVICE_TYPE_VENDOR:
	case CONNMAN_DEVICE_TYPE_GPS:
		break;
	case CONNMAN_DEVICE_TYPE_ETHERNET:
		return CONNMAN_SERVICE_TYPE_ETHERNET;
	case CONNMAN_DEVICE_TYPE_WIFI:
		return CONNMAN_SERVICE_TYPE_WIFI;
	case CONNMAN_DEVICE_TYPE_BLUETOOTH:
		return CONNMAN_SERVICE_TYPE_BLUETOOTH;
	case CONNMAN_DEVICE_TYPE_CELLULAR:
		return CONNMAN_SERVICE_TYPE_CELLULAR;
	case CONNMAN_DEVICE_TYPE_GADGET:
		return CONNMAN_SERVICE_TYPE_GADGET;

	}

	return CONNMAN_SERVICE_TYPE_UNKNOWN;
}

static gboolean device_pending_reset(gpointer user_data)
{
	struct connman_device *device = user_data;

	DBG("device %p", device);

	/* Power request timedout, reset power pending state. */
	device->pending_timeout = 0;
	device->powered_pending = PENDING_NONE;

	return FALSE;
}

int __connman_device_enable(struct connman_device *device)
{
	int err;

	DBG("device %p", device);

	if (!device->driver || !device->driver->enable)
		return -EOPNOTSUPP;

	/* There is an ongoing power disable request. */
	if (device->powered_pending == PENDING_DISABLE)
		return -EBUSY;

	if (device->powered_pending == PENDING_ENABLE)
		return -EALREADY;

	if (device->powered_pending == PENDING_NONE && device->powered)
		return -EALREADY;

	if (device->index > 0) {
		err = connman_inet_ifup(device->index);
		if (err < 0 && err != -EALREADY)
			return err;
	}

	device->powered_pending = PENDING_ENABLE;

	err = device->driver->enable(device);
	/*
	 * device gets enabled right away.
	 * Invoke the callback
	 */
	if (err == 0) {
		connman_device_set_powered(device, true);
		goto done;
	}

	if (err == -EALREADY) {
		/* If device is already powered, but connman is not updated */
		connman_device_set_powered(device, true);
		goto done;
	}
	/*
	 * if err == -EINPROGRESS, then the DBus call to the respective daemon
	 * was successful. We set a 4 sec timeout so if the daemon never
	 * returns a reply, we would reset the pending request.
	 */
	if (err == -EINPROGRESS)
		device->pending_timeout = g_timeout_add_seconds(4,
					device_pending_reset, device);
done:
	return err;
}

int __connman_device_disable(struct connman_device *device)
{
	int err;

	DBG("device %p", device);

	/* Ongoing power enable request */
	if (device->powered_pending == PENDING_ENABLE)
		return -EBUSY;

	if (device->powered_pending == PENDING_DISABLE)
		return -EALREADY;

	if (device->powered_pending == PENDING_NONE && !device->powered)
		return -EALREADY;

	device->powered_pending = PENDING_DISABLE;

	if (device->network) {
		struct connman_service *service =
			connman_service_lookup_from_network(device->network);

		if (service)
			__connman_service_disconnect(service);
		else
			connman_network_set_connected(device->network, false);
	}

	if (!device->driver || !device->driver->disable)
		return -EOPNOTSUPP;

	err = device->driver->disable(device);
	if (err == 0 || err == -EALREADY) {
		connman_device_set_powered(device, false);
		goto done;
	}

	if (err == -EINPROGRESS)
		device->pending_timeout = g_timeout_add_seconds(4,
					device_pending_reset, device);
done:
	return err;
}

static void probe_driver(struct connman_device_driver *driver)
{
	GSList *list;

	DBG("driver %p name %s", driver, driver->name);

	for (list = device_list; list; list = list->next) {
		struct connman_device *device = list->data;

		if (device->driver)
			continue;

		if (driver->type != device->type)
			continue;

		if (driver->probe(device) < 0)
			continue;

		device->driver = driver;

		__connman_technology_add_device(device);
	}
}

static void remove_device(struct connman_device *device)
{
	DBG("device %p", device);

	__connman_device_disable(device);

	__connman_technology_remove_device(device);

	if (device->driver->remove)
		device->driver->remove(device);

	device->driver = NULL;
}

static void remove_driver(struct connman_device_driver *driver)
{
	GSList *list;

	DBG("driver %p name %s", driver, driver->name);

	for (list = device_list; list; list = list->next) {
		struct connman_device *device = list->data;

		if (device->driver == driver)
			remove_device(device);
	}
}

bool __connman_device_has_driver(struct connman_device *device)
{
	if (!device || !device->driver)
		return false;

	return true;
}

static GSList *driver_list = NULL;

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_device_driver *driver1 = a;
	const struct connman_device_driver *driver2 = b;

	return driver2->priority - driver1->priority;
}

/**
 * connman_device_driver_register:
 * @driver: device driver definition
 *
 * Register a new device driver
 *
 * Returns: %0 on success
 */
int connman_device_driver_register(struct connman_device_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_insert_sorted(driver_list, driver,
							compare_priority);
	probe_driver(driver);

	return 0;
}

/**
 * connman_device_driver_unregister:
 * @driver: device driver definition
 *
 * Remove a previously registered device driver
 */
void connman_device_driver_unregister(struct connman_device_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_remove(driver_list, driver);

	remove_driver(driver);
}

static void free_network(gpointer data)
{
	struct connman_network *network = data;

	DBG("network %p", network);

	__connman_network_set_device(network, NULL);

	connman_network_unref(network);
}

static void device_destruct(struct connman_device *device)
{
	DBG("device %p name %s", device, device->name);

	clear_pending_trigger(device);

	g_free(device->ident);
	g_free(device->node);
	g_free(device->name);
	g_free(device->address);
	g_free(device->interface);
	g_free(device->path);

	g_free(device->last_network);

	g_hash_table_destroy(device->networks);
	device->networks = NULL;

	g_free(device);
}

/**
 * connman_device_create:
 * @node: device node name (for example an address)
 * @type: device type
 *
 * Allocate a new device of given #type and assign the #node name to it.
 *
 * Returns: a newly-allocated #connman_device structure
 */
struct connman_device *connman_device_create(const char *node,
						enum connman_device_type type)
{
	struct connman_device *device;

	DBG("node %s type %d", node, type);

	device = g_try_new0(struct connman_device, 1);
	if (!device)
		return NULL;

	DBG("device %p", device);

	device->refcount = 1;

	device->type = type;
	device->name = g_strdup(type2description(device->type));

	device->networks = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, free_network);

	device_list = g_slist_prepend(device_list, device);

	return device;
}

/**
 * connman_device_ref:
 * @device: device structure
 *
 * Increase reference counter of device
 */
struct connman_device *connman_device_ref_debug(struct connman_device *device,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", device, device->refcount + 1,
		file, line, caller);

	__sync_fetch_and_add(&device->refcount, 1);

	return device;
}

/**
 * connman_device_unref:
 * @device: device structure
 *
 * Decrease reference counter of device
 */
void connman_device_unref_debug(struct connman_device *device,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", device, device->refcount - 1,
		file, line, caller);

	if (__sync_fetch_and_sub(&device->refcount, 1) != 1)
		return;

	if (device->driver) {
		device->driver->remove(device);
		device->driver = NULL;
	}

	device_list = g_slist_remove(device_list, device);

	device_destruct(device);
}

const char *__connman_device_get_type(struct connman_device *device)
{
	return type2string(device->type);
}

/**
 * connman_device_get_type:
 * @device: device structure
 *
 * Get type of device
 */
enum connman_device_type connman_device_get_type(struct connman_device *device)
{
	return device->type;
}

/**
 * connman_device_set_index:
 * @device: device structure
 * @index: index number
 *
 * Set index number of device
 */
void connman_device_set_index(struct connman_device *device, int index)
{
	device->index = index;
}

/**
 * connman_device_get_index:
 * @device: device structure
 *
 * Get index number of device
 */
int connman_device_get_index(struct connman_device *device)
{
	return device->index;
}

/**
 * connman_device_set_interface:
 * @device: device structure
 * @interface: interface name
 *
 * Set interface name of device
 */
void connman_device_set_interface(struct connman_device *device,
						const char *interface)
{
	g_free(device->interface);
	device->interface = g_strdup(interface);

	if (!device->name) {
		const char *str = type2description(device->type);
		if (str && device->interface)
			device->name = g_strdup_printf("%s (%s)", str,
							device->interface);
	}
}

/**
 * connman_device_set_ident:
 * @device: device structure
 * @ident: unique identifier
 *
 * Set unique identifier of device
 */
void connman_device_set_ident(struct connman_device *device,
							const char *ident)
{
	g_free(device->ident);
	device->ident = g_strdup(ident);
}

const char *connman_device_get_ident(struct connman_device *device)
{
	return device->ident;
}

/**
 * connman_device_set_powered:
 * @device: device structure
 * @powered: powered state
 *
 * Change power state of device
 */
int connman_device_set_powered(struct connman_device *device,
						bool powered)
{
	enum connman_service_type type;

	DBG("driver %p powered %d", device, powered);

	if (device->powered == powered)
		return -EALREADY;

	clear_pending_trigger(device);

	device->powered_pending = PENDING_NONE;

	device->powered = powered;

	type = __connman_device_get_service_type(device);

	if (!device->powered) {
		__connman_technology_disabled(type);
		return 0;
	}

	__connman_technology_enabled(type);

	connman_device_set_disconnected(device, false);
	device->scanning = false;

	if (device->driver && device->driver->scan)
		device->driver->scan(CONNMAN_SERVICE_TYPE_UNKNOWN, device,
					NULL, 0, NULL, NULL, NULL, NULL);

	return 0;
}

bool connman_device_get_powered(struct connman_device *device)
{
	return device->powered;
}

static int device_scan(enum connman_service_type type,
				struct connman_device *device)
{
	if (!device->driver || !device->driver->scan)
		return -EOPNOTSUPP;

	if (!device->powered)
		return -ENOLINK;

	return device->driver->scan(type, device, NULL, 0,
					NULL, NULL, NULL, NULL);
}

int __connman_device_disconnect(struct connman_device *device)
{
	GHashTableIter iter;
	gpointer key, value;

	DBG("device %p", device);

	connman_device_set_disconnected(device, true);

	g_hash_table_iter_init(&iter, device->networks);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct connman_network *network = value;

		if (connman_network_get_connecting(network)) {
			/*
			 * Skip network in the process of connecting.
			 * This is a workaround for WiFi networks serviced
			 * by the supplicant plugin that hold a reference
			 * to the network.  If we disconnect the network
			 * here then the referenced object will not be
			 * registered and usage (like launching DHCP client)
			 * will fail.  There is nothing to be gained by
			 * removing the network here anyway.
			 */
			connman_warn("Skipping disconnect of %s, network is connecting.",
				connman_network_get_identifier(network));
			continue;
		}

		__connman_network_disconnect(network);
	}

	return 0;
}

int connman_device_reconnect_service(struct connman_device *device)
{
	DBG("device %p", device);

	__connman_service_auto_connect(CONNMAN_SERVICE_CONNECT_REASON_AUTO);

	return 0;
}

static void mark_network_available(gpointer key, gpointer value,
							gpointer user_data)
{
	struct connman_network *network = value;

	connman_network_set_available(network, true);
}

static void mark_network_unavailable(gpointer key, gpointer value,
							gpointer user_data)
{
	struct connman_network *network = value;

	if (connman_network_get_connected(network) ||
			connman_network_get_connecting(network))
		return;

	connman_network_set_available(network, false);
}

static gboolean remove_unavailable_network(gpointer key, gpointer value,
							gpointer user_data)
{
	struct connman_network *network = value;

	if (connman_network_get_connected(network))
		return FALSE;

	if (connman_network_get_available(network))
		return FALSE;

	return TRUE;
}

void __connman_device_cleanup_networks(struct connman_device *device)
{
	g_hash_table_foreach_remove(device->networks,
					remove_unavailable_network, NULL);
}

bool connman_device_get_scanning(struct connman_device *device)
{
	return device->scanning;
}

void connman_device_reset_scanning(struct connman_device *device)
{
	g_hash_table_foreach(device->networks,
				mark_network_available, NULL);
}

/**
 * connman_device_set_scanning:
 * @device: device structure
 * @scanning: scanning state
 *
 * Change scanning state of device
 */
int connman_device_set_scanning(struct connman_device *device,
				enum connman_service_type type, bool scanning)
{
	DBG("device %p scanning %d", device, scanning);

	if (!device->driver || !device->driver->scan)
		return -EINVAL;

	if (device->scanning == scanning)
		return -EALREADY;

	device->scanning = scanning;

	if (scanning) {
		__connman_technology_scan_started(device);

		g_hash_table_foreach(device->networks,
					mark_network_unavailable, NULL);

		return 0;
	}

	__connman_device_cleanup_networks(device);

	__connman_technology_scan_stopped(device, type);

	__connman_service_auto_connect(CONNMAN_SERVICE_CONNECT_REASON_AUTO);

	return 0;
}

/**
 * connman_device_set_disconnected:
 * @device: device structure
 * @disconnected: disconnected state
 *
 * Change disconnected state of device (only for device with networks)
 */
int connman_device_set_disconnected(struct connman_device *device,
						bool disconnected)
{
	DBG("device %p disconnected %d", device, disconnected);

	if (device->disconnected == disconnected)
		return -EALREADY;

	device->disconnected = disconnected;

	return 0;
}

/**
 * connman_device_get_disconnected:
 * @device: device structure
 *
 * Get device disconnected state
 */
bool connman_device_get_disconnected(struct connman_device *device)
{
	return device->disconnected;
}

/**
 * connman_device_set_string:
 * @device: device structure
 * @key: unique identifier
 * @value: string value
 *
 * Set string value for specific key
 */
int connman_device_set_string(struct connman_device *device,
					const char *key, const char *value)
{
	DBG("device %p key %s value %s", device, key, value);

	if (g_str_equal(key, "Address")) {
		g_free(device->address);
		device->address = g_strdup(value);
	} else if (g_str_equal(key, "Name")) {
		g_free(device->name);
		device->name = g_strdup(value);
	} else if (g_str_equal(key, "Node")) {
		g_free(device->node);
		device->node = g_strdup(value);
	} else if (g_str_equal(key, "Path")) {
		g_free(device->path);
		device->path = g_strdup(value);
	} else {
		return -EINVAL;
	}

	return 0;
}

/**
 * connman_device_get_string:
 * @device: device structure
 * @key: unique identifier
 *
 * Get string value for specific key
 */
const char *connman_device_get_string(struct connman_device *device,
							const char *key)
{
	DBG("device %p key %s", device, key);

	if (g_str_equal(key, "Address"))
		return device->address;
	else if (g_str_equal(key, "Name"))
		return device->name;
	else if (g_str_equal(key, "Node"))
		return device->node;
	else if (g_str_equal(key, "Interface"))
		return device->interface;
	else if (g_str_equal(key, "Path"))
		return device->path;

	return NULL;
}

/**
 * connman_device_add_network:
 * @device: device structure
 * @network: network structure
 *
 * Add new network to the device
 */
int connman_device_add_network(struct connman_device *device,
					struct connman_network *network)
{
	const char *identifier = connman_network_get_identifier(network);

	DBG("device %p network %p", device, network);

	if (!identifier)
		return -EINVAL;

	connman_network_ref(network);

	__connman_network_set_device(network, device);

	g_hash_table_replace(device->networks, g_strdup(identifier),
								network);

	return 0;
}

/**
 * connman_device_get_network:
 * @device: device structure
 * @identifier: network identifier
 *
 * Get network for given identifier
 */
struct connman_network *connman_device_get_network(struct connman_device *device,
							const char *identifier)
{
	DBG("device %p identifier %s", device, identifier);

	return g_hash_table_lookup(device->networks, identifier);
}

/**
 * connman_device_remove_network:
 * @device: device structure
 * @identifier: network identifier
 *
 * Remove network for given identifier
 */
int connman_device_remove_network(struct connman_device *device,
						struct connman_network *network)
{
	const char *identifier;

	DBG("device %p network %p", device, network);

	if (!network)
		return 0;

	identifier = connman_network_get_identifier(network);
	g_hash_table_remove(device->networks, identifier);

	return 0;
}

void __connman_device_set_network(struct connman_device *device,
					struct connman_network *network)
{
	const char *name;

	if (!device)
		return;

	if (device->network == network)
		return;

	if (network) {
		name = connman_network_get_string(network, "Name");
		g_free(device->last_network);
		device->last_network = g_strdup(name);

		device->network = network;
	} else {
		g_free(device->last_network);
		device->last_network = NULL;

		device->network = NULL;
	}
}

static bool match_driver(struct connman_device *device,
					struct connman_device_driver *driver)
{
	if (device->type == driver->type ||
			driver->type == CONNMAN_DEVICE_TYPE_UNKNOWN)
		return true;

	return false;
}

/**
 * connman_device_register:
 * @device: device structure
 *
 * Register device with the system
 */
int connman_device_register(struct connman_device *device)
{
	GSList *list;

	DBG("device %p name %s", device, device->name);

	if (device->driver)
		return -EALREADY;

	for (list = driver_list; list; list = list->next) {
		struct connman_device_driver *driver = list->data;

		if (!match_driver(device, driver))
			continue;

		DBG("driver %p name %s", driver, driver->name);

		if (driver->probe(device) == 0) {
			device->driver = driver;
			break;
		}
	}

	if (!device->driver)
		return 0;

	return __connman_technology_add_device(device);
}

/**
 * connman_device_unregister:
 * @device: device structure
 *
 * Unregister device with the system
 */
void connman_device_unregister(struct connman_device *device)
{
	DBG("device %p name %s", device, device->name);

	if (!device->driver)
		return;

	remove_device(device);
}

/**
 * connman_device_get_data:
 * @device: device structure
 *
 * Get private device data pointer
 */
void *connman_device_get_data(struct connman_device *device)
{
	return device->driver_data;
}

/**
 * connman_device_set_data:
 * @device: device structure
 * @data: data pointer
 *
 * Set private device data pointer
 */
void connman_device_set_data(struct connman_device *device, void *data)
{
	device->driver_data = data;
}

struct connman_device *__connman_device_find_device(
				enum connman_service_type type)
{
	GSList *list;

	for (list = device_list; list; list = list->next) {
		struct connman_device *device = list->data;
		enum connman_service_type service_type =
			__connman_device_get_service_type(device);

		if (service_type != type)
			continue;

		return device;
	}

	return NULL;
}

struct connman_device *connman_device_find_by_index(int index)
{
	GSList *list;

	for (list = device_list; list; list = list->next) {
		struct connman_device *device = list->data;
		if (device->index == index)
			return device;
	}

	return NULL;
}

/**
 * connman_device_set_regdom
 * @device: device structure
 * @alpha2: string representing regulatory domain
 *
 * Set regulatory domain on device basis
 */
int connman_device_set_regdom(struct connman_device *device,
						const char *alpha2)
{
	if (!device->driver || !device->driver->set_regdom)
		return -ENOTSUP;

	if (!device->powered)
		return -EINVAL;

	return device->driver->set_regdom(device, alpha2);
}

/**
 * connman_device_regdom_notify
 * @device: device structure
 * @alpha2: string representing regulatory domain
 *
 * Notify on setting regulatory domain on device basis
 */
void connman_device_regdom_notify(struct connman_device *device,
					int result, const char *alpha2)
{
	__connman_technology_notify_regdom_by_device(device, result, alpha2);
}

int __connman_device_request_scan(enum connman_service_type type)
{
	bool success = false;
	int last_err = -ENOSYS;
	GSList *list;
	int err;

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_VPN:
	case CONNMAN_SERVICE_TYPE_GADGET:
		return -EOPNOTSUPP;
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	}

	for (list = device_list; list; list = list->next) {
		struct connman_device *device = list->data;
		enum connman_service_type service_type =
			__connman_device_get_service_type(device);

		if (service_type != CONNMAN_SERVICE_TYPE_UNKNOWN) {
			if (type == CONNMAN_SERVICE_TYPE_P2P) {
				if (service_type != CONNMAN_SERVICE_TYPE_WIFI)
					continue;
			} else if (service_type != type)
				continue;
		}

		err = device_scan(type, device);
		if (err == 0 || err == -EALREADY || err == -EINPROGRESS) {
			success = true;
		} else {
			last_err = err;
			DBG("device %p err %d", device, err);
		}
	}

	if (success)
		return 0;

	return last_err;
}

int __connman_device_request_hidden_scan(struct connman_device *device,
				const char *ssid, unsigned int ssid_len,
				const char *identity, const char *passphrase,
				const char *security, void *user_data)
{
	DBG("device %p", device);

	if (!device || !device->driver ||
			!device->driver->scan)
		return -EINVAL;

	return device->driver->scan(CONNMAN_SERVICE_TYPE_UNKNOWN,
					device, ssid, ssid_len, identity,
					passphrase, security, user_data);
}

static char *index2ident(int index, const char *prefix)
{
	struct ifreq ifr;
	struct ether_addr eth;
	char *str;
	int sk, err, len;

	if (index < 0)
		return NULL;

	sk = socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sk < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	err = ioctl(sk, SIOCGIFNAME, &ifr);

	if (err == 0)
		err = ioctl(sk, SIOCGIFHWADDR, &ifr);

	close(sk);

	if (err < 0)
		return NULL;

	len = prefix ? strlen(prefix) + 18 : 18;

	str = g_malloc(len);
	if (!str)
		return NULL;

	memcpy(&eth, &ifr.ifr_hwaddr.sa_data, sizeof(eth));
	snprintf(str, len, "%s%02x%02x%02x%02x%02x%02x",
						prefix ? prefix : "",
						eth.ether_addr_octet[0],
						eth.ether_addr_octet[1],
						eth.ether_addr_octet[2],
						eth.ether_addr_octet[3],
						eth.ether_addr_octet[4],
						eth.ether_addr_octet[5]);

	return str;
}

static char *index2addr(int index)
{
	struct ifreq ifr;
	struct ether_addr eth;
	char *str;
	int sk, err;

	if (index < 0)
		return NULL;

	sk = socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sk < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	err = ioctl(sk, SIOCGIFNAME, &ifr);

	if (err == 0)
		err = ioctl(sk, SIOCGIFHWADDR, &ifr);

	close(sk);

	if (err < 0)
		return NULL;

	str = g_malloc(18);
	if (!str)
		return NULL;

	memcpy(&eth, &ifr.ifr_hwaddr.sa_data, sizeof(eth));
	snprintf(str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
						eth.ether_addr_octet[0],
						eth.ether_addr_octet[1],
						eth.ether_addr_octet[2],
						eth.ether_addr_octet[3],
						eth.ether_addr_octet[4],
						eth.ether_addr_octet[5]);

	return str;
}

struct connman_device *connman_device_create_from_index(int index)
{
	enum connman_device_type type;
	struct connman_device *device;
	char *devname, *ident = NULL;
	char *addr = NULL, *name = NULL;

	if (index < 0)
		return NULL;

	devname = connman_inet_ifname(index);
	if (!devname)
		return NULL;

	if (__connman_device_isfiltered(devname)) {
		connman_info("Ignoring interface %s (filtered)", devname);
		g_free(devname);
		return NULL;
	}

	type = __connman_rtnl_get_device_type(index);

	switch (type) {
	case CONNMAN_DEVICE_TYPE_UNKNOWN:
		connman_info("Ignoring interface %s (type unknown)", devname);
		g_free(devname);
		return NULL;
	case CONNMAN_DEVICE_TYPE_ETHERNET:
	case CONNMAN_DEVICE_TYPE_GADGET:
	case CONNMAN_DEVICE_TYPE_WIFI:
		name = index2ident(index, "");
		addr = index2addr(index);
		break;
	case CONNMAN_DEVICE_TYPE_BLUETOOTH:
	case CONNMAN_DEVICE_TYPE_CELLULAR:
	case CONNMAN_DEVICE_TYPE_GPS:
	case CONNMAN_DEVICE_TYPE_VENDOR:
		name = g_strdup(devname);
		break;
	}

	device = connman_device_create(name, type);
	if (!device)
		goto done;

	switch (type) {
	case CONNMAN_DEVICE_TYPE_UNKNOWN:
	case CONNMAN_DEVICE_TYPE_VENDOR:
	case CONNMAN_DEVICE_TYPE_GPS:
		break;
	case CONNMAN_DEVICE_TYPE_ETHERNET:
	case CONNMAN_DEVICE_TYPE_GADGET:
		ident = index2ident(index, NULL);
		break;
	case CONNMAN_DEVICE_TYPE_WIFI:
		ident = index2ident(index, NULL);
		break;
	case CONNMAN_DEVICE_TYPE_BLUETOOTH:
		break;
	case CONNMAN_DEVICE_TYPE_CELLULAR:
		ident = index2ident(index, NULL);
		break;
	}

	connman_device_set_index(device, index);
	connman_device_set_interface(device, devname);

	if (ident) {
		connman_device_set_ident(device, ident);
		g_free(ident);
	}

	connman_device_set_string(device, "Address", addr);

done:
	g_free(devname);
	g_free(name);
	g_free(addr);

	return device;
}

bool __connman_device_isfiltered(const char *devname)
{
	char **pattern;
	char **blacklisted_interfaces;
	bool match;

	if (!device_filter)
		goto nodevice;

	for (pattern = device_filter, match = false; *pattern; pattern++) {
		if (g_pattern_match_simple(*pattern, devname)) {
			match = true;
			break;
		}
	}

	if (!match) {
		DBG("ignoring device %s (match)", devname);
		return true;
	}

nodevice:
	if (g_pattern_match_simple("dummy*", devname)) {
		DBG("ignoring dummy networking devices");
		return true;
	}

	if (!nodevice_filter)
		goto list;

	for (pattern = nodevice_filter; *pattern; pattern++) {
		if (g_pattern_match_simple(*pattern, devname)) {
			DBG("ignoring device %s (no match)", devname);
			return true;
		}
	}

list:
	blacklisted_interfaces =
		connman_setting_get_string_list("NetworkInterfaceBlacklist");
	if (!blacklisted_interfaces)
		return false;

	for (pattern = blacklisted_interfaces; *pattern; pattern++) {
		if (g_str_has_prefix(devname, *pattern)) {
			DBG("ignoring device %s (blacklist)", devname);
			return true;
		}
	}

	return false;
}

static void cleanup_devices(void)
{
	/*
	 * Check what interfaces are currently up and if connman is
	 * suppose to handle the interface, then cleanup the mess
	 * related to that interface. There might be weird routes etc
	 * that are related to that interface and that might confuse
	 * connmand. So in this case we just turn the interface down
	 * so that kernel removes routes/addresses automatically and
	 * then proceed the startup.
	 *
	 * Note that this cleanup must be done before rtnl/detect code
	 * has activated interface watches.
	 */

	char **interfaces;
	int i;

	interfaces = __connman_inet_get_running_interfaces();

	if (!interfaces)
		return;

	for (i = 0; interfaces[i]; i++) {
		bool filtered;
		int index;
		struct sockaddr_in sin_addr, sin_mask;

		filtered = __connman_device_isfiltered(interfaces[i]);
		if (filtered)
			continue;

		index = connman_inet_ifindex(interfaces[i]);
		if (index < 0)
			continue;

		if (!__connman_inet_get_address_netmask(index, &sin_addr,
							&sin_mask)) {
			char *address = g_strdup(inet_ntoa(sin_addr.sin_addr));
			char *netmask = g_strdup(inet_ntoa(sin_mask.sin_addr));

			if (__connman_config_address_provisioned(address,
								netmask)) {
				DBG("Skip %s which is already provisioned "
					"with %s/%s", interfaces[i], address,
					netmask);
				g_free(address);
				g_free(netmask);
				continue;
			}

			g_free(address);
			g_free(netmask);
		}

		DBG("cleaning up %s index %d", interfaces[i], index);

		connman_inet_ifdown(index);

		/*
		 * ConnMan will turn the interface UP automatically so
		 * no need to do it here.
		 */
	}

	g_strfreev(interfaces);
}

int __connman_device_init(const char *device, const char *nodevice)
{
	DBG("");

	if (device)
		device_filter = g_strsplit(device, ",", -1);

	if (nodevice)
		nodevice_filter = g_strsplit(nodevice, ",", -1);

	cleanup_devices();

	return 0;
}

void __connman_device_cleanup(void)
{
	DBG("");

	g_strfreev(nodevice_filter);
	g_strfreev(device_filter);
}
