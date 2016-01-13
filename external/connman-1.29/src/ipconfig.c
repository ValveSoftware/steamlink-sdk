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

#include <errno.h>
#include <stdio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_link.h>
#include <string.h>
#include <stdlib.h>

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP	0x10000
#endif

#include <gdbus.h>
#include <connman/ipaddress.h>

#include "connman.h"

struct connman_ipconfig {
	int refcount;
	int index;
	enum connman_ipconfig_type type;

	const struct connman_ipconfig_ops *ops;
	void *ops_data;

	bool enabled;
	enum connman_ipconfig_method method;
	struct connman_ipaddress *address;
	struct connman_ipaddress *system;

	int ipv6_privacy_config;
	char *last_dhcp_address;
	char **last_dhcpv6_prefixes;
};

struct connman_ipdevice {
	int index;
	unsigned short type;
	unsigned int flags;
	char *address;
	uint16_t mtu;
	uint32_t rx_packets;
	uint32_t tx_packets;
	uint32_t rx_bytes;
	uint32_t tx_bytes;
	uint32_t rx_errors;
	uint32_t tx_errors;
	uint32_t rx_dropped;
	uint32_t tx_dropped;

	GSList *address_list;
	char *ipv4_gateway;
	char *ipv6_gateway;

	char *pac;

	struct connman_ipconfig *config_ipv4;
	struct connman_ipconfig *config_ipv6;

	bool ipv6_enabled;
	int ipv6_privacy;
};

static GHashTable *ipdevice_hash = NULL;
static GList *ipconfig_list = NULL;
static bool is_ipv6_supported = false;

void __connman_ipconfig_clear_address(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return;

	connman_ipaddress_clear(ipconfig->address);
}

static void free_address_list(struct connman_ipdevice *ipdevice)
{
	GSList *list;

	for (list = ipdevice->address_list; list; list = list->next) {
		struct connman_ipaddress *ipaddress = list->data;

		connman_ipaddress_free(ipaddress);
		list->data = NULL;
	}

	g_slist_free(ipdevice->address_list);
	ipdevice->address_list = NULL;
}

static struct connman_ipaddress *find_ipaddress(struct connman_ipdevice *ipdevice,
				unsigned char prefixlen, const char *local)
{
	GSList *list;

	for (list = ipdevice->address_list; list; list = list->next) {
		struct connman_ipaddress *ipaddress = list->data;

		if (g_strcmp0(ipaddress->local, local) == 0 &&
					ipaddress->prefixlen == prefixlen)
			return ipaddress;
	}

	return NULL;
}

const char *__connman_ipconfig_type2string(enum connman_ipconfig_type type)
{
	switch (type) {
	case CONNMAN_IPCONFIG_TYPE_UNKNOWN:
	case CONNMAN_IPCONFIG_TYPE_ALL:
		return "unknown";
	case CONNMAN_IPCONFIG_TYPE_IPV4:
		return "IPv4";
	case CONNMAN_IPCONFIG_TYPE_IPV6:
		return "IPv6";
	}

	return NULL;
}

static const char *type2str(unsigned short type)
{
	switch (type) {
	case ARPHRD_ETHER:
		return "ETHER";
	case ARPHRD_LOOPBACK:
		return "LOOPBACK";
	case ARPHRD_PPP:
		return "PPP";
	case ARPHRD_NONE:
		return "NONE";
	case ARPHRD_VOID:
		return "VOID";
	}

	return "";
}

static const char *scope2str(unsigned char scope)
{
	switch (scope) {
	case 0:
		return "UNIVERSE";
	case 253:
		return "LINK";
	}

	return "";
}

static bool get_ipv6_state(gchar *ifname)
{
	int disabled;
	gchar *path;
	FILE *f;
	bool enabled = false;

	if (!ifname)
		path = g_strdup("/proc/sys/net/ipv6/conf/all/disable_ipv6");
	else
		path = g_strdup_printf(
			"/proc/sys/net/ipv6/conf/%s/disable_ipv6", ifname);

	if (!path)
		return enabled;

	f = fopen(path, "r");

	g_free(path);

	if (f) {
		if (fscanf(f, "%d", &disabled) > 0)
			enabled = !disabled;
		fclose(f);
	}

	return enabled;
}

static void set_ipv6_state(gchar *ifname, bool enable)
{
	gchar *path;
	FILE *f;

	if (!ifname)
		path = g_strdup("/proc/sys/net/ipv6/conf/all/disable_ipv6");
	else
		path = g_strdup_printf(
			"/proc/sys/net/ipv6/conf/%s/disable_ipv6", ifname);

	if (!path)
		return;

	f = fopen(path, "r+");

	g_free(path);

	if (!f)
		return;

	if (!enable)
		fprintf(f, "1");
	else
		fprintf(f, "0");

	fclose(f);
}

static int get_ipv6_privacy(gchar *ifname)
{
	gchar *path;
	FILE *f;
	int value;

	if (!ifname)
		return 0;

	path = g_strdup_printf("/proc/sys/net/ipv6/conf/%s/use_tempaddr",
								ifname);

	if (!path)
		return 0;

	f = fopen(path, "r");

	g_free(path);

	if (!f)
		return 0;

	if (fscanf(f, "%d", &value) <= 0)
		value = 0;

	fclose(f);

	return value;
}

/* Enable the IPv6 privacy extension for stateless address autoconfiguration.
 * The privacy extension is described in RFC 3041 and RFC 4941
 */
static void set_ipv6_privacy(gchar *ifname, int value)
{
	gchar *path;
	FILE *f;

	if (!ifname)
		return;

	path = g_strdup_printf("/proc/sys/net/ipv6/conf/%s/use_tempaddr",
								ifname);

	if (!path)
		return;

	if (value < 0)
		value = 0;

	f = fopen(path, "r+");

	g_free(path);

	if (!f)
		return;

	fprintf(f, "%d", value);
	fclose(f);
}

static int get_rp_filter(void)
{
	FILE *f;
	int value = -EINVAL, tmp;

	f = fopen("/proc/sys/net/ipv4/conf/all/rp_filter", "r");

	if (f) {
		if (fscanf(f, "%d", &tmp) == 1)
			value = tmp;
		fclose(f);
	}

	return value;
}

static void set_rp_filter(int value)
{
	FILE *f;

	f = fopen("/proc/sys/net/ipv4/conf/all/rp_filter", "r+");

	if (!f)
		return;

	fprintf(f, "%d", value);

	fclose(f);
}

int __connman_ipconfig_set_rp_filter()
{
	int value;

	value = get_rp_filter();

	if (value < 0)
		return value;

	set_rp_filter(2);

	connman_info("rp_filter set to 2 (loose mode routing), "
			"old value was %d", value);

	return value;
}

void __connman_ipconfig_unset_rp_filter(int old_value)
{
	set_rp_filter(old_value);

	connman_info("rp_filter restored to %d", old_value);
}

bool __connman_ipconfig_ipv6_privacy_enabled(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return false;

	return ipconfig->ipv6_privacy_config == 0 ? FALSE : TRUE;
}

bool __connman_ipconfig_ipv6_is_enabled(struct connman_ipconfig *ipconfig)
{
	struct connman_ipdevice *ipdevice;
	char *ifname;
	bool ret;

	if (!ipconfig)
		return false;

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return false;

	ifname = connman_inet_ifname(ipconfig->index);
	ret = get_ipv6_state(ifname);
	g_free(ifname);

	return ret;
}

static void free_ipdevice(gpointer data)
{
	struct connman_ipdevice *ipdevice = data;
	char *ifname = connman_inet_ifname(ipdevice->index);

	connman_info("%s {remove} index %d", ifname, ipdevice->index);

	if (ipdevice->config_ipv4) {
		__connman_ipconfig_unref(ipdevice->config_ipv4);
		ipdevice->config_ipv4 = NULL;
	}

	if (ipdevice->config_ipv6) {
		__connman_ipconfig_unref(ipdevice->config_ipv6);
		ipdevice->config_ipv6 = NULL;
	}

	free_address_list(ipdevice);
	g_free(ipdevice->ipv4_gateway);
	g_free(ipdevice->ipv6_gateway);
	g_free(ipdevice->pac);

	g_free(ipdevice->address);

	set_ipv6_state(ifname, ipdevice->ipv6_enabled);
	set_ipv6_privacy(ifname, ipdevice->ipv6_privacy);

	g_free(ifname);
	g_free(ipdevice);
}

static void __connman_ipconfig_lower_up(struct connman_ipdevice *ipdevice)
{
	DBG("ipconfig ipv4 %p ipv6 %p", ipdevice->config_ipv4,
					ipdevice->config_ipv6);
}

static void __connman_ipconfig_lower_down(struct connman_ipdevice *ipdevice)
{
	DBG("ipconfig ipv4 %p ipv6 %p", ipdevice->config_ipv4,
					ipdevice->config_ipv6);

	if (ipdevice->config_ipv4)
		connman_inet_clear_address(ipdevice->index,
					ipdevice->config_ipv4->address);

	if (ipdevice->config_ipv6)
		connman_inet_clear_ipv6_address(ipdevice->index,
				ipdevice->config_ipv6->address->local,
				ipdevice->config_ipv6->address->prefixlen);
}

static void update_stats(struct connman_ipdevice *ipdevice,
			const char *ifname, struct rtnl_link_stats *stats)
{
	struct connman_service *service;

	if (stats->rx_packets == 0 && stats->tx_packets == 0)
		return;

	connman_info("%s {RX} %u packets %u bytes", ifname,
					stats->rx_packets, stats->rx_bytes);
	connman_info("%s {TX} %u packets %u bytes", ifname,
					stats->tx_packets, stats->tx_bytes);

	if (!ipdevice->config_ipv4 && !ipdevice->config_ipv6)
		return;

	service = __connman_service_lookup_from_index(ipdevice->index);

	DBG("service %p", service);

	if (!service)
		return;

	ipdevice->rx_packets = stats->rx_packets;
	ipdevice->tx_packets = stats->tx_packets;
	ipdevice->rx_bytes = stats->rx_bytes;
	ipdevice->tx_bytes = stats->tx_bytes;
	ipdevice->rx_errors = stats->rx_errors;
	ipdevice->tx_errors = stats->tx_errors;
	ipdevice->rx_dropped = stats->rx_dropped;
	ipdevice->tx_dropped = stats->tx_dropped;

	__connman_service_notify(service,
				ipdevice->rx_packets, ipdevice->tx_packets,
				ipdevice->rx_bytes, ipdevice->tx_bytes,
				ipdevice->rx_errors, ipdevice->tx_errors,
				ipdevice->rx_dropped, ipdevice->tx_dropped);
}

void __connman_ipconfig_newlink(int index, unsigned short type,
				unsigned int flags, const char *address,
							unsigned short mtu,
						struct rtnl_link_stats *stats)
{
	struct connman_ipdevice *ipdevice;
	GList *list, *ipconfig_copy;
	GString *str;
	bool up = false, down = false;
	bool lower_up = false, lower_down = false;
	char *ifname;

	DBG("index %d", index);

	if (type == ARPHRD_LOOPBACK)
		return;

	ifname = connman_inet_ifname(index);

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (ipdevice)
		goto update;

	ipdevice = g_try_new0(struct connman_ipdevice, 1);
	if (!ipdevice)
		goto out;

	ipdevice->index = index;
	ipdevice->type = type;

	ipdevice->ipv6_enabled = get_ipv6_state(ifname);
	ipdevice->ipv6_privacy = get_ipv6_privacy(ifname);

	ipdevice->address = g_strdup(address);

	g_hash_table_insert(ipdevice_hash, GINT_TO_POINTER(index), ipdevice);

	connman_info("%s {create} index %d type %d <%s>", ifname,
						index, type, type2str(type));

update:
	ipdevice->mtu = mtu;

	update_stats(ipdevice, ifname, stats);

	if (flags == ipdevice->flags)
		goto out;

	if ((ipdevice->flags & IFF_UP) != (flags & IFF_UP)) {
		if (flags & IFF_UP)
			up = true;
		else
			down = true;
	}

	if ((ipdevice->flags & (IFF_RUNNING | IFF_LOWER_UP)) !=
				(flags & (IFF_RUNNING | IFF_LOWER_UP))) {
		if ((flags & (IFF_RUNNING | IFF_LOWER_UP)) ==
					(IFF_RUNNING | IFF_LOWER_UP))
			lower_up = true;
		else if ((flags & (IFF_RUNNING | IFF_LOWER_UP)) == 0)
			lower_down = true;
	}

	ipdevice->flags = flags;

	str = g_string_new(NULL);
	if (!str)
		goto out;

	if (flags & IFF_UP)
		g_string_append(str, "UP");
	else
		g_string_append(str, "DOWN");

	if (flags & IFF_RUNNING)
		g_string_append(str, ",RUNNING");

	if (flags & IFF_LOWER_UP)
		g_string_append(str, ",LOWER_UP");

	connman_info("%s {update} flags %u <%s>", ifname, flags, str->str);

	g_string_free(str, TRUE);

	ipconfig_copy = g_list_copy(ipconfig_list);

	for (list = g_list_first(ipconfig_copy); list;
						list = g_list_next(list)) {
		struct connman_ipconfig *ipconfig = list->data;

		if (index != ipconfig->index)
			continue;

		if (!ipconfig->ops)
			continue;

		if (up && ipconfig->ops->up)
			ipconfig->ops->up(ipconfig, ifname);
		if (lower_up && ipconfig->ops->lower_up)
			ipconfig->ops->lower_up(ipconfig, ifname);

		if (lower_down && ipconfig->ops->lower_down)
			ipconfig->ops->lower_down(ipconfig, ifname);
		if (down && ipconfig->ops->down)
			ipconfig->ops->down(ipconfig, ifname);
	}

	g_list_free(ipconfig_copy);

	if (lower_up)
		__connman_ipconfig_lower_up(ipdevice);
	if (lower_down)
		__connman_ipconfig_lower_down(ipdevice);

out:
	g_free(ifname);
}

void __connman_ipconfig_dellink(int index, struct rtnl_link_stats *stats)
{
	struct connman_ipdevice *ipdevice;
	GList *list;
	char *ifname;

	DBG("index %d", index);

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return;

	ifname = connman_inet_ifname(index);

	update_stats(ipdevice, ifname, stats);

	for (list = g_list_first(ipconfig_list); list;
						list = g_list_next(list)) {
		struct connman_ipconfig *ipconfig = list->data;

		if (index != ipconfig->index)
			continue;

		ipconfig->index = -1;

		if (!ipconfig->ops)
			continue;

		if (ipconfig->ops->lower_down)
			ipconfig->ops->lower_down(ipconfig, ifname);
		if (ipconfig->ops->down)
			ipconfig->ops->down(ipconfig, ifname);
	}

	g_free(ifname);

	__connman_ipconfig_lower_down(ipdevice);

	g_hash_table_remove(ipdevice_hash, GINT_TO_POINTER(index));
}

static inline gint check_duplicate_address(gconstpointer a, gconstpointer b)
{
	const struct connman_ipaddress *addr1 = a;
	const struct connman_ipaddress *addr2 = b;

	if (addr1->prefixlen != addr2->prefixlen)
		return addr2->prefixlen - addr1->prefixlen;

	return g_strcmp0(addr1->local, addr2->local);
}

int __connman_ipconfig_newaddr(int index, int family, const char *label,
				unsigned char prefixlen, const char *address)
{
	struct connman_ipdevice *ipdevice;
	struct connman_ipaddress *ipaddress;
	enum connman_ipconfig_type type;
	GList *list;
	char *ifname;

	DBG("index %d", index);

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return -ENXIO;

	ipaddress = connman_ipaddress_alloc(family);
	if (!ipaddress)
		return -ENOMEM;

	ipaddress->prefixlen = prefixlen;
	ipaddress->local = g_strdup(address);

	if (g_slist_find_custom(ipdevice->address_list, ipaddress,
					check_duplicate_address)) {
		connman_ipaddress_free(ipaddress);
		return -EALREADY;
	}

	if (family == AF_INET)
		type = CONNMAN_IPCONFIG_TYPE_IPV4;
	else if (family == AF_INET6)
		type = CONNMAN_IPCONFIG_TYPE_IPV6;
	else
		return -EINVAL;

	ipdevice->address_list = g_slist_prepend(ipdevice->address_list,
								ipaddress);

	ifname = connman_inet_ifname(index);
	connman_info("%s {add} address %s/%u label %s family %d",
		ifname, address, prefixlen, label, family);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		__connman_ippool_newaddr(index, address, prefixlen);

	if (ipdevice->config_ipv4 && family == AF_INET)
		connman_ipaddress_copy_address(ipdevice->config_ipv4->system,
					ipaddress);

	else if (ipdevice->config_ipv6 && family == AF_INET6)
		connman_ipaddress_copy_address(ipdevice->config_ipv6->system,
					ipaddress);
	else
		goto out;

	if ((ipdevice->flags & (IFF_RUNNING | IFF_LOWER_UP)) != (IFF_RUNNING | IFF_LOWER_UP))
		goto out;

	for (list = g_list_first(ipconfig_list); list;
						list = g_list_next(list)) {
		struct connman_ipconfig *ipconfig = list->data;

		if (index != ipconfig->index)
			continue;

		if (type != ipconfig->type)
			continue;

		if (!ipconfig->ops)
			continue;

		if (ipconfig->ops->ip_bound)
			ipconfig->ops->ip_bound(ipconfig, ifname);
	}

out:
	g_free(ifname);
	return 0;
}

void __connman_ipconfig_deladdr(int index, int family, const char *label,
				unsigned char prefixlen, const char *address)
{
	struct connman_ipdevice *ipdevice;
	struct connman_ipaddress *ipaddress;
	enum connman_ipconfig_type type;
	GList *list;
	char *ifname;

	DBG("index %d", index);

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return;

	ipaddress = find_ipaddress(ipdevice, prefixlen, address);
	if (!ipaddress)
		return;

	if (family == AF_INET)
		type = CONNMAN_IPCONFIG_TYPE_IPV4;
	else if (family == AF_INET6)
		type = CONNMAN_IPCONFIG_TYPE_IPV6;
	else
		return;

	ipdevice->address_list = g_slist_remove(ipdevice->address_list,
								ipaddress);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		__connman_ippool_deladdr(index, address, prefixlen);

	connman_ipaddress_clear(ipaddress);
	g_free(ipaddress);

	ifname = connman_inet_ifname(index);
	connman_info("%s {del} address %s/%u label %s", ifname,
						address, prefixlen, label);

	if ((ipdevice->flags & (IFF_RUNNING | IFF_LOWER_UP)) != (IFF_RUNNING | IFF_LOWER_UP))
		goto out;

	if (g_slist_length(ipdevice->address_list) > 0)
		goto out;

	for (list = g_list_first(ipconfig_list); list;
						list = g_list_next(list)) {
		struct connman_ipconfig *ipconfig = list->data;

		if (index != ipconfig->index)
			continue;

		if (type != ipconfig->type)
			continue;

		if (!ipconfig->ops)
			continue;

		if (ipconfig->ops->ip_release)
			ipconfig->ops->ip_release(ipconfig, ifname);
	}

out:
	g_free(ifname);
}

void __connman_ipconfig_newroute(int index, int family, unsigned char scope,
					const char *dst, const char *gateway)
{
	struct connman_ipdevice *ipdevice;
	char *ifname;

	DBG("index %d", index);

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return;

	ifname = connman_inet_ifname(index);

	if (scope == 0 && (g_strcmp0(dst, "0.0.0.0") == 0 ||
						g_strcmp0(dst, "::") == 0)) {
		GList *config_list;
		enum connman_ipconfig_type type;

		if (family == AF_INET6) {
			type = CONNMAN_IPCONFIG_TYPE_IPV6;
			g_free(ipdevice->ipv6_gateway);
			ipdevice->ipv6_gateway = g_strdup(gateway);

			if (ipdevice->config_ipv6 &&
				ipdevice->config_ipv6->system) {
				g_free(ipdevice->config_ipv6->system->gateway);
				ipdevice->config_ipv6->system->gateway =
					g_strdup(gateway);
			}
		} else if (family == AF_INET) {
			type = CONNMAN_IPCONFIG_TYPE_IPV4;
			g_free(ipdevice->ipv4_gateway);
			ipdevice->ipv4_gateway = g_strdup(gateway);

			if (ipdevice->config_ipv4 &&
				ipdevice->config_ipv4->system) {
				g_free(ipdevice->config_ipv4->system->gateway);
				ipdevice->config_ipv4->system->gateway =
					g_strdup(gateway);
			}
		} else
			goto out;

		for (config_list = g_list_first(ipconfig_list); config_list;
					config_list = g_list_next(config_list)) {
			struct connman_ipconfig *ipconfig = config_list->data;

			if (index != ipconfig->index)
				continue;

			if (type != ipconfig->type)
				continue;

			if (!ipconfig->ops)
				continue;

			if (ipconfig->ops->route_set)
				ipconfig->ops->route_set(ipconfig, ifname);
		}
	}

	connman_info("%s {add} route %s gw %s scope %u <%s>",
		ifname, dst, gateway, scope, scope2str(scope));

out:
	g_free(ifname);
}

void __connman_ipconfig_delroute(int index, int family, unsigned char scope,
					const char *dst, const char *gateway)
{
	struct connman_ipdevice *ipdevice;
	char *ifname;

	DBG("index %d", index);

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return;

	ifname = connman_inet_ifname(index);

	if (scope == 0 && (g_strcmp0(dst, "0.0.0.0") == 0 ||
						g_strcmp0(dst, "::") == 0)) {
		GList *config_list;
		enum connman_ipconfig_type type;

		if (family == AF_INET6) {
			type = CONNMAN_IPCONFIG_TYPE_IPV6;
			g_free(ipdevice->ipv6_gateway);
			ipdevice->ipv6_gateway = NULL;

			if (ipdevice->config_ipv6 &&
				ipdevice->config_ipv6->system) {
				g_free(ipdevice->config_ipv6->system->gateway);
				ipdevice->config_ipv6->system->gateway = NULL;
			}
		} else if (family == AF_INET) {
			type = CONNMAN_IPCONFIG_TYPE_IPV4;
			g_free(ipdevice->ipv4_gateway);
			ipdevice->ipv4_gateway = NULL;

			if (ipdevice->config_ipv4 &&
				ipdevice->config_ipv4->system) {
				g_free(ipdevice->config_ipv4->system->gateway);
				ipdevice->config_ipv4->system->gateway = NULL;
			}
		} else
			goto out;

		for (config_list = g_list_first(ipconfig_list); config_list;
					config_list = g_list_next(config_list)) {
			struct connman_ipconfig *ipconfig = config_list->data;

			if (index != ipconfig->index)
				continue;

			if (type != ipconfig->type)
				continue;

			if (!ipconfig->ops)
				continue;

			if (ipconfig->ops->route_unset)
				ipconfig->ops->route_unset(ipconfig, ifname);
		}
	}

	connman_info("%s {del} route %s gw %s scope %u <%s>",
		ifname, dst, gateway, scope, scope2str(scope));

out:
	g_free(ifname);
}

void __connman_ipconfig_foreach(void (*function) (int index, void *user_data),
							void *user_data)
{
	GList *list, *keys;

	keys = g_hash_table_get_keys(ipdevice_hash);
	if (!keys)
		return;

	for (list = g_list_first(keys); list; list = g_list_next(list)) {
		int index = GPOINTER_TO_INT(list->data);

		function(index, user_data);
	}

	g_list_free(keys);
}

enum connman_ipconfig_type __connman_ipconfig_get_config_type(
					struct connman_ipconfig *ipconfig)
{
	return ipconfig ? ipconfig->type : CONNMAN_IPCONFIG_TYPE_UNKNOWN;
}

unsigned short __connman_ipconfig_get_type_from_index(int index)
{
	struct connman_ipdevice *ipdevice;

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return ARPHRD_VOID;

	return ipdevice->type;
}

unsigned int __connman_ipconfig_get_flags_from_index(int index)
{
	struct connman_ipdevice *ipdevice;

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return 0;

	return ipdevice->flags;
}

const char *__connman_ipconfig_get_gateway_from_index(int index,
	enum connman_ipconfig_type type)
{
	struct connman_ipdevice *ipdevice;

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return NULL;

	if (type != CONNMAN_IPCONFIG_TYPE_IPV6) {
		if (ipdevice->ipv4_gateway)
			return ipdevice->ipv4_gateway;

		if (ipdevice->config_ipv4 &&
				ipdevice->config_ipv4->address)
			return ipdevice->config_ipv4->address->gateway;
	}

	if (type != CONNMAN_IPCONFIG_TYPE_IPV4) {
		if (ipdevice->ipv6_gateway)
			return ipdevice->ipv6_gateway;

		if (ipdevice->config_ipv6 &&
				ipdevice->config_ipv6->address)
			return ipdevice->config_ipv6->address->gateway;
	}

	return NULL;
}

void __connman_ipconfig_set_index(struct connman_ipconfig *ipconfig, int index)
{
	ipconfig->index = index;
}

const char *__connman_ipconfig_get_local(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return NULL;

	return ipconfig->address->local;
}

void __connman_ipconfig_set_local(struct connman_ipconfig *ipconfig,
					const char *address)
{
	if (!ipconfig->address)
		return;

	g_free(ipconfig->address->local);
	ipconfig->address->local = g_strdup(address);
}

const char *__connman_ipconfig_get_peer(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return NULL;

	return ipconfig->address->peer;
}

void __connman_ipconfig_set_peer(struct connman_ipconfig *ipconfig,
					const char *address)
{
	if (!ipconfig->address)
		return;

	g_free(ipconfig->address->peer);
	ipconfig->address->peer = g_strdup(address);
}

const char *__connman_ipconfig_get_broadcast(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return NULL;

	return ipconfig->address->broadcast;
}

void __connman_ipconfig_set_broadcast(struct connman_ipconfig *ipconfig,
					const char *broadcast)
{
	if (!ipconfig->address)
		return;

	g_free(ipconfig->address->broadcast);
	ipconfig->address->broadcast = g_strdup(broadcast);
}

const char *__connman_ipconfig_get_gateway(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return NULL;

	return ipconfig->address->gateway;
}

void __connman_ipconfig_set_gateway(struct connman_ipconfig *ipconfig,
					const char *gateway)
{
	DBG("");

	if (!ipconfig->address)
		return;
	g_free(ipconfig->address->gateway);
	ipconfig->address->gateway = g_strdup(gateway);
}

int __connman_ipconfig_gateway_add(struct connman_ipconfig *ipconfig)
{
	struct connman_service *service;

	DBG("");

	if (!ipconfig->address)
		return -EINVAL;

	service = __connman_service_lookup_from_index(ipconfig->index);
	if (!service)
		return -EINVAL;

	DBG("type %d gw %s peer %s", ipconfig->type,
		ipconfig->address->gateway, ipconfig->address->peer);

	if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV6 ||
				ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV4)
		return __connman_connection_gateway_add(service,
						ipconfig->address->gateway,
						ipconfig->type,
						ipconfig->address->peer);

	return 0;
}

void __connman_ipconfig_gateway_remove(struct connman_ipconfig *ipconfig)
{
	struct connman_service *service;

	DBG("");

	service = __connman_service_lookup_from_index(ipconfig->index);
	if (service)
		__connman_connection_gateway_remove(service, ipconfig->type);
}

unsigned char __connman_ipconfig_get_prefixlen(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return 0;

	return ipconfig->address->prefixlen;
}

void __connman_ipconfig_set_prefixlen(struct connman_ipconfig *ipconfig,
					unsigned char prefixlen)
{
	if (!ipconfig->address)
		return;

	ipconfig->address->prefixlen = prefixlen;
}

static struct connman_ipconfig *create_ipv6config(int index)
{
	struct connman_ipconfig *ipv6config;
	struct connman_ipdevice *ipdevice;

	DBG("index %d", index);

	ipv6config = g_try_new0(struct connman_ipconfig, 1);
	if (!ipv6config)
		return NULL;

	ipv6config->refcount = 1;

	ipv6config->index = index;
	ipv6config->enabled = false;
	ipv6config->type = CONNMAN_IPCONFIG_TYPE_IPV6;

	if (!is_ipv6_supported)
		ipv6config->method = CONNMAN_IPCONFIG_METHOD_OFF;
	else
		ipv6config->method = CONNMAN_IPCONFIG_METHOD_AUTO;

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (ipdevice)
		ipv6config->ipv6_privacy_config = ipdevice->ipv6_privacy;

	ipv6config->address = connman_ipaddress_alloc(AF_INET6);
	if (!ipv6config->address) {
		g_free(ipv6config);
		return NULL;
	}

	ipv6config->system = connman_ipaddress_alloc(AF_INET6);

	DBG("ipconfig %p method %s", ipv6config,
		__connman_ipconfig_method2string(ipv6config->method));

	return ipv6config;
}

/**
 * connman_ipconfig_create:
 *
 * Allocate a new ipconfig structure.
 *
 * Returns: a newly-allocated #connman_ipconfig structure
 */
struct connman_ipconfig *__connman_ipconfig_create(int index,
					enum connman_ipconfig_type type)
{
	struct connman_ipconfig *ipconfig;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		return create_ipv6config(index);

	DBG("index %d", index);

	ipconfig = g_try_new0(struct connman_ipconfig, 1);
	if (!ipconfig)
		return NULL;

	ipconfig->refcount = 1;

	ipconfig->index = index;
	ipconfig->enabled = false;
	ipconfig->type = CONNMAN_IPCONFIG_TYPE_IPV4;

	ipconfig->address = connman_ipaddress_alloc(AF_INET);
	if (!ipconfig->address) {
		g_free(ipconfig);
		return NULL;
	}

	ipconfig->system = connman_ipaddress_alloc(AF_INET);

	DBG("ipconfig %p", ipconfig);

	return ipconfig;
}


/**
 * connman_ipconfig_ref:
 * @ipconfig: ipconfig structure
 *
 * Increase reference counter of ipconfig
 */
struct connman_ipconfig *
__connman_ipconfig_ref_debug(struct connman_ipconfig *ipconfig,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", ipconfig, ipconfig->refcount + 1,
		file, line, caller);

	__sync_fetch_and_add(&ipconfig->refcount, 1);

	return ipconfig;
}

/**
 * connman_ipconfig_unref:
 * @ipconfig: ipconfig structure
 *
 * Decrease reference counter of ipconfig
 */
void __connman_ipconfig_unref_debug(struct connman_ipconfig *ipconfig,
				const char *file, int line, const char *caller)
{
	if (!ipconfig)
		return;

	DBG("%p ref %d by %s:%d:%s()", ipconfig, ipconfig->refcount - 1,
		file, line, caller);

	if (__sync_fetch_and_sub(&ipconfig->refcount, 1) != 1)
		return;

	if (__connman_ipconfig_disable(ipconfig) < 0)
		ipconfig_list = g_list_remove(ipconfig_list, ipconfig);

	__connman_ipconfig_set_ops(ipconfig, NULL);

	connman_ipaddress_free(ipconfig->system);
	connman_ipaddress_free(ipconfig->address);
	g_free(ipconfig->last_dhcp_address);
	g_strfreev(ipconfig->last_dhcpv6_prefixes);
	g_free(ipconfig);
}

/**
 * connman_ipconfig_get_data:
 * @ipconfig: ipconfig structure
 *
 * Get private data pointer
 */
void *__connman_ipconfig_get_data(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return NULL;

	return ipconfig->ops_data;
}

/**
 * connman_ipconfig_set_data:
 * @ipconfig: ipconfig structure
 * @data: data pointer
 *
 * Set private data pointer
 */
void __connman_ipconfig_set_data(struct connman_ipconfig *ipconfig, void *data)
{
	ipconfig->ops_data = data;
}

/**
 * connman_ipconfig_get_index:
 * @ipconfig: ipconfig structure
 *
 * Get interface index
 */
int __connman_ipconfig_get_index(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return -1;

	return ipconfig->index;
}

/**
 * connman_ipconfig_set_ops:
 * @ipconfig: ipconfig structure
 * @ops: operation callbacks
 *
 * Set the operation callbacks
 */
void __connman_ipconfig_set_ops(struct connman_ipconfig *ipconfig,
				const struct connman_ipconfig_ops *ops)
{
	ipconfig->ops = ops;
}

/**
 * connman_ipconfig_set_method:
 * @ipconfig: ipconfig structure
 * @method: configuration method
 *
 * Set the configuration method
 */
int __connman_ipconfig_set_method(struct connman_ipconfig *ipconfig,
					enum connman_ipconfig_method method)
{
	ipconfig->method = method;

	return 0;
}

enum connman_ipconfig_method __connman_ipconfig_get_method(
				struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return CONNMAN_IPCONFIG_METHOD_UNKNOWN;

	return ipconfig->method;
}

int __connman_ipconfig_address_add(struct connman_ipconfig *ipconfig)
{
	DBG("");

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		break;
	case CONNMAN_IPCONFIG_METHOD_AUTO:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV4)
			return connman_inet_set_address(ipconfig->index,
							ipconfig->address);
		else if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV6)
			return connman_inet_set_ipv6_address(
					ipconfig->index, ipconfig->address);
	}

	return 0;
}

int __connman_ipconfig_address_remove(struct connman_ipconfig *ipconfig)
{
	int err;

	DBG("");

	if (!ipconfig)
		return 0;

	DBG("method %d", ipconfig->method);

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		break;
	case CONNMAN_IPCONFIG_METHOD_AUTO:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		err = __connman_ipconfig_address_unset(ipconfig);
		connman_ipaddress_clear(ipconfig->address);

		return err;
	}

	return 0;
}

int __connman_ipconfig_address_unset(struct connman_ipconfig *ipconfig)
{
	int err;

	DBG("");

	if (!ipconfig)
		return 0;

	DBG("method %d", ipconfig->method);

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		break;
	case CONNMAN_IPCONFIG_METHOD_AUTO:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV4)
			err = connman_inet_clear_address(ipconfig->index,
							ipconfig->address);
		else if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV6)
			err = connman_inet_clear_ipv6_address(
						ipconfig->index,
						ipconfig->address->local,
						ipconfig->address->prefixlen);
		else
			err = -EINVAL;

		return err;
	}

	return 0;
}

int __connman_ipconfig_set_proxy_autoconfig(struct connman_ipconfig *ipconfig,
                                                        const char *url)
{
	struct connman_ipdevice *ipdevice;

	DBG("ipconfig %p", ipconfig);

	if (!ipconfig || ipconfig->index < 0)
		return -ENODEV;

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return -ENXIO;

	g_free(ipdevice->pac);
	ipdevice->pac = g_strdup(url);

	return 0;
}

const char *__connman_ipconfig_get_proxy_autoconfig(struct connman_ipconfig *ipconfig)
{
	struct connman_ipdevice *ipdevice;

	DBG("ipconfig %p", ipconfig);

	if (!ipconfig || ipconfig->index < 0)
		return NULL;

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return NULL;

	return ipdevice->pac;
}

void __connman_ipconfig_set_dhcp_address(struct connman_ipconfig *ipconfig,
					const char *address)
{
	if (!ipconfig)
		return;

	g_free(ipconfig->last_dhcp_address);
	ipconfig->last_dhcp_address = g_strdup(address);
}

char *__connman_ipconfig_get_dhcp_address(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return NULL;

	return ipconfig->last_dhcp_address;
}

void __connman_ipconfig_set_dhcpv6_prefixes(struct connman_ipconfig *ipconfig,
					char **prefixes)
{
	if (!ipconfig)
		return;

	g_strfreev(ipconfig->last_dhcpv6_prefixes);
	ipconfig->last_dhcpv6_prefixes = prefixes;
}

char **__connman_ipconfig_get_dhcpv6_prefixes(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return NULL;

	return ipconfig->last_dhcpv6_prefixes;
}

static void disable_ipv6(struct connman_ipconfig *ipconfig)
{
	struct connman_ipdevice *ipdevice;
	char *ifname;

	DBG("");

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return;

	ifname = connman_inet_ifname(ipconfig->index);

	set_ipv6_state(ifname, false);

	g_free(ifname);
}

static void enable_ipv6(struct connman_ipconfig *ipconfig)
{
	struct connman_ipdevice *ipdevice;
	char *ifname;

	DBG("");

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return;

	ifname = connman_inet_ifname(ipconfig->index);

	if (ipconfig->method == CONNMAN_IPCONFIG_METHOD_AUTO)
		set_ipv6_privacy(ifname, ipconfig->ipv6_privacy_config);

	set_ipv6_state(ifname, true);

	g_free(ifname);
}

void __connman_ipconfig_enable_ipv6(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig || ipconfig->type != CONNMAN_IPCONFIG_TYPE_IPV6)
		return;

	enable_ipv6(ipconfig);
}

void __connman_ipconfig_disable_ipv6(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig || ipconfig->type != CONNMAN_IPCONFIG_TYPE_IPV6)
		return;

	disable_ipv6(ipconfig);
}

bool __connman_ipconfig_is_usable(struct connman_ipconfig *ipconfig)
{
	if (!ipconfig)
		return false;

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		return false;
	case CONNMAN_IPCONFIG_METHOD_AUTO:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		break;
	}

	return true;
}

int __connman_ipconfig_enable(struct connman_ipconfig *ipconfig)
{
	struct connman_ipdevice *ipdevice;
	bool up = false, down = false;
	bool lower_up = false, lower_down = false;
	enum connman_ipconfig_type type;
	char *ifname;

	DBG("ipconfig %p", ipconfig);

	if (!ipconfig || ipconfig->index < 0)
		return -ENODEV;

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return -ENXIO;

	if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV4) {
		if (ipdevice->config_ipv4 == ipconfig)
			return -EALREADY;
		type = CONNMAN_IPCONFIG_TYPE_IPV4;
	} else if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV6) {
		if (ipdevice->config_ipv6 == ipconfig)
			return -EALREADY;
		type = CONNMAN_IPCONFIG_TYPE_IPV6;
	} else
		return -EINVAL;

	ipconfig->enabled = true;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4 &&
					ipdevice->config_ipv4) {
		ipconfig_list = g_list_remove(ipconfig_list,
							ipdevice->config_ipv4);

		connman_ipaddress_clear(ipdevice->config_ipv4->system);

		__connman_ipconfig_unref(ipdevice->config_ipv4);
	}

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6 &&
					ipdevice->config_ipv6) {
		ipconfig_list = g_list_remove(ipconfig_list,
							ipdevice->config_ipv6);

		connman_ipaddress_clear(ipdevice->config_ipv6->system);

		__connman_ipconfig_unref(ipdevice->config_ipv6);
	}

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		ipdevice->config_ipv4 = __connman_ipconfig_ref(ipconfig);
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6) {
		ipdevice->config_ipv6 = __connman_ipconfig_ref(ipconfig);

		enable_ipv6(ipdevice->config_ipv6);
	}
	ipconfig_list = g_list_append(ipconfig_list, ipconfig);

	if (ipdevice->flags & IFF_UP)
		up = true;
	else
		down = true;

	if ((ipdevice->flags & (IFF_RUNNING | IFF_LOWER_UP)) ==
			(IFF_RUNNING | IFF_LOWER_UP))
		lower_up = true;
	else if ((ipdevice->flags & (IFF_RUNNING | IFF_LOWER_UP)) == 0)
		lower_down = true;

	ifname = connman_inet_ifname(ipconfig->index);

	if (up && ipconfig->ops->up)
		ipconfig->ops->up(ipconfig, ifname);
	if (lower_up && ipconfig->ops->lower_up)
		ipconfig->ops->lower_up(ipconfig, ifname);

	if (lower_down && ipconfig->ops->lower_down)
		ipconfig->ops->lower_down(ipconfig, ifname);
	if (down && ipconfig->ops->down)
		ipconfig->ops->down(ipconfig, ifname);

	g_free(ifname);

	return 0;
}

int __connman_ipconfig_disable(struct connman_ipconfig *ipconfig)
{
	struct connman_ipdevice *ipdevice;

	DBG("ipconfig %p", ipconfig);

	if (!ipconfig || ipconfig->index < 0)
		return -ENODEV;

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return -ENXIO;

	if (!ipdevice->config_ipv4 && !ipdevice->config_ipv6)
		return -EINVAL;

	ipconfig->enabled = false;

	if (ipdevice->config_ipv4 == ipconfig) {
		ipconfig_list = g_list_remove(ipconfig_list, ipconfig);

		connman_ipaddress_clear(ipdevice->config_ipv4->system);
		__connman_ipconfig_unref(ipdevice->config_ipv4);
		ipdevice->config_ipv4 = NULL;
		return 0;
	}

	if (ipdevice->config_ipv6 == ipconfig) {
		ipconfig_list = g_list_remove(ipconfig_list, ipconfig);

		connman_ipaddress_clear(ipdevice->config_ipv6->system);
		__connman_ipconfig_unref(ipdevice->config_ipv6);
		ipdevice->config_ipv6 = NULL;
		return 0;
	}

	return -EINVAL;
}

const char *__connman_ipconfig_method2string(enum connman_ipconfig_method method)
{
	switch (method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
		break;
	case CONNMAN_IPCONFIG_METHOD_OFF:
		return "off";
	case CONNMAN_IPCONFIG_METHOD_FIXED:
		return "fixed";
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		return "manual";
	case CONNMAN_IPCONFIG_METHOD_DHCP:
		return "dhcp";
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		return "auto";
	}

	return NULL;
}

enum connman_ipconfig_method __connman_ipconfig_string2method(const char *method)
{
	if (g_strcmp0(method, "off") == 0)
		return CONNMAN_IPCONFIG_METHOD_OFF;
	else if (g_strcmp0(method, "fixed") == 0)
		return CONNMAN_IPCONFIG_METHOD_FIXED;
	else if (g_strcmp0(method, "manual") == 0)
		return CONNMAN_IPCONFIG_METHOD_MANUAL;
	else if (g_strcmp0(method, "dhcp") == 0)
		return CONNMAN_IPCONFIG_METHOD_DHCP;
	else if (g_strcmp0(method, "auto") == 0)
		return CONNMAN_IPCONFIG_METHOD_AUTO;
	else
		return CONNMAN_IPCONFIG_METHOD_UNKNOWN;
}

static const char *privacy2string(int privacy)
{
	if (privacy <= 0)
		return "disabled";
	else if (privacy == 1)
		return "enabled";
	else
		return "prefered";
}

static int string2privacy(const char *privacy)
{
	if (g_strcmp0(privacy, "disabled") == 0)
		return 0;
	else if (g_strcmp0(privacy, "enabled") == 0)
		return 1;
	else if (g_strcmp0(privacy, "preferred") == 0)
		return 2;
	else if (g_strcmp0(privacy, "prefered") == 0)
		return 2;
	else
		return 0;
}

int __connman_ipconfig_ipv6_reset_privacy(struct connman_ipconfig *ipconfig)
{
	struct connman_ipdevice *ipdevice;
	int err;

	if (!ipconfig)
		return -EINVAL;

	ipdevice = g_hash_table_lookup(ipdevice_hash,
						GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return -ENODEV;

	err = __connman_ipconfig_ipv6_set_privacy(ipconfig, privacy2string(
							ipdevice->ipv6_privacy));

	return err;
}

int __connman_ipconfig_ipv6_set_privacy(struct connman_ipconfig *ipconfig,
					const char *value)
{
	int privacy;

	if (!ipconfig)
		return -EINVAL;

	DBG("ipconfig %p privacy %s", ipconfig, value);

	privacy = string2privacy(value);

	ipconfig->ipv6_privacy_config = privacy;

	enable_ipv6(ipconfig);

	return 0;
}

void __connman_ipconfig_append_ipv4(struct connman_ipconfig *ipconfig,
							DBusMessageIter *iter)
{
	struct connman_ipaddress *append_addr = NULL;
	const char *str;

	DBG("");

	if (ipconfig->type != CONNMAN_IPCONFIG_TYPE_IPV4)
		return;

	str = __connman_ipconfig_method2string(ipconfig->method);
	if (!str)
		return;

	connman_dbus_dict_append_basic(iter, "Method", DBUS_TYPE_STRING, &str);

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		return;

	case CONNMAN_IPCONFIG_METHOD_FIXED:
		append_addr = ipconfig->address;
		break;

	case CONNMAN_IPCONFIG_METHOD_MANUAL:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
		append_addr = ipconfig->system;
		break;
	}

	if (!append_addr)
		return;

	if (append_addr->local) {
		in_addr_t addr;
		struct in_addr netmask;
		char *mask;

		connman_dbus_dict_append_basic(iter, "Address",
				DBUS_TYPE_STRING, &append_addr->local);

		addr = 0xffffffff << (32 - append_addr->prefixlen);
		netmask.s_addr = htonl(addr);
		mask = inet_ntoa(netmask);
		connman_dbus_dict_append_basic(iter, "Netmask",
						DBUS_TYPE_STRING, &mask);
	}

	if (append_addr->gateway)
		connman_dbus_dict_append_basic(iter, "Gateway",
				DBUS_TYPE_STRING, &append_addr->gateway);
}

void __connman_ipconfig_append_ipv6(struct connman_ipconfig *ipconfig,
					DBusMessageIter *iter,
					struct connman_ipconfig *ipconfig_ipv4)
{
	struct connman_ipaddress *append_addr = NULL;
	const char *str, *privacy;

	DBG("");

	if (ipconfig->type != CONNMAN_IPCONFIG_TYPE_IPV6)
		return;

	str = __connman_ipconfig_method2string(ipconfig->method);
	if (!str)
		return;

	if (ipconfig_ipv4 &&
			ipconfig->method == CONNMAN_IPCONFIG_METHOD_AUTO) {
		if (__connman_6to4_check(ipconfig_ipv4) == 1)
			str = "6to4";
	}

	connman_dbus_dict_append_basic(iter, "Method", DBUS_TYPE_STRING, &str);

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		return;

	case CONNMAN_IPCONFIG_METHOD_FIXED:
		append_addr = ipconfig->address;
		break;

	case CONNMAN_IPCONFIG_METHOD_MANUAL:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		append_addr = ipconfig->system;
		break;
	}

	if (!append_addr)
		return;

	if (append_addr->local) {
		connman_dbus_dict_append_basic(iter, "Address",
				DBUS_TYPE_STRING, &append_addr->local);
		connman_dbus_dict_append_basic(iter, "PrefixLength",
						DBUS_TYPE_BYTE,
						&append_addr->prefixlen);
	}

	if (append_addr->gateway)
		connman_dbus_dict_append_basic(iter, "Gateway",
				DBUS_TYPE_STRING, &append_addr->gateway);

	privacy = privacy2string(ipconfig->ipv6_privacy_config);
	connman_dbus_dict_append_basic(iter, "Privacy",
				DBUS_TYPE_STRING, &privacy);
}

void __connman_ipconfig_append_ipv6config(struct connman_ipconfig *ipconfig,
							DBusMessageIter *iter)
{
	const char *str, *privacy;

	DBG("");

	str = __connman_ipconfig_method2string(ipconfig->method);
	if (!str)
		return;

	connman_dbus_dict_append_basic(iter, "Method", DBUS_TYPE_STRING, &str);

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
		return;
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		break;
	}

	if (!ipconfig->address)
		return;

	if (ipconfig->address->local) {
		connman_dbus_dict_append_basic(iter, "Address",
				DBUS_TYPE_STRING, &ipconfig->address->local);
		connman_dbus_dict_append_basic(iter, "PrefixLength",
						DBUS_TYPE_BYTE,
						&ipconfig->address->prefixlen);
	}

	if (ipconfig->address->gateway)
		connman_dbus_dict_append_basic(iter, "Gateway",
				DBUS_TYPE_STRING, &ipconfig->address->gateway);

	privacy = privacy2string(ipconfig->ipv6_privacy_config);
	connman_dbus_dict_append_basic(iter, "Privacy",
				DBUS_TYPE_STRING, &privacy);
}

void __connman_ipconfig_append_ipv4config(struct connman_ipconfig *ipconfig,
							DBusMessageIter *iter)
{
	const char *str;

	DBG("");

	str = __connman_ipconfig_method2string(ipconfig->method);
	if (!str)
		return;

	connman_dbus_dict_append_basic(iter, "Method", DBUS_TYPE_STRING, &str);

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		return;
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		break;
	}

	if (!ipconfig->address)
		return;

	if (ipconfig->address->local) {
		in_addr_t addr;
		struct in_addr netmask;
		char *mask;

		connman_dbus_dict_append_basic(iter, "Address",
				DBUS_TYPE_STRING, &ipconfig->address->local);

		addr = 0xffffffff << (32 - ipconfig->address->prefixlen);
		netmask.s_addr = htonl(addr);
		mask = inet_ntoa(netmask);
		connman_dbus_dict_append_basic(iter, "Netmask",
						DBUS_TYPE_STRING, &mask);
	}

	if (ipconfig->address->gateway)
		connman_dbus_dict_append_basic(iter, "Gateway",
				DBUS_TYPE_STRING, &ipconfig->address->gateway);
}

int __connman_ipconfig_set_config(struct connman_ipconfig *ipconfig,
							DBusMessageIter *array)
{
	enum connman_ipconfig_method method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;
	const char *address = NULL, *netmask = NULL, *gateway = NULL,
		*privacy_string = NULL;
	int prefix_length = 0, privacy = 0;
	DBusMessageIter dict;
	int type = -1;

	DBG("ipconfig %p", ipconfig);

	if (dbus_message_iter_get_arg_type(array) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;
		int type;

		dbus_message_iter_recurse(&dict, &entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
			return -EINVAL;

		dbus_message_iter_get_basic(&entry, &key);
		dbus_message_iter_next(&entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
			return -EINVAL;

		dbus_message_iter_recurse(&entry, &value);

		type = dbus_message_iter_get_arg_type(&value);

		if (g_str_equal(key, "Method")) {
			const char *str;

			if (type != DBUS_TYPE_STRING)
				return -EINVAL;

			dbus_message_iter_get_basic(&value, &str);
			method = __connman_ipconfig_string2method(str);
		} else if (g_str_equal(key, "Address")) {
			if (type != DBUS_TYPE_STRING)
				return -EINVAL;

			dbus_message_iter_get_basic(&value, &address);
		} else if (g_str_equal(key, "PrefixLength")) {
			if (type != DBUS_TYPE_BYTE)
				return -EINVAL;

			dbus_message_iter_get_basic(&value, &prefix_length);

			if (prefix_length < 0 || prefix_length > 128)
				return -EINVAL;
		} else if (g_str_equal(key, "Netmask")) {
			if (type != DBUS_TYPE_STRING)
				return -EINVAL;

			dbus_message_iter_get_basic(&value, &netmask);
		} else if (g_str_equal(key, "Gateway")) {
			if (type != DBUS_TYPE_STRING)
				return -EINVAL;

			dbus_message_iter_get_basic(&value, &gateway);
		} else if (g_str_equal(key, "Privacy")) {
			if (type != DBUS_TYPE_STRING)
				return -EINVAL;

			dbus_message_iter_get_basic(&value, &privacy_string);
			privacy = string2privacy(privacy_string);
		}

		dbus_message_iter_next(&dict);
	}

	DBG("method %d address %s netmask %s gateway %s prefix_length %d "
		"privacy %s",
		method, address, netmask, gateway, prefix_length,
		privacy_string);

	switch (method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
		return -EINVAL;

	case CONNMAN_IPCONFIG_METHOD_OFF:
		ipconfig->method = method;

		break;

	case CONNMAN_IPCONFIG_METHOD_AUTO:
		if (ipconfig->type != CONNMAN_IPCONFIG_TYPE_IPV6)
			return -EOPNOTSUPP;

		ipconfig->method = method;
		if (privacy_string)
			ipconfig->ipv6_privacy_config = privacy;

		break;

	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		switch (ipconfig->type) {
		case CONNMAN_IPCONFIG_TYPE_IPV4:
			type = AF_INET;
			break;
		case CONNMAN_IPCONFIG_TYPE_IPV6:
			type = AF_INET6;
			break;
		case CONNMAN_IPCONFIG_TYPE_UNKNOWN:
		case CONNMAN_IPCONFIG_TYPE_ALL:
			type = -1;
			break;
		}

		if ((address && connman_inet_check_ipaddress(address)
						!= type) ||
				(netmask &&
				connman_inet_check_ipaddress(netmask)
						!= type) ||
				(gateway &&
				connman_inet_check_ipaddress(gateway)
						!= type))
			return -EINVAL;

		ipconfig->method = method;

		if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV4)
			connman_ipaddress_set_ipv4(ipconfig->address,
						address, netmask, gateway);
		else
			return connman_ipaddress_set_ipv6(
					ipconfig->address, address,
						prefix_length, gateway);

		break;

	case CONNMAN_IPCONFIG_METHOD_DHCP:
		if (ipconfig->type != CONNMAN_IPCONFIG_TYPE_IPV4)
			return -EOPNOTSUPP;

		ipconfig->method = method;
		break;
	}

	return 0;
}

void __connman_ipconfig_append_ethernet(struct connman_ipconfig *ipconfig,
							DBusMessageIter *iter)
{
	struct connman_ipdevice *ipdevice;
	const char *method = "auto";

	connman_dbus_dict_append_basic(iter, "Method",
						DBUS_TYPE_STRING, &method);

	ipdevice = g_hash_table_lookup(ipdevice_hash,
					GINT_TO_POINTER(ipconfig->index));
	if (!ipdevice)
		return;

	if (ipconfig->index >= 0) {
		char *ifname = connman_inet_ifname(ipconfig->index);
		if (ifname) {
			connman_dbus_dict_append_basic(iter, "Interface",
						DBUS_TYPE_STRING, &ifname);
			g_free(ifname);
		}
	}

	if (ipdevice->address)
		connman_dbus_dict_append_basic(iter, "Address",
					DBUS_TYPE_STRING, &ipdevice->address);

	if (ipdevice->mtu > 0)
		connman_dbus_dict_append_basic(iter, "MTU",
					DBUS_TYPE_UINT16, &ipdevice->mtu);
}

int __connman_ipconfig_load(struct connman_ipconfig *ipconfig,
		GKeyFile *keyfile, const char *identifier, const char *prefix)
{
	char *method;
	char *key;
	char *str;

	DBG("ipconfig %p identifier %s", ipconfig, identifier);

	key = g_strdup_printf("%smethod", prefix);
	method = g_key_file_get_string(keyfile, identifier, key, NULL);
	if (!method) {
		switch (ipconfig->type) {
		case CONNMAN_IPCONFIG_TYPE_IPV4:
			ipconfig->method = CONNMAN_IPCONFIG_METHOD_DHCP;
			break;
		case CONNMAN_IPCONFIG_TYPE_IPV6:
			ipconfig->method = CONNMAN_IPCONFIG_METHOD_AUTO;
			break;
		case CONNMAN_IPCONFIG_TYPE_UNKNOWN:
		case CONNMAN_IPCONFIG_TYPE_ALL:
			ipconfig->method = CONNMAN_IPCONFIG_METHOD_OFF;
			break;
		}
	} else
		ipconfig->method = __connman_ipconfig_string2method(method);

	if (ipconfig->method == CONNMAN_IPCONFIG_METHOD_UNKNOWN)
		ipconfig->method = CONNMAN_IPCONFIG_METHOD_OFF;

	if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV6) {
		gsize length;
		char *pprefix;

		if (ipconfig->method == CONNMAN_IPCONFIG_METHOD_AUTO ||
			ipconfig->method == CONNMAN_IPCONFIG_METHOD_MANUAL) {
			char *privacy;

			pprefix = g_strdup_printf("%sprivacy", prefix);
			privacy = g_key_file_get_string(keyfile, identifier,
							pprefix, NULL);
			ipconfig->ipv6_privacy_config = string2privacy(privacy);
			g_free(pprefix);
			g_free(privacy);
		}

		pprefix = g_strdup_printf("%sDHCP.LastPrefixes", prefix);
		ipconfig->last_dhcpv6_prefixes =
			g_key_file_get_string_list(keyfile, identifier, pprefix,
						&length, NULL);
		if (ipconfig->last_dhcpv6_prefixes && length == 0) {
			g_free(ipconfig->last_dhcpv6_prefixes);
			ipconfig->last_dhcpv6_prefixes = NULL;
		}
		g_free(pprefix);
	}

	g_free(method);
	g_free(key);

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		break;

	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:

		key = g_strdup_printf("%snetmask_prefixlen", prefix);
		ipconfig->address->prefixlen = g_key_file_get_integer(
				keyfile, identifier, key, NULL);
		g_free(key);

		key = g_strdup_printf("%slocal_address", prefix);
		g_free(ipconfig->address->local);
		ipconfig->address->local = g_key_file_get_string(
			keyfile, identifier, key, NULL);
		g_free(key);

		key = g_strdup_printf("%speer_address", prefix);
		g_free(ipconfig->address->peer);
		ipconfig->address->peer = g_key_file_get_string(
				keyfile, identifier, key, NULL);
		g_free(key);

		key = g_strdup_printf("%sbroadcast_address", prefix);
		g_free(ipconfig->address->broadcast);
		ipconfig->address->broadcast = g_key_file_get_string(
				keyfile, identifier, key, NULL);
		g_free(key);

		key = g_strdup_printf("%sgateway", prefix);
		g_free(ipconfig->address->gateway);
		ipconfig->address->gateway = g_key_file_get_string(
				keyfile, identifier, key, NULL);
		g_free(key);
		break;

	case CONNMAN_IPCONFIG_METHOD_DHCP:

		key = g_strdup_printf("%sDHCP.LastAddress", prefix);
		str = g_key_file_get_string(keyfile, identifier, key, NULL);
		if (str) {
			g_free(ipconfig->last_dhcp_address);
			ipconfig->last_dhcp_address = str;
		}
		g_free(key);

		break;

	case CONNMAN_IPCONFIG_METHOD_AUTO:
		break;
	}

	return 0;
}

int __connman_ipconfig_save(struct connman_ipconfig *ipconfig,
		GKeyFile *keyfile, const char *identifier, const char *prefix)
{
	const char *method;
	char *key;

	method = __connman_ipconfig_method2string(ipconfig->method);

	DBG("ipconfig %p identifier %s method %s", ipconfig, identifier,
								method);
	if (method) {
		key = g_strdup_printf("%smethod", prefix);
		g_key_file_set_string(keyfile, identifier, key, method);
		g_free(key);
	}

	if (ipconfig->type == CONNMAN_IPCONFIG_TYPE_IPV6) {
		const char *privacy;
		privacy = privacy2string(ipconfig->ipv6_privacy_config);
		key = g_strdup_printf("%sprivacy", prefix);
		g_key_file_set_string(keyfile, identifier, key, privacy);
		g_free(key);

		key = g_strdup_printf("%sDHCP.LastAddress", prefix);
		if (ipconfig->last_dhcp_address &&
				strlen(ipconfig->last_dhcp_address) > 0)
			g_key_file_set_string(keyfile, identifier, key,
					ipconfig->last_dhcp_address);
		else
			g_key_file_remove_key(keyfile, identifier, key, NULL);
		g_free(key);

		key = g_strdup_printf("%sDHCP.LastPrefixes", prefix);
		if (ipconfig->last_dhcpv6_prefixes &&
				ipconfig->last_dhcpv6_prefixes[0]) {
			guint len =
				g_strv_length(ipconfig->last_dhcpv6_prefixes);

			g_key_file_set_string_list(keyfile, identifier, key,
				(const gchar **)ipconfig->last_dhcpv6_prefixes,
						len);
		} else
			g_key_file_remove_key(keyfile, identifier, key, NULL);
		g_free(key);
	}

	switch (ipconfig->method) {
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
		break;
	case CONNMAN_IPCONFIG_METHOD_DHCP:
		key = g_strdup_printf("%sDHCP.LastAddress", prefix);
		if (ipconfig->last_dhcp_address &&
				strlen(ipconfig->last_dhcp_address) > 0)
			g_key_file_set_string(keyfile, identifier, key,
					ipconfig->last_dhcp_address);
		else
			g_key_file_remove_key(keyfile, identifier, key, NULL);
		g_free(key);
		/* fall through */
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		return 0;
	}

	key = g_strdup_printf("%snetmask_prefixlen", prefix);
	if (ipconfig->address->prefixlen != 0)
		g_key_file_set_integer(keyfile, identifier,
				key, ipconfig->address->prefixlen);
	g_free(key);

	key = g_strdup_printf("%slocal_address", prefix);
	if (ipconfig->address->local)
		g_key_file_set_string(keyfile, identifier,
				key, ipconfig->address->local);
	g_free(key);

	key = g_strdup_printf("%speer_address", prefix);
	if (ipconfig->address->peer)
		g_key_file_set_string(keyfile, identifier,
				key, ipconfig->address->peer);
	g_free(key);

	key = g_strdup_printf("%sbroadcast_address", prefix);
	if (ipconfig->address->broadcast)
		g_key_file_set_string(keyfile, identifier,
			key, ipconfig->address->broadcast);
	g_free(key);

	key = g_strdup_printf("%sgateway", prefix);
	if (ipconfig->address->gateway)
		g_key_file_set_string(keyfile, identifier,
			key, ipconfig->address->gateway);
	g_free(key);

	return 0;
}

int __connman_ipconfig_init(void)
{
	DBG("");

	ipdevice_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, free_ipdevice);

	is_ipv6_supported = connman_inet_is_ipv6_supported();

	return 0;
}

void __connman_ipconfig_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(ipdevice_hash);
	ipdevice_hash = NULL;
}
