/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
 *  Copyright (C) 2011	ProFUSION embedded systems
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
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <string.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <linux/if_bridge.h>

#include "connman.h"

#include <gdhcp/gdhcp.h>

#include <gdbus.h>

#ifndef DBUS_TYPE_UNIX_FD
#define DBUS_TYPE_UNIX_FD -1
#endif

#define BRIDGE_NAME "tether"

#define DEFAULT_MTU	1500

static char *private_network_primary_dns = NULL;
static char *private_network_secondary_dns = NULL;

static volatile int tethering_enabled;
static GDHCPServer *tethering_dhcp_server = NULL;
static struct connman_ippool *dhcp_ippool = NULL;
static DBusConnection *connection;
static GHashTable *pn_hash;

struct connman_private_network {
	char *owner;
	char *path;
	guint watch;
	DBusMessage *msg;
	DBusMessage *reply;
	int fd;
	char *interface;
	int index;
	guint iface_watch;
	struct connman_ippool *pool;
	char *primary_dns;
	char *secondary_dns;
};

const char *__connman_tethering_get_bridge(void)
{
	int sk, err;
	unsigned long args[3];

	sk = socket(AF_INET, SOCK_STREAM, 0);
	if (sk < 0)
		return NULL;

	args[0] = BRCTL_GET_VERSION;
	args[1] = args[2] = 0;
	err = ioctl(sk, SIOCGIFBR, &args);
	close(sk);
	if (err == -1) {
		connman_error("Missing support for 802.1d ethernet bridging");
		return NULL;
	}

	return BRIDGE_NAME;
}

static void dhcp_server_debug(const char *str, void *data)
{
	connman_info("%s: %s\n", (const char *) data, str);
}

static void dhcp_server_error(GDHCPServerError error)
{
	switch (error) {
	case G_DHCP_SERVER_ERROR_NONE:
		connman_error("OK");
		break;
	case G_DHCP_SERVER_ERROR_INTERFACE_UNAVAILABLE:
		connman_error("Interface unavailable");
		break;
	case G_DHCP_SERVER_ERROR_INTERFACE_IN_USE:
		connman_error("Interface in use");
		break;
	case G_DHCP_SERVER_ERROR_INTERFACE_DOWN:
		connman_error("Interface down");
		break;
	case G_DHCP_SERVER_ERROR_NOMEM:
		connman_error("No memory");
		break;
	case G_DHCP_SERVER_ERROR_INVALID_INDEX:
		connman_error("Invalid index");
		break;
	case G_DHCP_SERVER_ERROR_INVALID_OPTION:
		connman_error("Invalid option");
		break;
	case G_DHCP_SERVER_ERROR_IP_ADDRESS_INVALID:
		connman_error("Invalid address");
		break;
	}
}

static GDHCPServer *dhcp_server_start(const char *bridge,
				const char *router, const char *subnet,
				const char *start_ip, const char *end_ip,
				unsigned int lease_time, const char *dns)
{
	GDHCPServerError error;
	GDHCPServer *dhcp_server;
	int index;

	DBG("");

	index = connman_inet_ifindex(bridge);
	if (index < 0)
		return NULL;

	dhcp_server = g_dhcp_server_new(G_DHCP_IPV4, index, &error);
	if (!dhcp_server) {
		dhcp_server_error(error);
		return NULL;
	}

	g_dhcp_server_set_debug(dhcp_server, dhcp_server_debug, "DHCP server");

	g_dhcp_server_set_lease_time(dhcp_server, lease_time);
	g_dhcp_server_set_option(dhcp_server, G_DHCP_SUBNET, subnet);
	g_dhcp_server_set_option(dhcp_server, G_DHCP_ROUTER, router);
	g_dhcp_server_set_option(dhcp_server, G_DHCP_DNS_SERVER, dns);
	g_dhcp_server_set_ip_range(dhcp_server, start_ip, end_ip);

	g_dhcp_server_start(dhcp_server);

	return dhcp_server;
}

static void dhcp_server_stop(GDHCPServer *server)
{
	if (!server)
		return;

	g_dhcp_server_unref(server);
}

static void tethering_restart(struct connman_ippool *pool, void *user_data)
{
	DBG("pool %p", pool);
	__connman_tethering_set_disabled();
	__connman_tethering_set_enabled();
}

void __connman_tethering_set_enabled(void)
{
	int index;
	int err;
	const char *gateway;
	const char *broadcast;
	const char *subnet_mask;
	const char *start_ip;
	const char *end_ip;
	const char *dns;
	unsigned char prefixlen;
	char **ns;

	DBG("enabled %d", tethering_enabled + 1);

	if (__sync_fetch_and_add(&tethering_enabled, 1) != 0)
		return;

	err = __connman_bridge_create(BRIDGE_NAME);
	if (err < 0) {
		__sync_fetch_and_sub(&tethering_enabled, 1);
		return;
	}

	index = connman_inet_ifindex(BRIDGE_NAME);
	dhcp_ippool = __connman_ippool_create(index, 2, 252,
						tethering_restart, NULL);
	if (!dhcp_ippool) {
		connman_error("Fail to create IP pool");
		__connman_bridge_remove(BRIDGE_NAME);
		__sync_fetch_and_sub(&tethering_enabled, 1);
		return;
	}

	gateway = __connman_ippool_get_gateway(dhcp_ippool);
	broadcast = __connman_ippool_get_broadcast(dhcp_ippool);
	subnet_mask = __connman_ippool_get_subnet_mask(dhcp_ippool);
	start_ip = __connman_ippool_get_start_ip(dhcp_ippool);
	end_ip = __connman_ippool_get_end_ip(dhcp_ippool);

	err = __connman_bridge_enable(BRIDGE_NAME, gateway,
			connman_ipaddress_calc_netmask_len(subnet_mask),
			broadcast);
	if (err < 0 && err != -EALREADY) {
		__connman_ippool_unref(dhcp_ippool);
		__connman_bridge_remove(BRIDGE_NAME);
		__sync_fetch_and_sub(&tethering_enabled, 1);
		return;
	}

	ns = connman_setting_get_string_list("FallbackNameservers");
	if (ns) {
		if (ns[0]) {
			g_free(private_network_primary_dns);
			private_network_primary_dns = g_strdup(ns[0]);
		}
		if (ns[1]) {
			g_free(private_network_secondary_dns);
			private_network_secondary_dns = g_strdup(ns[1]);
		}

		DBG("Fallback ns primary %s secondary %s",
			private_network_primary_dns,
			private_network_secondary_dns);
	}

	dns = gateway;
	if (__connman_dnsproxy_add_listener(index) < 0) {
		connman_error("Can't add listener %s to DNS proxy",
								BRIDGE_NAME);
		dns = private_network_primary_dns;
		DBG("Serving %s nameserver to clients", dns);
	}

	tethering_dhcp_server = dhcp_server_start(BRIDGE_NAME,
						gateway, subnet_mask,
						start_ip, end_ip,
						24 * 3600, dns);
	if (!tethering_dhcp_server) {
		__connman_bridge_disable(BRIDGE_NAME);
		__connman_ippool_unref(dhcp_ippool);
		__connman_bridge_remove(BRIDGE_NAME);
		__sync_fetch_and_sub(&tethering_enabled, 1);
		return;
	}

	prefixlen = connman_ipaddress_calc_netmask_len(subnet_mask);
	err = __connman_nat_enable(BRIDGE_NAME, start_ip, prefixlen);
	if (err < 0) {
		connman_error("Cannot enable NAT %d/%s", err, strerror(-err));
		dhcp_server_stop(tethering_dhcp_server);
		__connman_bridge_disable(BRIDGE_NAME);
		__connman_ippool_unref(dhcp_ippool);
		__connman_bridge_remove(BRIDGE_NAME);
		__sync_fetch_and_sub(&tethering_enabled, 1);
		return;
	}

	err = __connman_ipv6pd_setup(BRIDGE_NAME);
	if (err < 0 && err != -EINPROGRESS)
		DBG("Cannot setup IPv6 prefix delegation %d/%s", err,
			strerror(-err));

	DBG("tethering started");
}

void __connman_tethering_set_disabled(void)
{
	int index;

	DBG("enabled %d", tethering_enabled - 1);

	if (__sync_fetch_and_sub(&tethering_enabled, 1) != 1)
		return;

	__connman_ipv6pd_cleanup();

	index = connman_inet_ifindex(BRIDGE_NAME);
	__connman_dnsproxy_remove_listener(index);

	__connman_nat_disable(BRIDGE_NAME);

	dhcp_server_stop(tethering_dhcp_server);

	tethering_dhcp_server = NULL;

	__connman_bridge_disable(BRIDGE_NAME);

	__connman_ippool_unref(dhcp_ippool);

	__connman_bridge_remove(BRIDGE_NAME);

	g_free(private_network_primary_dns);
	private_network_primary_dns = NULL;
	g_free(private_network_secondary_dns);
	private_network_secondary_dns = NULL;

	DBG("tethering stopped");
}

static void setup_tun_interface(unsigned int flags, unsigned change,
		void *data)
{
	struct connman_private_network *pn = data;
	unsigned char prefixlen;
	DBusMessageIter array, dict;
	const char *server_ip;
	const char *peer_ip;
	const char *subnet_mask;
	int err;

	DBG("index %d flags %d change %d", pn->index,  flags, change);

	if (flags & IFF_UP)
		return;

	subnet_mask = __connman_ippool_get_subnet_mask(pn->pool);
	server_ip = __connman_ippool_get_start_ip(pn->pool);
	peer_ip = __connman_ippool_get_end_ip(pn->pool);
	prefixlen = connman_ipaddress_calc_netmask_len(subnet_mask);

	if ((__connman_inet_modify_address(RTM_NEWADDR,
				NLM_F_REPLACE | NLM_F_ACK, pn->index, AF_INET,
				server_ip, peer_ip, prefixlen, NULL)) < 0) {
		DBG("address setting failed");
		return;
	}

	connman_inet_ifup(pn->index);

	err = __connman_nat_enable(BRIDGE_NAME, server_ip, prefixlen);
	if (err < 0) {
		connman_error("failed to enable NAT");
		goto error;
	}

	dbus_message_iter_init_append(pn->reply, &array);

	dbus_message_iter_append_basic(&array, DBUS_TYPE_OBJECT_PATH,
						&pn->path);

	connman_dbus_dict_open(&array, &dict);

	connman_dbus_dict_append_basic(&dict, "ServerIPv4",
					DBUS_TYPE_STRING, &server_ip);
	connman_dbus_dict_append_basic(&dict, "PeerIPv4",
					DBUS_TYPE_STRING, &peer_ip);
	if (pn->primary_dns)
		connman_dbus_dict_append_basic(&dict, "PrimaryDNS",
					DBUS_TYPE_STRING, &pn->primary_dns);

	if (pn->secondary_dns)
		connman_dbus_dict_append_basic(&dict, "SecondaryDNS",
					DBUS_TYPE_STRING, &pn->secondary_dns);

	connman_dbus_dict_close(&array, &dict);

	dbus_message_iter_append_basic(&array, DBUS_TYPE_UNIX_FD, &pn->fd);

	g_dbus_send_message(connection, pn->reply);

	return;

error:
	pn->reply = __connman_error_failed(pn->msg, -err);
	g_dbus_send_message(connection, pn->reply);

	g_hash_table_remove(pn_hash, pn->path);
}

static void remove_private_network(gpointer user_data)
{
	struct connman_private_network *pn = user_data;

	__connman_nat_disable(BRIDGE_NAME);
	connman_rtnl_remove_watch(pn->iface_watch);
	__connman_ippool_unref(pn->pool);

	if (pn->watch > 0) {
		g_dbus_remove_watch(connection, pn->watch);
		pn->watch = 0;
	}

	close(pn->fd);

	g_free(pn->interface);
	g_free(pn->owner);
	g_free(pn->path);
	g_free(pn->primary_dns);
	g_free(pn->secondary_dns);
	g_free(pn);
}

static void owner_disconnect(DBusConnection *conn, void *user_data)
{
	struct connman_private_network *pn = user_data;

	DBG("%s died", pn->owner);

	pn->watch = 0;

	g_hash_table_remove(pn_hash, pn->path);
}

static void ippool_disconnect(struct connman_ippool *pool, void *user_data)
{
	struct connman_private_network *pn = user_data;

	DBG("block used externally");

	g_hash_table_remove(pn_hash, pn->path);
}

int __connman_private_network_request(DBusMessage *msg, const char *owner)
{
	struct connman_private_network *pn;
	char *iface = NULL;
	char *path = NULL;
	int index, fd, err;

	if (DBUS_TYPE_UNIX_FD < 0)
		return -EINVAL;

	fd = connman_inet_create_tunnel(&iface);
	if (fd < 0)
		return fd;

	path = g_strdup_printf("/tethering/%s", iface);

	pn = g_hash_table_lookup(pn_hash, path);
	if (pn) {
		g_free(path);
		g_free(iface);
		close(fd);
		return -EEXIST;
	}

	index = connman_inet_ifindex(iface);
	if (index < 0) {
		err = -ENODEV;
		goto error;
	}
	DBG("interface %s", iface);

	err = connman_inet_set_mtu(index, DEFAULT_MTU);

	pn = g_try_new0(struct connman_private_network, 1);
	if (!pn) {
		err = -ENOMEM;
		goto error;
	}

	pn->owner = g_strdup(owner);
	pn->path = path;
	pn->watch = g_dbus_add_disconnect_watch(connection, pn->owner,
					owner_disconnect, pn, NULL);
	pn->msg = msg;
	pn->reply = dbus_message_new_method_return(pn->msg);
	if (!pn->reply)
		goto error;

	pn->fd = fd;
	pn->interface = iface;
	pn->index = index;
	pn->pool = __connman_ippool_create(pn->index, 1, 1, ippool_disconnect, pn);
	if (!pn->pool) {
		errno = -ENOMEM;
		goto error;
	}

	pn->primary_dns = g_strdup(private_network_primary_dns);
	pn->secondary_dns = g_strdup(private_network_secondary_dns);

	pn->iface_watch = connman_rtnl_add_newlink_watch(index,
						setup_tun_interface, pn);

	g_hash_table_insert(pn_hash, pn->path, pn);

	return 0;

error:
	close(fd);
	g_free(iface);
	g_free(path);
	g_free(pn);
	return err;
}

int __connman_private_network_release(const char *path)
{
	struct connman_private_network *pn;

	pn = g_hash_table_lookup(pn_hash, path);
	if (!pn)
		return -EACCES;

	g_hash_table_remove(pn_hash, path);
	return 0;
}

int __connman_tethering_init(void)
{
	DBG("");

	tethering_enabled = 0;

	connection = connman_dbus_get_connection();
	if (!connection)
		return -EFAULT;

	pn_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, remove_private_network);

	return 0;
}

void __connman_tethering_cleanup(void)
{
	DBG("enabled %d", tethering_enabled);

	__sync_synchronize();
	if (tethering_enabled > 0) {
		if (tethering_dhcp_server)
			dhcp_server_stop(tethering_dhcp_server);
		__connman_bridge_disable(BRIDGE_NAME);
		__connman_bridge_remove(BRIDGE_NAME);
		__connman_nat_disable(BRIDGE_NAME);
	}

	if (!connection)
		return;

	g_hash_table_destroy(pn_hash);
	dbus_connection_unref(connection);
}
