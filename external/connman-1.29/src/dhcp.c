/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <connman/ipconfig.h>
#include <include/setting.h>

#include <gdhcp/gdhcp.h>

#include <glib.h>

#include "connman.h"

#define RATE_LIMIT_INTERVAL	60	/* delay between successive attempts */

struct connman_dhcp {
	struct connman_ipconfig *ipconfig;
	struct connman_network *network;
	dhcp_cb callback;
	gpointer user_data;

	char **nameservers;
	char **timeservers;
	char *pac;

	unsigned int timeout;

	GDHCPClient *ipv4ll_client;
	GDHCPClient *dhcp_client;
	char *ipv4ll_debug_prefix;
	char *dhcp_debug_prefix;
};

static GHashTable *ipconfig_table;
static bool ipv4ll_running;

static void dhcp_free(struct connman_dhcp *dhcp)
{
	g_strfreev(dhcp->nameservers);
	g_strfreev(dhcp->timeservers);
	g_free(dhcp->pac);

	dhcp->nameservers = NULL;
	dhcp->timeservers = NULL;
	dhcp->pac = NULL;

	g_free(dhcp);
}

static void ipv4ll_stop_client(struct connman_dhcp *dhcp)
{
	if (!dhcp->ipv4ll_client)
		return;

	g_dhcp_client_stop(dhcp->ipv4ll_client);
	g_dhcp_client_unref(dhcp->ipv4ll_client);
	dhcp->ipv4ll_client = NULL;
	ipv4ll_running = false;

	g_free(dhcp->ipv4ll_debug_prefix);
	dhcp->ipv4ll_debug_prefix = NULL;
}

static bool apply_dhcp_invalidate_on_network(struct connman_dhcp *dhcp)
{
	struct connman_service *service;
	int i;

	if (!dhcp->network)
		return true;

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		connman_error("Can not lookup service");
		return false;
	}

	__connman_service_set_domainname(service, NULL);
	__connman_ipconfig_set_proxy_autoconfig(dhcp->ipconfig, NULL);

	if (dhcp->timeservers) {
		for (i = 0; dhcp->timeservers[i]; i++) {
			__connman_service_timeserver_remove(service,
							dhcp->timeservers[i]);
		}
	}
	if (dhcp->nameservers) {
		for (i = 0; dhcp->nameservers[i]; i++) {
			__connman_service_nameserver_remove(service,
						dhcp->nameservers[i], false);
		}
	}

	return true;
}

/**
 * dhcp_invalidate: Invalidate an existing DHCP lease
 * @dhcp: pointer to the DHCP lease to invalidate.
 * @callback: flag indicating whether or not to invoke the client callback
 *            if present.
 *
 * Invalidates an existing DHCP lease, optionally invoking the client
 * callback. The caller may wish to avoid the client callback invocation
 * when the invocation of that callback might otherwise unnecessarily upset
 * service state due to the IP configuration change implied by this
 * invalidation.
 */
static void dhcp_invalidate(struct connman_dhcp *dhcp, bool callback)
{
	DBG("dhcp %p callback %u", dhcp, callback);

	if (!dhcp)
		return;

	__connman_6to4_remove(dhcp->ipconfig);

	if (!apply_dhcp_invalidate_on_network(dhcp))
		return;

	__connman_ipconfig_set_dhcp_address(dhcp->ipconfig,
				__connman_ipconfig_get_local(dhcp->ipconfig));
	DBG("last address %s",
			__connman_ipconfig_get_dhcp_address(dhcp->ipconfig));

	__connman_ipconfig_address_remove(dhcp->ipconfig);

	__connman_ipconfig_set_local(dhcp->ipconfig, NULL);
	__connman_ipconfig_set_broadcast(dhcp->ipconfig, NULL);
	__connman_ipconfig_set_gateway(dhcp->ipconfig, NULL);
	__connman_ipconfig_set_prefixlen(dhcp->ipconfig, 0);

	if (dhcp->callback && callback)
		dhcp->callback(dhcp->ipconfig, dhcp->network,
						false, dhcp->user_data);
}

static void dhcp_valid(struct connman_dhcp *dhcp)
{
	if (dhcp->callback)
		dhcp->callback(dhcp->ipconfig, dhcp->network,
						true, dhcp->user_data);
}

static void dhcp_debug(const char *str, void *data)
{
	connman_info("%s: %s", (const char *) data, str);
}

static void ipv4ll_lost_cb(GDHCPClient *dhcp_client, gpointer user_data);
static void ipv4ll_available_cb(GDHCPClient *ipv4ll_client, gpointer user_data);

static int ipv4ll_start_client(struct connman_dhcp *dhcp)
{
	GDHCPClient *ipv4ll_client;
	GDHCPClientError error;
	const char *hostname;
	int index;
	int err;

	if (dhcp->ipv4ll_client)
		return -EALREADY;

	index = __connman_ipconfig_get_index(dhcp->ipconfig);

	ipv4ll_client = g_dhcp_client_new(G_DHCP_IPV4LL, index, &error);
	if (error != G_DHCP_CLIENT_ERROR_NONE)
		return -EINVAL;

	if (getenv("CONNMAN_DHCP_DEBUG")) {
		dhcp->ipv4ll_debug_prefix = g_strdup_printf("IPv4LL index %d",
							index);
		g_dhcp_client_set_debug(ipv4ll_client, dhcp_debug,
					dhcp->ipv4ll_debug_prefix);
	}

	g_dhcp_client_set_id(ipv4ll_client);

	if (dhcp->network) {
		hostname = connman_utsname_get_hostname();
		if (hostname)
			g_dhcp_client_set_send(ipv4ll_client,
						G_DHCP_HOST_NAME, hostname);
	}

	g_dhcp_client_register_event(ipv4ll_client,
			G_DHCP_CLIENT_EVENT_IPV4LL_LOST, ipv4ll_lost_cb, dhcp);

	g_dhcp_client_register_event(ipv4ll_client,
			G_DHCP_CLIENT_EVENT_IPV4LL_AVAILABLE,
						ipv4ll_available_cb, dhcp);

	dhcp->ipv4ll_client = ipv4ll_client;

	err = g_dhcp_client_start(dhcp->ipv4ll_client, NULL);
	if (err < 0) {
		ipv4ll_stop_client(dhcp);
		return err;
	}

	ipv4ll_running = true;
	return 0;
}

static gboolean dhcp_retry_cb(gpointer user_data)
{
	struct connman_dhcp *dhcp = user_data;

	dhcp->timeout = 0;

	g_dhcp_client_start(dhcp->dhcp_client,
			__connman_ipconfig_get_dhcp_address(dhcp->ipconfig));

	return FALSE;
}

static void no_lease_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcp *dhcp = user_data;
	int err;

	DBG("No lease available ipv4ll %d client %p", ipv4ll_running,
		dhcp->ipv4ll_client);

	dhcp->timeout = g_timeout_add_seconds(RATE_LIMIT_INTERVAL,
						dhcp_retry_cb,
						dhcp);
	if (ipv4ll_running)
		return;

	err = ipv4ll_start_client(dhcp);
	if (err < 0)
		DBG("Cannot start ipv4ll client (%d/%s)", err, strerror(-err));

	/* Only notify upper layer if we have a problem */
	dhcp_invalidate(dhcp, !ipv4ll_running);
}

static void lease_lost_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcp *dhcp = user_data;

	DBG("Lease lost");

	/* Upper layer will decide what to do, e.g. nothing or retry. */
	dhcp_invalidate(dhcp, true);
}

static void ipv4ll_lost_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcp *dhcp = user_data;

	DBG("Lease lost");

	ipv4ll_stop_client(dhcp);

	/*
	 * Since we lost our IPv4LL configuration we might as notify
	 * the upper layers.
	 */
	dhcp_invalidate(dhcp, true);
}

static bool compare_string_arrays(char **array_a, char **array_b)
{
	int i;

	if (!array_a || !array_b)
		return false;

	if (g_strv_length(array_a) != g_strv_length(array_b))
		return false;

	for (i = 0; array_a[i] &&
			     array_b[i]; i++) {
		if (g_strcmp0(array_a[i], array_b[i]) != 0)
			return false;
	}

	return true;
}

static bool apply_lease_available_on_network(GDHCPClient *dhcp_client,
						struct connman_dhcp *dhcp)
{
	char **nameservers, **timeservers, *pac = NULL;
	struct connman_service *service;
	GList *list, *option = NULL;
	int ns_entries;
	int i;

	if (!dhcp->network)
		return true;

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		connman_error("Can not lookup service");
		return false;
	}

	option = g_dhcp_client_get_option(dhcp_client, 252);
	if (option)
		pac = g_strdup(option->data);

	option = g_dhcp_client_get_option(dhcp_client, G_DHCP_DNS_SERVER);
	ns_entries = g_list_length(option);
	nameservers = g_try_new0(char *, ns_entries + 1);
	if (nameservers) {
		for (i = 0, list = option;list; list = list->next, i++)
			nameservers[i] = g_strdup(list->data);
		nameservers[ns_entries] = NULL;
	}

	option = g_dhcp_client_get_option(dhcp_client, G_DHCP_DOMAIN_NAME);
	if (option)
		__connman_service_set_domainname(service, option->data);

	option = g_dhcp_client_get_option(dhcp_client, G_DHCP_HOST_NAME);
	if (option)
		__connman_service_set_hostname(service, option->data);

	option = g_dhcp_client_get_option(dhcp_client, G_DHCP_NTP_SERVER);
	ns_entries = g_list_length(option);
	timeservers = g_try_new0(char *, ns_entries + 1);
	if (timeservers) {
		for (i = 0, list = option; list; list = list->next, i++)
			timeservers[i] = g_strdup(list->data);
		timeservers[ns_entries] = NULL;
	}

	if (!compare_string_arrays(nameservers, dhcp->nameservers)) {
		if (dhcp->nameservers) {
			for (i = 0; dhcp->nameservers[i]; i++) {
				__connman_service_nameserver_remove(service,
						dhcp->nameservers[i], false);
			}
			g_strfreev(dhcp->nameservers);
		}

		dhcp->nameservers = nameservers;

		for (i = 0; dhcp->nameservers && dhcp->nameservers[i]; i++) {
			__connman_service_nameserver_append(service,
						dhcp->nameservers[i], false);
		}
	} else {
		g_strfreev(nameservers);
	}

	if (!compare_string_arrays(timeservers, dhcp->timeservers)) {
		if (dhcp->timeservers) {
			for (i = 0; dhcp->timeservers[i]; i++) {
				__connman_service_timeserver_remove(service,
							dhcp->timeservers[i]);
			}
			g_strfreev(dhcp->timeservers);
		}

		dhcp->timeservers = timeservers;

		for (i = 0; dhcp->timeservers && dhcp->timeservers[i]; i++) {
			__connman_service_timeserver_append(service,
							dhcp->timeservers[i]);
		}
	} else {
		g_strfreev(timeservers);
	}

	if (g_strcmp0(pac, dhcp->pac) != 0) {
		g_free(dhcp->pac);
		dhcp->pac = pac;

		__connman_ipconfig_set_proxy_autoconfig(dhcp->ipconfig,
								dhcp->pac);
	}

	__connman_6to4_probe(service);

	return true;
}

static void lease_available_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcp *dhcp = user_data;
	GList *option = NULL;
	char *address, *netmask = NULL, *gateway = NULL;
	const char *c_address, *c_gateway;
	unsigned char prefixlen, c_prefixlen;
	bool ip_change;

	DBG("Lease available");

	if (dhcp->ipv4ll_client) {
		ipv4ll_stop_client(dhcp);
		dhcp_invalidate(dhcp, false);
	}

	c_address = __connman_ipconfig_get_local(dhcp->ipconfig);
	c_gateway = __connman_ipconfig_get_gateway(dhcp->ipconfig);
	c_prefixlen = __connman_ipconfig_get_prefixlen(dhcp->ipconfig);

	address = g_dhcp_client_get_address(dhcp_client);

	__connman_ipconfig_set_dhcp_address(dhcp->ipconfig, address);
	DBG("last address %s", address);

	option = g_dhcp_client_get_option(dhcp_client, G_DHCP_SUBNET);
	if (option)
		netmask = g_strdup(option->data);

	option = g_dhcp_client_get_option(dhcp_client, G_DHCP_ROUTER);
	if (option)
		gateway = g_strdup(option->data);

	prefixlen = connman_ipaddress_calc_netmask_len(netmask);
	if (prefixlen == 255)
		connman_warn("netmask: %s is invalid", netmask);

	DBG("c_address %s", c_address);

	if (g_strcmp0(address, c_address))
		ip_change = true;
	else if (g_strcmp0(gateway, c_gateway))
		ip_change = true;
	else if (prefixlen != c_prefixlen)
		ip_change = true;
	else
		ip_change = false;

	__connman_ipconfig_set_method(dhcp->ipconfig,
						CONNMAN_IPCONFIG_METHOD_DHCP);
	if (ip_change) {
		__connman_ipconfig_set_local(dhcp->ipconfig, address);
		__connman_ipconfig_set_prefixlen(dhcp->ipconfig, prefixlen);
		__connman_ipconfig_set_gateway(dhcp->ipconfig, gateway);
	}

	if (!apply_lease_available_on_network(dhcp_client, dhcp))
		return;

	if (ip_change)
		dhcp_valid(dhcp);

	g_free(address);
	g_free(netmask);
	g_free(gateway);
}

static void ipv4ll_available_cb(GDHCPClient *ipv4ll_client, gpointer user_data)
{
	struct connman_dhcp *dhcp = user_data;
	char *address, *netmask;
	unsigned char prefixlen;

	DBG("IPV4LL available");

	address = g_dhcp_client_get_address(ipv4ll_client);
	netmask = g_dhcp_client_get_netmask(ipv4ll_client);

	prefixlen = connman_ipaddress_calc_netmask_len(netmask);

	__connman_ipconfig_set_method(dhcp->ipconfig,
						CONNMAN_IPCONFIG_METHOD_DHCP);
	__connman_ipconfig_set_local(dhcp->ipconfig, address);
	__connman_ipconfig_set_prefixlen(dhcp->ipconfig, prefixlen);
	__connman_ipconfig_set_gateway(dhcp->ipconfig, NULL);

	dhcp_valid(dhcp);

	g_free(address);
	g_free(netmask);
}

static int dhcp_initialize(struct connman_dhcp *dhcp)
{
	GDHCPClient *dhcp_client;
	GDHCPClientError error;
	int index;

	DBG("dhcp %p", dhcp);

	index = __connman_ipconfig_get_index(dhcp->ipconfig);

	dhcp_client = g_dhcp_client_new(G_DHCP_IPV4, index, &error);
	if (error != G_DHCP_CLIENT_ERROR_NONE)
		return -EINVAL;

	if (getenv("CONNMAN_DHCP_DEBUG")) {
		dhcp->dhcp_debug_prefix = g_strdup_printf("DHCP index %d",
							index);
		g_dhcp_client_set_debug(dhcp_client, dhcp_debug,
					dhcp->dhcp_debug_prefix);
	}

	g_dhcp_client_set_id(dhcp_client);

	if (dhcp->network) {
		struct connman_service *service;
		const char *hostname;

		service = connman_service_lookup_from_network(dhcp->network);

		hostname = __connman_service_get_hostname(service);
		if (!hostname)
			hostname = connman_utsname_get_hostname();

		if (hostname)
			g_dhcp_client_set_send(dhcp_client,
						G_DHCP_HOST_NAME, hostname);

		g_dhcp_client_set_request(dhcp_client, G_DHCP_HOST_NAME);
		g_dhcp_client_set_request(dhcp_client, G_DHCP_DNS_SERVER);
		g_dhcp_client_set_request(dhcp_client, G_DHCP_DOMAIN_NAME);
		g_dhcp_client_set_request(dhcp_client, G_DHCP_NTP_SERVER);
		g_dhcp_client_set_request(dhcp_client, 252);
	}

	g_dhcp_client_set_request(dhcp_client, G_DHCP_SUBNET);
	g_dhcp_client_set_request(dhcp_client, G_DHCP_ROUTER);

	g_dhcp_client_register_event(dhcp_client,
			G_DHCP_CLIENT_EVENT_LEASE_AVAILABLE,
						lease_available_cb, dhcp);

	g_dhcp_client_register_event(dhcp_client,
			G_DHCP_CLIENT_EVENT_LEASE_LOST, lease_lost_cb, dhcp);

	g_dhcp_client_register_event(dhcp_client,
			G_DHCP_CLIENT_EVENT_NO_LEASE, no_lease_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return 0;
}

static int dhcp_release(struct connman_dhcp *dhcp)
{
	DBG("dhcp %p", dhcp);

	if (dhcp->timeout > 0) {
		g_source_remove(dhcp->timeout);
		dhcp->timeout = 0;
	}

	if (dhcp->dhcp_client) {
		g_dhcp_client_stop(dhcp->dhcp_client);
		g_dhcp_client_unref(dhcp->dhcp_client);
	}

	dhcp->dhcp_client = NULL;

	g_free(dhcp->dhcp_debug_prefix);
	dhcp->dhcp_debug_prefix = NULL;

	ipv4ll_stop_client(dhcp);

	return 0;
}

char *__connman_dhcp_get_server_address(struct connman_ipconfig *ipconfig)
{
	struct connman_dhcp *dhcp;

	dhcp = g_hash_table_lookup(ipconfig_table, ipconfig);
	if (!dhcp)
		return NULL;

	return g_dhcp_client_get_server_address(dhcp->dhcp_client);
}

int __connman_dhcp_start(struct connman_ipconfig *ipconfig,
			struct connman_network *network, dhcp_cb callback,
			gpointer user_data)
{
	const char *last_addr = NULL;
	struct connman_dhcp *dhcp;
	int err;

	DBG("");

	if (network) {
		struct connman_service *service;

		service = connman_service_lookup_from_network(network);
		if (!service)
			return -EINVAL;
	}

	last_addr = __connman_ipconfig_get_dhcp_address(ipconfig);

	dhcp = g_hash_table_lookup(ipconfig_table, ipconfig);
	if (!dhcp) {

		dhcp = g_try_new0(struct connman_dhcp, 1);
		if (!dhcp)
			return -ENOMEM;

		dhcp->ipconfig = ipconfig;
		__connman_ipconfig_ref(ipconfig);

		if (network) {
			dhcp->network = network;
			connman_network_ref(network);
		}

		err = dhcp_initialize(dhcp);

		if (err < 0) {
			if (network)
				connman_network_unref(network);
			g_free(dhcp);
			return err;
		}

		g_hash_table_insert(ipconfig_table, ipconfig, dhcp);
	}

	dhcp->callback = callback;
	dhcp->user_data = user_data;

	return g_dhcp_client_start(dhcp->dhcp_client, last_addr);
}

void __connman_dhcp_stop(struct connman_ipconfig *ipconfig)
{
	struct connman_dhcp *dhcp;

	DBG("ipconfig_table %p ipconfig %p", ipconfig_table, ipconfig);

	if (!ipconfig_table)
		return;

	dhcp = g_hash_table_lookup(ipconfig_table, ipconfig);
	if (dhcp) {
		g_hash_table_remove(ipconfig_table, ipconfig);
		__connman_ipconfig_unref(ipconfig);
		if (dhcp->network)
			connman_network_unref(dhcp->network);
		dhcp_release(dhcp);
		dhcp_invalidate(dhcp, false);
		dhcp_free(dhcp);
	}
}

int __connman_dhcp_init(void)
{
	DBG("");

	ipconfig_table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
								NULL, NULL);

	return 0;
}

void __connman_dhcp_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(ipconfig_table);
	ipconfig_table = NULL;

	dhcp_cleanup_random();
}
