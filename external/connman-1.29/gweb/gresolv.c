/*
 *
 *  Resolver library with GLib integration
 *
 *  Copyright (C) 2009-2013  Intel Corporation. All rights reserved.
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
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <resolv.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <net/if.h>

#include "gresolv.h"

struct sort_result {
	int precedence;
	int src_scope;
	int dst_scope;
	int src_label;
	int dst_label;
	bool reachable;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} src;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} dst;
};

struct resolv_query;

struct resolv_lookup {
	GResolv *resolv;
	guint id;

	int nr_results;
	struct sort_result *results;

	struct resolv_query *ipv4_query;
	struct resolv_query *ipv6_query;

	guint ipv4_status;
	guint ipv6_status;

	GResolvResultFunc result_func;
	gpointer result_data;
};

struct resolv_query {
	GResolv *resolv;

	int nr_ns;
	guint timeout;

	uint16_t msgid;

	struct resolv_lookup *lookup;
};

struct resolv_nameserver {
	GResolv *resolv;

	char *address;
	uint16_t port;
	unsigned long flags;

	GIOChannel *udp_channel;
	guint udp_watch;
};

struct _GResolv {
	int ref_count;

	int result_family;

	guint next_lookup_id;
	GQueue *lookup_queue;
	GQueue *query_queue;

	int index;
	GList *nameserver_list;

	struct __res_state res;

	GResolvDebugFunc debug_func;
	gpointer debug_data;
};

#define debug(resolv, format, arg...)				\
	_debug(resolv, __FILE__, __func__, format, ## arg)

static void _debug(GResolv *resolv, const char *file, const char *caller,
						const char *format, ...)
{
	char str[256];
	va_list ap;
	int len;

	if (!resolv->debug_func)
		return;

	va_start(ap, format);

	if ((len = snprintf(str, sizeof(str), "%s:%s() resolv %p ",
						file, caller, resolv)) > 0) {
		if (vsnprintf(str + len, sizeof(str) - len, format, ap) > 0)
			resolv->debug_func(str, resolv->debug_data);
	}

	va_end(ap);
}

static void destroy_query(struct resolv_query *query)
{
	debug(query->resolv, "query %p timeout %d", query, query->timeout);

	if (query->timeout > 0)
		g_source_remove(query->timeout);

	g_free(query);
}

static void destroy_lookup(struct resolv_lookup *lookup)
{
	debug(lookup->resolv, "lookup %p id %d ipv4 %p ipv6 %p",
		lookup, lookup->id, lookup->ipv4_query, lookup->ipv6_query);

	if (lookup->ipv4_query) {
		g_queue_remove(lookup->resolv->query_queue,
						lookup->ipv4_query);
		destroy_query(lookup->ipv4_query);
	}

	if (lookup->ipv6_query) {
		g_queue_remove(lookup->resolv->query_queue,
						lookup->ipv6_query);
		destroy_query(lookup->ipv6_query);
	}

	g_free(lookup->results);
	g_free(lookup);
}

static void find_srcaddr(struct sort_result *res)
{
	socklen_t sl = sizeof(res->src);
	int fd;

	fd = socket(res->dst.sa.sa_family, SOCK_DGRAM | SOCK_CLOEXEC,
			IPPROTO_IP);
	if (fd < 0)
		return;

	if (connect(fd, &res->dst.sa, sizeof(res->dst)) < 0) {
		close(fd);
		return;
	}

	if (getsockname(fd, &res->src.sa, &sl) < 0) {
		close(fd);
		return;
	}

	res->reachable = true;
	close(fd);
}

struct gai_table
{
	unsigned char addr[NS_IN6ADDRSZ];
	int mask;
	int value;
};

static const struct gai_table gai_labels[] = {
	{
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
		.mask = 128,
		.value = 0,
	}, {
		.addr = { 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 16,
		.value = 2,
	}, {
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 96,
		.value = 3,
	}, {
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 },
		.mask = 96,
		.value = 4,
	}, {
		/* Variations from RFC 3484, matching glibc behaviour */
		.addr = { 0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 10,
		.value = 5,
	}, {
		.addr = { 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 7,
		.value = 6,
	}, {
		.addr = { 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 32,
		.value = 7,
	}, {
		/* catch-all */
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 0,
		.value = 1,
	}
};

static const struct gai_table gai_precedences[] = {
	{
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
		.mask = 128,
		.value = 50,
	}, {
		.addr = { 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 16,
		.value = 30,
	}, {
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 96,
		.value = 20,
	}, {
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 },
		.mask = 96,
		.value = 10,
	}, {
		.addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.mask = 0,
		.value = 40,
	}
};

static unsigned char v4mapped[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 };

static bool mask_compare(const unsigned char *one,
					const unsigned char *two, int mask)
{
	if (mask > 8) {
		if (memcmp(one, two, mask / 8))
			return false;
		one += mask / 8;
		two += mask / 8;
		mask %= 8;
	}

	if (mask && ((*one ^ *two) >> (8 - mask)))
		return false;

	return true;
}

static int match_gai_table(struct sockaddr *sa, const struct gai_table *tbl)
{
	struct sockaddr_in *sin = (void *)sa;
	struct sockaddr_in6 *sin6 = (void *)sa;
	void *addr;

	if (sa->sa_family == AF_INET) {
		addr = v4mapped;
		memcpy(v4mapped+12, &sin->sin_addr, NS_INADDRSZ);
	} else
		addr = &sin6->sin6_addr;

	while (1) {
		if (mask_compare(addr, tbl->addr, tbl->mask))
			return tbl->value;
		tbl++;
	}
}

#define DQUAD(_a,_b,_c,_d) ( ((_a)<<24) | ((_b)<<16) | ((_c)<<8) | (_d) )
#define V4MATCH(addr, a,b,c,d, m) ( ((addr) ^ DQUAD(a,b,c,d)) >> (32 - (m)) )

#define RFC3484_SCOPE_LINK	2
#define RFC3484_SCOPE_SITE	5
#define RFC3484_SCOPE_GLOBAL	14

static int addr_scope(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		struct sockaddr_in *sin = (void *)sa;
		guint32 addr = ntohl(sin->sin_addr.s_addr);

		if (V4MATCH(addr, 169,254,0,0, 16) ||
					V4MATCH(addr, 127,0,0,0, 8))
			return RFC3484_SCOPE_LINK;

		/* Site-local */
		if (V4MATCH(addr, 10,0,0,0, 8) ||
				V4MATCH(addr, 172,16,0,0, 12) ||
				V4MATCH(addr, 192,168,0,0, 16))
			return RFC3484_SCOPE_SITE;

		/* Global */
		return RFC3484_SCOPE_GLOBAL;
	} else {
		struct sockaddr_in6 *sin6 = (void *)sa;

		/* Multicast addresses have a 4-bit scope field */
		if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
			return sin6->sin6_addr.s6_addr[1] & 0xf;

		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) ||
				IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
			return RFC3484_SCOPE_LINK;

		if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))
			return RFC3484_SCOPE_SITE;

		return RFC3484_SCOPE_GLOBAL;
	}
}

static int rfc3484_compare(const void *__one, const void *__two)
{
	const struct sort_result *one = __one;
	const struct sort_result *two = __two;

	/* Rule 1: Avoid unusable destinations */
	if (one->reachable && !two->reachable)
		return -1;
	else if (two->reachable && !one->reachable)
		return 1;

	/* Rule 2: Prefer matching scope */
	if (one->dst_scope == one->src_scope &&
				two->dst_scope != two->src_scope)
		return -1;
	else if (two->dst_scope == two->src_scope &&
				one->dst_scope != one->src_scope)
		return 1;

	/* Rule 3: Avoid deprecated addresses */

	/* Rule 4: Prefer home addresses */

	/* Rule 5: Prefer matching label */
	if (one->dst_label == one->src_label &&
				two->dst_label != two->src_label)
		return -1;
	else if (two->dst_label == two->src_label &&
				one->dst_label != one->src_label)
		return 1;

	/* Rule 6: Prefer higher precedence */
	if (one->precedence > two->precedence)
		return -1;
	else if (two->precedence > one->precedence)
		return 1;

	/* Rule 7: Prefer native transport */

	/* Rule 8: Prefer smaller scope */
	if (one->dst_scope != two->dst_scope)
		return one->dst_scope - two->dst_scope;

	/* Rule 9: Use longest matching prefix */
	if (one->dst.sa.sa_family == AF_INET) {
		/*
		 * Rule 9 is meaningless and counterproductive for Legacy IP
		 * unless perhaps we can tell that it's actually on the local
		 * subnet. But we don't (yet) have local interface config
		 * information, so do nothing here for Legacy IP for now.
		 */
	} else {
		int i;

		for (i = 0; i < 4; i++) {
			guint32 cmp_one, cmp_two;

			cmp_one = one->src.sin6.sin6_addr.s6_addr32[i] ^
					one->dst.sin6.sin6_addr.s6_addr32[i];
			cmp_two = two->src.sin6.sin6_addr.s6_addr32[i] ^
					two->dst.sin6.sin6_addr.s6_addr32[i];

			if (!cmp_two && !cmp_one)
				continue;

			if (cmp_one && !cmp_two)
				return 1;
			if (cmp_two && !cmp_one)
				return -1;

			/* g_bit_storage() is effectively fls() */
			cmp_one = g_bit_storage(ntohl(cmp_one));
			cmp_two = g_bit_storage(ntohl(cmp_two));

			if (cmp_one == cmp_two)
				break;

			return cmp_one - cmp_two;
		}
	}


	/* Rule 10: Otherwise, leave the order unchanged */
	if (one < two)
		return -1;
	else
		return 1;
}

static void rfc3484_sort_results(struct resolv_lookup *lookup)
{
	int i;

	for (i = 0; i < lookup->nr_results; i++) {
		struct sort_result *res = &lookup->results[i];
		find_srcaddr(res);
		res->precedence = match_gai_table(&res->dst.sa,
							gai_precedences);
		res->dst_label = match_gai_table(&res->dst.sa, gai_labels);
		res->src_label = match_gai_table(&res->src.sa, gai_labels);
		res->dst_scope = addr_scope(&res->dst.sa);
		res->src_scope = addr_scope(&res->src.sa);
	}

	qsort(lookup->results, lookup->nr_results,
			sizeof(struct sort_result), rfc3484_compare);
}

static void sort_and_return_results(struct resolv_lookup *lookup)
{
	char buf[INET6_ADDRSTRLEN + 1];
	GResolvResultStatus status;
	char **results = g_try_new0(char *, lookup->nr_results + 1);
	GResolvResultFunc result_func = lookup->result_func;
	void *result_data = lookup->result_data;
	int i, n = 0;

	if (!results)
		return;

	memset(buf, 0, INET6_ADDRSTRLEN + 1);

	rfc3484_sort_results(lookup);

	for (i = 0; i < lookup->nr_results; i++) {
		if (lookup->results[i].dst.sa.sa_family == AF_INET) {
			if (!inet_ntop(AF_INET,
					&lookup->results[i].dst.sin.sin_addr,
					buf, sizeof(buf) - 1))
				continue;
		} else if (lookup->results[i].dst.sa.sa_family == AF_INET6) {
			if (!inet_ntop(AF_INET6,
					&lookup->results[i].dst.sin6.sin6_addr,
					buf, sizeof(buf) - 1))
				continue;
		} else
			continue;

		results[n++] = g_strdup(buf);
	}

	results[n++] = NULL;

	if (lookup->resolv->result_family == AF_INET)
		status = lookup->ipv4_status;
	else if (lookup->resolv->result_family == AF_INET6)
		status = lookup->ipv6_status;
	else {
		if (lookup->ipv6_status == G_RESOLV_RESULT_STATUS_SUCCESS)
			status = lookup->ipv6_status;
		else
			status = lookup->ipv4_status;
	}

	debug(lookup->resolv, "lookup %p received %d results", lookup, n);

	g_queue_remove(lookup->resolv->lookup_queue, lookup);
	destroy_lookup(lookup);

	result_func(status, results, result_data);

	g_strfreev(results);
}

static gboolean query_timeout(gpointer user_data)
{
	struct resolv_query *query = user_data;
	struct resolv_lookup *lookup = query->lookup;
	GResolv *resolv = query->resolv;

	query->timeout = 0;

	if (query == lookup->ipv4_query) {
		lookup->ipv4_status = G_RESOLV_RESULT_STATUS_NO_RESPONSE;
		lookup->ipv4_query = NULL;
	} else if (query == lookup->ipv6_query) {
		lookup->ipv6_status = G_RESOLV_RESULT_STATUS_NO_RESPONSE;
		lookup->ipv6_query = NULL;
	}

	g_queue_remove(resolv->query_queue, query);
	destroy_query(query);

	if (!lookup->ipv4_query && !lookup->ipv6_query)
		sort_and_return_results(lookup);

	return FALSE;
}

static void free_nameserver(struct resolv_nameserver *nameserver)
{
	if (!nameserver)
		return;

	if (nameserver->udp_watch > 0)
		g_source_remove(nameserver->udp_watch);

	if (nameserver->udp_channel) {
		g_io_channel_shutdown(nameserver->udp_channel, TRUE, NULL);
		g_io_channel_unref(nameserver->udp_channel);
	}

	g_free(nameserver->address);
	g_free(nameserver);
}

static void flush_nameservers(GResolv *resolv)
{
	GList *list;

	for (list = g_list_first(resolv->nameserver_list);
					list; list = g_list_next(list))
		free_nameserver(list->data);

	g_list_free(resolv->nameserver_list);
	resolv->nameserver_list = NULL;
}

static int send_query(GResolv *resolv, const unsigned char *buf, int len)
{
	GList *list;
	int nr_ns;

	if (!resolv->nameserver_list)
		return -ENOENT;

	for (list = g_list_first(resolv->nameserver_list), nr_ns = 0;
				list; list = g_list_next(list), nr_ns++) {
		struct resolv_nameserver *nameserver = list->data;
		int sk, sent;

		if (!nameserver->udp_channel)
			continue;

		sk = g_io_channel_unix_get_fd(nameserver->udp_channel);

		sent = send(sk, buf, len, 0);
		if (sent < 0)
			continue;
	}

	return nr_ns;
}

static gint compare_lookup_id(gconstpointer a, gconstpointer b)
{
	const struct resolv_lookup *lookup = a;
	guint id = GPOINTER_TO_UINT(b);

	if (lookup->id < id)
		return -1;

	if (lookup->id > id)
		return 1;

	return 0;
}

static gint compare_query_msgid(gconstpointer a, gconstpointer b)
{
	const struct resolv_query *query = a;
	uint16_t msgid = GPOINTER_TO_UINT(b);

	if (query->msgid < msgid)
		return -1;

	if (query->msgid > msgid)
		return 1;

	return 0;
}

static void add_result(struct resolv_lookup *lookup, int family,
							const void *data)
{
	int n = lookup->nr_results++;
	lookup->results = g_try_realloc(lookup->results,
					sizeof(struct sort_result) * (n + 1));
	if (!lookup->results)
		return;

	memset(&lookup->results[n], 0, sizeof(struct sort_result));

	lookup->results[n].dst.sa.sa_family = family;
	if (family == AF_INET)
		memcpy(&lookup->results[n].dst.sin.sin_addr,
						data, NS_INADDRSZ);
	else
		memcpy(&lookup->results[n].dst.sin6.sin6_addr,
						data, NS_IN6ADDRSZ);
}

static void parse_response(struct resolv_nameserver *nameserver,
					const unsigned char *buf, int len)
{
	GResolv *resolv = nameserver->resolv;
	GResolvResultStatus status;
	struct resolv_query *query;
	struct resolv_lookup *lookup;
	GList *list;
	ns_msg msg;
	ns_rr rr;
	int i, rcode, count;

	debug(resolv, "response from %s", nameserver->address);

	if (ns_initparse(buf, len, &msg) < 0)
		return;

	list = g_queue_find_custom(resolv->query_queue,
			GUINT_TO_POINTER(ns_msg_id(msg)), compare_query_msgid);
	if (!list)
		return;

	rcode = ns_msg_getflag(msg, ns_f_rcode);
	count = ns_msg_count(msg, ns_s_an);

	debug(resolv, "msg id: 0x%04x rcode: %d count: %d",
					ns_msg_id(msg), rcode, count);

	switch (rcode) {
	case ns_r_noerror:
		status = G_RESOLV_RESULT_STATUS_SUCCESS;
		break;
	case ns_r_formerr:
		status = G_RESOLV_RESULT_STATUS_FORMAT_ERROR;
		break;
	case ns_r_servfail:
		status = G_RESOLV_RESULT_STATUS_SERVER_FAILURE;
		break;
	case ns_r_nxdomain:
		status = G_RESOLV_RESULT_STATUS_NAME_ERROR;
		break;
	case ns_r_notimpl:
		status = G_RESOLV_RESULT_STATUS_NOT_IMPLEMENTED;
		break;
	case ns_r_refused:
		status = G_RESOLV_RESULT_STATUS_REFUSED;
		break;
	default:
		status = G_RESOLV_RESULT_STATUS_ERROR;
		break;
	}

	query = list->data;
	query->nr_ns--;

	lookup = query->lookup;

	if (query == lookup->ipv6_query)
		lookup->ipv6_status = status;
	else if (query == lookup->ipv4_query)
		lookup->ipv4_status = status;

	for (i = 0; i < count; i++) {
		if (ns_parserr(&msg, ns_s_an, i, &rr) < 0)
			continue;

		if (ns_rr_class(rr) != ns_c_in)
			continue;

		g_assert(offsetof(struct sockaddr_in, sin_addr) ==
				offsetof(struct sockaddr_in6, sin6_flowinfo));

		if (ns_rr_type(rr) == ns_t_a &&
					ns_rr_rdlen(rr) == NS_INADDRSZ) {
			add_result(lookup, AF_INET, ns_rr_rdata(rr));
		} else if (ns_rr_type(rr) == ns_t_aaaa &&
					ns_rr_rdlen(rr) == NS_IN6ADDRSZ) {
			add_result(lookup, AF_INET6, ns_rr_rdata(rr));
		}
	}

	if (status != G_RESOLV_RESULT_STATUS_SUCCESS && query->nr_ns > 0)
		return;

	if (query == lookup->ipv6_query)
		lookup->ipv6_query = NULL;
	else
		lookup->ipv4_query = NULL;

	g_queue_remove(resolv->query_queue, query);
	destroy_query(query);

	if (!lookup->ipv4_query && !lookup->ipv6_query)
		sort_and_return_results(lookup);
}

static gboolean received_udp_data(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	struct resolv_nameserver *nameserver = user_data;
	unsigned char buf[4096];
	int sk, len;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		nameserver->udp_watch = 0;
		return FALSE;
	}

	sk = g_io_channel_unix_get_fd(nameserver->udp_channel);

	len = recv(sk, buf, sizeof(buf), 0);
	if (len < 12)
		return TRUE;

	parse_response(nameserver, buf, len);

	return TRUE;
}

static int connect_udp_channel(struct resolv_nameserver *nameserver)
{
	struct addrinfo hints, *rp;
	char portnr[6];
	int err, sk;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_NUMERICHOST;

	sprintf(portnr, "%d", nameserver->port);
	err = getaddrinfo(nameserver->address, portnr, &hints, &rp);
	if (err)
		return -EINVAL;

	/*
	 * Do not blindly copy this code elsewhere; it doesn't loop over the
	 * results using ->ai_next as it should. That's OK in *this* case
	 * because it was a numeric lookup; we *know* there's only one.
	 */
	if (!rp)
		return -EINVAL;

	sk = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
	if (sk < 0) {
		freeaddrinfo(rp);
		return -EIO;
	}

	/*
	 * If nameserver points to localhost ip, their is no need to
	 * bind the socket on any interface.
	 */
	if (nameserver->resolv->index > 0 &&
			strncmp(nameserver->address, "127.0.0.1", 9) != 0) {
		char interface[IF_NAMESIZE];

		memset(interface, 0, IF_NAMESIZE);
		if (if_indextoname(nameserver->resolv->index, interface)) {
			if (setsockopt(sk, SOL_SOCKET, SO_BINDTODEVICE,
						interface, IF_NAMESIZE) < 0) {
				close(sk);
				freeaddrinfo(rp);
				return -EIO;
			}
		}
	}

	if (connect(sk, rp->ai_addr, rp->ai_addrlen) < 0) {
		close(sk);
		freeaddrinfo(rp);
		return -EIO;
	}

	freeaddrinfo(rp);

	nameserver->udp_channel = g_io_channel_unix_new(sk);
	if (!nameserver->udp_channel) {
		close(sk);
		return -ENOMEM;
	}

	g_io_channel_set_close_on_unref(nameserver->udp_channel, TRUE);

	nameserver->udp_watch = g_io_add_watch(nameserver->udp_channel,
				G_IO_IN | G_IO_NVAL | G_IO_ERR | G_IO_HUP,
				received_udp_data, nameserver);

	return 0;
}

GResolv *g_resolv_new(int index)
{
	GResolv *resolv;

	if (index < 0)
		return NULL;

	resolv = g_try_new0(GResolv, 1);
	if (!resolv)
		return NULL;

	resolv->ref_count = 1;

	resolv->result_family = AF_UNSPEC;

	resolv->next_lookup_id = 1;

	resolv->query_queue = g_queue_new();
	if (!resolv->query_queue) {
		g_free(resolv);
		return NULL;
	}

	resolv->lookup_queue = g_queue_new();
	if (!resolv->lookup_queue) {
		g_queue_free(resolv->query_queue);
		g_free(resolv);
		return NULL;
	}

	resolv->index = index;
	resolv->nameserver_list = NULL;

	res_ninit(&resolv->res);

	return resolv;
}

GResolv *g_resolv_ref(GResolv *resolv)
{
	if (!resolv)
		return NULL;

	__sync_fetch_and_add(&resolv->ref_count, 1);

	return resolv;
}

void g_resolv_unref(GResolv *resolv)
{
	struct resolv_query *query;
	struct resolv_lookup *lookup;

	if (!resolv)
		return;

	if (__sync_fetch_and_sub(&resolv->ref_count, 1) != 1)
		return;

	while ((lookup = g_queue_pop_head(resolv->lookup_queue))) {
		debug(resolv, "lookup %p id %d", lookup, lookup->id);
		destroy_lookup(lookup);
	}

	while ((query = g_queue_pop_head(resolv->query_queue))) {
		debug(resolv, "query %p", query);
		destroy_query(query);
	}

	g_queue_free(resolv->query_queue);
	g_queue_free(resolv->lookup_queue);

	flush_nameservers(resolv);

	res_nclose(&resolv->res);

	g_free(resolv);
}

void g_resolv_set_debug(GResolv *resolv, GResolvDebugFunc func,
						gpointer user_data)
{
	if (!resolv)
		return;

	resolv->debug_func = func;
	resolv->debug_data = user_data;
}

bool g_resolv_add_nameserver(GResolv *resolv, const char *address,
					uint16_t port, unsigned long flags)
{
	struct resolv_nameserver *nameserver;

	if (!resolv)
		return false;

	nameserver = g_try_new0(struct resolv_nameserver, 1);
	if (!nameserver)
		return false;

	nameserver->address = g_strdup(address);
	nameserver->port = port;
	nameserver->flags = flags;
	nameserver->resolv = resolv;

	if (connect_udp_channel(nameserver) < 0) {
		free_nameserver(nameserver);
		return false;
	}

	resolv->nameserver_list = g_list_append(resolv->nameserver_list,
								nameserver);

	debug(resolv, "setting nameserver %s", address);

	return true;
}

void g_resolv_flush_nameservers(GResolv *resolv)
{
	if (!resolv)
		return;

	flush_nameservers(resolv);
}

static gint add_query(struct resolv_lookup *lookup, const char *hostname, int type)
{
	struct resolv_query *query = g_try_new0(struct resolv_query, 1);
	unsigned char buf[4096];
	int len;

	if (!query)
		return -ENOMEM;

	len = res_mkquery(ns_o_query, hostname, ns_c_in, type,
					NULL, 0, NULL, buf, sizeof(buf));

	query->msgid = buf[0] << 8 | buf[1];

	debug(lookup->resolv, "sending %d bytes", len);

	query->nr_ns = send_query(lookup->resolv, buf, len);
	if (query->nr_ns <= 0) {
		g_free(query);
		return -EIO;
	}

	query->resolv = lookup->resolv;
	query->lookup = lookup;

	g_queue_push_tail(lookup->resolv->query_queue, query);

	debug(lookup->resolv, "lookup %p id %d query %p", lookup, lookup->id,
									query);

	query->timeout = g_timeout_add_seconds(5, query_timeout, query);

	if (type == ns_t_aaaa)
		lookup->ipv6_query = query;
	else
		lookup->ipv4_query = query;

	return 0;
}

guint g_resolv_lookup_hostname(GResolv *resolv, const char *hostname,
				GResolvResultFunc func, gpointer user_data)
{
	struct resolv_lookup *lookup;

	if (!resolv)
		return 0;

	debug(resolv, "hostname %s", hostname);

	if (!resolv->nameserver_list) {
		int i;

		for (i = 0; i < resolv->res.nscount; i++) {
			char buf[100];
			int family = resolv->res.nsaddr_list[i].sin_family;
			void *sa_addr = &resolv->res.nsaddr_list[i].sin_addr;

			if (family != AF_INET &&
					resolv->res._u._ext.nsaddrs[i]) {
				family = AF_INET6;
				sa_addr = &resolv->res._u._ext.nsaddrs[i]->sin6_addr;
			}

			if (family != AF_INET && family != AF_INET6)
				continue;

			if (inet_ntop(family, sa_addr, buf, sizeof(buf)))
				g_resolv_add_nameserver(resolv, buf, 53, 0);
		}

		if (!resolv->nameserver_list)
			g_resolv_add_nameserver(resolv, "127.0.0.1", 53, 0);
	}

	lookup = g_try_new0(struct resolv_lookup, 1);
	if (!lookup)
		return 0;

	lookup->resolv = resolv;
	lookup->result_func = func;
	lookup->result_data = user_data;
	lookup->id = resolv->next_lookup_id++;

	if (resolv->result_family != AF_INET6) {
		if (add_query(lookup, hostname, ns_t_a)) {
			g_free(lookup);
			return -EIO;
		}
	}

	if (resolv->result_family != AF_INET) {
		if (add_query(lookup, hostname, ns_t_aaaa)) {
			if (resolv->result_family != AF_INET6) {
				g_queue_remove(resolv->query_queue,
						lookup->ipv4_query);
				destroy_query(lookup->ipv4_query);
			}

			g_free(lookup);
			return -EIO;
		}
	}

	g_queue_push_tail(resolv->lookup_queue, lookup);

	debug(resolv, "lookup %p id %d", lookup, lookup->id);

	return lookup->id;
}

bool g_resolv_cancel_lookup(GResolv *resolv, guint id)
{
	struct resolv_lookup *lookup;
	GList *list;

	debug(resolv, "lookup id %d", id);

	list = g_queue_find_custom(resolv->lookup_queue,
				GUINT_TO_POINTER(id), compare_lookup_id);

	if (!list)
		return false;

	lookup = list->data;

	debug(resolv, "lookup %p", lookup);

	g_queue_remove(resolv->lookup_queue, lookup);
	destroy_lookup(lookup);

	return true;
}

bool g_resolv_set_address_family(GResolv *resolv, int family)
{
	if (!resolv)
		return false;

	if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6)
		return false;

	resolv->result_family = family;

	return true;
}
