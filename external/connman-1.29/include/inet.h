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

#ifndef __CONNMAN_INET_H
#define __CONNMAN_INET_H

#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <connman/device.h>
#include <connman/ipconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

int connman_inet_ifindex(const char *name);
char *connman_inet_ifname(int index);

int connman_inet_ifup(int index);
int connman_inet_ifdown(int index);

int connman_inet_set_address(int index, struct connman_ipaddress *ipaddress);
int connman_inet_clear_address(int index, struct connman_ipaddress *ipaddress);
int connman_inet_add_host_route(int index, const char *host, const char *gateway);
int connman_inet_del_host_route(int index, const char *host);
int connman_inet_add_network_route(int index, const char *host, const char *gateway,
					const char *netmask);
int connman_inet_del_network_route(int index, const char *host);
int connman_inet_clear_gateway_address(int index, const char *gateway);
int connman_inet_set_gateway_interface(int index);
int connman_inet_clear_gateway_interface(int index);
bool connman_inet_compare_subnet(int index, const char *host);
int connman_inet_set_ipv6_address(int index,
		struct connman_ipaddress *ipaddress);
int connman_inet_clear_ipv6_address(int index,
		const char *address, int prefix_len);
int connman_inet_add_ipv6_network_route(int index, const char *host,
					const char *gateway, unsigned char prefix_len);
int connman_inet_add_ipv6_host_route(int index, const char *host,
						const char *gateway);
int connman_inet_del_ipv6_network_route(int index, const char *host,
					unsigned char prefix_len);
int connman_inet_del_ipv6_host_route(int index, const char *host);
int connman_inet_clear_ipv6_gateway_address(int index, const char *gateway);
int connman_inet_set_ipv6_gateway_interface(int index);
int connman_inet_clear_ipv6_gateway_interface(int index);

int connman_inet_add_to_bridge(int index, const char *bridge);
int connman_inet_remove_from_bridge(int index, const char *bridge);

int connman_inet_set_mtu(int index, int mtu);
int connman_inet_setup_tunnel(char *tunnel, int mtu);
int connman_inet_create_tunnel(char **iface);
int connman_inet_get_dest_addr(int index, char **dest);
int connman_inet_ipv6_get_dest_addr(int index, char **dest);
int connman_inet_check_ipaddress(const char *host);
bool connman_inet_check_hostname(const char *ptr, size_t len);
bool connman_inet_is_ipv6_supported();

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_INET_H */
