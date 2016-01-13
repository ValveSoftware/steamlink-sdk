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
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include <connman/ipaddress.h>

#include "connman.h"

unsigned char connman_ipaddress_calc_netmask_len(const char *netmask)
{
	unsigned char bits;
	in_addr_t mask;
	in_addr_t host;

	if (!netmask)
		return 32;

	mask = inet_network(netmask);
	host = ~mask;

	/* a valid netmask must be 2^n - 1 */
	if ((host & (host + 1)) != 0)
		return -1;

	bits = 0;
	for (; mask; mask <<= 1)
		++bits;

	return bits;
}

struct connman_ipaddress *connman_ipaddress_alloc(int family)
{
	struct connman_ipaddress *ipaddress;

	ipaddress = g_try_new0(struct connman_ipaddress, 1);
	if (!ipaddress)
		return NULL;

	ipaddress->family = family;
	ipaddress->prefixlen = 0;
	ipaddress->local = NULL;
	ipaddress->peer = NULL;
	ipaddress->broadcast = NULL;
	ipaddress->gateway = NULL;

	return ipaddress;
}

void connman_ipaddress_free(struct connman_ipaddress *ipaddress)
{
	if (!ipaddress)
		return;

	g_free(ipaddress->broadcast);
	g_free(ipaddress->peer);
	g_free(ipaddress->local);
	g_free(ipaddress->gateway);
	g_free(ipaddress);
}

static bool check_ipv6_address(const char *address)
{
	unsigned char buf[sizeof(struct in6_addr)];
	int err;

	if (!address)
		return false;

	err = inet_pton(AF_INET6, address, buf);
	if (err > 0)
		return true;

	return false;
}

int connman_ipaddress_set_ipv6(struct connman_ipaddress *ipaddress,
				const char *address,
				unsigned char prefix_length,
				const char *gateway)
{
	if (!ipaddress)
		return -EINVAL;

	if (!check_ipv6_address(address))
		return -EINVAL;

	DBG("prefix_len %d address %s gateway %s",
			prefix_length, address, gateway);

	ipaddress->family = AF_INET6;

	ipaddress->prefixlen = prefix_length;

	g_free(ipaddress->local);
	ipaddress->local = g_strdup(address);

	g_free(ipaddress->gateway);
	ipaddress->gateway = g_strdup(gateway);

	return 0;
}

int connman_ipaddress_get_ip(struct connman_ipaddress *ipaddress,
			const char **address,
			unsigned char *netmask_prefix_length)
{
	if (!ipaddress)
		return -EINVAL;

	*netmask_prefix_length = ipaddress->prefixlen;
	*address = ipaddress->local;

	return 0;
}

int connman_ipaddress_set_ipv4(struct connman_ipaddress *ipaddress,
		const char *address, const char *netmask, const char *gateway)
{
	if (!ipaddress)
		return -EINVAL;

	ipaddress->family = AF_INET;

	ipaddress->prefixlen = connman_ipaddress_calc_netmask_len(netmask);

	g_free(ipaddress->local);
	ipaddress->local = g_strdup(address);

	g_free(ipaddress->gateway);
	ipaddress->gateway = g_strdup(gateway);

	return 0;
}

void connman_ipaddress_set_peer(struct connman_ipaddress *ipaddress,
				const char *peer)
{
	if (!ipaddress)
		return;

	g_free(ipaddress->peer);
	ipaddress->peer = g_strdup(peer);
}

void connman_ipaddress_clear(struct connman_ipaddress *ipaddress)
{
	if (!ipaddress)
		return;

	ipaddress->prefixlen = 0;

	g_free(ipaddress->local);
	ipaddress->local = NULL;

	g_free(ipaddress->peer);
	ipaddress->peer = NULL;

	g_free(ipaddress->broadcast);
	ipaddress->broadcast = NULL;

	g_free(ipaddress->gateway);
	ipaddress->gateway = NULL;
}

/*
 * Note that this copy function only copies the actual address and
 * prefixlen. Use the other copy function to copy the whole struct.
 */
void connman_ipaddress_copy_address(struct connman_ipaddress *ipaddress,
					struct connman_ipaddress *source)
{
	if (!ipaddress || !source)
		return;

	ipaddress->family = source->family;
	ipaddress->prefixlen = source->prefixlen;

	g_free(ipaddress->local);
	ipaddress->local = g_strdup(source->local);
}

struct connman_ipaddress *
connman_ipaddress_copy(struct connman_ipaddress *ipaddress)
{
	struct connman_ipaddress *copy;

	if (!ipaddress)
		return NULL;

	copy = g_new0(struct connman_ipaddress, 1);

	copy->family = ipaddress->family;
	copy->prefixlen = ipaddress->prefixlen;
	copy->local = g_strdup(ipaddress->local);
	copy->peer = g_strdup(ipaddress->peer);
	copy->broadcast = g_strdup(ipaddress->broadcast);
	copy->gateway = g_strdup(ipaddress->gateway);

	return copy;
}
