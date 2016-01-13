/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
 *  Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *  Copyright (C) 2011-2014  BMW Car IT GmbH.
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
#include <stdlib.h>

#include <gdbus.h>
#include <string.h>
#include <stdint.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/device.h>
#include <connman/network.h>
#include <connman/inet.h>
#include <connman/dbus.h>
#include <connman/log.h>
#include <connman/technology.h>

#include "mcc.h"

#define OFONO_SERVICE			"org.ofono"

#define OFONO_MANAGER_INTERFACE		OFONO_SERVICE ".Manager"
#define OFONO_MODEM_INTERFACE		OFONO_SERVICE ".Modem"
#define OFONO_SIM_INTERFACE		OFONO_SERVICE ".SimManager"
#define OFONO_NETREG_INTERFACE		OFONO_SERVICE ".NetworkRegistration"
#define OFONO_CM_INTERFACE		OFONO_SERVICE ".ConnectionManager"
#define OFONO_CONTEXT_INTERFACE		OFONO_SERVICE ".ConnectionContext"
#define OFONO_CDMA_CM_INTERFACE		OFONO_SERVICE ".cdma.ConnectionManager"
#define OFONO_CDMA_NETREG_INTERFACE	OFONO_SERVICE ".cdma.NetworkRegistration"

#define MODEM_ADDED			"ModemAdded"
#define MODEM_REMOVED			"ModemRemoved"
#define PROPERTY_CHANGED		"PropertyChanged"
#define CONTEXT_ADDED			"ContextAdded"
#define CONTEXT_REMOVED			"ContextRemoved"

#define GET_PROPERTIES			"GetProperties"
#define SET_PROPERTY			"SetProperty"
#define GET_MODEMS			"GetModems"
#define GET_CONTEXTS			"GetContexts"

#define TIMEOUT 40000

enum ofono_api {
	OFONO_API_SIM =		0x1,
	OFONO_API_NETREG =	0x2,
	OFONO_API_CM =		0x4,
	OFONO_API_CDMA_NETREG =	0x8,
	OFONO_API_CDMA_CM =	0x10,
};

/*
 * The way this plugin works is following:
 *
 *   powered -> SubscriberIdentity or Online = True -> gprs, context ->
 *     attached -> netreg -> ready
 *
 * Depending on the modem type, this plugin will behave differently.
 *
 * GSM working flow:
 *
 * When a new modem appears, the plugin always powers it up. This
 * allows the plugin to create a connman_device. The core will call
 * modem_enable() if the technology is enabled. modem_enable() will
 * then set the modem online. If the technology is disabled then
 * modem_disable() will just set the modem offline. The modem is
 * always kept powered all the time.
 *
 * After setting the modem online the plugin waits for the
 * ConnectionManager and ConnectionContext to appear. When the context
 * signals that it is attached and the NetworkRegistration interface
 * appears, a new Service will be created and registered at the core.
 *
 * When asked to connect to the network (network_connect()) the plugin
 * will set the Active property on the context. If this operation is
 * successful the modem is connected to the network. oFono will inform
 * the plugin about IP configuration through the updating the context's
 * properties.
 *
 * CDMA working flow:
 *
 * When a new modem appears, the plugin always powers it up. This
 * allows the plugin to create connman_device either using IMSI either
 * using modem Serial if the modem got a SIM interface or not.
 *
 * As for GSM, the core will call modem_enable() if the technology
 * is enabled. modem_enable() will then set the modem online.
 * If the technology is disabled then modem_disable() will just set the
 * modem offline. The modem is always kept powered all the time.
 *
 * After setting the modem online the plugin waits for CdmaConnectionManager
 * interface to appear. Then, once CdmaNetworkRegistration appears, a new
 * Service will be created and registered at the core.
 *
 * When asked to connect to the network (network_connect()) the plugin
 * will power up the CdmaConnectionManager interface.
 * If the operation is successful the modem is connected to the network.
 * oFono will inform the plugin about IP configuration through the
 * updating CdmaConnectionManager settings properties.
 */

static DBusConnection *connection;

static GHashTable *modem_hash;
static GHashTable *context_hash;

struct network_context {
	char *path;
	int index;

	enum connman_ipconfig_method ipv4_method;
	struct connman_ipaddress *ipv4_address;
	char *ipv4_nameservers;

	enum connman_ipconfig_method ipv6_method;
	struct connman_ipaddress *ipv6_address;
	char *ipv6_nameservers;
};

struct modem_data {
	char *path;

	struct connman_device *device;
	struct connman_network *network;

	struct network_context *context;

	/* Modem Interface */
	char *serial;
	bool powered;
	bool online;
	uint8_t interfaces;
	bool ignore;

	bool set_powered;

	/* CDMA ConnectionManager Interface */
	bool cdma_cm_powered;

	/* ConnectionManager Interface */
	bool attached;
	bool cm_powered;

	/* ConnectionContext Interface */
	bool active;
	bool valid_apn; /* APN is 'valid' if length > 0 */

	/* SimManager Interface */
	char *imsi;

	/* Netreg Interface */
	char *name;
	uint8_t strength;
	uint8_t data_strength; /* 1xEVDO signal strength */
	bool registered;
	bool roaming;

	/* pending calls */
	DBusPendingCall	*call_set_property;
	DBusPendingCall	*call_get_properties;
	DBusPendingCall *call_get_contexts;
};

static const char *api2string(enum ofono_api api)
{
	switch (api) {
	case OFONO_API_SIM:
		return "sim";
	case OFONO_API_NETREG:
		return "netreg";
	case OFONO_API_CM:
		return "cm";
	case OFONO_API_CDMA_NETREG:
		return "cdma-netreg";
	case OFONO_API_CDMA_CM:
		return "cmda-cm";
	}

	return "unknown";
}

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

static struct network_context *network_context_alloc(const char *path)
{
	struct network_context *context;

	context = g_try_new0(struct network_context, 1);
	if (!context)
		return NULL;

	context->path = g_strdup(path);
	context->index = -1;

	context->ipv4_method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;
	context->ipv4_address = NULL;
	context->ipv4_nameservers = NULL;

	context->ipv6_method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;
	context->ipv6_address = NULL;
	context->ipv6_nameservers = NULL;

	return context;
}

static void network_context_free(struct network_context *context)
{
	g_free(context->path);

	connman_ipaddress_free(context->ipv4_address);
	g_free(context->ipv4_nameservers);

	connman_ipaddress_free(context->ipv6_address);
	g_free(context->ipv6_nameservers);

	free(context);
}

static void set_connected(struct modem_data *modem)
{
	struct connman_service *service;
	bool setip = false;
	enum connman_ipconfig_method method;
	char *nameservers;
	int index;

	DBG("%s", modem->path);

	index = modem->context->index;

	method = modem->context->ipv4_method;
	if (index < 0 || (!modem->context->ipv4_address &&
				method == CONNMAN_IPCONFIG_METHOD_FIXED)) {
		connman_error("Invalid index and/or address");
		return;
	}

	service = connman_service_lookup_from_network(modem->network);
	if (!service)
		return;

	connman_service_create_ip4config(service, index);
	connman_network_set_ipv4_method(modem->network, method);

	if (method == CONNMAN_IPCONFIG_METHOD_FIXED ||
			method == CONNMAN_IPCONFIG_METHOD_DHCP)	{
		setip = true;
	}

	if (method == CONNMAN_IPCONFIG_METHOD_FIXED) {
		connman_network_set_ipaddress(modem->network,
						modem->context->ipv4_address);
	}

	method = modem->context->ipv6_method;
	connman_service_create_ip6config(service, index);
	connman_network_set_ipv6_method(modem->network, method);

	if (method == CONNMAN_IPCONFIG_METHOD_AUTO) {
		setip = true;
	}

	/* Set the nameservers */
	if (modem->context->ipv4_nameservers &&
			modem->context->ipv6_nameservers) {
		nameservers = g_strdup_printf("%s %s",
					modem->context->ipv4_nameservers,
					modem->context->ipv6_nameservers);
		connman_network_set_nameservers(modem->network, nameservers);
		g_free(nameservers);
	} else if (modem->context->ipv4_nameservers) {
		connman_network_set_nameservers(modem->network,
					modem->context->ipv4_nameservers);
	} else if (modem->context->ipv6_nameservers) {
		connman_network_set_nameservers(modem->network,
					modem->context->ipv6_nameservers);
	}

	if (setip) {
		connman_network_set_index(modem->network, index);
		connman_network_set_connected(modem->network, true);
	}
}

static void set_disconnected(struct modem_data *modem)
{
	DBG("%s", modem->path);

	if (modem->network)
		connman_network_set_connected(modem->network, false);

	if (modem->context) {
		g_free(modem->context->ipv4_nameservers);
		modem->context->ipv4_nameservers = NULL;
		if (modem->context->ipv4_method != CONNMAN_IPCONFIG_METHOD_OFF)
			modem->context->ipv4_method =
					CONNMAN_IPCONFIG_METHOD_UNKNOWN;

		g_free(modem->context->ipv6_nameservers);
		modem->context->ipv6_nameservers = NULL;
		if (modem->context->ipv6_method != CONNMAN_IPCONFIG_METHOD_OFF)
			modem->context->ipv6_method =
					CONNMAN_IPCONFIG_METHOD_UNKNOWN;
	}
}

typedef void (*set_property_cb)(struct modem_data *data,
				bool success);
typedef void (*get_properties_cb)(struct modem_data *data,
				DBusMessageIter *dict);

struct property_info {
	struct modem_data *modem;
	const char *path;
	const char *interface;
	const char *property;
	set_property_cb set_property_cb;
	get_properties_cb get_properties_cb;
};

static void set_property_reply(DBusPendingCall *call, void *user_data)
{
	struct property_info *info = user_data;
	DBusMessage *reply;
	DBusError error;
	bool success = true;

	DBG("%s path %s %s.%s", info->modem->path,
		info->path, info->interface, info->property);

	info->modem->call_set_property = NULL;

	dbus_error_init(&error);

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("Failed to change property: %s %s.%s: %s %s",
				info->path, info->interface, info->property,
				error.name, error.message);
		dbus_error_free(&error);
		success = false;
	}

	if (info->set_property_cb)
		(*info->set_property_cb)(info->modem, success);

	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int set_property(struct modem_data *modem,
			const char *path, const char *interface,
			const char *property, int type, void *value,
			set_property_cb notify)
{
	DBusMessage *message;
	DBusMessageIter iter;
	struct property_info *info;

	DBG("%s path %s %s.%s", modem->path, path, interface, property);

	if (modem->call_set_property) {
		DBG("Cancel pending SetProperty");

		dbus_pending_call_cancel(modem->call_set_property);
		modem->call_set_property = NULL;
	}

	message = dbus_message_new_method_call(OFONO_SERVICE, path,
					interface, SET_PROPERTY);
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);
	connman_dbus_property_append_basic(&iter, property, type, value);

	if (!dbus_connection_send_with_reply(connection, message,
					&modem->call_set_property, TIMEOUT)) {
		connman_error("Failed to change property: %s %s.%s",
				path, interface, property);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!modem->call_set_property) {
		connman_error("D-Bus connection not available");
		dbus_message_unref(message);
		return -EINVAL;
	}

	info = g_try_new0(struct property_info, 1);
	if (!info) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	info->modem = modem;
	info->path = path;
	info->interface = interface;
	info->property = property;
	info->set_property_cb = notify;

	dbus_pending_call_set_notify(modem->call_set_property,
					set_property_reply, info, g_free);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static void get_properties_reply(DBusPendingCall *call, void *user_data)
{
	struct property_info *info = user_data;
	DBusMessageIter array, dict;
	DBusMessage *reply;
	DBusError error;

	DBG("%s path %s %s", info->modem->path, info->path, info->interface);

	info->modem->call_get_properties = NULL;

	dbus_error_init(&error);

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("Failed to get properties: %s %s: %s %s",
				info->path, info->interface,
				error.name, error.message);
		dbus_error_free(&error);

		goto done;
	}

	if (!dbus_message_iter_init(reply, &array))
		goto done;

	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_ARRAY)
		goto done;

	dbus_message_iter_recurse(&array, &dict);

	if (info->get_properties_cb)
		(*info->get_properties_cb)(info->modem, &dict);

done:

	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int get_properties(const char *path, const char *interface,
				get_properties_cb notify,
				struct modem_data *modem)
{
	DBusMessage *message;
	struct property_info *info;

	DBG("%s path %s %s", modem->path, path, interface);

	if (modem->call_get_properties) {
		connman_error("Pending GetProperties");
		return -EBUSY;
	}

	message = dbus_message_new_method_call(OFONO_SERVICE, path,
					interface, GET_PROPERTIES);
	if (!message)
		return -ENOMEM;

	if (!dbus_connection_send_with_reply(connection, message,
					&modem->call_get_properties, TIMEOUT)) {
		connman_error("Failed to call %s.GetProperties()", interface);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!modem->call_get_properties) {
		connman_error("D-Bus connection not available");
		dbus_message_unref(message);
		return -EINVAL;
	}

	info = g_try_new0(struct property_info, 1);
	if (!info) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	info->modem = modem;
	info->path = path;
	info->interface = interface;
	info->get_properties_cb = notify;

	dbus_pending_call_set_notify(modem->call_get_properties,
					get_properties_reply, info, g_free);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static void context_set_active_reply(struct modem_data *modem,
					bool success)
{
	DBG("%s", modem->path);

	if (success) {
		/*
		 * Don't handle do anything on success here. oFono will send
		 * the change via PropertyChanged singal.
		 */
		return;
	}

	/*
	 * Active = True might fail due a timeout. That means oFono
	 * still tries to go online. If we retry to set Active = True,
	 * we just get a InProgress error message. Should we power
	 * cycle the modem in such cases?
	 */

	if (!modem->network) {
		/*
		 * In the case where we power down the device
		 * we don't wait for the reply, therefore the network
		 * might already be gone.
		 */
		return;
	}

	connman_network_set_error(modem->network,
				CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL);
}

static int context_set_active(struct modem_data *modem,
				dbus_bool_t active)
{
	int err;

	DBG("%s active %d", modem->path, active);

	err = set_property(modem, modem->context->path,
				OFONO_CONTEXT_INTERFACE,
				"Active", DBUS_TYPE_BOOLEAN,
				&active,
				context_set_active_reply);

	if (!active && err == -EINPROGRESS)
		return 0;

	return err;
}

static void cdma_cm_set_powered_reply(struct modem_data *modem,
					bool success)
{
	DBG("%s", modem->path);

	if (success) {
		/*
		 * Don't handle do anything on success here. oFono will send
		 * the change via PropertyChanged singal.
		 */
		return;
	}

	/*
	 * Powered = True might fail due a timeout. That means oFono
	 * still tries to go online. If we retry to set Powered = True,
	 * we just get a InProgress error message. Should we power
	 * cycle the modem in such cases?
	 */

	if (!modem->network) {
		/*
		 * In the case where we power down the device
		 * we don't wait for the reply, therefore the network
		 * might already be gone.
		 */
		return;
	}

	connman_network_set_error(modem->network,
				CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL);
}

static int cdma_cm_set_powered(struct modem_data *modem, dbus_bool_t powered)
{
	int err;

	DBG("%s powered %d", modem->path, powered);

	err = set_property(modem, modem->path, OFONO_CDMA_CM_INTERFACE,
				"Powered", DBUS_TYPE_BOOLEAN,
				&powered,
				cdma_cm_set_powered_reply);

	if (!powered && err == -EINPROGRESS)
		return 0;

	return err;
}

static int modem_set_online(struct modem_data *modem, dbus_bool_t online)
{
	DBG("%s online %d", modem->path, online);

	return set_property(modem, modem->path,
				OFONO_MODEM_INTERFACE,
				"Online", DBUS_TYPE_BOOLEAN,
				&online,
				NULL);
}

static int cm_set_powered(struct modem_data *modem, dbus_bool_t powered)
{
	int err;

	DBG("%s powered %d", modem->path, powered);

	err = set_property(modem, modem->path,
				OFONO_CM_INTERFACE,
				"Powered", DBUS_TYPE_BOOLEAN,
				&powered,
				NULL);

	if (!powered && err == -EINPROGRESS)
		return 0;

	return err;
}

static int modem_set_powered(struct modem_data *modem, dbus_bool_t powered)
{
	int err;

	DBG("%s powered %d", modem->path, powered);

	modem->set_powered = powered;

	err = set_property(modem, modem->path,
				OFONO_MODEM_INTERFACE,
				"Powered", DBUS_TYPE_BOOLEAN,
				&powered,
				NULL);

	if (!powered && err == -EINPROGRESS)
		return 0;

	return err;
}

static bool has_interface(uint8_t interfaces,
					enum ofono_api api)
{
	if ((interfaces & api) == api)
		return true;

	return false;
}

static uint8_t extract_interfaces(DBusMessageIter *array)
{
	DBusMessageIter entry;
	uint8_t interfaces = 0;

	dbus_message_iter_recurse(array, &entry);

	while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
		const char *name;

		dbus_message_iter_get_basic(&entry, &name);

		if (g_str_equal(name, OFONO_SIM_INTERFACE))
			interfaces |= OFONO_API_SIM;
		else if (g_str_equal(name, OFONO_NETREG_INTERFACE))
			interfaces |= OFONO_API_NETREG;
		else if (g_str_equal(name, OFONO_CM_INTERFACE))
			interfaces |= OFONO_API_CM;
		else if (g_str_equal(name, OFONO_CDMA_CM_INTERFACE))
			interfaces |= OFONO_API_CDMA_CM;
		else if (g_str_equal(name, OFONO_CDMA_NETREG_INTERFACE))
			interfaces |= OFONO_API_CDMA_NETREG;

		dbus_message_iter_next(&entry);
	}

	return interfaces;
}

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

static void extract_ipv4_settings(DBusMessageIter *array,
				struct network_context *context)
{
	DBusMessageIter dict;
	char *address = NULL, *netmask = NULL, *gateway = NULL;
	char *nameservers = NULL;
	const char *interface = NULL;
	int index = -1;

	connman_ipaddress_free(context->ipv4_address);
	context->ipv4_address = NULL;
	context->index = -1;

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
		} else if (g_str_equal(key, "Method")) {
			dbus_message_iter_get_basic(&value, &val);

			DBG("Method %s", val);

			if (g_strcmp0(val, "static") == 0)
				context->ipv4_method = CONNMAN_IPCONFIG_METHOD_FIXED;
			else if (g_strcmp0(val, "dhcp") == 0)
				context->ipv4_method = CONNMAN_IPCONFIG_METHOD_DHCP;

		} else if (g_str_equal(key, "Address")) {
			dbus_message_iter_get_basic(&value, &val);

			address = g_strdup(val);

			DBG("Address %s", address);
		} else if (g_str_equal(key, "Netmask")) {
			dbus_message_iter_get_basic(&value, &val);

			netmask = g_strdup(val);

			DBG("Netmask %s", netmask);
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

	context->index = index;

	if (context->ipv4_method != CONNMAN_IPCONFIG_METHOD_FIXED)
		goto out;

	context->ipv4_address = connman_ipaddress_alloc(CONNMAN_IPCONFIG_TYPE_IPV4);
	if (!context->ipv4_address) {
		context->index = -1;
		goto out;
	}

	connman_ipaddress_set_ipv4(context->ipv4_address, address,
				netmask, gateway);

	g_free(context->ipv4_nameservers);
	context->ipv4_nameservers = nameservers;

out:
	if (context->ipv4_nameservers != nameservers)
		g_free(nameservers);

	g_free(address);
	g_free(netmask);
	g_free(gateway);
}

static void extract_ipv6_settings(DBusMessageIter *array,
				struct network_context *context)
{
	DBusMessageIter dict;
	char *address = NULL, *gateway = NULL;
	unsigned char prefix_length = 0;
	char *nameservers = NULL;
	const char *interface = NULL;
	int index = -1;

	connman_ipaddress_free(context->ipv6_address);
	context->ipv6_address = NULL;
	context->index = -1;

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
		} else if (g_str_equal(key, "Address")) {
			dbus_message_iter_get_basic(&value, &val);

			address = g_strdup(val);

			DBG("Address %s", address);
		} else if (g_str_equal(key, "PrefixLength")) {
			dbus_message_iter_get_basic(&value, &prefix_length);

			DBG("prefix length %d", prefix_length);
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

	context->ipv6_method = CONNMAN_IPCONFIG_METHOD_AUTO;

	context->ipv6_address =
		connman_ipaddress_alloc(CONNMAN_IPCONFIG_TYPE_IPV6);
	if (!context->ipv6_address)
		goto out;

	context->index = index;
	connman_ipaddress_set_ipv6(context->ipv6_address, address,
				prefix_length, gateway);

	g_free(context->ipv6_nameservers);
	context->ipv6_nameservers = nameservers;

out:
	if (context->ipv6_nameservers != nameservers)
		g_free(nameservers);

	g_free(address);
	g_free(gateway);
}

static bool ready_to_create_device(struct modem_data *modem)
{
	/*
	 * There are three different modem types which behave slightly
	 * different:
	 * - GSM modems will expose the SIM interface then the
	 *   CM interface.
	 * - CDMA modems will expose CM first and sometime later
	 *   a unique serial number.
	 *
	 * This functions tests if we have the necessary information gathered
	 * before we are able to create a device.
	 */

	if (modem->device)
		return false;

	if (modem->imsi || modem->serial)
		return true;

	return false;
}

static void create_device(struct modem_data *modem)
{
	struct connman_device *device;
	char *ident = NULL;

	DBG("%s", modem->path);

	if (modem->imsi)
		ident = modem->imsi;
	else if (modem->serial)
		ident = modem->serial;

	if (!connman_dbus_validate_ident(ident))
		ident = connman_dbus_encode_string(ident);
	else
		ident = g_strdup(ident);

	device = connman_device_create("ofono", CONNMAN_DEVICE_TYPE_CELLULAR);
	if (!device)
		goto out;

	DBG("device %p", device);

	connman_device_set_ident(device, ident);

	connman_device_set_string(device, "Path", modem->path);

	connman_device_set_data(device, modem);

	if (connman_device_register(device) < 0) {
		connman_error("Failed to register cellular device");
		connman_device_unref(device);
		goto out;
	}

	modem->device = device;

	connman_device_set_powered(modem->device, modem->online);
out:
	g_free(ident);
}

static void destroy_device(struct modem_data *modem)
{
	DBG("%s", modem->path);

	connman_device_set_powered(modem->device, false);

	if (modem->network) {
		connman_device_remove_network(modem->device, modem->network);
		connman_network_unref(modem->network);
		modem->network = NULL;
	}

	connman_device_unregister(modem->device);
	connman_device_unref(modem->device);

	modem->device = NULL;
}

static void add_network(struct modem_data *modem)
{
	const char *group;

	DBG("%s", modem->path);

	if (modem->network)
		return;

	modem->network = connman_network_create(modem->context->path,
						CONNMAN_NETWORK_TYPE_CELLULAR);
	if (!modem->network)
		return;

	DBG("network %p", modem->network);

	connman_network_set_data(modem->network, modem);

	connman_network_set_string(modem->network, "Path",
					modem->context->path);

	if (modem->name)
		connman_network_set_name(modem->network, modem->name);
	else
		connman_network_set_name(modem->network, "");

	connman_network_set_strength(modem->network, modem->strength);

	group = get_ident(modem->context->path);
	connman_network_set_group(modem->network, group);

	connman_network_set_bool(modem->network, "Roaming",
					modem->roaming);

	if (connman_device_add_network(modem->device, modem->network) < 0) {
		connman_network_unref(modem->network);
		modem->network = NULL;
		return;
	}
}

static void remove_network(struct modem_data *modem)
{
	DBG("%s", modem->path);

	if (!modem->network)
		return;

	DBG("network %p", modem->network);

	connman_device_remove_network(modem->device, modem->network);
	connman_network_unref(modem->network);
	modem->network = NULL;
}

static int set_context_ipconfig(struct network_context *context,
				const char *protocol)
{
	DBG("context %p protocol %s", context, protocol);

	if (!context || !protocol)
		return -EINVAL;

	if (g_str_equal(protocol, "ip")) {
		if (context->ipv4_method == CONNMAN_IPCONFIG_METHOD_OFF)
			context->ipv4_method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;

		context->ipv6_method = CONNMAN_IPCONFIG_METHOD_OFF;

		connman_ipaddress_free(context->ipv6_address);
		context->ipv6_address = NULL;

	} else if (g_str_equal(protocol, "ipv6")) {
		if (context->ipv6_method == CONNMAN_IPCONFIG_METHOD_OFF)
			context->ipv6_method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;

		context->ipv4_method = CONNMAN_IPCONFIG_METHOD_OFF;

		connman_ipaddress_free(context->ipv4_address);
		context->ipv4_address = NULL;

	} else if (g_str_equal(protocol, "dual")) {
		if (context->ipv4_method == CONNMAN_IPCONFIG_METHOD_OFF)
			context->ipv4_method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;

		if (context->ipv6_method == CONNMAN_IPCONFIG_METHOD_OFF)
			context->ipv6_method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;
	}

	DBG("ipv4 method %d ipv6 method %d", context->ipv4_method,
		context->ipv6_method);

	return 0;
}

static int add_cm_context(struct modem_data *modem, const char *context_path,
				DBusMessageIter *dict)
{
	const char *context_type = NULL;
	struct network_context *context = NULL;
	dbus_bool_t active = FALSE;
	const char *ip_protocol = NULL;

	DBG("%s context path %s", modem->path, context_path);

	if (modem->context) {
		/*
		 * We have already assigned a context to this modem
		 * and we do only support one Internet context.
		 */
		return -EALREADY;
	}

	context = network_context_alloc(context_path);
	if (!context)
		return -ENOMEM;

	while (dbus_message_iter_get_arg_type(dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Type")) {
			dbus_message_iter_get_basic(&value, &context_type);

			DBG("%s context %s type %s", modem->path,
				context_path, context_type);
		} else if (g_str_equal(key, "Settings")) {
			DBG("%s Settings", modem->path);

			extract_ipv4_settings(&value, context);
		} else if (g_str_equal(key, "IPv6.Settings")) {
			DBG("%s IPv6.Settings", modem->path);

			extract_ipv6_settings(&value, context);
		} else if (g_str_equal(key, "Active")) {
			dbus_message_iter_get_basic(&value, &active);

			DBG("%s Active %d", modem->path, active);
		} else if (g_str_equal(key, "AccessPointName")) {
			const char *apn;

			dbus_message_iter_get_basic(&value, &apn);
			if (apn && strlen(apn) > 0)
				modem->valid_apn = true;
			else
				modem->valid_apn = false;

			DBG("%s AccessPointName '%s'", modem->path, apn);
		}  else if (g_str_equal(key, "Protocol") &&
			dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING ) {

			dbus_message_iter_get_basic(&value, &ip_protocol);

			DBG("%s Protocol %s", modem->path, ip_protocol);
		}

		dbus_message_iter_next(dict);
	}

	if (g_strcmp0(context_type, "internet") != 0) {
		network_context_free(context);
		return -EINVAL;
	}

	if (ip_protocol)
		set_context_ipconfig(context, ip_protocol);

	modem->context = context;
	modem->active = active;

	g_hash_table_replace(context_hash, g_strdup(context_path), modem);

	if (modem->valid_apn && modem->attached &&
			has_interface(modem->interfaces,
				OFONO_API_NETREG)) {
		add_network(modem);
	}

	return 0;
}

static void remove_cm_context(struct modem_data *modem,
				const char *context_path)
{
	if (!modem->context)
		return;

	if (modem->network)
		remove_network(modem);

	g_hash_table_remove(context_hash, context_path);

	network_context_free(modem->context);
	modem->context = NULL;

	modem->valid_apn = false;

	if (modem->network)
		remove_network(modem);
}

static gboolean context_changed(DBusConnection *conn,
				DBusMessage *message,
				void *user_data)
{
	const char *context_path = dbus_message_get_path(message);
	struct modem_data *modem = NULL;
	DBusMessageIter iter, value;
	const char *key;

	DBG("context_path %s", context_path);

	modem = g_hash_table_lookup(context_hash, context_path);
	if (!modem)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	/*
	 * oFono guarantees the ordering of Settings and
	 * Active. Settings will always be send before Active = True.
	 * That means we don't have to order here.
	 */
	if (g_str_equal(key, "Settings")) {
		DBG("%s Settings", modem->path);

		extract_ipv4_settings(&value, modem->context);
	} else if (g_str_equal(key, "IPv6.Settings")) {
		DBG("%s IPv6.Settings", modem->path);

		extract_ipv6_settings(&value, modem->context);
	} else if (g_str_equal(key, "Active")) {
		dbus_bool_t active;

		dbus_message_iter_get_basic(&value, &active);
		modem->active = active;

		DBG("%s Active %d", modem->path, modem->active);

		if (modem->active)
			set_connected(modem);
		else
			set_disconnected(modem);
	} else if (g_str_equal(key, "AccessPointName")) {
		const char *apn;

		dbus_message_iter_get_basic(&value, &apn);

		DBG("%s AccessPointName %s", modem->path, apn);

		if (apn && strlen(apn) > 0) {
			modem->valid_apn = true;

			if (modem->network)
				return TRUE;

			if (!modem->attached)
				return TRUE;

			if (!has_interface(modem->interfaces,
						OFONO_API_NETREG))
				return TRUE;

			add_network(modem);

			if (modem->active)
				set_connected(modem);
		} else {
			modem->valid_apn = false;

			if (!modem->network)
				return TRUE;

			remove_network(modem);
		}

	} else if (g_str_equal(key, "Protocol") &&
		dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING ) {
		const char *ip_protocol;

		dbus_message_iter_get_basic(&value, &ip_protocol);

		set_context_ipconfig(modem->context, ip_protocol);
	}

	return TRUE;
}

static void cm_get_contexts_reply(DBusPendingCall *call, void *user_data)
{
	struct modem_data *modem = user_data;
	DBusMessageIter array, dict, entry, value;
	DBusMessage *reply;
	DBusError error;

	DBG("%s", modem->path);

	modem->call_get_contexts = NULL;

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("%s", error.message);
		dbus_error_free(&error);
		goto done;
	}

	if (!dbus_message_iter_init(reply, &array))
		goto done;

	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_ARRAY)
		goto done;

	dbus_message_iter_recurse(&array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_STRUCT) {
		const char *context_path;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &context_path);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (add_cm_context(modem, context_path, &value) == 0)
			break;

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int cm_get_contexts(struct modem_data *modem)
{
	DBusMessage *message;

	DBG("%s", modem->path);

	if (modem->call_get_contexts)
		return -EBUSY;

	message = dbus_message_new_method_call(OFONO_SERVICE, modem->path,
					OFONO_CM_INTERFACE, GET_CONTEXTS);
	if (!message)
		return -ENOMEM;

	if (!dbus_connection_send_with_reply(connection, message,
					&modem->call_get_contexts, TIMEOUT)) {
		connman_error("Failed to call GetContexts()");
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!modem->call_get_contexts) {
		connman_error("D-Bus connection not available");
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(modem->call_get_contexts,
					cm_get_contexts_reply,
					modem, NULL);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static gboolean cm_context_added(DBusConnection *conn,
					DBusMessage *message,
					void *user_data)
{
	const char *path = dbus_message_get_path(message);
	char *context_path;
	struct modem_data *modem;
	DBusMessageIter iter, properties;

	DBG("%s", path);

	modem = g_hash_table_lookup(modem_hash, path);
	if (!modem)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &context_path);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &properties);

	if (add_cm_context(modem, context_path, &properties) != 0)
		return TRUE;

	return TRUE;
}

static gboolean cm_context_removed(DBusConnection *conn,
					DBusMessage *message,
					void *user_data)
{
	const char *path = dbus_message_get_path(message);
	const char *context_path;
	struct modem_data *modem;
	DBusMessageIter iter;

	DBG("context path %s", path);

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &context_path);

	modem = g_hash_table_lookup(context_hash, context_path);
	if (!modem)
		return TRUE;

	remove_cm_context(modem, context_path);

	return TRUE;
}

static void netreg_update_name(struct modem_data *modem,
				DBusMessageIter* value)
{
	char *name;

	dbus_message_iter_get_basic(value, &name);

	DBG("%s Name %s", modem->path, name);

	g_free(modem->name);
	modem->name = g_strdup(name);

	if (!modem->network)
		return;

	connman_network_set_name(modem->network, modem->name);
	connman_network_update(modem->network);
}

static void netreg_update_strength(struct modem_data *modem,
					DBusMessageIter *value)
{
	dbus_message_iter_get_basic(value, &modem->strength);

	DBG("%s Strength %d", modem->path, modem->strength);

	if (!modem->network)
		return;

	/*
	 * GSM:
	 * We don't have 2 signal notifications we always report the strength
	 * signal. data_strength is always equal to 0.
	 *
	 * CDMA:
	 * In the case we have a data_strength signal (from 1xEVDO network)
	 * we don't need to update the value with strength signal (from 1xCDMA)
	 * because the modem is registered to 1xEVDO network for data call.
	 * In case we have no data_strength signal (not registered to 1xEVDO
	 * network), we must report the strength signal (registered to 1xCDMA
	 * network e.g slow mode).
	 */
	if (modem->data_strength != 0)
		return;

	connman_network_set_strength(modem->network, modem->strength);
	connman_network_update(modem->network);
}

/* Retrieve 1xEVDO Data Strength signal */
static void netreg_update_datastrength(struct modem_data *modem,
					DBusMessageIter *value)
{
	dbus_message_iter_get_basic(value, &modem->data_strength);

	DBG("%s Data Strength %d", modem->path, modem->data_strength);

	if (!modem->network)
		return;

	/*
	 * CDMA modem is not registered to 1xEVDO network, let
	 * update_signal_strength() reporting the value on the Strength signal
	 * notification.
	 */
	if (modem->data_strength == 0)
		return;

	connman_network_set_strength(modem->network, modem->data_strength);
	connman_network_update(modem->network);
}

static void netreg_update_status(struct modem_data *modem,
					DBusMessageIter *value)
{
	char *status;
	bool roaming;

	dbus_message_iter_get_basic(value, &status);

	roaming = g_str_equal(status, "roaming");
	modem->registered = roaming || g_str_equal(status, "registered");

	if (roaming == modem->roaming)
		return;

	modem->roaming = roaming;

	if (!modem->network)
		return;

	connman_network_set_bool(modem->network,
				"Roaming", modem->roaming);
	connman_network_update(modem->network);
}

static void netreg_update_regdom(struct modem_data *modem,
				DBusMessageIter *value)
{
	char *mobile_country_code;
	char *alpha2;
	int mcc;

	dbus_message_iter_get_basic(value, &mobile_country_code);

	DBG("%s MobileContryCode %s", modem->path, mobile_country_code);


	mcc = atoi(mobile_country_code);
	if (mcc > 799 || mcc < 200)
		return;

	alpha2 = mcc_country_codes[mcc - 200];
	if (alpha2)
		connman_technology_set_regdom(alpha2);
}

static gboolean netreg_changed(DBusConnection *conn, DBusMessage *message,
				void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct modem_data *modem;
	DBusMessageIter iter, value;
	const char *key;

	modem = g_hash_table_lookup(modem_hash, path);
	if (!modem)
		return TRUE;

	if (modem->ignore)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	if (g_str_equal(key, "Name"))
		netreg_update_name(modem, &value);
	else if (g_str_equal(key, "Strength"))
		netreg_update_strength(modem, &value);
	else if (g_str_equal(key, "Status"))
		netreg_update_status(modem, &value);
	else if (g_str_equal(key, "MobileCountryCode"))
		netreg_update_regdom(modem, &value);

	return TRUE;
}

static void netreg_properties_reply(struct modem_data *modem,
					DBusMessageIter *dict)
{
	DBG("%s", modem->path);

	while (dbus_message_iter_get_arg_type(dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Name"))
			netreg_update_name(modem, &value);
		else if (g_str_equal(key, "Strength"))
			netreg_update_strength(modem, &value);
		else if (g_str_equal(key, "Status"))
			netreg_update_status(modem, &value);
		else if (g_str_equal(key, "MobileCountryCode"))
			netreg_update_regdom(modem, &value);

		dbus_message_iter_next(dict);
	}

	if (!modem->context) {
		/*
		 * netgreg_get_properties() was issued after we got
		 * cm_get_contexts_reply() where we create the
		 * context. Though before we got the
		 * netreg_properties_reply the context was removed
		 * again. Therefore we have to skip the network
		 * creation.
		 */
		return;
	}

	if (modem->valid_apn)
		add_network(modem);

	if (modem->active)
		set_connected(modem);
}

static int netreg_get_properties(struct modem_data *modem)
{
	return get_properties(modem->path, OFONO_NETREG_INTERFACE,
			netreg_properties_reply, modem);
}

static void add_cdma_network(struct modem_data *modem)
{
	/* Be sure that device is created before adding CDMA network */
	if (!modem->device)
		return;

	/*
	 * CDMA modems don't need contexts for data call, however the current
	 * add_network() logic needs one, so we create one to proceed.
	 */
	if (!modem->context)
		modem->context = network_context_alloc(modem->path);

	if (!modem->name)
		modem->name = g_strdup("CDMA Network");

	add_network(modem);

	if (modem->cdma_cm_powered)
		set_connected(modem);
}

static gboolean cdma_netreg_changed(DBusConnection *conn,
					DBusMessage *message,
					void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct modem_data *modem;
	DBusMessageIter iter, value;
	const char *key;

	DBG("");

	modem = g_hash_table_lookup(modem_hash, path);
	if (!modem)
		return TRUE;

	if (modem->ignore)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	if (g_str_equal(key, "Name"))
		netreg_update_name(modem, &value);
	else if (g_str_equal(key, "Strength"))
		netreg_update_strength(modem, &value);
	else if (g_str_equal(key, "DataStrength"))
		netreg_update_datastrength(modem, &value);
	else if (g_str_equal(key, "Status"))
		netreg_update_status(modem, &value);

	if (modem->registered)
		add_cdma_network(modem);
	else
		remove_network(modem);

	return TRUE;
}

static void cdma_netreg_properties_reply(struct modem_data *modem,
					DBusMessageIter *dict)
{
	DBG("%s", modem->path);

	while (dbus_message_iter_get_arg_type(dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Name"))
			netreg_update_name(modem, &value);
		else if (g_str_equal(key, "Strength"))
			netreg_update_strength(modem, &value);
		else if (g_str_equal(key, "DataStrength"))
			netreg_update_datastrength(modem, &value);
		else if (g_str_equal(key, "Status"))
			netreg_update_status(modem, &value);

		dbus_message_iter_next(dict);
	}

	if (modem->registered)
		add_cdma_network(modem);
	else
		remove_network(modem);
}

static int cdma_netreg_get_properties(struct modem_data *modem)
{
	return get_properties(modem->path, OFONO_CDMA_NETREG_INTERFACE,
			cdma_netreg_properties_reply, modem);
}

static void cm_update_attached(struct modem_data *modem,
				DBusMessageIter *value)
{
	dbus_bool_t attached;

	dbus_message_iter_get_basic(value, &attached);
	modem->attached = attached;

	DBG("%s Attached %d", modem->path, modem->attached);

	if (!modem->attached) {
		remove_network(modem);
		return;
	}

	if (!has_interface(modem->interfaces, OFONO_API_NETREG))
		return;

	netreg_get_properties(modem);
}

static void cm_update_powered(struct modem_data *modem,
				DBusMessageIter *value)
{
	dbus_bool_t cm_powered;

	dbus_message_iter_get_basic(value, &cm_powered);
	modem->cm_powered = cm_powered;

	DBG("%s ConnnectionManager Powered %d", modem->path,
		modem->cm_powered);

	if (modem->cm_powered)
		return;

	cm_set_powered(modem, TRUE);
}

static gboolean cm_changed(DBusConnection *conn, DBusMessage *message,
				void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct modem_data *modem;
	DBusMessageIter iter, value;
	const char *key;

	modem = g_hash_table_lookup(modem_hash, path);
	if (!modem)
		return TRUE;

	if (modem->ignore)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	if (g_str_equal(key, "Attached"))
		cm_update_attached(modem, &value);
	else if (g_str_equal(key, "Powered"))
		cm_update_powered(modem, &value);

	return TRUE;
}

static void cdma_cm_update_powered(struct modem_data *modem,
					DBusMessageIter *value)
{
	dbus_bool_t cdma_cm_powered;

	dbus_message_iter_get_basic(value, &cdma_cm_powered);
	modem->cdma_cm_powered = cdma_cm_powered;

	DBG("%s CDMA cm Powered %d", modem->path, modem->cdma_cm_powered);

	if (!modem->network)
		return;

	if (modem->cdma_cm_powered)
		set_connected(modem);
	else
		set_disconnected(modem);
}

static void cdma_cm_update_settings(struct modem_data *modem,
					DBusMessageIter *value)
{
	DBG("%s Settings", modem->path);

	extract_ipv4_settings(value, modem->context);
}

static gboolean cdma_cm_changed(DBusConnection *conn,
				DBusMessage *message, void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct modem_data *modem;
	DBusMessageIter iter, value;
	const char *key;

	modem = g_hash_table_lookup(modem_hash, path);
	if (!modem)
		return TRUE;

	if (modem->online && !modem->network)
		cdma_netreg_get_properties(modem);

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	if (g_str_equal(key, "Powered"))
		cdma_cm_update_powered(modem, &value);
	if (g_str_equal(key, "Settings"))
		cdma_cm_update_settings(modem, &value);

	return TRUE;
}

static void cm_properties_reply(struct modem_data *modem, DBusMessageIter *dict)
{
	DBG("%s", modem->path);

	while (dbus_message_iter_get_arg_type(dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Attached"))
			cm_update_attached(modem, &value);
		else if (g_str_equal(key, "Powered"))
			cm_update_powered(modem, &value);

		dbus_message_iter_next(dict);
	}
}

static int cm_get_properties(struct modem_data *modem)
{
	return get_properties(modem->path, OFONO_CM_INTERFACE,
				cm_properties_reply, modem);
}

static void cdma_cm_properties_reply(struct modem_data *modem,
					DBusMessageIter *dict)
{
	DBG("%s", modem->path);

	if (modem->online)
		cdma_netreg_get_properties(modem);

	while (dbus_message_iter_get_arg_type(dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Powered"))
			cdma_cm_update_powered(modem, &value);
		if (g_str_equal(key, "Settings"))
			cdma_cm_update_settings(modem, &value);

		dbus_message_iter_next(dict);
	}
}

static int cdma_cm_get_properties(struct modem_data *modem)
{
	return get_properties(modem->path, OFONO_CDMA_CM_INTERFACE,
				cdma_cm_properties_reply, modem);
}

static void sim_update_imsi(struct modem_data *modem,
				DBusMessageIter *value)
{
	char *imsi;

	dbus_message_iter_get_basic(value, &imsi);

	DBG("%s imsi %s", modem->path, imsi);

	g_free(modem->imsi);
	modem->imsi = g_strdup(imsi);
}

static gboolean sim_changed(DBusConnection *conn, DBusMessage *message,
				void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct modem_data *modem;
	DBusMessageIter iter, value;
	const char *key;

	modem = g_hash_table_lookup(modem_hash, path);
	if (!modem)
		return TRUE;

	if (modem->ignore)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	if (g_str_equal(key, "SubscriberIdentity")) {
		sim_update_imsi(modem, &value);

		if (!ready_to_create_device(modem))
			return TRUE;

		/*
		 * This is a GSM modem. Create the device and
		 * register it at the core. Enabling (setting
		 * it online is done through the
		 * modem_enable() callback.
		 */
		create_device(modem);
	}

	return TRUE;
}

static void sim_properties_reply(struct modem_data *modem,
					DBusMessageIter *dict)
{
	DBG("%s", modem->path);

	while (dbus_message_iter_get_arg_type(dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "SubscriberIdentity")) {
			sim_update_imsi(modem, &value);

			if (!ready_to_create_device(modem))
				return;

			/*
			 * This is a GSM modem. Create the device and
			 * register it at the core. Enabling (setting
			 * it online is done through the
			 * modem_enable() callback.
			 */
			create_device(modem);

			if (!modem->online)
				return;

			/*
			 * The modem is already online and we have the CM interface.
			 * There will be no interface update and therefore our
			 * state machine will not go to next step. We have to
			 * trigger it from here.
			 */
			if (has_interface(modem->interfaces, OFONO_API_CM)) {
				cm_get_properties(modem);
				cm_get_contexts(modem);
			}
			return;
		}

		dbus_message_iter_next(dict);
	}
}

static int sim_get_properties(struct modem_data *modem)
{
	return get_properties(modem->path, OFONO_SIM_INTERFACE,
				sim_properties_reply, modem);
}

static bool api_added(uint8_t old_iface, uint8_t new_iface,
				enum ofono_api api)
{
	if (!has_interface(old_iface, api) &&
			has_interface(new_iface, api)) {
		DBG("%s added", api2string(api));
		return true;
	}

	return false;
}

static bool api_removed(uint8_t old_iface, uint8_t new_iface,
				enum ofono_api api)
{
	if (has_interface(old_iface, api) &&
			!has_interface(new_iface, api)) {
		DBG("%s removed", api2string(api));
		return true;
	}

	return false;
}

static void modem_update_interfaces(struct modem_data *modem,
				uint8_t old_ifaces,
				uint8_t new_ifaces)
{
	DBG("%s", modem->path);

	if (api_added(old_ifaces, new_ifaces, OFONO_API_SIM)) {
		if (!modem->imsi &&
				!modem->set_powered) {
			/*
			 * Only use do GetProperties() when
			 * device has not been powered up.
			 */
			sim_get_properties(modem);
		}
	}

	if (api_added(old_ifaces, new_ifaces, OFONO_API_CM)) {
		if (modem->device) {
			cm_get_properties(modem);
			cm_get_contexts(modem);
		}
	}

	if (api_added(old_ifaces, new_ifaces, OFONO_API_CDMA_CM)) {
		if (ready_to_create_device(modem)) {
			create_device(modem);
			if (modem->registered)
				add_cdma_network(modem);
		}

		if (modem->device)
			cdma_cm_get_properties(modem);
	}

	if (api_added(old_ifaces, new_ifaces, OFONO_API_NETREG)) {
		if (modem->attached)
			netreg_get_properties(modem);
	}

	if (api_added(old_ifaces, new_ifaces, OFONO_API_CDMA_NETREG))
		cdma_netreg_get_properties(modem);

	if (api_removed(old_ifaces, new_ifaces, OFONO_API_CM))
		remove_cm_context(modem, modem->context->path);

	if (api_removed(old_ifaces, new_ifaces, OFONO_API_CDMA_CM))
		remove_cm_context(modem, modem->context->path);

	if (api_removed(old_ifaces, new_ifaces, OFONO_API_NETREG))
		remove_network(modem);

	if (api_removed(old_ifaces, new_ifaces, OFONO_API_CDMA_NETREG))
		remove_network(modem);
}

static gboolean modem_changed(DBusConnection *conn, DBusMessage *message,
				void *user_data)
{
	const char *path = dbus_message_get_path(message);
	struct modem_data *modem;
	DBusMessageIter iter, value;
	const char *key;

	modem = g_hash_table_lookup(modem_hash, path);
	if (!modem)
		return TRUE;

	if (modem->ignore)
		return TRUE;

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &key);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &value);

	if (g_str_equal(key, "Powered")) {
		dbus_bool_t powered;

		dbus_message_iter_get_basic(&value, &powered);
		modem->powered = powered;

		DBG("%s Powered %d", modem->path, modem->powered);

		if (!modem->powered)
			modem_set_powered(modem, TRUE);
	} else if (g_str_equal(key, "Online")) {
		dbus_bool_t online;

		dbus_message_iter_get_basic(&value, &online);
		modem->online = online;

		DBG("%s Online %d", modem->path, modem->online);

		if (!modem->device)
			return TRUE;

		connman_device_set_powered(modem->device, modem->online);
	} else if (g_str_equal(key, "Interfaces")) {
		uint8_t interfaces;

		interfaces = extract_interfaces(&value);

		if (interfaces == modem->interfaces)
			return TRUE;

		DBG("%s Interfaces 0x%02x", modem->path, interfaces);

		modem_update_interfaces(modem, modem->interfaces, interfaces);

		modem->interfaces = interfaces;
	} else if (g_str_equal(key, "Serial")) {
		char *serial;

		dbus_message_iter_get_basic(&value, &serial);

		g_free(modem->serial);
		modem->serial = g_strdup(serial);

		DBG("%s Serial %s", modem->path, modem->serial);

		if (has_interface(modem->interfaces,
					 OFONO_API_CDMA_CM)) {
			if (ready_to_create_device(modem)) {
				create_device(modem);
				if (modem->registered)
					add_cdma_network(modem);
			}
		}
	}

	return TRUE;
}

static void add_modem(const char *path, DBusMessageIter *prop)
{
	struct modem_data *modem;

	DBG("%s", path);

	modem = g_hash_table_lookup(modem_hash, path);
	if (modem) {
		/*
		 * When oFono powers up we ask for the modems and oFono is
		 * reporting with modem_added signal the modems. Only
		 * handle them once.
		 */
		return;
	}

	modem = g_try_new0(struct modem_data, 1);
	if (!modem)
		return;

	modem->path = g_strdup(path);

	g_hash_table_insert(modem_hash, g_strdup(path), modem);

	while (dbus_message_iter_get_arg_type(prop) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(prop, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		if (g_str_equal(key, "Powered")) {
			dbus_bool_t powered;

			dbus_message_iter_get_basic(&value, &powered);
			modem->powered = powered;

			DBG("%s Powered %d", modem->path, modem->powered);
		} else if (g_str_equal(key, "Online")) {
			dbus_bool_t online;

			dbus_message_iter_get_basic(&value, &online);
			modem->online = online;

			DBG("%s Online %d", modem->path, modem->online);
		} else if (g_str_equal(key, "Interfaces")) {
			modem->interfaces = extract_interfaces(&value);

			DBG("%s Interfaces 0x%02x", modem->path,
				modem->interfaces);
		} else if (g_str_equal(key, "Serial")) {
			char *serial;

			dbus_message_iter_get_basic(&value, &serial);
			modem->serial = g_strdup(serial);

			DBG("%s Serial %s", modem->path, modem->serial);
		} else if (g_str_equal(key, "Type")) {
			char *type;

			dbus_message_iter_get_basic(&value, &type);

			DBG("%s Type %s", modem->path, type);
			if (g_strcmp0(type, "hardware") != 0) {
				DBG("%s Ignore this modem", modem->path);
				modem->ignore = true;
			}
		}

		dbus_message_iter_next(prop);
	}

	if (modem->ignore)
		return;

	if (!modem->powered) {
		modem_set_powered(modem, TRUE);
		return;
	}

	modem_update_interfaces(modem, 0, modem->interfaces);
}

static void modem_power_down(gpointer key, gpointer value, gpointer user_data)
{
	struct modem_data *modem = value;

	DBG("%s", modem->path);

	if (modem->ignore)
		return;

	modem_set_powered(modem, FALSE);
}

static void remove_modem(gpointer data)
{
	struct modem_data *modem = data;

	DBG("%s", modem->path);

	if (modem->call_set_property)
		dbus_pending_call_cancel(modem->call_set_property);

	if (modem->call_get_properties)
		dbus_pending_call_cancel(modem->call_get_properties);

	if (modem->call_get_contexts)
		dbus_pending_call_cancel(modem->call_get_contexts);

	if (modem->device)
		destroy_device(modem);

	if (modem->context)
		remove_cm_context(modem, modem->context->path);

	g_free(modem->serial);
	g_free(modem->name);
	g_free(modem->imsi);
	g_free(modem->path);

	g_free(modem);
}

static gboolean modem_added(DBusConnection *conn,
				DBusMessage *message, void *user_data)
{
	DBusMessageIter iter, properties;
	const char *path;

	DBG("");

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &path);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &properties);

	add_modem(path, &properties);

	return TRUE;
}

static gboolean modem_removed(DBusConnection *conn,
				DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;
	const char *path;

	DBG("");

	if (!dbus_message_iter_init(message, &iter))
		return TRUE;

	dbus_message_iter_get_basic(&iter, &path);

	g_hash_table_remove(modem_hash, path);

	return TRUE;
}

static void manager_get_modems_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;
	DBusMessageIter array, dict;

	DBG("");

	reply = dbus_pending_call_steal_reply(call);

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

		add_modem(path, &properties);

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

static int manager_get_modems(void)
{
	DBusMessage *message;
	DBusPendingCall *call;

	DBG("");

	message = dbus_message_new_method_call(OFONO_SERVICE, "/",
					OFONO_MANAGER_INTERFACE, GET_MODEMS);
	if (!message)
		return -ENOMEM;

	if (!dbus_connection_send_with_reply(connection, message,
						&call, TIMEOUT)) {
		connman_error("Failed to call GetModems()");
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (!call) {
		connman_error("D-Bus connection not available");
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(call, manager_get_modems_reply,
					NULL, NULL);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static void ofono_connect(DBusConnection *conn, void *user_data)
{
	DBG("");

	modem_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, remove_modem);
	if (!modem_hash)
		return;

	context_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, NULL);
	if (!context_hash) {
		g_hash_table_destroy(modem_hash);
		return;
	}

	manager_get_modems();
}

static void ofono_disconnect(DBusConnection *conn, void *user_data)
{
	DBG("");

	if (!modem_hash || !context_hash)
		return;

	g_hash_table_destroy(modem_hash);
	modem_hash = NULL;

	g_hash_table_destroy(context_hash);
	context_hash = NULL;
}

static int network_probe(struct connman_network *network)
{
	struct modem_data *modem = connman_network_get_data(network);

	DBG("%s network %p", modem->path, network);

	return 0;
}

static void network_remove(struct connman_network *network)
{
	struct modem_data *modem = connman_network_get_data(network);

	DBG("%s network %p", modem->path, network);
}

static int network_connect(struct connman_network *network)
{
	struct modem_data *modem = connman_network_get_data(network);

	DBG("%s network %p", modem->path, network);

	if (has_interface(modem->interfaces, OFONO_API_CM))
		return context_set_active(modem, TRUE);
	else if (has_interface(modem->interfaces, OFONO_API_CDMA_CM))
		return cdma_cm_set_powered(modem, TRUE);

	connman_error("Connection manager interface not available");

	return -ENOSYS;
}

static int network_disconnect(struct connman_network *network)
{
	struct modem_data *modem = connman_network_get_data(network);

	DBG("%s network %p", modem->path, network);

	if (has_interface(modem->interfaces, OFONO_API_CM))
		return context_set_active(modem, FALSE);
	else if (has_interface(modem->interfaces, OFONO_API_CDMA_CM))
		return cdma_cm_set_powered(modem, FALSE);

	connman_error("Connection manager interface not available");

	return -ENOSYS;
}

static struct connman_network_driver network_driver = {
	.name		= "cellular",
	.type		= CONNMAN_NETWORK_TYPE_CELLULAR,
	.probe		= network_probe,
	.remove		= network_remove,
	.connect	= network_connect,
	.disconnect	= network_disconnect,
};

static int modem_probe(struct connman_device *device)
{
	struct modem_data *modem = connman_device_get_data(device);

	DBG("%s device %p", modem->path, device);

	return 0;
}

static void modem_remove(struct connman_device *device)
{
	struct modem_data *modem = connman_device_get_data(device);

	DBG("%s device %p", modem->path, device);
}

static int modem_enable(struct connman_device *device)
{
	struct modem_data *modem = connman_device_get_data(device);

	DBG("%s device %p", modem->path, device);

	if (modem->online)
		return 0;

	return modem_set_online(modem, TRUE);
}

static int modem_disable(struct connman_device *device)
{
	struct modem_data *modem = connman_device_get_data(device);

	DBG("%s device %p", modem->path, device);

	if (!modem->online)
		return 0;

	return modem_set_online(modem, FALSE);
}

static struct connman_device_driver modem_driver = {
	.name		= "modem",
	.type		= CONNMAN_DEVICE_TYPE_CELLULAR,
	.probe		= modem_probe,
	.remove		= modem_remove,
	.enable		= modem_enable,
	.disable	= modem_disable,
};

static int tech_probe(struct connman_technology *technology)
{
	return 0;
}

static void tech_remove(struct connman_technology *technology)
{
}

static struct connman_technology_driver tech_driver = {
	.name		= "cellular",
	.type		= CONNMAN_SERVICE_TYPE_CELLULAR,
	.probe		= tech_probe,
	.remove		= tech_remove,
};

static guint watch;
static guint modem_added_watch;
static guint modem_removed_watch;
static guint modem_watch;
static guint cm_watch;
static guint sim_watch;
static guint context_added_watch;
static guint context_removed_watch;
static guint netreg_watch;
static guint context_watch;
static guint cdma_cm_watch;
static guint cdma_netreg_watch;

static int ofono_init(void)
{
	int err;

	DBG("");

	connection = connman_dbus_get_connection();
	if (!connection)
		return -EIO;

	watch = g_dbus_add_service_watch(connection,
					OFONO_SERVICE, ofono_connect,
					ofono_disconnect, NULL, NULL);

	modem_added_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE,
						NULL, OFONO_MANAGER_INTERFACE,
						MODEM_ADDED,
						modem_added,
						NULL, NULL);

	modem_removed_watch = g_dbus_add_signal_watch(connection,
						OFONO_SERVICE, NULL,
						OFONO_MANAGER_INTERFACE,
						MODEM_REMOVED,
						modem_removed,
						NULL, NULL);

	modem_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE, NULL,
						OFONO_MODEM_INTERFACE,
						PROPERTY_CHANGED,
						modem_changed,
						NULL, NULL);

	cm_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE, NULL,
						OFONO_CM_INTERFACE,
						PROPERTY_CHANGED,
						cm_changed,
						NULL, NULL);

	sim_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE, NULL,
						OFONO_SIM_INTERFACE,
						PROPERTY_CHANGED,
						sim_changed,
						NULL, NULL);

	context_added_watch = g_dbus_add_signal_watch(connection,
						OFONO_SERVICE, NULL,
						OFONO_CM_INTERFACE,
						CONTEXT_ADDED,
						cm_context_added,
						NULL, NULL);

	context_removed_watch = g_dbus_add_signal_watch(connection,
						OFONO_SERVICE, NULL,
						OFONO_CM_INTERFACE,
						CONTEXT_REMOVED,
						cm_context_removed,
						NULL, NULL);

	context_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE,
						NULL, OFONO_CONTEXT_INTERFACE,
						PROPERTY_CHANGED,
						context_changed,
						NULL, NULL);

	netreg_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE, NULL,
						OFONO_NETREG_INTERFACE,
						PROPERTY_CHANGED,
						netreg_changed,
						NULL, NULL);

	cdma_cm_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE,
						NULL, OFONO_CDMA_CM_INTERFACE,
						PROPERTY_CHANGED,
						cdma_cm_changed,
						NULL, NULL);

	cdma_netreg_watch = g_dbus_add_signal_watch(connection, OFONO_SERVICE,
						NULL, OFONO_CDMA_NETREG_INTERFACE,
						PROPERTY_CHANGED,
						cdma_netreg_changed,
						NULL, NULL);


	if (watch == 0 || modem_added_watch == 0 || modem_removed_watch == 0 ||
			modem_watch == 0 || cm_watch == 0 || sim_watch == 0 ||
			context_added_watch == 0 ||
			context_removed_watch == 0 ||
			context_watch == 0 || netreg_watch == 0 ||
			cdma_cm_watch == 0 || cdma_netreg_watch == 0) {
		err = -EIO;
		goto remove;
	}

	err = connman_network_driver_register(&network_driver);
	if (err < 0)
		goto remove;

	err = connman_device_driver_register(&modem_driver);
	if (err < 0) {
		connman_network_driver_unregister(&network_driver);
		goto remove;
	}

	err = connman_technology_driver_register(&tech_driver);
	if (err < 0) {
		connman_device_driver_unregister(&modem_driver);
		connman_network_driver_unregister(&network_driver);
		goto remove;
	}

	return 0;

remove:
	g_dbus_remove_watch(connection, cdma_netreg_watch);
	g_dbus_remove_watch(connection, cdma_cm_watch);
	g_dbus_remove_watch(connection, netreg_watch);
	g_dbus_remove_watch(connection, context_watch);
	g_dbus_remove_watch(connection, context_removed_watch);
	g_dbus_remove_watch(connection, context_added_watch);
	g_dbus_remove_watch(connection, sim_watch);
	g_dbus_remove_watch(connection, cm_watch);
	g_dbus_remove_watch(connection, modem_watch);
	g_dbus_remove_watch(connection, modem_removed_watch);
	g_dbus_remove_watch(connection, modem_added_watch);
	g_dbus_remove_watch(connection, watch);
	dbus_connection_unref(connection);

	return err;
}

static void ofono_exit(void)
{
	DBG("");

	if (modem_hash) {
		/*
		 * We should propably wait for the SetProperty() reply
		 * message, because ...
		 */
		g_hash_table_foreach(modem_hash, modem_power_down, NULL);

		/*
		 * ... here we will cancel the call.
		 */
		g_hash_table_destroy(modem_hash);
		modem_hash = NULL;
	}

	if (context_hash) {
		g_hash_table_destroy(context_hash);
		context_hash = NULL;
	}

	connman_technology_driver_unregister(&tech_driver);
	connman_device_driver_unregister(&modem_driver);
	connman_network_driver_unregister(&network_driver);

	g_dbus_remove_watch(connection, cdma_netreg_watch);
	g_dbus_remove_watch(connection, cdma_cm_watch);
	g_dbus_remove_watch(connection, netreg_watch);
	g_dbus_remove_watch(connection, context_watch);
	g_dbus_remove_watch(connection, context_removed_watch);
	g_dbus_remove_watch(connection, context_added_watch);
	g_dbus_remove_watch(connection, sim_watch);
	g_dbus_remove_watch(connection, cm_watch);
	g_dbus_remove_watch(connection, modem_watch);
	g_dbus_remove_watch(connection, modem_added_watch);
	g_dbus_remove_watch(connection, modem_removed_watch);
	g_dbus_remove_watch(connection, watch);

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(ofono, "oFono telephony plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, ofono_init, ofono_exit)
