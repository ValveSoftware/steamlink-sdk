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

#ifndef __CONNMAN_IPADDRESS_H
#define __CONNMAN_IPADDRESS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:ipaddress
 * @title: IP address premitives
 * @short_description: Functions for IP address handling
 */

struct connman_ipaddress;

unsigned char connman_ipaddress_calc_netmask_len(const char *netmask);
struct connman_ipaddress *connman_ipaddress_alloc(int family);
void connman_ipaddress_free(struct connman_ipaddress *ipaddress);
int connman_ipaddress_set_ipv4(struct connman_ipaddress *ipaddress,
				const char *address, const char *netmask,
				const char *gateway);
int connman_ipaddress_set_ipv6(struct connman_ipaddress *ipaddress,
				const char *address,
				unsigned char prefix_length,
				const char *gateway);
int connman_ipaddress_get_ip(struct connman_ipaddress *ipaddress,
			const char **address, unsigned char *prefix_length);
void connman_ipaddress_set_peer(struct connman_ipaddress *ipaddress,
				const char *peer);
void connman_ipaddress_clear(struct connman_ipaddress *ipaddress);
void connman_ipaddress_copy_address(struct connman_ipaddress *ipaddress,
					struct connman_ipaddress *source);
struct connman_ipaddress *connman_ipaddress_copy(struct connman_ipaddress *ipaddress);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_IPADDRESS_H */
