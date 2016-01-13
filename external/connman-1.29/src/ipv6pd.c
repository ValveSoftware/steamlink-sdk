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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include "connman.h"

#include <gdhcp/gdhcp.h>

#define DEFAULT_ROUTER_LIFETIME 180  /* secs */
#define DEFAULT_RA_INTERVAL 120  /* secs */

static int bridge_index = -1;
static guint timer_uplink;
static guint timer_ra;
static char *default_interface;
static GSList *prefixes;
static GHashTable *timer_hash;
static void *rs_context;

static int setup_prefix_delegation(struct connman_service *service);
static void dhcpv6_callback(struct connman_network *network,
			enum __connman_dhcpv6_status status, gpointer data);

static int enable_ipv6_forward(bool enable)
{
	FILE *f;

	f = fopen("/proc/sys/net/ipv6/ip_forward", "r+");
	if (!f)
		return -errno;

	if (enable)
		fprintf(f, "1");
	else
		fprintf(f, "0");

	fclose(f);

	return 0;
}

static gboolean send_ra(gpointer data)
{
	__connman_inet_ipv6_send_ra(bridge_index, NULL, prefixes,
					DEFAULT_ROUTER_LIFETIME);

	return TRUE;
}

static void start_ra(int ifindex, GSList *prefix)
{
	if (prefixes)
		g_slist_free_full(prefixes, g_free);

	prefixes = g_dhcpv6_copy_prefixes(prefix);

	enable_ipv6_forward(true);

	if (timer_ra > 0)
		g_source_remove(timer_ra);

	send_ra(NULL);

	timer_ra = g_timeout_add_seconds(DEFAULT_RA_INTERVAL, send_ra, NULL);
}

static void stop_ra(int ifindex)
{
	__connman_inet_ipv6_send_ra(ifindex, NULL, prefixes, 0);

	if (timer_ra > 0) {
		g_source_remove(timer_ra);
		timer_ra = 0;
	}

	enable_ipv6_forward(false);

	if (prefixes) {
		g_slist_free_full(prefixes, g_free);
		prefixes = NULL;
	}
}

static void rs_received(struct nd_router_solicit *reply,
			unsigned int length, void *user_data)
{
	GDHCPIAPrefix *prefix;
	GSList *list;

	if (!prefixes)
		return;

	DBG("");

	for (list = prefixes; list; list = list->next) {
		prefix = list->data;

		prefix->valid -= time(NULL) - prefix->expire;
		prefix->preferred -= time(NULL) - prefix->expire;
	}

	__connman_inet_ipv6_send_ra(bridge_index, NULL, prefixes,
					DEFAULT_ROUTER_LIFETIME);
}

static gboolean do_setup(gpointer data)
{
	int ret;

	timer_uplink = 0;

	if (!default_interface)
		DBG("No uplink connection, retrying prefix delegation");

	ret = setup_prefix_delegation(__connman_service_get_default());
	if (ret < 0 && ret != -EINPROGRESS)
		return TRUE; /* delegation error, try again */

	return FALSE;
}

static void dhcpv6_renew_callback(struct connman_network *network,
				enum __connman_dhcpv6_status status,
				gpointer data)
{
	DBG("network %p status %d data %p", network, status, data);

	if (status == CONNMAN_DHCPV6_STATUS_SUCCEED)
		dhcpv6_callback(network, status, data);
	else
		setup_prefix_delegation(__connman_service_get_default());
}

static void cleanup(void)
{
	if (timer_uplink != 0) {
		g_source_remove(timer_uplink);
		timer_uplink = 0;
	}

	if (timer_hash) {
		g_hash_table_destroy(timer_hash);
		timer_hash = NULL;
	}

	if (prefixes) {
		g_slist_free_full(prefixes, g_free);
		prefixes = NULL;
	}
}

static void dhcpv6_callback(struct connman_network *network,
			enum __connman_dhcpv6_status status, gpointer data)
{
	GSList *prefix_list = data;

	DBG("network %p status %d data %p", network, status, data);

	if (status == CONNMAN_DHCPV6_STATUS_FAIL) {
		DBG("Prefix delegation request failed");
		cleanup();
		return;
	}

	if (!prefix_list) {
		DBG("No prefixes, retrying");
		if (timer_uplink == 0)
			timer_uplink = g_timeout_add_seconds(10, do_setup,
									NULL);
		return;
	}

	/*
	 * After we have got a list of prefixes, we can start to send router
	 * advertisements (RA) to tethering interface.
	 */
	start_ra(bridge_index, prefix_list);

	if (__connman_dhcpv6_start_pd_renew(network,
					dhcpv6_renew_callback) == -ETIMEDOUT)
		dhcpv6_renew_callback(network, CONNMAN_DHCPV6_STATUS_FAIL,
									NULL);
}

static int setup_prefix_delegation(struct connman_service *service)
{
	struct connman_ipconfig *ipconfig;
	char *interface;
	int err = 0, ifindex;

	if (!service) {
		/*
		 * We do not have uplink connection. We just wait until
		 * default interface is updated.
		 */
		return -EINPROGRESS;
	}

	interface = connman_service_get_interface(service);

	DBG("interface %s bridge_index %d", interface, bridge_index);

	if (default_interface) {
		stop_ra(bridge_index);

		ifindex = connman_inet_ifindex(default_interface);
		__connman_dhcpv6_stop_pd(ifindex);
	}

	g_free(default_interface);

	ipconfig = __connman_service_get_ip6config(service);
	if (!__connman_ipconfig_ipv6_is_enabled(ipconfig)) {
		g_free(interface);
		default_interface = NULL;
		return -EPFNOSUPPORT;
	}

	default_interface = interface;

	if (default_interface) {
		ifindex = connman_inet_ifindex(default_interface);

		/*
		 * Try to get a IPv6 prefix so we can start to advertise it.
		 */
		err = __connman_dhcpv6_start_pd(ifindex, prefixes,
						dhcpv6_callback);
		if (err < 0)
			DBG("prefix delegation %d/%s", err, strerror(-err));
	}

	return err;
}

static void cleanup_timer(gpointer user_data)
{
	guint timer = GPOINTER_TO_UINT(user_data);

	g_source_remove(timer);
}

static void update_default_interface(struct connman_service *service)
{
	setup_prefix_delegation(service);
}

static void update_ipconfig(struct connman_service *service,
				struct connman_ipconfig *ipconfig)
{
	if (!service || service != __connman_service_get_default())
		return;

	if (ipconfig != __connman_service_get_ip6config(service))
		return;

	if (!__connman_ipconfig_ipv6_is_enabled(ipconfig)) {
		if (default_interface) {
			int ifindex;

			ifindex = connman_inet_ifindex(default_interface);
			__connman_dhcpv6_stop_pd(ifindex);

			g_free(default_interface);
			default_interface = NULL;
		}

		DBG("No IPv6 support for interface index %d",
			__connman_ipconfig_get_index(ipconfig));
		return;
	}

	/*
	 * Did we had PD activated already? If not, then start it.
	 */
	if (!default_interface) {
		DBG("IPv6 ipconfig %p changed for interface index %d", ipconfig,
			__connman_ipconfig_get_index(ipconfig));

		setup_prefix_delegation(service);
	}
}

static struct connman_notifier pd_notifier = {
	.name			= "IPv6 prefix delegation",
	.default_changed	= update_default_interface,
	.ipconfig_changed	= update_ipconfig,
};

int __connman_ipv6pd_setup(const char *bridge)
{
	struct connman_service *service;
	int err;

	if (!connman_inet_is_ipv6_supported())
		return -EPFNOSUPPORT;

	if (bridge_index >= 0) {
		DBG("Prefix delegation already running");
		return -EALREADY;
	}

	err = connman_notifier_register(&pd_notifier);
	if (err < 0)
		return err;

	bridge_index = connman_inet_ifindex(bridge);

	timer_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
						NULL, cleanup_timer);

	err = __connman_inet_ipv6_start_recv_rs(bridge_index, rs_received,
						NULL, &rs_context);
	if (err < 0)
		DBG("Cannot receive router solicitation %d/%s",
			err, strerror(-err));

	service = __connman_service_get_default();
	if (service) {
		/*
		 * We have an uplink connection already, try to use it.
		 */
		return setup_prefix_delegation(service);
	}

	/*
	 * The prefix delegation is started after have got the uplink
	 * connection up i.e., when the default service is setup in which
	 * case the update_default_interface() will be called.
	 */
	return -EINPROGRESS;
}

void __connman_ipv6pd_cleanup(void)
{
	int ifindex;

	if (!connman_inet_is_ipv6_supported())
		return;

	connman_notifier_unregister(&pd_notifier);

	__connman_inet_ipv6_stop_recv_rs(rs_context);
	rs_context = NULL;

	cleanup();

	stop_ra(bridge_index);

	if (default_interface) {
		ifindex = connman_inet_ifindex(default_interface);
		__connman_dhcpv6_stop_pd(ifindex);
		g_free(default_interface);
		default_interface = NULL;
	}

	bridge_index = -1;
}
