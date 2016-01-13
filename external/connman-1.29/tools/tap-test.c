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

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_tun.h>

static int inet_ifup(const char *ifname)
{
	struct ifreq ifr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sk < 0)
		return -errno;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

	if (ioctl(sk, SIOCGIFFLAGS, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	if (ifr.ifr_flags & IFF_UP) {
		err = -EALREADY;
		goto done;
	}

	ifr.ifr_flags |= IFF_UP;

	if (ioctl(sk, SIOCSIFFLAGS, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	err = 0;

done:
	close(sk);

	return err;
}

static int create_tap(const char *ifname)
{
	struct ifreq ifr;
	int fd, val;

	fd = open("/dev/net/tun", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("Failed to open TUN/TAP device");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

	if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) {
		perror("Failed to set TUN/TAP interface");
		close(fd);
		return -1;
	}

	val = ARPHRD_ETHER;

	if (ioctl(fd, TUNSETLINK, (unsigned long) val) < 0)
		perror("Failed to set TUN/TAP link type");

	return fd;
}

static void dump_packet(unsigned char *buf, int len)
{
	int i;

	printf("[");
	for (i = 0; i < len; i++) {
		printf(" %02x", buf[i]);
		if ((i + 1) % 16 == 0)
			printf("\n ");
		else if ((i + 1) % 8 == 0)
			printf(" ");
	}
	printf(" ]\n");
}

static void decode_ip(unsigned char *buf, int len)
{
	struct iphdr *ip = (void *) buf;

	printf("    IP proto %d saddr 0x%08x daddr 0x%08x\n",
					ip->protocol, ip->saddr, ip->daddr);
}

static void decode_packet(unsigned char *buf, int len)
{
	struct ether_header *eh = (void *) buf;
	char src[18], dst[18];
	uint16_t type;

	snprintf(dst, sizeof(dst), "%02x:%02x:%02x:%02x:%02x:%02x",
				eh->ether_dhost[0], eh->ether_dhost[1],
				eh->ether_dhost[2], eh->ether_dhost[3],
				eh->ether_dhost[4], eh->ether_dhost[5]);

	snprintf(src, sizeof(src), "%02x:%02x:%02x:%02x:%02x:%02x",
				eh->ether_shost[0], eh->ether_shost[1],
				eh->ether_shost[2], eh->ether_shost[3],
				eh->ether_shost[4], eh->ether_shost[5]);

	type = ntohs(eh->ether_type);

	printf("> type 0x%04x src %s dst %s <\n", type, src, dst);

	switch (type) {
	case ETHERTYPE_IP:
		decode_ip(buf + 14, len - 14);
		break;
	case ETHERTYPE_LOOPBACK:
		dump_packet(buf, len);
		break;
	}
}

int main(int argc, char *argv[])
{
	const char *ifname = "xxx";
	struct pollfd p;
	int fd;

	fd = create_tap(ifname);
	if (fd < 0)
		return 1;

	if (inet_ifup(ifname) < 0) {
		close(fd);
		return 1;
	}

	memset(&p, 0, sizeof(p));
	p.fd = fd;
	p.events = POLLHUP | POLLIN;

	while (1) {
		unsigned char buf[2048];
		int len;

		len = poll(&p, 1, -1);
		if (len < 0)
			break;
		if (len == 0)
			continue;

		len = read(fd, buf, sizeof(buf));
		if (len < 0)
			break;

		decode_packet(buf, len);
	}

	return 0;
}
