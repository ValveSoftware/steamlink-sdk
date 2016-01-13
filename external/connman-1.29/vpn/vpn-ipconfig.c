/*
 *
 *  ConnMan VPN daemon
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
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
#include <stdint.h>
#include <arpa/inet.h>

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP	0x10000
#endif

#include <gdbus.h>

#include "../src/connman.h"

#include "vpn.h"

struct vpn_ipconfig {
	int refcount;
	int index;
	int family;
	bool enabled;
	struct connman_ipaddress *address;
	struct connman_ipaddress *system;
};

struct vpn_ipdevice {
	int index;
	char *ifname;
	unsigned short type;
	unsigned int flags;
	char *address;
	uint16_t mtu;

	GSList *address_list;
	char *ipv4_gateway;
	char *ipv6_gateway;

	char *pac;
};

static GHashTable *ipdevice_hash = NULL;

struct connman_ipaddress *
__vpn_ipconfig_get_address(struct vpn_ipconfig *ipconfig)
{
	if (!ipconfig)
		return NULL;

	return ipconfig->address;
}

const char *__vpn_ipconfig_get_peer(struct vpn_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return NULL;

	return ipconfig->address->peer;
}

unsigned short __vpn_ipconfig_get_type_from_index(int index)
{
	struct vpn_ipdevice *ipdevice;

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return ARPHRD_VOID;

	return ipdevice->type;
}

unsigned int __vpn_ipconfig_get_flags_from_index(int index)
{
	struct vpn_ipdevice *ipdevice;

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return 0;

	return ipdevice->flags;
}

void __vpn_ipconfig_foreach(void (*function) (int index,
					void *user_data), void *user_data)
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

void __vpn_ipconfig_set_local(struct vpn_ipconfig *ipconfig,
							const char *address)
{
	if (!ipconfig->address)
		return;

	g_free(ipconfig->address->local);
	ipconfig->address->local = g_strdup(address);
}

const char *__vpn_ipconfig_get_local(struct vpn_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return NULL;

	return ipconfig->address->local;
}

void __vpn_ipconfig_set_peer(struct vpn_ipconfig *ipconfig,
							const char *address)
{
	if (!ipconfig->address)
		return;

	g_free(ipconfig->address->peer);
	ipconfig->address->peer = g_strdup(address);
}

void __vpn_ipconfig_set_broadcast(struct vpn_ipconfig *ipconfig,
					const char *broadcast)
{
	if (!ipconfig->address)
		return;

	g_free(ipconfig->address->broadcast);
	ipconfig->address->broadcast = g_strdup(broadcast);
}

void __vpn_ipconfig_set_gateway(struct vpn_ipconfig *ipconfig,
							const char *gateway)
{
	DBG("");

	if (!ipconfig->address)
		return;
	g_free(ipconfig->address->gateway);
	ipconfig->address->gateway = g_strdup(gateway);
}

const char *
__vpn_ipconfig_get_gateway(struct vpn_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return NULL;

	return ipconfig->address->gateway;
}

void __vpn_ipconfig_set_prefixlen(struct vpn_ipconfig *ipconfig,
					unsigned char prefixlen)
{
	if (!ipconfig->address)
		return;

	ipconfig->address->prefixlen = prefixlen;
}

unsigned char
__vpn_ipconfig_get_prefixlen(struct vpn_ipconfig *ipconfig)
{
	if (!ipconfig->address)
		return 0;

	return ipconfig->address->prefixlen;
}

int __vpn_ipconfig_address_add(struct vpn_ipconfig *ipconfig, int family)
{
	DBG("ipconfig %p family %d", ipconfig, family);

	if (!ipconfig)
		return -EINVAL;

	if (family == AF_INET)
		return connman_inet_set_address(ipconfig->index,
						ipconfig->address);
	else if (family == AF_INET6)
		return connman_inet_set_ipv6_address(ipconfig->index,
							ipconfig->address);

	return 0;
}

int __vpn_ipconfig_gateway_add(struct vpn_ipconfig *ipconfig, int family)
{
	DBG("ipconfig %p family %d", ipconfig, family);

	if (!ipconfig || !ipconfig->address)
		return -EINVAL;

	DBG("family %d gw %s peer %s", family,
		ipconfig->address->gateway, ipconfig->address->peer);

	if (family == AF_INET)
		connman_inet_add_host_route(ipconfig->index,
					ipconfig->address->gateway,
					ipconfig->address->peer);
	else if (family == AF_INET6)
		connman_inet_add_ipv6_host_route(ipconfig->index,
						ipconfig->address->gateway,
						ipconfig->address->peer);
	else
		return -EINVAL;

	return 0;
}

void __vpn_ipconfig_unref_debug(struct vpn_ipconfig *ipconfig,
				const char *file, int line, const char *caller)
{
	if (!ipconfig)
		return;

	DBG("%p ref %d by %s:%d:%s()", ipconfig, ipconfig->refcount - 1,
		file, line, caller);

	if (__sync_fetch_and_sub(&ipconfig->refcount, 1) != 1)
		return;

	connman_ipaddress_free(ipconfig->system);
	connman_ipaddress_free(ipconfig->address);
	g_free(ipconfig);
}

static struct vpn_ipconfig *create_ipv6config(int index)
{
	struct vpn_ipconfig *ipv6config;

	DBG("index %d", index);

	ipv6config = g_try_new0(struct vpn_ipconfig, 1);
	if (!ipv6config)
		return NULL;

	ipv6config->refcount = 1;

	ipv6config->index = index;
	ipv6config->enabled = false;
	ipv6config->family = AF_INET6;

	ipv6config->address = connman_ipaddress_alloc(AF_INET6);
	if (!ipv6config->address) {
		g_free(ipv6config);
		return NULL;
	}

	ipv6config->system = connman_ipaddress_alloc(AF_INET6);

	DBG("ipconfig %p", ipv6config);

	return ipv6config;
}

struct vpn_ipconfig *__vpn_ipconfig_create(int index, int family)
{
	struct vpn_ipconfig *ipconfig;

	if (family == AF_INET6)
		return create_ipv6config(index);

	DBG("index %d", index);

	ipconfig = g_try_new0(struct vpn_ipconfig, 1);
	if (!ipconfig)
		return NULL;

	ipconfig->refcount = 1;

	ipconfig->index = index;
	ipconfig->enabled = false;
	ipconfig->family = family;

	ipconfig->address = connman_ipaddress_alloc(AF_INET);
	if (!ipconfig->address) {
		g_free(ipconfig);
		return NULL;
	}

	ipconfig->system = connman_ipaddress_alloc(AF_INET);

	DBG("ipconfig %p", ipconfig);

	return ipconfig;
}

void __vpn_ipconfig_set_index(struct vpn_ipconfig *ipconfig, int index)
{
	ipconfig->index = index;
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

void __vpn_ipconfig_newlink(int index, unsigned short type,
				unsigned int flags,
				const char *address,
				unsigned short mtu,
				struct rtnl_link_stats *stats)
{
	struct vpn_ipdevice *ipdevice;
	GString *str;

	if (type == ARPHRD_LOOPBACK)
		return;

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (ipdevice)
		goto update;

	ipdevice = g_try_new0(struct vpn_ipdevice, 1);
	if (!ipdevice)
		return;

	ipdevice->index = index;
	ipdevice->ifname = connman_inet_ifname(index);
	ipdevice->type = type;

	ipdevice->address = g_strdup(address);

	g_hash_table_insert(ipdevice_hash, GINT_TO_POINTER(index), ipdevice);

	connman_info("%s {create} index %d type %d <%s>", ipdevice->ifname,
						index, type, type2str(type));

update:
	ipdevice->mtu = mtu;

	if (flags == ipdevice->flags)
		return;

	ipdevice->flags = flags;

	str = g_string_new(NULL);
	if (!str)
		return;

	if (flags & IFF_UP)
		g_string_append(str, "UP");
	else
		g_string_append(str, "DOWN");

	if (flags & IFF_RUNNING)
		g_string_append(str, ",RUNNING");

	if (flags & IFF_LOWER_UP)
		g_string_append(str, ",LOWER_UP");

	connman_info("%s {update} flags %u <%s>", ipdevice->ifname,
							flags, str->str);

	g_string_free(str, TRUE);
}

void __vpn_ipconfig_dellink(int index, struct rtnl_link_stats *stats)
{
	struct vpn_ipdevice *ipdevice;

	DBG("index %d", index);

	ipdevice = g_hash_table_lookup(ipdevice_hash, GINT_TO_POINTER(index));
	if (!ipdevice)
		return;

	g_hash_table_remove(ipdevice_hash, GINT_TO_POINTER(index));
}

static void free_ipdevice(gpointer data)
{
	struct vpn_ipdevice *ipdevice = data;

	connman_info("%s {remove} index %d", ipdevice->ifname,
							ipdevice->index);

	g_free(ipdevice->ipv4_gateway);
	g_free(ipdevice->ipv6_gateway);
	g_free(ipdevice->pac);

	g_free(ipdevice->address);

	g_free(ipdevice->ifname);
	g_free(ipdevice);
}

int __vpn_ipconfig_init(void)
{
	DBG("");

	ipdevice_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, free_ipdevice);

	return 0;
}

void __vpn_ipconfig_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(ipdevice_hash);
	ipdevice_hash = NULL;
}
