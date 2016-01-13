/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
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
#include <arpa/inet.h>

static unsigned char netmask2prefixlen(const char *netmask)
{
	unsigned char bits = 0;
	in_addr_t mask = inet_network(netmask);
	in_addr_t host = ~mask;

	/* a valid netmask must be 2^n - 1 */
	if ((host & (host + 1)) != 0)
		return -1;

	for (; mask; mask <<= 1)
		++bits;

	return bits;
}

int main(int argc, char *argv[])
{
	const char *masks[] = { "255.255.255.255", "255.255.255.0",
						"255.255.0.0", "255.0.0.0" };
	struct in_addr addr;
	int i, len;

	for (i = 0; i < 4; i++) {
		printf("subnet %-16s  prefixlen %u\n", masks[i],
						netmask2prefixlen(masks[i]));
	}

	printf("\n");

	for (i = 0; i < 4; i++) {
		len = (i + 1) * 8;
		addr.s_addr = htonl(~(0xfffffffflu >> len));
		printf("prefixlen %-2u  netmask %s\n", len, inet_ntoa(addr));
	}

	return 0;
}
