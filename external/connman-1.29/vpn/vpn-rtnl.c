/*
 *
 *  ConnMan VPN daemon
 *
 *  Copyright (C) 2012-2013  Intel Corporation. All rights reserved.
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
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netinet/icmp6.h>
#include <net/if_arp.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>

#include <glib.h>

#include <connman/log.h>

#include "vpn.h"

#include "vpn-rtnl.h"

#ifndef ARPHDR_PHONET_PIPE
#define ARPHDR_PHONET_PIPE (821)
#endif

#define print(arg...) do { if (0) connman_info(arg); } while (0)
#define debug(arg...) do { if (0) DBG(arg); } while (0)

struct watch_data {
	unsigned int id;
	int index;
	vpn_rtnl_link_cb_t newlink;
	void *user_data;
};

static GSList *watch_list = NULL;
static unsigned int watch_id = 0;

static GSList *update_list = NULL;
static guint update_interval = G_MAXUINT;
static guint update_timeout = 0;

struct interface_data {
	int index;
	char *ident;
};

static GHashTable *interface_list = NULL;

static void free_interface(gpointer data)
{
	struct interface_data *interface = data;

	g_free(interface->ident);
	g_free(interface);
}

/**
 * vpn_rtnl_add_newlink_watch:
 * @index: network device index
 * @callback: callback function
 * @user_data: callback data;
 *
 * Add a new RTNL watch for newlink events
 *
 * Returns: %0 on failure and a unique id on success
 */
unsigned int vpn_rtnl_add_newlink_watch(int index,
			vpn_rtnl_link_cb_t callback, void *user_data)
{
	struct watch_data *watch;

	watch = g_try_new0(struct watch_data, 1);
	if (!watch)
		return 0;

	watch->id = ++watch_id;
	watch->index = index;

	watch->newlink = callback;
	watch->user_data = user_data;

	watch_list = g_slist_prepend(watch_list, watch);

	DBG("id %d", watch->id);

	if (callback) {
		unsigned int flags = __vpn_ipconfig_get_flags_from_index(index);

		if (flags > 0)
			callback(flags, 0, user_data);
	}

	return watch->id;
}

/**
 * vpn_rtnl_remove_watch:
 * @id: watch identifier
 *
 * Remove the RTNL watch for the identifier
 */
void vpn_rtnl_remove_watch(unsigned int id)
{
	GSList *list;

	DBG("id %d", id);

	if (id == 0)
		return;

	for (list = watch_list; list; list = list->next) {
		struct watch_data *watch = list->data;

		if (watch->id  == id) {
			watch_list = g_slist_remove(watch_list, watch);
			g_free(watch);
			break;
		}
	}
}

static void trigger_rtnl(int index, void *user_data)
{
	struct vpn_rtnl *rtnl = user_data;

	if (rtnl->newlink) {
		unsigned short type = __vpn_ipconfig_get_type_from_index(index);
		unsigned int flags = __vpn_ipconfig_get_flags_from_index(index);

		rtnl->newlink(type, index, flags, 0);
	}
}

static GSList *rtnl_list = NULL;

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct vpn_rtnl *rtnl1 = a;
	const struct vpn_rtnl *rtnl2 = b;

	return rtnl2->priority - rtnl1->priority;
}

/**
 * vpn_rtnl_register:
 * @rtnl: RTNL module
 *
 * Register a new RTNL module
 *
 * Returns: %0 on success
 */
int vpn_rtnl_register(struct vpn_rtnl *rtnl)
{
	DBG("rtnl %p name %s", rtnl, rtnl->name);

	rtnl_list = g_slist_insert_sorted(rtnl_list, rtnl,
							compare_priority);

	__vpn_ipconfig_foreach(trigger_rtnl, rtnl);

	return 0;
}

/**
 * vpn_rtnl_unregister:
 * @rtnl: RTNL module
 *
 * Remove a previously registered RTNL module
 */
void vpn_rtnl_unregister(struct vpn_rtnl *rtnl)
{
	DBG("rtnl %p name %s", rtnl, rtnl->name);

	rtnl_list = g_slist_remove(rtnl_list, rtnl);
}

static const char *operstate2str(unsigned char operstate)
{
	switch (operstate) {
	case IF_OPER_UNKNOWN:
		return "UNKNOWN";
	case IF_OPER_NOTPRESENT:
		return "NOT-PRESENT";
	case IF_OPER_DOWN:
		return "DOWN";
	case IF_OPER_LOWERLAYERDOWN:
		return "LOWER-LAYER-DOWN";
	case IF_OPER_TESTING:
		return "TESTING";
	case IF_OPER_DORMANT:
		return "DORMANT";
	case IF_OPER_UP:
		return "UP";
	}

	return "";
}

static void extract_link(struct ifinfomsg *msg, int bytes,
				struct ether_addr *address, const char **ifname,
				unsigned int *mtu, unsigned char *operstate,
						struct rtnl_link_stats *stats)
{
	struct rtattr *attr;

	for (attr = IFLA_RTA(msg); RTA_OK(attr, bytes);
					attr = RTA_NEXT(attr, bytes)) {
		switch (attr->rta_type) {
		case IFLA_ADDRESS:
			if (address)
				memcpy(address, RTA_DATA(attr), ETH_ALEN);
			break;
		case IFLA_IFNAME:
			if (ifname)
				*ifname = RTA_DATA(attr);
			break;
		case IFLA_MTU:
			if (mtu)
				*mtu = *((unsigned int *) RTA_DATA(attr));
			break;
		case IFLA_STATS:
			if (stats)
				memcpy(stats, RTA_DATA(attr),
					sizeof(struct rtnl_link_stats));
			break;
		case IFLA_OPERSTATE:
			if (operstate)
				*operstate = *((unsigned char *) RTA_DATA(attr));
			break;
		case IFLA_LINKMODE:
			break;
		}
	}
}

static void process_newlink(unsigned short type, int index, unsigned flags,
			unsigned change, struct ifinfomsg *msg, int bytes)
{
	struct ether_addr address = {{ 0, 0, 0, 0, 0, 0 }};
	struct ether_addr compare = {{ 0, 0, 0, 0, 0, 0 }};
	struct rtnl_link_stats stats;
	unsigned char operstate = 0xff;
	struct interface_data *interface;
	const char *ifname = NULL;
	unsigned int mtu = 0;
	char ident[13], str[18];
	GSList *list;

	memset(&stats, 0, sizeof(stats));
	extract_link(msg, bytes, &address, &ifname, &mtu, &operstate, &stats);

	snprintf(ident, 13, "%02x%02x%02x%02x%02x%02x",
						address.ether_addr_octet[0],
						address.ether_addr_octet[1],
						address.ether_addr_octet[2],
						address.ether_addr_octet[3],
						address.ether_addr_octet[4],
						address.ether_addr_octet[5]);

	snprintf(str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
						address.ether_addr_octet[0],
						address.ether_addr_octet[1],
						address.ether_addr_octet[2],
						address.ether_addr_octet[3],
						address.ether_addr_octet[4],
						address.ether_addr_octet[5]);

	switch (type) {
	case ARPHRD_ETHER:
	case ARPHRD_LOOPBACK:
	case ARPHDR_PHONET_PIPE:
	case ARPHRD_NONE:
		__vpn_ipconfig_newlink(index, type, flags,
							str, mtu, &stats);
		break;
	}

	if (memcmp(&address, &compare, ETH_ALEN) != 0)
		connman_info("%s {newlink} index %d address %s mtu %u",
						ifname, index, str, mtu);

	if (operstate != 0xff)
		connman_info("%s {newlink} index %d operstate %u <%s>",
						ifname, index, operstate,
						operstate2str(operstate));

	interface = g_hash_table_lookup(interface_list, GINT_TO_POINTER(index));
	if (!interface) {
		interface = g_new0(struct interface_data, 1);
		interface->index = index;
		interface->ident = g_strdup(ident);

		g_hash_table_insert(interface_list,
					GINT_TO_POINTER(index), interface);
	}

	for (list = rtnl_list; list; list = list->next) {
		struct vpn_rtnl *rtnl = list->data;

		if (rtnl->newlink)
			rtnl->newlink(type, index, flags, change);
	}

	for (list = watch_list; list; list = list->next) {
		struct watch_data *watch = list->data;

		if (watch->index != index)
			continue;

		if (watch->newlink)
			watch->newlink(flags, change, watch->user_data);
	}
}

static void process_dellink(unsigned short type, int index, unsigned flags,
			unsigned change, struct ifinfomsg *msg, int bytes)
{
	struct rtnl_link_stats stats;
	unsigned char operstate = 0xff;
	const char *ifname = NULL;
	GSList *list;

	memset(&stats, 0, sizeof(stats));
	extract_link(msg, bytes, NULL, &ifname, NULL, &operstate, &stats);

	if (operstate != 0xff)
		connman_info("%s {dellink} index %d operstate %u <%s>",
						ifname, index, operstate,
						operstate2str(operstate));

	for (list = rtnl_list; list; list = list->next) {
		struct vpn_rtnl *rtnl = list->data;

		if (rtnl->dellink)
			rtnl->dellink(type, index, flags, change);
	}

	switch (type) {
	case ARPHRD_ETHER:
	case ARPHRD_LOOPBACK:
	case ARPHRD_NONE:
		__vpn_ipconfig_dellink(index, &stats);
		break;
	}

	g_hash_table_remove(interface_list, GINT_TO_POINTER(index));
}

static void extract_ipv4_route(struct rtmsg *msg, int bytes, int *index,
						struct in_addr *dst,
						struct in_addr *gateway)
{
	struct rtattr *attr;

	for (attr = RTM_RTA(msg); RTA_OK(attr, bytes);
					attr = RTA_NEXT(attr, bytes)) {
		switch (attr->rta_type) {
		case RTA_DST:
			if (dst)
				*dst = *((struct in_addr *) RTA_DATA(attr));
			break;
		case RTA_GATEWAY:
			if (gateway)
				*gateway = *((struct in_addr *) RTA_DATA(attr));
			break;
		case RTA_OIF:
			if (index)
				*index = *((int *) RTA_DATA(attr));
			break;
		}
	}
}

static void extract_ipv6_route(struct rtmsg *msg, int bytes, int *index,
						struct in6_addr *dst,
						struct in6_addr *gateway)
{
	struct rtattr *attr;

	for (attr = RTM_RTA(msg); RTA_OK(attr, bytes);
					attr = RTA_NEXT(attr, bytes)) {
		switch (attr->rta_type) {
		case RTA_DST:
			if (dst)
				*dst = *((struct in6_addr *) RTA_DATA(attr));
			break;
		case RTA_GATEWAY:
			if (gateway)
				*gateway =
					*((struct in6_addr *) RTA_DATA(attr));
			break;
		case RTA_OIF:
			if (index)
				*index = *((int *) RTA_DATA(attr));
			break;
		}
	}
}

static void process_newroute(unsigned char family, unsigned char scope,
						struct rtmsg *msg, int bytes)
{
	GSList *list;
	char dststr[INET6_ADDRSTRLEN], gatewaystr[INET6_ADDRSTRLEN];
	int index = -1;

	if (family == AF_INET) {
		struct in_addr dst = { INADDR_ANY }, gateway = { INADDR_ANY };

		extract_ipv4_route(msg, bytes, &index, &dst, &gateway);

		inet_ntop(family, &dst, dststr, sizeof(dststr));
		inet_ntop(family, &gateway, gatewaystr, sizeof(gatewaystr));

		/* skip host specific routes */
		if (scope != RT_SCOPE_UNIVERSE &&
			!(scope == RT_SCOPE_LINK && dst.s_addr == INADDR_ANY))
			return;

		if (dst.s_addr != INADDR_ANY)
			return;

	} else if (family == AF_INET6) {
		struct in6_addr dst = IN6ADDR_ANY_INIT,
				gateway = IN6ADDR_ANY_INIT;

		extract_ipv6_route(msg, bytes, &index, &dst, &gateway);

		inet_ntop(family, &dst, dststr, sizeof(dststr));
		inet_ntop(family, &gateway, gatewaystr, sizeof(gatewaystr));

		/* skip host specific routes */
		if (scope != RT_SCOPE_UNIVERSE &&
			!(scope == RT_SCOPE_LINK &&
				IN6_IS_ADDR_UNSPECIFIED(&dst)))
			return;

		if (!IN6_IS_ADDR_UNSPECIFIED(&dst))
			return;
	} else
		return;

	for (list = rtnl_list; list; list = list->next) {
		struct vpn_rtnl *rtnl = list->data;

		if (rtnl->newgateway)
			rtnl->newgateway(index, gatewaystr);
	}
}

static void process_delroute(unsigned char family, unsigned char scope,
						struct rtmsg *msg, int bytes)
{
	GSList *list;
	char dststr[INET6_ADDRSTRLEN], gatewaystr[INET6_ADDRSTRLEN];
	int index = -1;

	if (family == AF_INET) {
		struct in_addr dst = { INADDR_ANY }, gateway = { INADDR_ANY };

		extract_ipv4_route(msg, bytes, &index, &dst, &gateway);

		inet_ntop(family, &dst, dststr, sizeof(dststr));
		inet_ntop(family, &gateway, gatewaystr, sizeof(gatewaystr));

		/* skip host specific routes */
		if (scope != RT_SCOPE_UNIVERSE &&
			!(scope == RT_SCOPE_LINK && dst.s_addr == INADDR_ANY))
			return;

		if (dst.s_addr != INADDR_ANY)
			return;

	}  else if (family == AF_INET6) {
		struct in6_addr dst = IN6ADDR_ANY_INIT,
				gateway = IN6ADDR_ANY_INIT;

		extract_ipv6_route(msg, bytes, &index, &dst, &gateway);

		inet_ntop(family, &dst, dststr, sizeof(dststr));
		inet_ntop(family, &gateway, gatewaystr, sizeof(gatewaystr));

		/* skip host specific routes */
		if (scope != RT_SCOPE_UNIVERSE &&
			!(scope == RT_SCOPE_LINK &&
				IN6_IS_ADDR_UNSPECIFIED(&dst)))
			return;

		if (!IN6_IS_ADDR_UNSPECIFIED(&dst))
			return;
	} else
		return;

	for (list = rtnl_list; list; list = list->next) {
		struct vpn_rtnl *rtnl = list->data;

		if (rtnl->delgateway)
			rtnl->delgateway(index, gatewaystr);
	}
}

static inline void print_ether(struct rtattr *attr, const char *name)
{
	int len = (int) RTA_PAYLOAD(attr);

	if (len == ETH_ALEN) {
		struct ether_addr eth;
		memcpy(&eth, RTA_DATA(attr), ETH_ALEN);
		print("  attr %s (len %d) %s\n", name, len, ether_ntoa(&eth));
	} else
		print("  attr %s (len %d)\n", name, len);
}

static inline void print_inet(struct rtattr *attr, const char *name,
							unsigned char family)
{
	int len = (int) RTA_PAYLOAD(attr);

	if (family == AF_INET && len == sizeof(struct in_addr)) {
		struct in_addr addr;
		addr = *((struct in_addr *) RTA_DATA(attr));
		print("  attr %s (len %d) %s\n", name, len, inet_ntoa(addr));
	} else
		print("  attr %s (len %d)\n", name, len);
}

static inline void print_string(struct rtattr *attr, const char *name)
{
	print("  attr %s (len %d) %s\n", name, (int) RTA_PAYLOAD(attr),
						(char *) RTA_DATA(attr));
}

static inline void print_byte(struct rtattr *attr, const char *name)
{
	print("  attr %s (len %d) 0x%02x\n", name, (int) RTA_PAYLOAD(attr),
					*((unsigned char *) RTA_DATA(attr)));
}

static inline void print_integer(struct rtattr *attr, const char *name)
{
	print("  attr %s (len %d) %d\n", name, (int) RTA_PAYLOAD(attr),
						*((int *) RTA_DATA(attr)));
}

static inline void print_attr(struct rtattr *attr, const char *name)
{
	int len = (int) RTA_PAYLOAD(attr);

	if (name && len > 0)
		print("  attr %s (len %d)\n", name, len);
	else
		print("  attr %d (len %d)\n", attr->rta_type, len);
}

static void rtnl_link(struct nlmsghdr *hdr)
{
	struct ifinfomsg *msg;
	struct rtattr *attr;
	int bytes;

	msg = (struct ifinfomsg *) NLMSG_DATA(hdr);
	bytes = IFLA_PAYLOAD(hdr);

	print("ifi_index %d ifi_flags 0x%04x", msg->ifi_index, msg->ifi_flags);

	for (attr = IFLA_RTA(msg); RTA_OK(attr, bytes);
					attr = RTA_NEXT(attr, bytes)) {
		switch (attr->rta_type) {
		case IFLA_ADDRESS:
			print_ether(attr, "address");
			break;
		case IFLA_BROADCAST:
			print_ether(attr, "broadcast");
			break;
		case IFLA_IFNAME:
			print_string(attr, "ifname");
			break;
		case IFLA_MTU:
			print_integer(attr, "mtu");
			break;
		case IFLA_LINK:
			print_attr(attr, "link");
			break;
		case IFLA_QDISC:
			print_attr(attr, "qdisc");
			break;
		case IFLA_STATS:
			print_attr(attr, "stats");
			break;
		case IFLA_COST:
			print_attr(attr, "cost");
			break;
		case IFLA_PRIORITY:
			print_attr(attr, "priority");
			break;
		case IFLA_MASTER:
			print_attr(attr, "master");
			break;
		case IFLA_WIRELESS:
			print_attr(attr, "wireless");
			break;
		case IFLA_PROTINFO:
			print_attr(attr, "protinfo");
			break;
		case IFLA_TXQLEN:
			print_integer(attr, "txqlen");
			break;
		case IFLA_MAP:
			print_attr(attr, "map");
			break;
		case IFLA_WEIGHT:
			print_attr(attr, "weight");
			break;
		case IFLA_OPERSTATE:
			print_byte(attr, "operstate");
			break;
		case IFLA_LINKMODE:
			print_byte(attr, "linkmode");
			break;
		default:
			print_attr(attr, NULL);
			break;
		}
	}
}

static void rtnl_newlink(struct nlmsghdr *hdr)
{
	struct ifinfomsg *msg = (struct ifinfomsg *) NLMSG_DATA(hdr);

	rtnl_link(hdr);

	process_newlink(msg->ifi_type, msg->ifi_index, msg->ifi_flags,
				msg->ifi_change, msg, IFA_PAYLOAD(hdr));
}

static void rtnl_dellink(struct nlmsghdr *hdr)
{
	struct ifinfomsg *msg = (struct ifinfomsg *) NLMSG_DATA(hdr);

	rtnl_link(hdr);

	process_dellink(msg->ifi_type, msg->ifi_index, msg->ifi_flags,
				msg->ifi_change, msg, IFA_PAYLOAD(hdr));
}

static void rtnl_route(struct nlmsghdr *hdr)
{
	struct rtmsg *msg;
	struct rtattr *attr;
	int bytes;

	msg = (struct rtmsg *) NLMSG_DATA(hdr);
	bytes = RTM_PAYLOAD(hdr);

	print("rtm_family %d rtm_table %d rtm_protocol %d",
			msg->rtm_family, msg->rtm_table, msg->rtm_protocol);
	print("rtm_scope %d rtm_type %d rtm_flags 0x%04x",
				msg->rtm_scope, msg->rtm_type, msg->rtm_flags);

	for (attr = RTM_RTA(msg); RTA_OK(attr, bytes);
					attr = RTA_NEXT(attr, bytes)) {
		switch (attr->rta_type) {
		case RTA_DST:
			print_inet(attr, "dst", msg->rtm_family);
			break;
		case RTA_SRC:
			print_inet(attr, "src", msg->rtm_family);
			break;
		case RTA_IIF:
			print_string(attr, "iif");
			break;
		case RTA_OIF:
			print_integer(attr, "oif");
			break;
		case RTA_GATEWAY:
			print_inet(attr, "gateway", msg->rtm_family);
			break;
		case RTA_PRIORITY:
			print_attr(attr, "priority");
			break;
		case RTA_PREFSRC:
			print_inet(attr, "prefsrc", msg->rtm_family);
			break;
		case RTA_METRICS:
			print_attr(attr, "metrics");
			break;
		case RTA_TABLE:
			print_integer(attr, "table");
			break;
		default:
			print_attr(attr, NULL);
			break;
		}
	}
}

static bool is_route_rtmsg(struct rtmsg *msg)
{

	if (msg->rtm_table != RT_TABLE_MAIN)
		return false;

	if (msg->rtm_protocol != RTPROT_BOOT &&
			msg->rtm_protocol != RTPROT_KERNEL)
		return false;

	if (msg->rtm_type != RTN_UNICAST)
		return false;

	return true;
}

static void rtnl_newroute(struct nlmsghdr *hdr)
{
	struct rtmsg *msg = (struct rtmsg *) NLMSG_DATA(hdr);

	rtnl_route(hdr);

	if (is_route_rtmsg(msg))
		process_newroute(msg->rtm_family, msg->rtm_scope,
						msg, RTM_PAYLOAD(hdr));
}

static void rtnl_delroute(struct nlmsghdr *hdr)
{
	struct rtmsg *msg = (struct rtmsg *) NLMSG_DATA(hdr);

	rtnl_route(hdr);

	if (is_route_rtmsg(msg))
		process_delroute(msg->rtm_family, msg->rtm_scope,
						msg, RTM_PAYLOAD(hdr));
}

static const char *type2string(uint16_t type)
{
	switch (type) {
	case NLMSG_NOOP:
		return "NOOP";
	case NLMSG_ERROR:
		return "ERROR";
	case NLMSG_DONE:
		return "DONE";
	case NLMSG_OVERRUN:
		return "OVERRUN";
	case RTM_GETLINK:
		return "GETLINK";
	case RTM_NEWLINK:
		return "NEWLINK";
	case RTM_DELLINK:
		return "DELLINK";
	case RTM_NEWADDR:
		return "NEWADDR";
	case RTM_DELADDR:
		return "DELADDR";
	case RTM_GETROUTE:
		return "GETROUTE";
	case RTM_NEWROUTE:
		return "NEWROUTE";
	case RTM_DELROUTE:
		return "DELROUTE";
	case RTM_NEWNDUSEROPT:
		return "NEWNDUSEROPT";
	default:
		return "UNKNOWN";
	}
}

static GIOChannel *channel = NULL;

struct rtnl_request {
	struct nlmsghdr hdr;
	struct rtgenmsg msg;
};
#define RTNL_REQUEST_SIZE  (sizeof(struct nlmsghdr) + sizeof(struct rtgenmsg))

static GSList *request_list = NULL;
static guint32 request_seq = 0;

static struct rtnl_request *find_request(guint32 seq)
{
	GSList *list;

	for (list = request_list; list; list = list->next) {
		struct rtnl_request *req = list->data;

		if (req->hdr.nlmsg_seq == seq)
			return req;
	}

	return NULL;
}

static int send_request(struct rtnl_request *req)
{
	struct sockaddr_nl addr;
	int sk;

	debug("%s len %d type %d flags 0x%04x seq %d",
				type2string(req->hdr.nlmsg_type),
				req->hdr.nlmsg_len, req->hdr.nlmsg_type,
				req->hdr.nlmsg_flags, req->hdr.nlmsg_seq);

	sk = g_io_channel_unix_get_fd(channel);

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;

	return sendto(sk, req, req->hdr.nlmsg_len, 0,
				(struct sockaddr *) &addr, sizeof(addr));
}

static int queue_request(struct rtnl_request *req)
{
	request_list = g_slist_append(request_list, req);

	if (g_slist_length(request_list) > 1)
		return 0;

	return send_request(req);
}

static int process_response(guint32 seq)
{
	struct rtnl_request *req;

	debug("seq %d", seq);

	req = find_request(seq);
	if (req) {
		request_list = g_slist_remove(request_list, req);
		g_free(req);
	}

	req = g_slist_nth_data(request_list, 0);
	if (!req)
		return 0;

	return send_request(req);
}

static void rtnl_message(void *buf, size_t len)
{
	debug("buf %p len %zd", buf, len);

	while (len > 0) {
		struct nlmsghdr *hdr = buf;
		struct nlmsgerr *err;

		if (!NLMSG_OK(hdr, len))
			break;

		debug("%s len %d type %d flags 0x%04x seq %d pid %d",
					type2string(hdr->nlmsg_type),
					hdr->nlmsg_len, hdr->nlmsg_type,
					hdr->nlmsg_flags, hdr->nlmsg_seq,
					hdr->nlmsg_pid);

		switch (hdr->nlmsg_type) {
		case NLMSG_NOOP:
		case NLMSG_OVERRUN:
			return;
		case NLMSG_DONE:
			process_response(hdr->nlmsg_seq);
			return;
		case NLMSG_ERROR:
			err = NLMSG_DATA(hdr);
			DBG("error %d (%s)", -err->error,
						strerror(-err->error));
			return;
		case RTM_NEWLINK:
			rtnl_newlink(hdr);
			break;
		case RTM_DELLINK:
			rtnl_dellink(hdr);
			break;
		case RTM_NEWADDR:
			break;
		case RTM_DELADDR:
			break;
		case RTM_NEWROUTE:
			rtnl_newroute(hdr);
			break;
		case RTM_DELROUTE:
			rtnl_delroute(hdr);
			break;
		}

		len -= hdr->nlmsg_len;
		buf += hdr->nlmsg_len;
	}
}

static gboolean netlink_event(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	unsigned char buf[4096];
	struct sockaddr_nl nladdr;
	socklen_t addr_len = sizeof(nladdr);
	ssize_t status;
	int fd;

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
		return FALSE;

	memset(buf, 0, sizeof(buf));
	memset(&nladdr, 0, sizeof(nladdr));

	fd = g_io_channel_unix_get_fd(chan);

	status = recvfrom(fd, buf, sizeof(buf), 0,
                       (struct sockaddr *) &nladdr, &addr_len);
	if (status < 0) {
		if (errno == EINTR || errno == EAGAIN)
			return TRUE;

		return FALSE;
	}

	if (status == 0)
		return FALSE;

	if (nladdr.nl_pid != 0) { /* not sent by kernel, ignore */
		DBG("Received msg from %u, ignoring it", nladdr.nl_pid);
		return TRUE;
	}

	rtnl_message(buf, status);

	return TRUE;
}

static int send_getlink(void)
{
	struct rtnl_request *req;

	debug("");

	req = g_try_malloc0(RTNL_REQUEST_SIZE);
	if (!req)
		return -ENOMEM;

	req->hdr.nlmsg_len = RTNL_REQUEST_SIZE;
	req->hdr.nlmsg_type = RTM_GETLINK;
	req->hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req->hdr.nlmsg_pid = 0;
	req->hdr.nlmsg_seq = request_seq++;
	req->msg.rtgen_family = AF_INET;

	return queue_request(req);
}

static int send_getaddr(void)
{
	struct rtnl_request *req;

	debug("");

	req = g_try_malloc0(RTNL_REQUEST_SIZE);
	if (!req)
		return -ENOMEM;

	req->hdr.nlmsg_len = RTNL_REQUEST_SIZE;
	req->hdr.nlmsg_type = RTM_GETADDR;
	req->hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req->hdr.nlmsg_pid = 0;
	req->hdr.nlmsg_seq = request_seq++;
	req->msg.rtgen_family = AF_INET;

	return queue_request(req);
}

static int send_getroute(void)
{
	struct rtnl_request *req;

	debug("");

	req = g_try_malloc0(RTNL_REQUEST_SIZE);
	if (!req)
		return -ENOMEM;

	req->hdr.nlmsg_len = RTNL_REQUEST_SIZE;
	req->hdr.nlmsg_type = RTM_GETROUTE;
	req->hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req->hdr.nlmsg_pid = 0;
	req->hdr.nlmsg_seq = request_seq++;
	req->msg.rtgen_family = AF_INET;

	return queue_request(req);
}

static gboolean update_timeout_cb(gpointer user_data)
{
	__vpn_rtnl_request_update();

	return TRUE;
}

static void update_interval_callback(guint min)
{
	if (update_timeout > 0)
		g_source_remove(update_timeout);

	if (min < G_MAXUINT) {
		update_interval = min;
		update_timeout = g_timeout_add_seconds(update_interval,
						update_timeout_cb, NULL);
	} else {
		update_timeout = 0;
		update_interval = G_MAXUINT;
	}
}

static gint compare_interval(gconstpointer a, gconstpointer b)
{
	guint val_a = GPOINTER_TO_UINT(a);
	guint val_b = GPOINTER_TO_UINT(b);

	return val_a - val_b;
}

unsigned int __vpn_rtnl_update_interval_add(unsigned int interval)
{
	guint min;

	if (interval == 0)
		return 0;

	update_list = g_slist_insert_sorted(update_list,
			GUINT_TO_POINTER(interval), compare_interval);

	min = GPOINTER_TO_UINT(g_slist_nth_data(update_list, 0));
	if (min < update_interval) {
		update_interval_callback(min);
		__vpn_rtnl_request_update();
	}

	return update_interval;
}

unsigned int __vpn_rtnl_update_interval_remove(unsigned int interval)
{
	guint min = G_MAXUINT;

	if (interval == 0)
		return 0;

	update_list = g_slist_remove(update_list, GINT_TO_POINTER(interval));

	if (update_list)
		min = GPOINTER_TO_UINT(g_slist_nth_data(update_list, 0));

	if (min > update_interval)
		update_interval_callback(min);

	return min;
}

int __vpn_rtnl_request_update(void)
{
	return send_getlink();
}

int __vpn_rtnl_init(void)
{
	struct sockaddr_nl addr;
	int sk;

	DBG("");

	interface_list = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, free_interface);

	sk = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (sk < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE |
				RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE |
				(1<<(RTNLGRP_ND_USEROPT-1));

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(sk);
		return -1;
	}

	channel = g_io_channel_unix_new(sk);
	g_io_channel_set_close_on_unref(channel, TRUE);

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	g_io_add_watch(channel, G_IO_IN | G_IO_NVAL | G_IO_HUP | G_IO_ERR,
							netlink_event, NULL);

	return 0;
}

void __vpn_rtnl_start(void)
{
	DBG("");

	send_getlink();
	send_getaddr();
	send_getroute();
}

void __vpn_rtnl_cleanup(void)
{
	GSList *list;

	DBG("");

	for (list = watch_list; list; list = list->next) {
		struct watch_data *watch = list->data;

		DBG("removing watch %d", watch->id);

		g_free(watch);
		list->data = NULL;
	}

	g_slist_free(watch_list);
	watch_list = NULL;

	g_slist_free(update_list);
	update_list = NULL;

	for (list = request_list; list; list = list->next) {
		struct rtnl_request *req = list->data;

		debug("%s len %d type %d flags 0x%04x seq %d",
				type2string(req->hdr.nlmsg_type),
				req->hdr.nlmsg_len, req->hdr.nlmsg_type,
				req->hdr.nlmsg_flags, req->hdr.nlmsg_seq);

		g_free(req);
		list->data = NULL;
	}

	g_slist_free(request_list);
	request_list = NULL;

	g_io_channel_shutdown(channel, TRUE, NULL);
	g_io_channel_unref(channel);

	channel = NULL;

	g_hash_table_destroy(interface_list);
}
