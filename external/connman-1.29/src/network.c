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

#include "connman.h"

/*
 * How many times to send RS with the purpose of
 * refreshing RDNSS entries before they actually expire.
 * With a value of 1, one RS will be sent, with no retries.
 */
#define RS_REFRESH_COUNT	1

/*
 * Value in seconds to wait for RA after RS was sent.
 * After this time elapsed, we can send another RS.
 */
#define RS_REFRESH_TIMEOUT	3

static GSList *network_list = NULL;
static GSList *driver_list = NULL;

struct connman_network {
	int refcount;
	enum connman_network_type type;
	bool available;
	bool connected;
	bool roaming;
	uint8_t strength;
	uint16_t frequency;
	char *identifier;
	char *name;
	char *node;
	char *group;
	char *path;
	int index;
	int router_solicit_count;
	int router_solicit_refresh_count;

	struct connman_network_driver *driver;
	void *driver_data;

	bool connecting;
	bool associating;

	struct connman_device *device;

	struct {
		void *ssid;
		int ssid_len;
		char *mode;
		unsigned short channel;
		char *security;
		char *passphrase;
		char *eap;
		char *identity;
		char *agent_identity;
		char *ca_cert_path;
		char *client_cert_path;
		char *private_key_path;
		char *private_key_passphrase;
		char *phase2_auth;
		bool wps;
		bool use_wps;
		char *pin_wps;
	} wifi;

};

static const char *type2string(enum connman_network_type type)
{
	switch (type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
		break;
	case CONNMAN_NETWORK_TYPE_ETHERNET:
		return "ethernet";
	case CONNMAN_NETWORK_TYPE_GADGET:
		return "gadget";
	case CONNMAN_NETWORK_TYPE_WIFI:
		return "wifi";
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
		return "bluetooth";
	case CONNMAN_NETWORK_TYPE_CELLULAR:
		return "cellular";
	}

	return NULL;
}

static bool match_driver(struct connman_network *network,
					struct connman_network_driver *driver)
{
	if (network->type == driver->type ||
			driver->type == CONNMAN_NETWORK_TYPE_UNKNOWN)
		return true;

	return false;
}

static void set_configuration(struct connman_network *network,
			enum connman_ipconfig_type type)
{
	struct connman_service *service;

	DBG("network %p", network);

	if (!network->device)
		return;

	__connman_device_set_network(network->device, network);

	connman_device_set_disconnected(network->device, false);

	service = connman_service_lookup_from_network(network);
	__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_CONFIGURATION,
					type);
}

static void dhcp_success(struct connman_network *network)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig_ipv4;
	int err;

	service = connman_service_lookup_from_network(network);
	if (!service)
		goto err;

	ipconfig_ipv4 = __connman_service_get_ip4config(service);

	DBG("lease acquired for ipconfig %p", ipconfig_ipv4);

	if (!ipconfig_ipv4)
		return;

	err = __connman_ipconfig_address_add(ipconfig_ipv4);
	if (err < 0)
		goto err;

	err = __connman_ipconfig_gateway_add(ipconfig_ipv4);
	if (err < 0)
		goto err;

	return;

err:
	connman_network_set_error(network,
				CONNMAN_NETWORK_ERROR_CONFIGURE_FAIL);
}

static void dhcp_failure(struct connman_network *network)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig_ipv4;

	service = connman_service_lookup_from_network(network);
	if (!service)
		return;

	ipconfig_ipv4 = __connman_service_get_ip4config(service);

	DBG("lease lost for ipconfig %p", ipconfig_ipv4);

	if (!ipconfig_ipv4)
		return;

	__connman_ipconfig_address_remove(ipconfig_ipv4);
	__connman_ipconfig_gateway_remove(ipconfig_ipv4);
}

static void dhcp_callback(struct connman_ipconfig *ipconfig,
			struct connman_network *network,
			bool success, gpointer data)
{
	network->connecting = false;

	if (success)
		dhcp_success(network);
	else
		dhcp_failure(network);
}

static int set_connected_manual(struct connman_network *network)
{
	int err = 0;
	struct connman_service *service;
	struct connman_ipconfig *ipconfig;

	DBG("network %p", network);

	network->connecting = false;

	service = connman_service_lookup_from_network(network);

	ipconfig = __connman_service_get_ip4config(service);

	if (!__connman_ipconfig_get_local(ipconfig))
		__connman_service_read_ip4config(service);

	err = __connman_ipconfig_address_add(ipconfig);
	if (err < 0)
		goto err;

	err = __connman_ipconfig_gateway_add(ipconfig);
	if (err < 0)
		goto err;

err:
	return err;
}

static int set_connected_dhcp(struct connman_network *network)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig_ipv4;
	int err;

	DBG("network %p", network);

	service = connman_service_lookup_from_network(network);
	ipconfig_ipv4 = __connman_service_get_ip4config(service);

	err = __connman_dhcp_start(ipconfig_ipv4, network,
							dhcp_callback, NULL);
	if (err < 0) {
		connman_error("Can not request DHCP lease");
		return err;
	}

	return 0;
}

static int manual_ipv6_set(struct connman_network *network,
				struct connman_ipconfig *ipconfig_ipv6)
{
	struct connman_service *service;
	int err;

	DBG("network %p ipv6 %p", network, ipconfig_ipv6);

	service = connman_service_lookup_from_network(network);
	if (!service)
		return -EINVAL;

	if (!__connman_ipconfig_get_local(ipconfig_ipv6))
		__connman_service_read_ip6config(service);

	__connman_ipconfig_enable_ipv6(ipconfig_ipv6);

	err = __connman_ipconfig_address_add(ipconfig_ipv6);
	if (err < 0) {
		connman_network_set_error(network,
			CONNMAN_NETWORK_ERROR_CONFIGURE_FAIL);
		return err;
	}

	err = __connman_ipconfig_gateway_add(ipconfig_ipv6);
	if (err < 0)
		return err;

	__connman_connection_gateway_activate(service,
						CONNMAN_IPCONFIG_TYPE_IPV6);

	__connman_device_set_network(network->device, network);

	connman_device_set_disconnected(network->device, false);

	connman_network_set_associating(network, false);

	network->connecting = false;

	return 0;
}

static void stop_dhcpv6(struct connman_network *network)
{
	network->connecting = false;

	__connman_dhcpv6_stop(network);
}

static void dhcpv6_release_callback(struct connman_network *network,
				enum __connman_dhcpv6_status status,
				gpointer data)
{
	DBG("status %d", status);

	stop_dhcpv6(network);
}

static void release_dhcpv6(struct connman_network *network)
{
	__connman_dhcpv6_start_release(network, dhcpv6_release_callback);
	stop_dhcpv6(network);
}

static void dhcpv6_info_callback(struct connman_network *network,
				enum __connman_dhcpv6_status status,
				gpointer data)
{
	DBG("status %d", status);

	stop_dhcpv6(network);
}

static int dhcpv6_set_addresses(struct connman_network *network)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig_ipv6;
	int err = -EINVAL;

	service = connman_service_lookup_from_network(network);
	if (!service)
		goto err;

	network->connecting = false;

	ipconfig_ipv6 = __connman_service_get_ip6config(service);
	err = __connman_ipconfig_address_add(ipconfig_ipv6);
	if (err < 0)
		goto err;

	return 0;

err:
	connman_network_set_error(network,
				CONNMAN_NETWORK_ERROR_CONFIGURE_FAIL);
	return err;
}

static void autoconf_ipv6_set(struct connman_network *network);
static void dhcpv6_callback(struct connman_network *network,
			enum __connman_dhcpv6_status status, gpointer data);

/*
 * Have a separate callback for renew so that we do not do autoconf
 * in wrong phase as the dhcpv6_callback() is also called when doing
 * DHCPv6 solicitation.
 */
static void dhcpv6_renew_callback(struct connman_network *network,
				enum __connman_dhcpv6_status status,
				gpointer data)
{
	switch (status) {
	case CONNMAN_DHCPV6_STATUS_SUCCEED:
		dhcpv6_callback(network, status, data);
		break;
	case CONNMAN_DHCPV6_STATUS_FAIL:
	case CONNMAN_DHCPV6_STATUS_RESTART:
		stop_dhcpv6(network);

		/* restart and do solicit again. */
		autoconf_ipv6_set(network);
		break;
	}
}

static void dhcpv6_callback(struct connman_network *network,
			enum __connman_dhcpv6_status status, gpointer data)
{
	DBG("status %d", status);

	/* Start the renew process if necessary */
	if (status == CONNMAN_DHCPV6_STATUS_SUCCEED) {

		if (dhcpv6_set_addresses(network) < 0) {
			stop_dhcpv6(network);
			return;
		}

		if (__connman_dhcpv6_start_renew(network,
					dhcpv6_renew_callback) == -ETIMEDOUT)
			dhcpv6_renew_callback(network,
						CONNMAN_DHCPV6_STATUS_FAIL,
						data);

	} else if (status == CONNMAN_DHCPV6_STATUS_RESTART) {
		stop_dhcpv6(network);
		autoconf_ipv6_set(network);
	} else
		stop_dhcpv6(network);
}

static void check_dhcpv6(struct nd_router_advert *reply,
			unsigned int length, void *user_data)
{
	struct connman_network *network = user_data;
	struct connman_service *service;
	GSList *prefixes;

	DBG("reply %p", reply);

	if (!reply) {
		/*
		 * Router solicitation message seem to get lost easily so
		 * try to send it again.
		 */
		if (network->router_solicit_count > 0) {
			DBG("re-send router solicitation %d",
						network->router_solicit_count);
			network->router_solicit_count--;
			__connman_inet_ipv6_send_rs(network->index, 1,
						check_dhcpv6, network);
			return;
		}
		connman_network_unref(network);
		return;
	}

	network->router_solicit_count = 0;

	/*
	 * If we were disconnected while waiting router advertisement,
	 * we just quit and do not start DHCPv6
	 */
	if (!network->connected) {
		connman_network_unref(network);
		return;
	}

	prefixes = __connman_inet_ipv6_get_prefixes(reply, length);

	/*
	 * If IPv6 config is missing from service, then create it.
	 * The ipconfig might be missing if we got a rtnl message
	 * that disabled IPv6 config and thus removed it. This
	 * can happen if we are switching from one service to
	 * another in the same interface. The only way to get IPv6
	 * config back is to re-create it here.
	 */
	service = connman_service_lookup_from_network(network);
	if (service) {
		connman_service_create_ip6config(service, network->index);

		connman_network_set_associating(network, false);

		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_CONFIGURATION,
					CONNMAN_IPCONFIG_TYPE_IPV6);
	}

	/*
	 * We do stateful/stateless DHCPv6 if router advertisement says so.
	 */
	if (reply->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) {
		__connman_dhcpv6_start(network, prefixes, dhcpv6_callback);
	} else {
		if (reply->nd_ra_flags_reserved & ND_RA_FLAG_OTHER)
			__connman_dhcpv6_start_info(network,
							dhcpv6_info_callback);

		g_slist_free_full(prefixes, g_free);
		network->connecting = false;
	}

	connman_network_unref(network);
}

static void receive_refresh_rs_reply(struct nd_router_advert *reply,
		unsigned int length, void *user_data)
{
	struct connman_network *network = user_data;

	DBG("reply %p", reply);

	if (!reply) {
		/*
		 * Router solicitation message seem to get lost easily so
		 * try to send it again.
		 */
		if (network->router_solicit_refresh_count > 1) {
			network->router_solicit_refresh_count--;
			DBG("re-send router solicitation %d",
					network->router_solicit_refresh_count);
			__connman_inet_ipv6_send_rs(network->index,
					RS_REFRESH_TIMEOUT,
					receive_refresh_rs_reply,
					network);
			return;
		}
	}

	/* RS refresh not in progress anymore */
	network->router_solicit_refresh_count = 0;

	connman_network_unref(network);
	return;
}

int __connman_network_refresh_rs_ipv6(struct connman_network *network,
					int index)
{
	int ret = 0;

	DBG("network %p index %d", network, index);

	/* Send only one RS for all RDNSS entries which are about to expire */
	if (network->router_solicit_refresh_count > 0) {
		DBG("RS refresh already started");
		return 0;
	}

	network->router_solicit_refresh_count = RS_REFRESH_COUNT;

	connman_network_ref(network);

	ret = __connman_inet_ipv6_send_rs(index, RS_REFRESH_TIMEOUT,
			receive_refresh_rs_reply, network);
	return ret;
}

static void autoconf_ipv6_set(struct connman_network *network)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig;
	int index;

	DBG("network %p", network);

	if (network->router_solicit_count > 0) {
		/*
		 * The autoconfiguration is already pending and we have sent
		 * router solicitation messages and are now waiting answers.
		 * There is no need to continue any further.
		 */
		DBG("autoconfiguration already started");
		return;
	}

	__connman_device_set_network(network->device, network);

	connman_device_set_disconnected(network->device, false);

	service = connman_service_lookup_from_network(network);
	if (!service)
		return;

	ipconfig = __connman_service_get_ip6config(service);
	if (!ipconfig)
		return;

	__connman_ipconfig_enable_ipv6(ipconfig);

	__connman_ipconfig_address_remove(ipconfig);

	index = __connman_ipconfig_get_index(ipconfig);

	connman_network_ref(network);

	/* Try to get stateless DHCPv6 information, RFC 3736 */
	network->router_solicit_count = 3;
	__connman_inet_ipv6_send_rs(index, 1, check_dhcpv6, network);
}

static void set_connected(struct connman_network *network)
{
	struct connman_ipconfig *ipconfig_ipv4, *ipconfig_ipv6;
	struct connman_service *service;

	if (network->connected)
		return;

	connman_network_set_associating(network, false);

	network->connected = true;

	service = connman_service_lookup_from_network(network);

	ipconfig_ipv4 = __connman_service_get_ip4config(service);
	ipconfig_ipv6 = __connman_service_get_ip6config(service);

	DBG("service %p ipv4 %p ipv6 %p", service, ipconfig_ipv4,
		ipconfig_ipv6);

	__connman_network_enable_ipconfig(network, ipconfig_ipv4);
	__connman_network_enable_ipconfig(network, ipconfig_ipv6);
}

static void set_disconnected(struct connman_network *network)
{
	struct connman_ipconfig *ipconfig_ipv4, *ipconfig_ipv6;
	enum connman_ipconfig_method ipv4_method, ipv6_method;
	enum connman_service_state state;
	struct connman_service *service;

	service = connman_service_lookup_from_network(network);

	ipconfig_ipv4 = __connman_service_get_ip4config(service);
	ipconfig_ipv6 = __connman_service_get_ip6config(service);

	DBG("service %p ipv4 %p ipv6 %p", service, ipconfig_ipv4,
		ipconfig_ipv6);

	ipv4_method = __connman_ipconfig_get_method(ipconfig_ipv4);
	ipv6_method = __connman_ipconfig_get_method(ipconfig_ipv6);

	DBG("method ipv4 %d ipv6 %d", ipv4_method, ipv6_method);

	/*
	 * Resetting solicit count here will prevent the RS resend loop
	 * from sending packets in check_dhcpv6()
	 */
	network->router_solicit_count = 0;

	__connman_device_set_network(network->device, NULL);

	if (network->connected) {
		switch (ipv6_method) {
		case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
		case CONNMAN_IPCONFIG_METHOD_OFF:
		case CONNMAN_IPCONFIG_METHOD_FIXED:
		case CONNMAN_IPCONFIG_METHOD_MANUAL:
			break;
		case CONNMAN_IPCONFIG_METHOD_DHCP:
		case CONNMAN_IPCONFIG_METHOD_AUTO:
			release_dhcpv6(network);
			break;
		}

		switch (ipv4_method) {
		case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
		case CONNMAN_IPCONFIG_METHOD_OFF:
		case CONNMAN_IPCONFIG_METHOD_AUTO:
		case CONNMAN_IPCONFIG_METHOD_FIXED:
		case CONNMAN_IPCONFIG_METHOD_MANUAL:
			break;
		case CONNMAN_IPCONFIG_METHOD_DHCP:
			__connman_dhcp_stop(ipconfig_ipv4);
			break;
		}
	}

	/*
	 * We only set the disconnect state if we were not in idle
	 * or in failure. It does not make sense to go to disconnect
	 * state if we were not connected.
	 */
	state = __connman_service_ipconfig_get_state(service,
						CONNMAN_IPCONFIG_TYPE_IPV4);
	if (state != CONNMAN_SERVICE_STATE_IDLE &&
			state != CONNMAN_SERVICE_STATE_FAILURE)
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_DISCONNECT,
					CONNMAN_IPCONFIG_TYPE_IPV4);

	state = __connman_service_ipconfig_get_state(service,
						CONNMAN_IPCONFIG_TYPE_IPV6);
	if (state != CONNMAN_SERVICE_STATE_IDLE &&
				state != CONNMAN_SERVICE_STATE_FAILURE)
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_DISCONNECT,
					CONNMAN_IPCONFIG_TYPE_IPV6);

	if (network->connected) {
		__connman_connection_gateway_remove(service,
						CONNMAN_IPCONFIG_TYPE_ALL);

		__connman_ipconfig_address_unset(ipconfig_ipv4);
		__connman_ipconfig_address_unset(ipconfig_ipv6);

		/*
		 * Special handling for IPv6 autoconfigured address.
		 * The simplest way to remove autoconfigured routes is to
		 * disable IPv6 temporarily so that kernel will do the cleanup
		 * automagically.
		 */
		if (ipv6_method == CONNMAN_IPCONFIG_METHOD_AUTO) {
			__connman_ipconfig_disable_ipv6(ipconfig_ipv6);
			__connman_ipconfig_enable_ipv6(ipconfig_ipv6);
		}
	}

	__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_IDLE,
						CONNMAN_IPCONFIG_TYPE_IPV4);

	__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_IDLE,
						CONNMAN_IPCONFIG_TYPE_IPV6);

	network->connecting = false;
	network->connected = false;

	connman_network_set_associating(network, false);
}



static int network_probe(struct connman_network *network)
{
	GSList *list;
	struct connman_network_driver *driver = NULL;

	DBG("network %p name %s", network, network->name);

	if (network->driver)
		return -EALREADY;

	for (list = driver_list; list; list = list->next) {
		driver = list->data;

		if (!match_driver(network, driver)) {
			driver = NULL;
			continue;
		}

		DBG("driver %p name %s", driver, driver->name);

		if (driver->probe(network) == 0)
			break;

		driver = NULL;
	}

	if (!driver)
		return -ENODEV;

	if (!network->group)
		return -EINVAL;

	switch (network->type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
		return 0;
	case CONNMAN_NETWORK_TYPE_ETHERNET:
	case CONNMAN_NETWORK_TYPE_GADGET:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
	case CONNMAN_NETWORK_TYPE_CELLULAR:
	case CONNMAN_NETWORK_TYPE_WIFI:
		network->driver = driver;
		if (!__connman_service_create_from_network(network)) {
			network->driver = NULL;
			return -EINVAL;
		}
	}

	return 0;
}

static void network_remove(struct connman_network *network)
{
	DBG("network %p name %s", network, network->name);

	if (!network->driver)
		return;

	if (network->connected)
		set_disconnected(network);

	switch (network->type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
		break;
	case CONNMAN_NETWORK_TYPE_ETHERNET:
	case CONNMAN_NETWORK_TYPE_GADGET:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
	case CONNMAN_NETWORK_TYPE_CELLULAR:
	case CONNMAN_NETWORK_TYPE_WIFI:
		if (network->group) {
			__connman_service_remove_from_network(network);

			g_free(network->group);
			network->group = NULL;
		}
		break;
	}

	if (network->driver->remove)
		network->driver->remove(network);

	network->driver = NULL;
}

static void network_change(struct connman_network *network)
{
	DBG("network %p name %s", network, network->name);

	if (!network->connected)
		return;

	connman_device_set_disconnected(network->device, true);

	if (network->driver && network->driver->disconnect) {
		network->driver->disconnect(network);
		return;
	}

	network->connected = false;
}

static void probe_driver(struct connman_network_driver *driver)
{
	GSList *list;

	DBG("driver %p name %s", driver, driver->name);

	for (list = network_list; list; list = list->next) {
		struct connman_network *network = list->data;

		if (network->driver)
			continue;

		if (driver->type != network->type)
			continue;

		if (driver->probe(network) < 0)
			continue;

		network->driver = driver;
	}
}

static void remove_driver(struct connman_network_driver *driver)
{
	GSList *list;

	DBG("driver %p name %s", driver, driver->name);

	for (list = network_list; list; list = list->next) {
		struct connman_network *network = list->data;

		if (network->driver == driver)
			network_remove(network);
	}
}

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_network_driver *driver1 = a;
	const struct connman_network_driver *driver2 = b;

	return driver2->priority - driver1->priority;
}

/**
 * connman_network_driver_register:
 * @driver: network driver definition
 *
 * Register a new network driver
 *
 * Returns: %0 on success
 */
int connman_network_driver_register(struct connman_network_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_insert_sorted(driver_list, driver,
							compare_priority);

	probe_driver(driver);

	return 0;
}

/**
 * connman_network_driver_unregister:
 * @driver: network driver definition
 *
 * Remove a previously registered network driver
 */
void connman_network_driver_unregister(struct connman_network_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_remove(driver_list, driver);

	remove_driver(driver);
}

static void network_destruct(struct connman_network *network)
{
	DBG("network %p name %s", network, network->name);

	g_free(network->wifi.ssid);
	g_free(network->wifi.mode);
	g_free(network->wifi.security);
	g_free(network->wifi.passphrase);
	g_free(network->wifi.eap);
	g_free(network->wifi.identity);
	g_free(network->wifi.agent_identity);
	g_free(network->wifi.ca_cert_path);
	g_free(network->wifi.client_cert_path);
	g_free(network->wifi.private_key_path);
	g_free(network->wifi.private_key_passphrase);
	g_free(network->wifi.phase2_auth);
	g_free(network->wifi.pin_wps);

	g_free(network->path);
	g_free(network->group);
	g_free(network->node);
	g_free(network->name);
	g_free(network->identifier);

	network->device = NULL;

	g_free(network);
}

/**
 * connman_network_create:
 * @identifier: network identifier (for example an unqiue name)
 *
 * Allocate a new network and assign the #identifier to it.
 *
 * Returns: a newly-allocated #connman_network structure
 */
struct connman_network *connman_network_create(const char *identifier,
						enum connman_network_type type)
{
	struct connman_network *network;
	char *ident;

	DBG("identifier %s type %d", identifier, type);

	network = g_try_new0(struct connman_network, 1);
	if (!network)
		return NULL;

	DBG("network %p", network);

	network->refcount = 1;

	ident = g_strdup(identifier);

	if (!ident) {
		g_free(network);
		return NULL;
	}

	network->type       = type;
	network->identifier = ident;

	network_list = g_slist_prepend(network_list, network);

	return network;
}

/**
 * connman_network_ref:
 * @network: network structure
 *
 * Increase reference counter of  network
 */
struct connman_network *
connman_network_ref_debug(struct connman_network *network,
			const char *file, int line, const char *caller)
{
	DBG("%p name %s ref %d by %s:%d:%s()", network, network->name,
		network->refcount + 1, file, line, caller);

	__sync_fetch_and_add(&network->refcount, 1);

	return network;
}

/**
 * connman_network_unref:
 * @network: network structure
 *
 * Decrease reference counter of network
 */
void connman_network_unref_debug(struct connman_network *network,
				const char *file, int line, const char *caller)
{
	DBG("%p name %s ref %d by %s:%d:%s()", network, network->name,
		network->refcount - 1, file, line, caller);

	if (__sync_fetch_and_sub(&network->refcount, 1) != 1)
		return;

	network_list = g_slist_remove(network_list, network);

	network_destruct(network);
}

const char *__connman_network_get_type(struct connman_network *network)
{
	return type2string(network->type);
}

/**
 * connman_network_get_type:
 * @network: network structure
 *
 * Get type of network
 */
enum connman_network_type connman_network_get_type(
				struct connman_network *network)
{
	return network->type;
}

/**
 * connman_network_get_identifier:
 * @network: network structure
 *
 * Get identifier of network
 */
const char *connman_network_get_identifier(struct connman_network *network)
{
	return network->identifier;
}

/**
 * connman_network_set_index:
 * @network: network structure
 * @index: index number
 *
 * Set index number of network
 */
void connman_network_set_index(struct connman_network *network, int index)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig;

	service = connman_service_lookup_from_network(network);
	if (!service)
		goto done;

	ipconfig = __connman_service_get_ip4config(service);
	if (ipconfig) {
		__connman_ipconfig_set_index(ipconfig, index);

		DBG("index %d service %p ip4config %p", network->index,
			service, ipconfig);
	}

	ipconfig = __connman_service_get_ip6config(service);
	if (ipconfig) {
		__connman_ipconfig_set_index(ipconfig, index);

		DBG("index %d service %p ip6config %p", network->index,
			service, ipconfig);
	}

done:
	network->index = index;
}

/**
 * connman_network_get_index:
 * @network: network structure
 *
 * Get index number of network
 */
int connman_network_get_index(struct connman_network *network)
{
	return network->index;
}

/**
 * connman_network_set_group:
 * @network: network structure
 * @group: group name
 *
 * Set group name for automatic clustering
 */
void connman_network_set_group(struct connman_network *network,
							const char *group)
{
	switch (network->type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
		return;
	case CONNMAN_NETWORK_TYPE_ETHERNET:
	case CONNMAN_NETWORK_TYPE_GADGET:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
	case CONNMAN_NETWORK_TYPE_CELLULAR:
	case CONNMAN_NETWORK_TYPE_WIFI:
		break;
	}

	if (g_strcmp0(network->group, group) == 0) {
		if (group)
			__connman_service_update_from_network(network);
		return;
	}

	if (network->group) {
		__connman_service_remove_from_network(network);

		g_free(network->group);
	}

	network->group = g_strdup(group);

	if (network->group)
		network_probe(network);
}

/**
 * connman_network_get_group:
 * @network: network structure
 *
 * Get group name for automatic clustering
 */
const char *connman_network_get_group(struct connman_network *network)
{
	return network->group;
}

const char *__connman_network_get_ident(struct connman_network *network)
{
	if (!network->device)
		return NULL;

	return connman_device_get_ident(network->device);
}

bool __connman_network_get_weakness(struct connman_network *network)
{
	switch (network->type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
	case CONNMAN_NETWORK_TYPE_ETHERNET:
	case CONNMAN_NETWORK_TYPE_GADGET:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
	case CONNMAN_NETWORK_TYPE_CELLULAR:
		break;
	case CONNMAN_NETWORK_TYPE_WIFI:
		if (network->strength > 0 && network->strength < 20)
			return true;
		break;
	}

	return false;
}

bool connman_network_get_connecting(struct connman_network *network)
{
	return network->connecting;
}

/**
 * connman_network_set_available:
 * @network: network structure
 * @available: availability state
 *
 * Change availability state of network (in range)
 */
int connman_network_set_available(struct connman_network *network,
						bool available)
{
	DBG("network %p available %d", network, available);

	if (network->available == available)
		return -EALREADY;

	network->available = available;

	return 0;
}

/**
 * connman_network_get_available:
 * @network: network structure
 *
 * Get network available setting
 */
bool connman_network_get_available(struct connman_network *network)
{
	return network->available;
}

/**
 * connman_network_set_associating:
 * @network: network structure
 * @associating: associating state
 *
 * Change associating state of network
 */
int connman_network_set_associating(struct connman_network *network,
						bool associating)
{
	DBG("network %p associating %d", network, associating);

	if (network->associating == associating)
		return -EALREADY;

	network->associating = associating;

	if (associating) {
		struct connman_service *service;

		service = connman_service_lookup_from_network(network);
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_ASSOCIATION,
					CONNMAN_IPCONFIG_TYPE_IPV4);
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_ASSOCIATION,
					CONNMAN_IPCONFIG_TYPE_IPV6);
	}

	return 0;
}

static void set_associate_error(struct connman_network *network)
{
	struct connman_service *service;

	service = connman_service_lookup_from_network(network);

	__connman_service_indicate_error(service,
					CONNMAN_SERVICE_ERROR_CONNECT_FAILED);
}

static void set_configure_error(struct connman_network *network)
{
	struct connman_service *service;

	service = connman_service_lookup_from_network(network);

	__connman_service_indicate_error(service,
					CONNMAN_SERVICE_ERROR_CONNECT_FAILED);
}

static void set_invalid_key_error(struct connman_network *network)
{
	struct connman_service *service;

	service = connman_service_lookup_from_network(network);

	__connman_service_indicate_error(service,
					CONNMAN_SERVICE_ERROR_INVALID_KEY);
}

static void set_connect_error(struct connman_network *network)
{
	struct connman_service *service;

	service = connman_service_lookup_from_network(network);

	__connman_service_indicate_error(service,
					CONNMAN_SERVICE_ERROR_CONNECT_FAILED);
}

void connman_network_set_ipv4_method(struct connman_network *network,
					enum connman_ipconfig_method method)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig;

	service = connman_service_lookup_from_network(network);
	if (!service)
		return;

	ipconfig = __connman_service_get_ip4config(service);
	if (!ipconfig)
		return;

	__connman_ipconfig_set_method(ipconfig, method);
}

void connman_network_set_ipv6_method(struct connman_network *network,
					enum connman_ipconfig_method method)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig;

	service = connman_service_lookup_from_network(network);
	if (!service)
		return;

	ipconfig = __connman_service_get_ip6config(service);
	if (!ipconfig)
		return;

	__connman_ipconfig_set_method(ipconfig, method);
}

void connman_network_set_error(struct connman_network *network,
					enum connman_network_error error)
{
	DBG("network %p error %d", network, error);

	network->connecting = false;
	network->associating = false;

	switch (error) {
	case CONNMAN_NETWORK_ERROR_UNKNOWN:
		return;
	case CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL:
		set_associate_error(network);
		break;
	case CONNMAN_NETWORK_ERROR_CONFIGURE_FAIL:
		set_configure_error(network);
		break;
	case CONNMAN_NETWORK_ERROR_INVALID_KEY:
		set_invalid_key_error(network);
		break;
	case CONNMAN_NETWORK_ERROR_CONNECT_FAIL:
		set_connect_error(network);
		break;
	}

	network_change(network);
}

/**
 * connman_network_set_connected:
 * @network: network structure
 * @connected: connected state
 *
 * Change connected state of network
 */
int connman_network_set_connected(struct connman_network *network,
						bool connected)
{
	DBG("network %p connected %d/%d connecting %d associating %d",
		network, network->connected, connected, network->connecting,
		network->associating);

	if ((network->connecting || network->associating) &&
							!connected) {
		connman_network_set_error(network,
					CONNMAN_NETWORK_ERROR_CONNECT_FAIL);
		if (__connman_network_disconnect(network) == 0)
			return 0;
	}

	if (network->connected == connected)
		return -EALREADY;

	if (!connected)
		set_disconnected(network);
	else
		set_connected(network);

	return 0;
}

/**
 * connman_network_get_connected:
 * @network: network structure
 *
 * Get network connection status
 */
bool connman_network_get_connected(struct connman_network *network)
{
	return network->connected;
}

/**
 * connman_network_get_associating:
 * @network: network structure
 *
 * Get network associating status
 */
bool connman_network_get_associating(struct connman_network *network)
{
	return network->associating;
}

void connman_network_clear_hidden(void *user_data)
{
	if (!user_data)
		return;

	DBG("user_data %p", user_data);

	/*
	 * Hidden service does not have a connect timeout so
	 * we do not need to remove it. We can just return
	 * error to the caller telling that we could not find
	 * any network that we could connect to.
	 */
	connman_dbus_reply_pending(user_data, EIO, NULL);
}

int connman_network_connect_hidden(struct connman_network *network,
			char *identity, char *passphrase, void *user_data)
{
	int err = 0;
	struct connman_service *service;

	service = connman_service_lookup_from_network(network);

	DBG("network %p service %p user_data %p", network, service, user_data);

	if (!service)
		return -EINVAL;

	if (identity)
		__connman_service_set_agent_identity(service, identity);

	if (passphrase)
		err = __connman_service_set_passphrase(service, passphrase);

	if (err == -ENOKEY) {
		__connman_service_indicate_error(service,
					CONNMAN_SERVICE_ERROR_INVALID_KEY);
		goto out;
	} else {
		__connman_service_set_hidden(service);
		__connman_service_set_hidden_data(service, user_data);
		return __connman_service_connect(service,
					CONNMAN_SERVICE_CONNECT_REASON_USER);
	}

out:
	__connman_service_return_error(service, -err, user_data);
	return err;
}

/**
 * __connman_network_connect:
 * @network: network structure
 *
 * Connect network
 */
int __connman_network_connect(struct connman_network *network)
{
	int err;

	DBG("network %p", network);

	if (network->connected)
		return -EISCONN;

	if (network->connecting || network->associating)
		return -EALREADY;

	if (!network->driver)
		return -EUNATCH;

	if (!network->driver->connect)
		return -ENOSYS;

	if (!network->device)
		return -ENODEV;

	network->connecting = true;

	__connman_device_disconnect(network->device);

	err = network->driver->connect(network);
	if (err < 0) {
		if (err == -EINPROGRESS)
			connman_network_set_associating(network, true);
		else
			network->connecting = false;

		return err;
	}

	set_connected(network);

	return err;
}

/**
 * __connman_network_disconnect:
 * @network: network structure
 *
 * Disconnect network
 */
int __connman_network_disconnect(struct connman_network *network)
{
	int err = 0;

	DBG("network %p", network);

	if (!network->connected && !network->connecting &&
						!network->associating)
		return -ENOTCONN;

	if (!network->driver)
		return -EUNATCH;

	network->connecting = false;

	if (network->driver->disconnect)
		err = network->driver->disconnect(network);

	if (err != -EINPROGRESS)
		set_disconnected(network);

	return err;
}

int __connman_network_clear_ipconfig(struct connman_network *network,
					struct connman_ipconfig *ipconfig)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig_ipv4;
	enum connman_ipconfig_method method;
	enum connman_ipconfig_type type;

	service = connman_service_lookup_from_network(network);
	if (!service)
		return -EINVAL;

	ipconfig_ipv4 = __connman_service_get_ip4config(service);
	method = __connman_ipconfig_get_method(ipconfig);
	type = __connman_ipconfig_get_config_type(ipconfig);

	switch (method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
		return -EINVAL;
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		release_dhcpv6(network);
		break;
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		__connman_ipconfig_address_remove(ipconfig);
		break;
	case CONNMAN_IPCONFIG_METHOD_DHCP:
		__connman_dhcp_stop(ipconfig_ipv4);
		break;
	}

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_CONFIGURATION,
					CONNMAN_IPCONFIG_TYPE_IPV6);
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_CONFIGURATION,
					CONNMAN_IPCONFIG_TYPE_IPV4);

	return 0;
}

int __connman_network_enable_ipconfig(struct connman_network *network,
				struct connman_ipconfig *ipconfig)
{
	int r = 0;
	enum connman_ipconfig_type type;
	enum connman_ipconfig_method method;

	if (!network || !ipconfig)
		return -EINVAL;

	type = __connman_ipconfig_get_config_type(ipconfig);

	switch (type) {
	case CONNMAN_IPCONFIG_TYPE_UNKNOWN:
	case CONNMAN_IPCONFIG_TYPE_ALL:
		return -ENOSYS;

	case CONNMAN_IPCONFIG_TYPE_IPV6:
		set_configuration(network, type);

		method = __connman_ipconfig_get_method(ipconfig);

		DBG("ipv6 ipconfig method %d", method);

		switch (method) {
		case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
			break;

		case CONNMAN_IPCONFIG_METHOD_OFF:
			__connman_ipconfig_disable_ipv6(ipconfig);
			break;

		case CONNMAN_IPCONFIG_METHOD_AUTO:
			autoconf_ipv6_set(network);
			break;

		case CONNMAN_IPCONFIG_METHOD_FIXED:
		case CONNMAN_IPCONFIG_METHOD_MANUAL:
			r = manual_ipv6_set(network, ipconfig);
			break;

		case CONNMAN_IPCONFIG_METHOD_DHCP:
			r = -ENOSYS;
			break;
		}

		break;

	case CONNMAN_IPCONFIG_TYPE_IPV4:
		set_configuration(network, type);

		method = __connman_ipconfig_get_method(ipconfig);

		DBG("ipv4 ipconfig method %d", method);

		switch (method) {
		case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
		case CONNMAN_IPCONFIG_METHOD_OFF:
			break;

		case CONNMAN_IPCONFIG_METHOD_AUTO:
			r = -ENOSYS;
			break;

		case CONNMAN_IPCONFIG_METHOD_FIXED:
		case CONNMAN_IPCONFIG_METHOD_MANUAL:
			r = set_connected_manual(network);
			break;

		case CONNMAN_IPCONFIG_METHOD_DHCP:
			r = set_connected_dhcp(network);
			break;
		}

		break;
	}

	if (r < 0)
		connman_network_set_error(network,
					CONNMAN_NETWORK_ERROR_CONFIGURE_FAIL);

	return r;
}

int connman_network_set_ipaddress(struct connman_network *network,
					struct connman_ipaddress *ipaddress)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig = NULL;

	DBG("network %p", network);

	service = connman_service_lookup_from_network(network);
	if (!service)
		return -EINVAL;

	ipconfig = __connman_service_get_ipconfig(service, ipaddress->family);
	if (!ipconfig)
		return -EINVAL;

	__connman_ipconfig_set_local(ipconfig, ipaddress->local);
	__connman_ipconfig_set_peer(ipconfig, ipaddress->peer);
	__connman_ipconfig_set_broadcast(ipconfig, ipaddress->broadcast);
	__connman_ipconfig_set_prefixlen(ipconfig, ipaddress->prefixlen);
	__connman_ipconfig_set_gateway(ipconfig, ipaddress->gateway);

	return 0;
}

int connman_network_set_nameservers(struct connman_network *network,
				const char *nameservers)
{
	struct connman_service *service;
	char **nameservers_array;
	int i;

	DBG("network %p nameservers %s", network, nameservers);

	service = connman_service_lookup_from_network(network);
	if (!service)
		return -EINVAL;

	__connman_service_nameserver_clear(service);

	if (!nameservers)
		return 0;

	nameservers_array = g_strsplit(nameservers, " ", 0);

	for (i = 0; nameservers_array[i]; i++) {
		__connman_service_nameserver_append(service,
						nameservers_array[i], false);
	}

	g_strfreev(nameservers_array);

	return 0;
}

int connman_network_set_domain(struct connman_network *network,
				const char *domain)
{
	struct connman_service *service;

	DBG("network %p domain %s", network, domain);

	service = connman_service_lookup_from_network(network);
	if (!service)
		return -EINVAL;

	__connman_service_set_domainname(service, domain);

	return 0;
}

/**
 * connman_network_set_name:
 * @network: network structure
 * @name: name value
 *
 * Set display name value for network
 */
int connman_network_set_name(struct connman_network *network,
							const char *name)
{
	DBG("network %p name %s", network, name);

	g_free(network->name);
	network->name = g_strdup(name);

	return 0;
}

/**
 * connman_network_set_strength:
 * @network: network structure
 * @strength: strength value
 *
 * Set signal strength value for network
 */

int connman_network_set_strength(struct connman_network *network,
						uint8_t strength)
{
	DBG("network %p strengh %d", network, strength);

	network->strength = strength;

	return 0;
}

uint8_t connman_network_get_strength(struct connman_network *network)
{
	return network->strength;
}

int connman_network_set_frequency(struct connman_network *network,
						uint16_t frequency)
{
	DBG("network %p frequency %d", network, frequency);

	network->frequency = frequency;

	return 0;
}

uint16_t connman_network_get_frequency(struct connman_network *network)
{
	return network->frequency;
}

int connman_network_set_wifi_channel(struct connman_network *network,
						uint16_t channel)
{
	DBG("network %p wifi channel %d", network, channel);

	network->wifi.channel = channel;

	return 0;
}

uint16_t connman_network_get_wifi_channel(struct connman_network *network)
{
	return network->wifi.channel;
}

/**
 * connman_network_set_string:
 * @network: network structure
 * @key: unique identifier
 * @value: string value
 *
 * Set string value for specific key
 */
int connman_network_set_string(struct connman_network *network,
					const char *key, const char *value)
{
	DBG("network %p key %s value %s", network, key, value);

	if (g_strcmp0(key, "Name") == 0)
		return connman_network_set_name(network, value);

	if (g_str_equal(key, "Path")) {
		g_free(network->path);
		network->path = g_strdup(value);
	} else if (g_str_equal(key, "Node")) {
		g_free(network->node);
		network->node = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.Mode")) {
		g_free(network->wifi.mode);
		network->wifi.mode = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.Security")) {
		g_free(network->wifi.security);
		network->wifi.security = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.Passphrase")) {
		g_free(network->wifi.passphrase);
		network->wifi.passphrase = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.EAP")) {
		g_free(network->wifi.eap);
		network->wifi.eap = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.Identity")) {
		g_free(network->wifi.identity);
		network->wifi.identity = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.AgentIdentity")) {
		g_free(network->wifi.agent_identity);
		network->wifi.agent_identity = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.CACertFile")) {
		g_free(network->wifi.ca_cert_path);
		network->wifi.ca_cert_path = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.ClientCertFile")) {
		g_free(network->wifi.client_cert_path);
		network->wifi.client_cert_path = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.PrivateKeyFile")) {
		g_free(network->wifi.private_key_path);
		network->wifi.private_key_path = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.PrivateKeyPassphrase")) {
		g_free(network->wifi.private_key_passphrase);
		network->wifi.private_key_passphrase = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.Phase2")) {
		g_free(network->wifi.phase2_auth);
		network->wifi.phase2_auth = g_strdup(value);
	} else if (g_str_equal(key, "WiFi.PinWPS")) {
		g_free(network->wifi.pin_wps);
		network->wifi.pin_wps = g_strdup(value);
	} else {
		return -EINVAL;
	}

	return 0;
}

/**
 * connman_network_get_string:
 * @network: network structure
 * @key: unique identifier
 *
 * Get string value for specific key
 */
const char *connman_network_get_string(struct connman_network *network,
							const char *key)
{
	DBG("network %p key %s", network, key);

	if (g_str_equal(key, "Path"))
		return network->path;
	else if (g_str_equal(key, "Name"))
		return network->name;
	else if (g_str_equal(key, "Node"))
		return network->node;
	else if (g_str_equal(key, "WiFi.Mode"))
		return network->wifi.mode;
	else if (g_str_equal(key, "WiFi.Security"))
		return network->wifi.security;
	else if (g_str_equal(key, "WiFi.Passphrase"))
		return network->wifi.passphrase;
	else if (g_str_equal(key, "WiFi.EAP"))
		return network->wifi.eap;
	else if (g_str_equal(key, "WiFi.Identity"))
		return network->wifi.identity;
	else if (g_str_equal(key, "WiFi.AgentIdentity"))
		return network->wifi.agent_identity;
	else if (g_str_equal(key, "WiFi.CACertFile"))
		return network->wifi.ca_cert_path;
	else if (g_str_equal(key, "WiFi.ClientCertFile"))
		return network->wifi.client_cert_path;
	else if (g_str_equal(key, "WiFi.PrivateKeyFile"))
		return network->wifi.private_key_path;
	else if (g_str_equal(key, "WiFi.PrivateKeyPassphrase"))
		return network->wifi.private_key_passphrase;
	else if (g_str_equal(key, "WiFi.Phase2"))
		return network->wifi.phase2_auth;
	else if (g_str_equal(key, "WiFi.PinWPS"))
		return network->wifi.pin_wps;

	return NULL;
}

/**
 * connman_network_set_bool:
 * @network: network structure
 * @key: unique identifier
 * @value: boolean value
 *
 * Set boolean value for specific key
 */
int connman_network_set_bool(struct connman_network *network,
					const char *key, bool value)
{
	DBG("network %p key %s value %d", network, key, value);

	if (g_strcmp0(key, "Roaming") == 0)
		network->roaming = value;
	else if (g_strcmp0(key, "WiFi.WPS") == 0)
		network->wifi.wps = value;
	else if (g_strcmp0(key, "WiFi.UseWPS") == 0)
		network->wifi.use_wps = value;

	return -EINVAL;
}

/**
 * connman_network_get_bool:
 * @network: network structure
 * @key: unique identifier
 *
 * Get boolean value for specific key
 */
bool connman_network_get_bool(struct connman_network *network,
							const char *key)
{
	DBG("network %p key %s", network, key);

	if (g_str_equal(key, "Roaming"))
		return network->roaming;
	else if (g_str_equal(key, "WiFi.WPS"))
		return network->wifi.wps;
	else if (g_str_equal(key, "WiFi.UseWPS"))
		return network->wifi.use_wps;

	return false;
}

/**
 * connman_network_set_blob:
 * @network: network structure
 * @key: unique identifier
 * @data: blob data
 * @size: blob size
 *
 * Set binary blob value for specific key
 */
int connman_network_set_blob(struct connman_network *network,
			const char *key, const void *data, unsigned int size)
{
	DBG("network %p key %s size %d", network, key, size);

	if (g_str_equal(key, "WiFi.SSID")) {
		g_free(network->wifi.ssid);
		network->wifi.ssid = g_try_malloc(size);
		if (network->wifi.ssid) {
			memcpy(network->wifi.ssid, data, size);
			network->wifi.ssid_len = size;
		} else
			network->wifi.ssid_len = 0;
	} else {
		return -EINVAL;
	}

	return 0;
}

/**
 * connman_network_get_blob:
 * @network: network structure
 * @key: unique identifier
 * @size: pointer to blob size
 *
 * Get binary blob value for specific key
 */
const void *connman_network_get_blob(struct connman_network *network,
					const char *key, unsigned int *size)
{
	DBG("network %p key %s", network, key);

	if (g_str_equal(key, "WiFi.SSID")) {
		if (size)
			*size = network->wifi.ssid_len;
		return network->wifi.ssid;
	}

	return NULL;
}

void __connman_network_set_device(struct connman_network *network,
					struct connman_device *device)
{
	if (network->device == device)
		return;

	if (network->device)
		network_remove(network);

	network->device = device;

	if (network->device)
		network_probe(network);
}

/**
 * connman_network_get_device:
 * @network: network structure
 *
 * Get parent device of network
 */
struct connman_device *connman_network_get_device(struct connman_network *network)
{
	return network->device;
}

/**
 * connman_network_get_data:
 * @network: network structure
 *
 * Get private network data pointer
 */
void *connman_network_get_data(struct connman_network *network)
{
	return network->driver_data;
}

/**
 * connman_network_set_data:
 * @network: network structure
 * @data: data pointer
 *
 * Set private network data pointer
 */
void connman_network_set_data(struct connman_network *network, void *data)
{
	network->driver_data = data;
}

void connman_network_update(struct connman_network *network)
{
	switch (network->type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
		return;
	case CONNMAN_NETWORK_TYPE_ETHERNET:
	case CONNMAN_NETWORK_TYPE_GADGET:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
	case CONNMAN_NETWORK_TYPE_CELLULAR:
	case CONNMAN_NETWORK_TYPE_WIFI:
		break;
	}

	if (network->group)
		__connman_service_update_from_network(network);
}

int __connman_network_init(void)
{
	DBG("");

	return 0;
}

void __connman_network_cleanup(void)
{
	DBG("");
}
