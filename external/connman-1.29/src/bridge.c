/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
 *  Copyright (C) 2012-2014  BMW Car IT GmbH.
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

#include "connman.h"

static int set_forward_delay(const char *name, unsigned int delay)
{
	FILE *f;
	char *forward_delay_path;

	forward_delay_path =
		g_strdup_printf("/sys/class/net/%s/bridge/forward_delay", name);

	if (!forward_delay_path)
		return -ENOMEM;

	f = fopen(forward_delay_path, "r+");

	g_free(forward_delay_path);

	if (!f)
		return -errno;

	fprintf(f, "%d", delay);

	fclose(f);

	return 0;
}

int __connman_bridge_create(const char *name)
{
	int sk, err;

	DBG("name %s", name);

	sk = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sk < 0)
		return -EOPNOTSUPP;

	if (ioctl(sk, SIOCBRADDBR, name) == -1) {
		err = -errno;
		if (err != -EEXIST) {
			close(sk);
			return -EOPNOTSUPP;
		}
	}

	err = set_forward_delay(name, 0);

	if (err < 0)
		ioctl(sk, SIOCBRDELBR, name);

	close(sk);

	return err;
}

int __connman_bridge_remove(const char *name)
{
	int sk, err;

	DBG("name %s", name);

	sk = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sk < 0)
		return -EOPNOTSUPP;

	err = ioctl(sk, SIOCBRDELBR, name);

	close(sk);

	if (err < 0)
		return -EOPNOTSUPP;

	return 0;
}

int __connman_bridge_enable(const char *name, const char *ip_address,
			int prefix_len, const char *broadcast)
{
	int err, index;

	index = connman_inet_ifindex(name);
	if (index < 0)
		return index;

	err = __connman_inet_modify_address(RTM_NEWADDR,
				NLM_F_REPLACE | NLM_F_ACK, index, AF_INET,
				ip_address, NULL, prefix_len, broadcast);
	if (err < 0)
		return err;

	return connman_inet_ifup(index);
}

int __connman_bridge_disable(const char *name)
{
	int index;

	index = connman_inet_ifindex(name);
	if (index < 0)
		return index;

	return connman_inet_ifdown(index);
}
