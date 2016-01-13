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

#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <glib.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/utsname.h>
#include <connman/log.h>
#include <connman/inet.h>

static in_addr_t loopback_address;
static in_addr_t loopback_netmask;

static char system_hostname[HOST_NAME_MAX + 1];

static void create_hostname(void)
{
	const char *name = "localhost";

	if (sethostname(name, strlen(name)) < 0)
		connman_error("Failed to set hostname to %s", name);

	strncpy(system_hostname, name, HOST_NAME_MAX);
}

static int setup_hostname(void)
{
	char name[HOST_NAME_MAX + 1];

	memset(system_hostname, 0, sizeof(system_hostname));

	if (gethostname(system_hostname, HOST_NAME_MAX) < 0) {
		connman_error("Failed to get current hostname");
		return -EIO;
	}

	if (strlen(system_hostname) > 0 &&
				strcmp(system_hostname, "(none)") != 0)
		connman_info("System hostname is %s", system_hostname);
	else
		create_hostname();

	memset(name, 0, sizeof(name));

	if (getdomainname(name, HOST_NAME_MAX) < 0) {
		connman_error("Failed to get current domainname");
		return -EIO;
	}

	if (strlen(name) > 0 && strcmp(name, "(none)") != 0)
		connman_info("System domainname is %s", name);

	return 0;
}

static bool valid_loopback(int sk, struct ifreq *ifr)
{
	struct sockaddr_in *addr;
	int err;
	char buf[INET_ADDRSTRLEN];

	/* It is possible to end up in situations in which the
	 * loopback interface is up but has no valid address. In that
	 * case, we expect EADDRNOTAVAIL and should return FALSE.
	 */

	err = ioctl(sk, SIOCGIFADDR, ifr);
	if (err < 0) {
		err = -errno;
		connman_error("Getting address failed (%s)", strerror(-err));
		return err != -EADDRNOTAVAIL ? TRUE : FALSE;
	}

	addr = (struct sockaddr_in *) &ifr->ifr_addr;
	if (addr->sin_addr.s_addr != loopback_address) {
		connman_warn("Invalid loopback address %s",
			inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf)));
		return false;
	}

	err = ioctl(sk, SIOCGIFNETMASK, ifr);
	if (err < 0) {
		err = -errno;
		connman_error("Getting netmask failed (%s)", strerror(-err));
		return true;
	}

	addr = (struct sockaddr_in *) &ifr->ifr_netmask;
	if (addr->sin_addr.s_addr != loopback_netmask) {
		connman_warn("Invalid loopback netmask %s",
			inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf)));
		return false;
	}

	return true;
}

static int setup_loopback(void)
{
	struct ifreq ifr;
	struct sockaddr_in addr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sk < 0)
		return -errno;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "lo");

	if (ioctl(sk, SIOCGIFFLAGS, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	if (ifr.ifr_flags & IFF_UP) {
		connman_info("Checking loopback interface settings");
		if (valid_loopback(sk, &ifr)) {
			err = -EALREADY;
			goto done;
		}

		connman_warn("Correcting wrong loopback settings");
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = loopback_address;
	memcpy(&ifr.ifr_addr, &addr, sizeof(ifr.ifr_addr));

	err = ioctl(sk, SIOCSIFADDR, &ifr);
	if (err < 0) {
		err = -errno;
		connman_error("Setting address failed (%s)", strerror(-err));
		goto done;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = loopback_netmask;
	memcpy(&ifr.ifr_netmask, &addr, sizeof(ifr.ifr_netmask));

	err = ioctl(sk, SIOCSIFNETMASK, &ifr);
	if (err < 0) {
		err = -errno;
		connman_error("Setting netmask failed (%s)", strerror(-err));
		goto done;
	}

	if (ioctl(sk, SIOCGIFFLAGS, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	ifr.ifr_flags |= IFF_UP;

	if (ioctl(sk, SIOCSIFFLAGS, &ifr) < 0) {
		err = -errno;
		connman_error("Activating loopback interface failed (%s)",
							strerror(-err));
		goto done;
	}

done:
	close(sk);

	return err;
}

static const char *loopback_get_hostname(void)
{
	return system_hostname;
}

static int loopback_set_hostname(const char *hostname)
{
	const char *ptr;
	int err, len;

	if (g_strcmp0(hostname, "<hostname>") == 0)
		return 0;

	len = strlen(hostname);

	if (!connman_inet_check_hostname(hostname, len))
		return -EINVAL;

	if ((ptr = strstr(hostname, ".")))
		len = ptr - hostname;

	if (sethostname(hostname, len) < 0) {
		err = -errno;
		connman_error("Failed to set hostname to %s", hostname);
		return err;
	}

	connman_info("Setting hostname to %s", hostname);

	return 0;
}

static int loopback_set_domainname(const char *domainname)
{
	int err, len;

	len = strlen(domainname);

	if (!connman_inet_check_hostname(domainname, len))
		return -EINVAL;

	if (setdomainname(domainname, len) < 0) {
		err = -errno;
		connman_error("Failed to set domainname to %s", domainname);
		return err;
	}

	connman_info("Setting domainname to %s", domainname);

	return 0;
}

static struct connman_utsname_driver loopback_driver = {
	.name		= "loopback",
	.get_hostname	= loopback_get_hostname,
	.set_hostname	= loopback_set_hostname,
	.set_domainname	= loopback_set_domainname,
};

static int loopback_init(void)
{
	loopback_address = inet_addr("127.0.0.1");
	loopback_netmask = inet_addr("255.0.0.0");

	setup_loopback();

	setup_hostname();

	connman_utsname_driver_register(&loopback_driver);

	return 0;
}

static void loopback_exit(void)
{
	connman_utsname_driver_unregister(&loopback_driver);
}

CONNMAN_PLUGIN_DEFINE(loopback, "Loopback device plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_HIGH, loopback_init, loopback_exit)
