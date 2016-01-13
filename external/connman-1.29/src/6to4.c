/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2011  Nokia Corporation. All rights reserved.
 *  Copyright (C) Alexey Kuznetsov et al. from iproute2 package.
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
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "connman.h"
#include <connman/log.h>
#include <connman/ipconfig.h>
#include "gweb/gweb.h"

static int tunnel_created;
static int tunnel_pending;
static char *tunnel_ip_address;
static GWeb *web;
static guint web_request_id;
static unsigned int newlink_watch;
static unsigned int newlink_flags;
static int newlink_timeout_id;

#define STATUS_URL "http://ipv6.connman.net/online/status.html"

#ifndef IP_DF
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#endif

static int tunnel_create(struct in_addr *addr)
{
	struct ip_tunnel_parm p;
	struct ifreq ifr;
	int fd = -1;
	int ret;

	/* ip tunnel add tun6to4 mode sit remote any local 1.2.3.4 ttl 64 */

	memset(&p, 0, sizeof(struct ip_tunnel_parm));
	memset(&ifr, 0, sizeof(struct ifreq));

	p.iph.version = 4;
	p.iph.ihl = 5;
	p.iph.frag_off = htons(IP_DF);
	p.iph.protocol = IPPROTO_IPV6;
	p.iph.saddr = addr->s_addr;
	p.iph.ttl = 64;
	strncpy(p.name, "tun6to4", sizeof(p.name) - 1);

	strncpy(ifr.ifr_name, "sit0", sizeof(ifr.ifr_name) - 1);
	ifr.ifr_ifru.ifru_data = (void *)&p;
	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -errno;
	ret = ioctl(fd, SIOCADDTUNNEL, &ifr);
	if (ret)
		connman_error("add tunnel %s failed: %s", ifr.ifr_name,
							strerror(errno));
	close(fd);

	return -ret;
}

static void tunnel_destroy(void)
{
	struct ip_tunnel_parm p;
	struct ifreq ifr;
	int fd = -1;
	int ret;

	if (tunnel_created == 0)
		return;

	/* ip tunnel del tun6to4 */

	memset(&p, 0, sizeof(struct ip_tunnel_parm));
	memset(&ifr, 0, sizeof(struct ifreq));

	p.iph.version = 4;
	p.iph.ihl = 5;
	p.iph.protocol = IPPROTO_IPV6;
	strncpy(p.name, "tun6to4", sizeof(p.name) - 1);

	strncpy(ifr.ifr_name, "tun6to4", sizeof(ifr.ifr_name) - 1);
	ifr.ifr_ifru.ifru_data = (void *)&p;
	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		connman_error("socket failed: %s", strerror(errno));
		return;
	}

	ret = ioctl(fd, SIOCDELTUNNEL, &ifr);
	if (ret)
		connman_error("del tunnel %s failed: %s", ifr.ifr_name,
							strerror(errno));
	else
		tunnel_created = 0;

	tunnel_pending = 0;
	close(fd);

	g_free(tunnel_ip_address);
	tunnel_ip_address = NULL;
}

static int tunnel_add_route(void)
{
	struct __connman_inet_rtnl_handle rth;
	struct in6_addr addr6;
	int index;
	int ret = 0;

	/* ip -6 route add ::/0 via ::192.88.99.1 dev tun6to4 metric 1 */

	index = if_nametoindex("tun6to4");
	if (index == 0) {
		DBG("Can not find device tun6to4");
		return -1;
	}

	memset(&rth, 0, sizeof(rth));

	rth.req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	rth.req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	rth.req.n.nlmsg_type = RTM_NEWROUTE;
	rth.req.u.r.rt.rtm_family = AF_INET6;
	rth.req.u.r.rt.rtm_table = RT_TABLE_MAIN;
	rth.req.u.r.rt.rtm_protocol = RTPROT_BOOT;
	rth.req.u.r.rt.rtm_scope = RT_SCOPE_UNIVERSE;
	rth.req.u.r.rt.rtm_type = RTN_UNICAST;
	rth.req.u.r.rt.rtm_dst_len = 0;

	inet_pton(AF_INET6, "::192.88.99.1", &addr6);

	__connman_inet_rtnl_addattr_l(&rth.req.n, sizeof(rth.req), RTA_GATEWAY,
					&addr6.s6_addr, 16);
	__connman_inet_rtnl_addattr32(&rth.req.n, sizeof(rth.req), RTA_OIF,
					index);
	__connman_inet_rtnl_addattr32(&rth.req.n, sizeof(rth.req),
					RTA_PRIORITY, 1);

	ret = __connman_inet_rtnl_open(&rth);
	if (ret < 0)
		goto done;

	ret = __connman_inet_rtnl_send(&rth, &rth.req.n);

done:
	__connman_inet_rtnl_close(&rth);
	return ret;
}

static int tunnel_set_addr(unsigned int a, unsigned int b,
			unsigned int c, unsigned int d)
{
	struct __connman_inet_rtnl_handle rth;
	struct in6_addr addr6;
	char *ip6addr;
	int ret;

	/* ip -6 addr add dev tun6to4 2002:0102:0304::1/64 */

	memset(&rth, 0, sizeof(rth));

	rth.req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	rth.req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	rth.req.n.nlmsg_type = RTM_NEWADDR;
	rth.req.u.i.ifa.ifa_family = AF_INET6;
	rth.req.u.i.ifa.ifa_prefixlen = 64;
	rth.req.u.i.ifa.ifa_index = if_nametoindex("tun6to4");
	if (rth.req.u.i.ifa.ifa_index == 0) {
		connman_error("Can not find device tun6to4");
		ret = -1;
		goto done;
	}

	ip6addr = g_strdup_printf("2002:%02x%02x:%02x%02x::1", a, b, c, d);
	inet_pton(AF_INET6, ip6addr, &addr6);
	DBG("ipv6 address %s", ip6addr);
	g_free(ip6addr);

	__connman_inet_rtnl_addattr_l(&rth.req.n, sizeof(rth.req), IFA_LOCAL,
					&addr6.s6_addr, 16);
	__connman_inet_rtnl_addattr_l(&rth.req.n, sizeof(rth.req), IFA_ADDRESS,
					&addr6.s6_addr, 16);

	ret = __connman_inet_rtnl_open(&rth);
	if (ret < 0)
		goto done;

	ret = __connman_inet_rtnl_send(&rth, &rth.req.n);

done:
	__connman_inet_rtnl_close(&rth);
	return ret;
}

static gboolean unref_web(gpointer user_data)
{
	g_web_unref(web);
	return FALSE;
}

static bool web_result(GWebResult *result, gpointer user_data)
{
	guint16 status;

	if (web_request_id == 0)
		return false;

	status = g_web_result_get_status(result);

	DBG("status %u", status);

	if (status >= 400 && status < 500)
		tunnel_destroy();
	else
		tunnel_pending = 0;

	web_request_id = 0;

	g_timeout_add_seconds(1, unref_web, NULL);

	return false;
}

static void web_debug(const char *str, void *data)
{
	connman_info("%s: %s\n", (const char *) data, str);
}

static gboolean newlink_timeout(gpointer user_data)
{
	/*
	 * Stop if the timeout has been cancelled already by tun_newlink()
	 */
	if (newlink_timeout_id == 0)
		return FALSE;

	DBG("");

	if (newlink_watch != 0) {
		connman_rtnl_remove_watch(newlink_watch);
		newlink_watch = 0;
	}

	newlink_flags = 0;

	if (web_request_id == 0)
		tunnel_destroy();

	newlink_timeout_id = 0;

	return FALSE;
}

static void tun_newlink(unsigned flags, unsigned change, void *user_data)
{
	int index = GPOINTER_TO_INT(user_data);

	if ((newlink_flags & IFF_UP) == (flags & IFF_UP)) {
		newlink_flags = flags;
		return;
	}

	if (flags & IFF_UP) {
		/*
		 * We try to verify that connectivity through tunnel works ok.
		 */
		if (newlink_timeout_id > 0) {
			g_source_remove(newlink_timeout_id);
			newlink_timeout_id = 0;
		}

		web = g_web_new(index);
		if (!web) {
			tunnel_destroy();
			return;
		}

		g_web_set_accept(web, NULL);
		g_web_set_user_agent(web, "ConnMan/%s", VERSION);
		g_web_set_close_connection(web, TRUE);

		if (getenv("CONNMAN_WEB_DEBUG"))
			g_web_set_debug(web, web_debug, "6to4");

		web_request_id = g_web_request_get(web, STATUS_URL,
				web_result, NULL,  NULL);

		newlink_timeout(NULL);
	}

	newlink_flags = flags;
}

static int init_6to4(struct in_addr *ip4addr)
{
	unsigned int a, b, c, d;
	int ret, if_index;
	in_addr_t addr;

	DBG("");

	addr = ntohl(ip4addr->s_addr);

	a = (addr & 0xff000000) >> 24;
	b = (addr & 0x00ff0000) >> 16;
	c = (addr & 0x0000ff00) >> 8;
	d = addr & 0x000000ff;

	ret = tunnel_create(ip4addr);
	if (ret)
		return -1;

	tunnel_created = 1;

	ret = connman_inet_setup_tunnel("tun6to4", 1472);
	if (ret)
		goto error;

	ret = tunnel_set_addr(a, b, c, d);
	if (ret)
		goto error;

	ret = tunnel_add_route();
	if (ret)
		goto error;

	if_index = connman_inet_ifindex("tun6to4");
	if (if_index < 0)
		goto error;

	newlink_watch = connman_rtnl_add_newlink_watch(if_index,
				tun_newlink, GINT_TO_POINTER(if_index));

	newlink_timeout_id = g_timeout_add_seconds(1, newlink_timeout, NULL);

	return 0;

error:
	tunnel_destroy();
	return -1;
}

static void receive_rs_reply(struct nd_router_advert *reply,
			unsigned int length, void *user_data)
{
	char *address = user_data;
	struct in_addr ip4addr;

	DBG("reply %p len %d address %s", reply, length, address);

	/* We try to create tunnel if autoconfiguration did not work i.e.,
	 * we did not receive any reply to router solicitation message.
	 */
	if (!reply && inet_aton(address, &ip4addr) != 0)
		init_6to4(&ip4addr);

	g_free(address);
}

int __connman_6to4_probe(struct connman_service *service)
{
	struct connman_ipconfig *ip4config, *ip6config;
	enum connman_ipconfig_method method;
	unsigned int a, b;
	struct in_addr ip4addr;
	in_addr_t addr;
	const char *address;
	char *ip_address;
	int index;

	DBG("service %p", service);

	if (tunnel_created || tunnel_pending)
		return 0;

	if (!service)
		return -1;

	ip4config = __connman_service_get_ip4config(service);
	if (!ip4config)
		return -1;

	ip6config = __connman_service_get_ip6config(service);
	if (!ip6config)
		return -1;

	method = __connman_ipconfig_get_method(ip6config);
	if (method != CONNMAN_IPCONFIG_METHOD_AUTO)
		return -1;

	address = __connman_ipconfig_get_local(ip4config);
	if (!address)
		return -1;

	if (inet_aton(address, &ip4addr) == 0)
		return -1;

	addr = ntohl(ip4addr.s_addr);

	a = (addr & 0xff000000) >> 24;
	b = (addr & 0x00ff0000) >> 16;

	/* 6to4 tunnel is only usable if we have a public IPv4 address */
	if (a == 10 || (a == 192 && b == 168) ||
					(a == 172 && (b >= 16 && b <= 31)))
		return -1;

	index = __connman_ipconfig_get_index(ip4config);
	ip_address = g_strdup(address);
	tunnel_pending = 1;

	g_free(tunnel_ip_address);
	tunnel_ip_address = g_strdup(address);

	return __connman_inet_ipv6_send_rs(index, 2, receive_rs_reply,
							ip_address);
}

void __connman_6to4_remove(struct connman_ipconfig *ip4config)
{
	const char *address;

	DBG("tunnel ip address %s", tunnel_ip_address);

	if (!ip4config)
		return;

	address = __connman_ipconfig_get_local(ip4config);
	if (!address)
		return;

	if (g_strcmp0(address, tunnel_ip_address) != 0)
		return;

	if (tunnel_created)
		tunnel_destroy();
}

int __connman_6to4_check(struct connman_ipconfig *ip4config)
{
	const char *address;

	if (!ip4config || tunnel_created == 0 ||
					tunnel_pending == 1)
		return -1;

	DBG("tunnel ip address %s", tunnel_ip_address);

	address = __connman_ipconfig_get_local(ip4config);
	if (!address)
		return -1;

	if (g_strcmp0(address, tunnel_ip_address) == 0)
		return 1;

	return 0;
}
