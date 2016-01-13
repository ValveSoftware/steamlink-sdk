/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2014  Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <gweb/gresolv.h>

#include <glib.h>

#include "connman.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
struct domain_hdr {
	uint16_t id;
	uint8_t rd:1;
	uint8_t tc:1;
	uint8_t aa:1;
	uint8_t opcode:4;
	uint8_t qr:1;
	uint8_t rcode:4;
	uint8_t z:3;
	uint8_t ra:1;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} __attribute__ ((packed));
#elif __BYTE_ORDER == __BIG_ENDIAN
struct domain_hdr {
	uint16_t id;
	uint8_t qr:1;
	uint8_t opcode:4;
	uint8_t aa:1;
	uint8_t tc:1;
	uint8_t rd:1;
	uint8_t ra:1;
	uint8_t z:3;
	uint8_t rcode:4;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} __attribute__ ((packed));
#else
#error "Unknown byte order"
#endif

struct partial_reply {
	uint16_t len;
	uint16_t received;
	unsigned char buf[];
};

struct server_data {
	int index;
	GList *domains;
	char *server;
	struct sockaddr *server_addr;
	socklen_t server_addr_len;
	int protocol;
	GIOChannel *channel;
	guint watch;
	guint timeout;
	bool enabled;
	bool connected;
	struct partial_reply *incoming_reply;
};

struct request_data {
	union {
		struct sockaddr_in6 __sin6; /* Only for the length */
		struct sockaddr sa;
	};
	socklen_t sa_len;
	int client_sk;
	int protocol;
	int family;
	guint16 srcid;
	guint16 dstid;
	guint16 altid;
	guint timeout;
	guint watch;
	guint numserv;
	guint numresp;
	gpointer request;
	gsize request_len;
	gpointer name;
	gpointer resp;
	gsize resplen;
	struct listener_data *ifdata;
	bool append_domain;
};

struct listener_data {
	int index;

	GIOChannel *udp4_listener_channel;
	GIOChannel *tcp4_listener_channel;
	guint udp4_listener_watch;
	guint tcp4_listener_watch;

	GIOChannel *udp6_listener_channel;
	GIOChannel *tcp6_listener_channel;
	guint udp6_listener_watch;
	guint tcp6_listener_watch;
};

/*
 * The TCP client requires some extra handling as we need to
 * be prepared to receive also partial DNS requests.
 */
struct tcp_partial_client_data {
	int family;
	struct listener_data *ifdata;
	GIOChannel *channel;
	guint watch;
	unsigned char *buf;
	unsigned int buf_end;
	guint timeout;
};

struct cache_data {
	time_t inserted;
	time_t valid_until;
	time_t cache_until;
	int timeout;
	uint16_t type;
	uint16_t answers;
	unsigned int data_len;
	unsigned char *data; /* contains DNS header + body */
};

struct cache_entry {
	char *key;
	bool want_refresh;
	int hits;
	struct cache_data *ipv4;
	struct cache_data *ipv6;
};

struct domain_question {
	uint16_t type;
	uint16_t class;
} __attribute__ ((packed));

struct domain_rr {
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdlen;
} __attribute__ ((packed));

/*
 * Max length of the DNS TCP packet.
 */
#define TCP_MAX_BUF_LEN 4096

/*
 * We limit how long the cached DNS entry stays in the cache.
 * By default the TTL (time-to-live) of the DNS response is used
 * when setting the cache entry life time. The value is in seconds.
 */
#define MAX_CACHE_TTL (60 * 30)
/*
 * Also limit the other end, cache at least for 30 seconds.
 */
#define MIN_CACHE_TTL (30)

/*
 * We limit the cache size to some sane value so that cached data does
 * not occupy too much memory. Each cached entry occupies on average
 * about 100 bytes memory (depending on DNS name length).
 * Example: caching www.connman.net uses 97 bytes memory.
 * The value is the max amount of cached DNS responses (count).
 */
#define MAX_CACHE_SIZE 256

static int cache_size;
static GHashTable *cache;
static int cache_refcount;
static GSList *server_list = NULL;
static GSList *request_list = NULL;
static GHashTable *listener_table = NULL;
static time_t next_refresh;
static GHashTable *partial_tcp_req_table;
static guint cache_timer = 0;

static guint16 get_id(void)
{
	uint64_t rand;

	__connman_util_get_random(&rand);

	return rand;
}

static int protocol_offset(int protocol)
{
	switch (protocol) {
	case IPPROTO_UDP:
		return 0;

	case IPPROTO_TCP:
		return 2;

	default:
		return -EINVAL;
	}

}

/*
 * There is a power and efficiency benefit to have entries
 * in our cache expire at the same time. To this extend,
 * we round down the cache valid time to common boundaries.
 */
static time_t round_down_ttl(time_t end_time, int ttl)
{
	if (ttl < 15)
		return end_time;

	/* Less than 5 minutes, round to 10 second boundary */
	if (ttl < 300) {
		end_time = end_time / 10;
		end_time = end_time * 10;
	} else { /* 5 or more minutes, round to 30 seconds */
		end_time = end_time / 30;
		end_time = end_time * 30;
	}
	return end_time;
}

static struct request_data *find_request(guint16 id)
{
	GSList *list;

	for (list = request_list; list; list = list->next) {
		struct request_data *req = list->data;

		if (req->dstid == id || req->altid == id)
			return req;
	}

	return NULL;
}

static struct server_data *find_server(int index,
					const char *server,
						int protocol)
{
	GSList *list;

	DBG("index %d server %s proto %d", index, server, protocol);

	for (list = server_list; list; list = list->next) {
		struct server_data *data = list->data;

		if (index < 0 && data->index < 0 &&
				g_str_equal(data->server, server) &&
				data->protocol == protocol)
			return data;

		if (index < 0 ||
				data->index < 0 || !data->server)
			continue;

		if (data->index == index &&
				g_str_equal(data->server, server) &&
				data->protocol == protocol)
			return data;
	}

	return NULL;
}

/* we can keep using the same resolve's */
static GResolv *ipv4_resolve;
static GResolv *ipv6_resolve;

static void dummy_resolve_func(GResolvResultStatus status,
					char **results, gpointer user_data)
{
}

/*
 * Refresh a DNS entry, but also age the hit count a bit */
static void refresh_dns_entry(struct cache_entry *entry, char *name)
{
	int age = 1;

	if (!ipv4_resolve) {
		ipv4_resolve = g_resolv_new(0);
		g_resolv_set_address_family(ipv4_resolve, AF_INET);
		g_resolv_add_nameserver(ipv4_resolve, "127.0.0.1", 53, 0);
	}

	if (!ipv6_resolve) {
		ipv6_resolve = g_resolv_new(0);
		g_resolv_set_address_family(ipv6_resolve, AF_INET6);
		g_resolv_add_nameserver(ipv6_resolve, "::1", 53, 0);
	}

	if (!entry->ipv4) {
		DBG("Refresing A record for %s", name);
		g_resolv_lookup_hostname(ipv4_resolve, name,
					dummy_resolve_func, NULL);
		age = 4;
	}

	if (!entry->ipv6) {
		DBG("Refresing AAAA record for %s", name);
		g_resolv_lookup_hostname(ipv6_resolve, name,
					dummy_resolve_func, NULL);
		age = 4;
	}

	entry->hits -= age;
	if (entry->hits < 0)
		entry->hits = 0;
}

static int dns_name_length(unsigned char *buf)
{
	if ((buf[0] & NS_CMPRSFLGS) == NS_CMPRSFLGS) /* compressed name */
		return 2;
	return strlen((char *)buf);
}

static void update_cached_ttl(unsigned char *buf, int len, int new_ttl)
{
	unsigned char *c;
	uint16_t w;
	int l;

	/* skip the header */
	c = buf + 12;
	len -= 12;

	/* skip the query, which is a name and 2 16 bit words */
	l = dns_name_length(c);
	c += l;
	len -= l;
	c += 4;
	len -= 4;

	/* now we get the answer records */

	while (len > 0) {
		/* first a name */
		l = dns_name_length(c);
		c += l;
		len -= l;
		if (len < 0)
			break;
		/* then type + class, 2 bytes each */
		c += 4;
		len -= 4;
		if (len < 0)
			break;

		/* now the 4 byte TTL field */
		c[0] = new_ttl >> 24 & 0xff;
		c[1] = new_ttl >> 16 & 0xff;
		c[2] = new_ttl >> 8 & 0xff;
		c[3] = new_ttl & 0xff;
		c += 4;
		len -= 4;
		if (len < 0)
			break;

		/* now the 2 byte rdlen field */
		w = c[0] << 8 | c[1];
		c += w + 2;
		len -= w + 2;
	}
}

static void send_cached_response(int sk, unsigned char *buf, int len,
				const struct sockaddr *to, socklen_t tolen,
				int protocol, int id, uint16_t answers, int ttl)
{
	struct domain_hdr *hdr;
	unsigned char *ptr = buf;
	int err, offset, dns_len, adj_len = len - 2;

	/*
	 * The cached packet contains always the TCP offset (two bytes)
	 * so skip them for UDP.
	 */
	switch (protocol) {
	case IPPROTO_UDP:
		ptr += 2;
		len -= 2;
		dns_len = len;
		offset = 0;
		break;
	case IPPROTO_TCP:
		offset = 2;
		dns_len = ptr[0] * 256 + ptr[1];
		break;
	default:
		return;
	}

	if (len < 12)
		return;

	hdr = (void *) (ptr + offset);

	hdr->id = id;
	hdr->qr = 1;
	hdr->rcode = ns_r_noerror;
	hdr->ancount = htons(answers);
	hdr->nscount = 0;
	hdr->arcount = 0;

	/* if this is a negative reply, we are authorative */
	if (answers == 0)
		hdr->aa = 1;
	else
		update_cached_ttl((unsigned char *)hdr, adj_len, ttl);

	DBG("sk %d id 0x%04x answers %d ptr %p length %d dns %d",
		sk, hdr->id, answers, ptr, len, dns_len);

	err = sendto(sk, ptr, len, MSG_NOSIGNAL, to, tolen);
	if (err < 0) {
		connman_error("Cannot send cached DNS response: %s",
				strerror(errno));
		return;
	}

	if (err != len || (dns_len != (len - 2) && protocol == IPPROTO_TCP) ||
				(dns_len != len && protocol == IPPROTO_UDP))
		DBG("Packet length mismatch, sent %d wanted %d dns %d",
			err, len, dns_len);
}

static void send_response(int sk, unsigned char *buf, int len,
				const struct sockaddr *to, socklen_t tolen,
				int protocol)
{
	struct domain_hdr *hdr;
	int err, offset = protocol_offset(protocol);

	DBG("sk %d", sk);

	if (offset < 0)
		return;

	if (len < 12)
		return;

	hdr = (void *) (buf + offset);

	DBG("id 0x%04x qr %d opcode %d", hdr->id, hdr->qr, hdr->opcode);

	hdr->qr = 1;
	hdr->rcode = ns_r_servfail;

	hdr->ancount = 0;
	hdr->nscount = 0;
	hdr->arcount = 0;

	err = sendto(sk, buf, len, MSG_NOSIGNAL, to, tolen);
	if (err < 0) {
		connman_error("Failed to send DNS response to %d: %s",
				sk, strerror(errno));
		return;
	}
}

static int get_req_udp_socket(struct request_data *req)
{
	GIOChannel *channel;

	if (req->family == AF_INET)
		channel = req->ifdata->udp4_listener_channel;
	else
		channel = req->ifdata->udp6_listener_channel;

	if (!channel)
		return -1;

	return g_io_channel_unix_get_fd(channel);
}

static void destroy_request_data(struct request_data *req)
{
	if (req->timeout > 0)
		g_source_remove(req->timeout);

	g_free(req->resp);
	g_free(req->request);
	g_free(req->name);
	g_free(req);
}

static gboolean request_timeout(gpointer user_data)
{
	struct request_data *req = user_data;
	struct sockaddr *sa;
	int sk;

	if (!req)
		return FALSE;

	DBG("id 0x%04x", req->srcid);

	request_list = g_slist_remove(request_list, req);

	if (req->protocol == IPPROTO_UDP) {
		sk = get_req_udp_socket(req);
		sa = &req->sa;
	} else if (req->protocol == IPPROTO_TCP) {
		sk = req->client_sk;
		sa = NULL;
	} else
		goto out;

	if (req->resplen > 0 && req->resp) {
		/*
		 * Here we have received at least one reply (probably telling
		 * "not found" result), so send that back to client instead
		 * of more fatal server failed error.
		 */
		if (sk >= 0)
			sendto(sk, req->resp, req->resplen, MSG_NOSIGNAL,
				sa, req->sa_len);

	} else if (req->request) {
		/*
		 * There was not reply from server at all.
		 */
		struct domain_hdr *hdr;

		hdr = (void *)(req->request + protocol_offset(req->protocol));
		hdr->id = req->srcid;

		if (sk >= 0)
			send_response(sk, req->request, req->request_len,
				sa, req->sa_len, req->protocol);
	}

	/*
	 * We cannot leave TCP client hanging so just kick it out
	 * if we get a request timeout from server.
	 */
	if (req->protocol == IPPROTO_TCP) {
		DBG("client %d removed", req->client_sk);
		g_hash_table_remove(partial_tcp_req_table,
				GINT_TO_POINTER(req->client_sk));
	}

out:
	req->timeout = 0;
	destroy_request_data(req);

	return FALSE;
}

static int append_query(unsigned char *buf, unsigned int size,
				const char *query, const char *domain)
{
	unsigned char *ptr = buf;
	int len;

	DBG("query %s domain %s", query, domain);

	while (query) {
		const char *tmp;

		tmp = strchr(query, '.');
		if (!tmp) {
			len = strlen(query);
			if (len == 0)
				break;
			*ptr = len;
			memcpy(ptr + 1, query, len);
			ptr += len + 1;
			break;
		}

		*ptr = tmp - query;
		memcpy(ptr + 1, query, tmp - query);
		ptr += tmp - query + 1;

		query = tmp + 1;
	}

	while (domain) {
		const char *tmp;

		tmp = strchr(domain, '.');
		if (!tmp) {
			len = strlen(domain);
			if (len == 0)
				break;
			*ptr = len;
			memcpy(ptr + 1, domain, len);
			ptr += len + 1;
			break;
		}

		*ptr = tmp - domain;
		memcpy(ptr + 1, domain, tmp - domain);
		ptr += tmp - domain + 1;

		domain = tmp + 1;
	}

	*ptr++ = 0x00;

	return ptr - buf;
}

static bool cache_check_is_valid(struct cache_data *data,
				time_t current_time)
{
	if (!data)
		return false;

	if (data->cache_until < current_time)
		return false;

	return true;
}

/*
 * remove stale cached entries so that they can be refreshed
 */
static void cache_enforce_validity(struct cache_entry *entry)
{
	time_t current_time = time(NULL);

	if (!cache_check_is_valid(entry->ipv4, current_time)
							&& entry->ipv4) {
		DBG("cache timeout \"%s\" type A", entry->key);
		g_free(entry->ipv4->data);
		g_free(entry->ipv4);
		entry->ipv4 = NULL;

	}

	if (!cache_check_is_valid(entry->ipv6, current_time)
							&& entry->ipv6) {
		DBG("cache timeout \"%s\" type AAAA", entry->key);
		g_free(entry->ipv6->data);
		g_free(entry->ipv6);
		entry->ipv6 = NULL;
	}
}

static uint16_t cache_check_validity(char *question, uint16_t type,
				struct cache_entry *entry)
{
	time_t current_time = time(NULL);
	bool want_refresh = false;

	/*
	 * if we have a popular entry, we want a refresh instead of
	 * total destruction of the entry.
	 */
	if (entry->hits > 2)
		want_refresh = true;

	cache_enforce_validity(entry);

	switch (type) {
	case 1:		/* IPv4 */
		if (!cache_check_is_valid(entry->ipv4, current_time)) {
			DBG("cache %s \"%s\" type A", entry->ipv4 ?
					"timeout" : "entry missing", question);

			if (want_refresh)
				entry->want_refresh = true;

			/*
			 * We do not remove cache entry if there is still
			 * valid IPv6 entry found in the cache.
			 */
			if (!cache_check_is_valid(entry->ipv6, current_time) && !want_refresh) {
				g_hash_table_remove(cache, question);
				type = 0;
			}
		}
		break;

	case 28:	/* IPv6 */
		if (!cache_check_is_valid(entry->ipv6, current_time)) {
			DBG("cache %s \"%s\" type AAAA", entry->ipv6 ?
					"timeout" : "entry missing", question);

			if (want_refresh)
				entry->want_refresh = true;

			if (!cache_check_is_valid(entry->ipv4, current_time) && !want_refresh) {
				g_hash_table_remove(cache, question);
				type = 0;
			}
		}
		break;
	}

	return type;
}

static void cache_element_destroy(gpointer value)
{
	struct cache_entry *entry = value;

	if (!entry)
		return;

	if (entry->ipv4) {
		g_free(entry->ipv4->data);
		g_free(entry->ipv4);
	}

	if (entry->ipv6) {
		g_free(entry->ipv6->data);
		g_free(entry->ipv6);
	}

	g_free(entry->key);
	g_free(entry);

	if (--cache_size < 0)
		cache_size = 0;
}

static gboolean try_remove_cache(gpointer user_data)
{
	cache_timer = 0;

	if (__sync_fetch_and_sub(&cache_refcount, 1) == 1) {
		DBG("No cache users, removing it.");

		g_hash_table_destroy(cache);
		cache = NULL;
	}

	return FALSE;
}

static void create_cache(void)
{
	if (__sync_fetch_and_add(&cache_refcount, 1) == 0)
		cache = g_hash_table_new_full(g_str_hash,
					g_str_equal,
					NULL,
					cache_element_destroy);
}

static struct cache_entry *cache_check(gpointer request, int *qtype, int proto)
{
	char *question;
	struct cache_entry *entry;
	struct domain_question *q;
	uint16_t type;
	int offset, proto_offset;

	if (!request)
		return NULL;

	proto_offset = protocol_offset(proto);
	if (proto_offset < 0)
		return NULL;

	question = request + proto_offset + 12;

	offset = strlen(question) + 1;
	q = (void *) (question + offset);
	type = ntohs(q->type);

	/* We only cache either A (1) or AAAA (28) requests */
	if (type != 1 && type != 28)
		return NULL;

	if (!cache) {
		create_cache();
		return NULL;
	}

	entry = g_hash_table_lookup(cache, question);
	if (!entry)
		return NULL;

	type = cache_check_validity(question, type, entry);
	if (type == 0)
		return NULL;

	*qtype = type;
	return entry;
}

/*
 * Get a label/name from DNS resource record. The function decompresses the
 * label if necessary. The function does not convert the name to presentation
 * form. This means that the result string will contain label lengths instead
 * of dots between labels. We intentionally do not want to convert to dotted
 * format so that we can cache the wire format string directly.
 */
static int get_name(int counter,
		unsigned char *pkt, unsigned char *start, unsigned char *max,
		unsigned char *output, int output_max, int *output_len,
		unsigned char **end, char *name, int *name_len)
{
	unsigned char *p;

	/* Limit recursion to 10 (this means up to 10 labels in domain name) */
	if (counter > 10)
		return -EINVAL;

	p = start;
	while (*p) {
		if ((*p & NS_CMPRSFLGS) == NS_CMPRSFLGS) {
			uint16_t offset = (*p & 0x3F) * 256 + *(p + 1);

			if (offset >= max - pkt)
				return -ENOBUFS;

			if (!*end)
				*end = p + 2;

			return get_name(counter + 1, pkt, pkt + offset, max,
					output, output_max, output_len, end,
					name, name_len);
		} else {
			unsigned label_len = *p;

			if (pkt + label_len > max)
				return -ENOBUFS;

			if (*output_len > output_max)
				return -ENOBUFS;

			/*
			 * We need the original name in order to check
			 * if this answer is the correct one.
			 */
			name[(*name_len)++] = label_len;
			memcpy(name + *name_len, p + 1,	label_len + 1);
			*name_len += label_len;

			/* We compress the result */
			output[0] = NS_CMPRSFLGS;
			output[1] = 0x0C;
			*output_len = 2;

			p += label_len + 1;

			if (!*end)
				*end = p;

			if (p >= max)
				return -ENOBUFS;
		}
	}

	return 0;
}

static int parse_rr(unsigned char *buf, unsigned char *start,
			unsigned char *max,
			unsigned char *response, unsigned int *response_size,
			uint16_t *type, uint16_t *class, int *ttl, int *rdlen,
			unsigned char **end,
			char *name)
{
	struct domain_rr *rr;
	int err, offset;
	int name_len = 0, output_len = 0, max_rsp = *response_size;

	err = get_name(0, buf, start, max, response, max_rsp,
		&output_len, end, name, &name_len);
	if (err < 0)
		return err;

	offset = output_len;

	if ((unsigned int) offset > *response_size)
		return -ENOBUFS;

	rr = (void *) (*end);

	if (!rr)
		return -EINVAL;

	*type = ntohs(rr->type);
	*class = ntohs(rr->class);
	*ttl = ntohl(rr->ttl);
	*rdlen = ntohs(rr->rdlen);

	if (*ttl < 0)
		return -EINVAL;

	memcpy(response + offset, *end, sizeof(struct domain_rr));

	offset += sizeof(struct domain_rr);
	*end += sizeof(struct domain_rr);

	if ((unsigned int) (offset + *rdlen) > *response_size)
		return -ENOBUFS;

	memcpy(response + offset, *end, *rdlen);

	*end += *rdlen;

	*response_size = offset + *rdlen;

	return 0;
}

static bool check_alias(GSList *aliases, char *name)
{
	GSList *list;

	if (aliases) {
		for (list = aliases; list; list = list->next) {
			int len = strlen((char *)list->data);
			if (strncmp((char *)list->data, name, len) == 0)
				return true;
		}
	}

	return false;
}

static int parse_response(unsigned char *buf, int buflen,
			char *question, int qlen,
			uint16_t *type, uint16_t *class, int *ttl,
			unsigned char *response, unsigned int *response_len,
			uint16_t *answers)
{
	struct domain_hdr *hdr = (void *) buf;
	struct domain_question *q;
	unsigned char *ptr;
	uint16_t qdcount = ntohs(hdr->qdcount);
	uint16_t ancount = ntohs(hdr->ancount);
	int err, i;
	uint16_t qtype, qclass;
	unsigned char *next = NULL;
	unsigned int maxlen = *response_len;
	GSList *aliases = NULL, *list;
	char name[NS_MAXDNAME + 1];

	if (buflen < 12)
		return -EINVAL;

	DBG("qr %d qdcount %d", hdr->qr, qdcount);

	/* We currently only cache responses where question count is 1 */
	if (hdr->qr != 1 || qdcount != 1)
		return -EINVAL;

	ptr = buf + sizeof(struct domain_hdr);

	strncpy(question, (char *) ptr, qlen);
	qlen = strlen(question);
	ptr += qlen + 1; /* skip \0 */

	q = (void *) ptr;
	qtype = ntohs(q->type);

	/* We cache only A and AAAA records */
	if (qtype != 1 && qtype != 28)
		return -ENOMSG;

	qclass = ntohs(q->class);

	ptr += 2 + 2; /* ptr points now to answers */

	err = -ENOMSG;
	*response_len = 0;
	*answers = 0;

	memset(name, 0, sizeof(name));

	/*
	 * We have a bunch of answers (like A, AAAA, CNAME etc) to
	 * A or AAAA question. We traverse the answers and parse the
	 * resource records. Only A and AAAA records are cached, all
	 * the other records in answers are skipped.
	 */
	for (i = 0; i < ancount; i++) {
		/*
		 * Get one address at a time to this buffer.
		 * The max size of the answer is
		 *   2 (pointer) + 2 (type) + 2 (class) +
		 *   4 (ttl) + 2 (rdlen) + addr (16 or 4) = 28
		 * for A or AAAA record.
		 * For CNAME the size can be bigger.
		 */
		unsigned char rsp[NS_MAXCDNAME];
		unsigned int rsp_len = sizeof(rsp) - 1;
		int ret, rdlen;

		memset(rsp, 0, sizeof(rsp));

		ret = parse_rr(buf, ptr, buf + buflen, rsp, &rsp_len,
			type, class, ttl, &rdlen, &next, name);
		if (ret != 0) {
			err = ret;
			goto out;
		}

		/*
		 * Now rsp contains compressed or uncompressed resource
		 * record. Next we check if this record answers the question.
		 * The name var contains the uncompressed label.
		 * One tricky bit is the CNAME records as they alias
		 * the name we might be interested in.
		 */

		/*
		 * Go to next answer if the class is not the one we are
		 * looking for.
		 */
		if (*class != qclass) {
			ptr = next;
			next = NULL;
			continue;
		}

		/*
		 * Try to resolve aliases also, type is CNAME(5).
		 * This is important as otherwise the aliased names would not
		 * be cached at all as the cache would not contain the aliased
		 * question.
		 *
		 * If any CNAME is found in DNS packet, then we cache the alias
		 * IP address instead of the question (as the server
		 * said that question has only an alias).
		 * This means in practice that if e.g., ipv6.google.com is
		 * queried, DNS server returns CNAME of that name which is
		 * ipv6.l.google.com. We then cache the address of the CNAME
		 * but return the question name to client. So the alias
		 * status of the name is not saved in cache and thus not
		 * returned to the client. We do not return DNS packets from
		 * cache to client saying that ipv6.google.com is an alias to
		 * ipv6.l.google.com but we return instead a DNS packet that
		 * says ipv6.google.com has address xxx which is in fact the
		 * address of ipv6.l.google.com. For caching purposes this
		 * should not cause any issues.
		 */
		if (*type == 5 && strncmp(question, name, qlen) == 0) {
			/*
			 * So now the alias answered the question. This is
			 * not very useful from caching point of view as
			 * the following A or AAAA records will not match the
			 * question. We need to find the real A/AAAA record
			 * of the alias and cache that.
			 */
			unsigned char *end = NULL;
			int name_len = 0, output_len = 0;

			memset(rsp, 0, sizeof(rsp));
			rsp_len = sizeof(rsp) - 1;

			/*
			 * Alias is in rdata part of the message,
			 * and next-rdlen points to it. So we need to get
			 * the real name of the alias.
			 */
			ret = get_name(0, buf, next - rdlen, buf + buflen,
					rsp, rsp_len, &output_len, &end,
					name, &name_len);
			if (ret != 0) {
				/* just ignore the error at this point */
				ptr = next;
				next = NULL;
				continue;
			}

			/*
			 * We should now have the alias of the entry we might
			 * want to cache. Just remember it for a while.
			 * We check the alias list when we have parsed the
			 * A or AAAA record.
			 */
			aliases = g_slist_prepend(aliases, g_strdup(name));

			ptr = next;
			next = NULL;
			continue;
		}

		if (*type == qtype) {
			/*
			 * We found correct type (A or AAAA)
			 */
			if (check_alias(aliases, name) ||
				(!aliases && strncmp(question, name,
							qlen) == 0)) {
				/*
				 * We found an alias or the name of the rr
				 * matches the question. If so, we append
				 * the compressed label to the cache.
				 * The end result is a response buffer that
				 * will contain one or more cached and
				 * compressed resource records.
				 */
				if (*response_len + rsp_len > maxlen) {
					err = -ENOBUFS;
					goto out;
				}
				memcpy(response + *response_len, rsp, rsp_len);
				*response_len += rsp_len;
				(*answers)++;
				err = 0;
			}
		}

		ptr = next;
		next = NULL;
	}

out:
	for (list = aliases; list; list = list->next)
		g_free(list->data);
	g_slist_free(aliases);

	return err;
}

struct cache_timeout {
	time_t current_time;
	int max_timeout;
	int try_harder;
};

static gboolean cache_check_entry(gpointer key, gpointer value,
					gpointer user_data)
{
	struct cache_timeout *data = user_data;
	struct cache_entry *entry = value;
	int max_timeout;

	/* Scale the number of hits by half as part of cache aging */

	entry->hits /= 2;

	/*
	 * If either IPv4 or IPv6 cached entry has expired, we
	 * remove both from the cache.
	 */

	if (entry->ipv4 && entry->ipv4->timeout > 0) {
		max_timeout = entry->ipv4->cache_until;
		if (max_timeout > data->max_timeout)
			data->max_timeout = max_timeout;

		if (entry->ipv4->cache_until < data->current_time)
			return TRUE;
	}

	if (entry->ipv6 && entry->ipv6->timeout > 0) {
		max_timeout = entry->ipv6->cache_until;
		if (max_timeout > data->max_timeout)
			data->max_timeout = max_timeout;

		if (entry->ipv6->cache_until < data->current_time)
			return TRUE;
	}

	/*
	 * if we're asked to try harder, also remove entries that have
	 * few hits
	 */
	if (data->try_harder && entry->hits < 4)
		return TRUE;

	return FALSE;
}

static void cache_cleanup(void)
{
	static int max_timeout;
	struct cache_timeout data;
	int count = 0;

	data.current_time = time(NULL);
	data.max_timeout = 0;
	data.try_harder = 0;

	/*
	 * In the first pass, we only remove entries that have timed out.
	 * We use a cache of the first time to expire to do this only
	 * when it makes sense.
	 */
	if (max_timeout <= data.current_time) {
		count = g_hash_table_foreach_remove(cache, cache_check_entry,
						&data);
	}
	DBG("removed %d in the first pass", count);

	/*
	 * In the second pass, if the first pass turned up blank,
	 * we also expire entries with a low hit count,
	 * while aging the hit count at the same time.
	 */
	data.try_harder = 1;
	if (count == 0)
		count = g_hash_table_foreach_remove(cache, cache_check_entry,
						&data);

	if (count == 0)
		/*
		 * If we could not remove anything, then remember
		 * what is the max timeout and do nothing if we
		 * have not yet reached it. This will prevent
		 * constant traversal of the cache if it is full.
		 */
		max_timeout = data.max_timeout;
	else
		max_timeout = 0;
}

static gboolean cache_invalidate_entry(gpointer key, gpointer value,
					gpointer user_data)
{
	struct cache_entry *entry = value;

	/* first, delete any expired elements */
	cache_enforce_validity(entry);

	/* if anything is not expired, mark the entry for refresh */
	if (entry->hits > 0 && (entry->ipv4 || entry->ipv6))
		entry->want_refresh = true;

	/* delete the cached data */
	if (entry->ipv4) {
		g_free(entry->ipv4->data);
		g_free(entry->ipv4);
		entry->ipv4 = NULL;
	}

	if (entry->ipv6) {
		g_free(entry->ipv6->data);
		g_free(entry->ipv6);
		entry->ipv6 = NULL;
	}

	/* keep the entry if we want it refreshed, delete it otherwise */
	if (entry->want_refresh)
		return FALSE;
	else
		return TRUE;
}

/*
 * cache_invalidate is called from places where the DNS landscape
 * has changed, say because connections are added or we entered a VPN.
 * The logic is to wipe all cache data, but mark all non-expired
 * parts of the cache for refresh rather than deleting the whole cache.
 */
static void cache_invalidate(void)
{
	DBG("Invalidating the DNS cache %p", cache);

	if (!cache)
		return;

	g_hash_table_foreach_remove(cache, cache_invalidate_entry, NULL);
}

static void cache_refresh_entry(struct cache_entry *entry)
{

	cache_enforce_validity(entry);

	if (entry->hits > 2 && !entry->ipv4)
		entry->want_refresh = true;
	if (entry->hits > 2 && !entry->ipv6)
		entry->want_refresh = true;

	if (entry->want_refresh) {
		char *c;
		char dns_name[NS_MAXDNAME + 1];
		entry->want_refresh = false;

		/* turn a DNS name into a hostname with dots */
		strncpy(dns_name, entry->key, NS_MAXDNAME);
		c = dns_name;
		while (c && *c) {
			int jump;
			jump = *c;
			*c = '.';
			c += jump + 1;
		}
		DBG("Refreshing %s\n", dns_name);
		/* then refresh the hostname */
		refresh_dns_entry(entry, &dns_name[1]);
	}
}

static void cache_refresh_iterator(gpointer key, gpointer value,
					gpointer user_data)
{
	struct cache_entry *entry = value;

	cache_refresh_entry(entry);
}

static void cache_refresh(void)
{
	if (!cache)
		return;

	g_hash_table_foreach(cache, cache_refresh_iterator, NULL);
}

static int reply_query_type(unsigned char *msg, int len)
{
	unsigned char *c;
	int l;
	int type;

	/* skip the header */
	c = msg + sizeof(struct domain_hdr);
	len -= sizeof(struct domain_hdr);

	if (len < 0)
		return 0;

	/* now the query, which is a name and 2 16 bit words */
	l = dns_name_length(c) + 1;
	c += l;
	type = c[0] << 8 | c[1];

	return type;
}

static int cache_update(struct server_data *srv, unsigned char *msg,
			unsigned int msg_len)
{
	int offset = protocol_offset(srv->protocol);
	int err, qlen, ttl = 0;
	uint16_t answers = 0, type = 0, class = 0;
	struct domain_hdr *hdr = (void *)(msg + offset);
	struct domain_question *q;
	struct cache_entry *entry;
	struct cache_data *data;
	char question[NS_MAXDNAME + 1];
	unsigned char response[NS_MAXDNAME + 1];
	unsigned char *ptr;
	unsigned int rsplen;
	bool new_entry = true;
	time_t current_time;

	if (cache_size >= MAX_CACHE_SIZE) {
		cache_cleanup();
		if (cache_size >= MAX_CACHE_SIZE)
			return 0;
	}

	current_time = time(NULL);

	/* don't do a cache refresh more than twice a minute */
	if (next_refresh < current_time) {
		cache_refresh();
		next_refresh = current_time + 30;
	}

	if (offset < 0)
		return 0;

	DBG("offset %d hdr %p msg %p rcode %d", offset, hdr, msg, hdr->rcode);

	/* Continue only if response code is 0 (=ok) */
	if (hdr->rcode != ns_r_noerror)
		return 0;

	if (!cache)
		create_cache();

	rsplen = sizeof(response) - 1;
	question[sizeof(question) - 1] = '\0';

	err = parse_response(msg + offset, msg_len - offset,
				question, sizeof(question) - 1,
				&type, &class, &ttl,
				response, &rsplen, &answers);

	/*
	 * special case: if we do a ipv6 lookup and get no result
	 * for a record that's already in our ipv4 cache.. we want
	 * to cache the negative response.
	 */
	if ((err == -ENOMSG || err == -ENOBUFS) &&
			reply_query_type(msg + offset,
					msg_len - offset) == 28) {
		entry = g_hash_table_lookup(cache, question);
		if (entry && entry->ipv4 && !entry->ipv6) {
			int cache_offset = 0;

			data = g_try_new(struct cache_data, 1);
			if (!data)
				return -ENOMEM;
			data->inserted = entry->ipv4->inserted;
			data->type = type;
			data->answers = ntohs(hdr->ancount);
			data->timeout = entry->ipv4->timeout;
			if (srv->protocol == IPPROTO_UDP)
				cache_offset = 2;
			data->data_len = msg_len + cache_offset;
			data->data = ptr = g_malloc(data->data_len);
			ptr[0] = (data->data_len - 2) / 256;
			ptr[1] = (data->data_len - 2) - ptr[0] * 256;
			if (srv->protocol == IPPROTO_UDP)
				ptr += 2;
			data->valid_until = entry->ipv4->valid_until;
			data->cache_until = entry->ipv4->cache_until;
			memcpy(ptr, msg, msg_len);
			entry->ipv6 = data;
			/*
			 * we will get a "hit" when we serve the response
			 * out of the cache
			 */
			entry->hits--;
			if (entry->hits < 0)
				entry->hits = 0;
			return 0;
		}
	}

	if (err < 0 || ttl == 0)
		return 0;

	qlen = strlen(question);

	/*
	 * If the cache contains already data, check if the
	 * type of the cached data is the same and do not add
	 * to cache if data is already there.
	 * This is needed so that we can cache both A and AAAA
	 * records for the same name.
	 */
	entry = g_hash_table_lookup(cache, question);
	if (!entry) {
		entry = g_try_new(struct cache_entry, 1);
		if (!entry)
			return -ENOMEM;

		data = g_try_new(struct cache_data, 1);
		if (!data) {
			g_free(entry);
			return -ENOMEM;
		}

		entry->key = g_strdup(question);
		entry->ipv4 = entry->ipv6 = NULL;
		entry->want_refresh = false;
		entry->hits = 0;

		if (type == 1)
			entry->ipv4 = data;
		else
			entry->ipv6 = data;
	} else {
		if (type == 1 && entry->ipv4)
			return 0;

		if (type == 28 && entry->ipv6)
			return 0;

		data = g_try_new(struct cache_data, 1);
		if (!data)
			return -ENOMEM;

		if (type == 1)
			entry->ipv4 = data;
		else
			entry->ipv6 = data;

		/*
		 * compensate for the hit we'll get for serving
		 * the response out of the cache
		 */
		entry->hits--;
		if (entry->hits < 0)
			entry->hits = 0;

		new_entry = false;
	}

	if (ttl < MIN_CACHE_TTL)
		ttl = MIN_CACHE_TTL;

	data->inserted = current_time;
	data->type = type;
	data->answers = answers;
	data->timeout = ttl;
	/*
	 * The "2" in start of the length is the TCP offset. We allocate it
	 * here even for UDP packet because it simplifies the sending
	 * of cached packet.
	 */
	data->data_len = 2 + 12 + qlen + 1 + 2 + 2 + rsplen;
	data->data = ptr = g_malloc(data->data_len);
	data->valid_until = current_time + ttl;

	/*
	 * Restrict the cached DNS record TTL to some sane value
	 * in order to prevent data staying in the cache too long.
	 */
	if (ttl > MAX_CACHE_TTL)
		ttl = MAX_CACHE_TTL;

	data->cache_until = round_down_ttl(current_time + ttl, ttl);

	if (!data->data) {
		g_free(entry->key);
		g_free(data);
		g_free(entry);
		return -ENOMEM;
	}

	/*
	 * We cache the two extra bytes at the start of the message
	 * in a TCP packet. When sending UDP packet, we skip the first
	 * two bytes. This way we do not need to know the format
	 * (UDP/TCP) of the cached message.
	 */
	if (srv->protocol == IPPROTO_UDP)
		memcpy(ptr + 2, msg, offset + 12);
	else
		memcpy(ptr, msg, offset + 12);

	ptr[0] = (data->data_len - 2) / 256;
	ptr[1] = (data->data_len - 2) - ptr[0] * 256;
	if (srv->protocol == IPPROTO_UDP)
		ptr += 2;

	memcpy(ptr + offset + 12, question, qlen + 1); /* copy also the \0 */

	q = (void *) (ptr + offset + 12 + qlen + 1);
	q->type = htons(type);
	q->class = htons(class);
	memcpy(ptr + offset + 12 + qlen + 1 + sizeof(struct domain_question),
		response, rsplen);

	if (new_entry) {
		g_hash_table_replace(cache, entry->key, entry);
		cache_size++;
	}

	DBG("cache %d %squestion \"%s\" type %d ttl %d size %zd packet %u "
								"dns len %u",
		cache_size, new_entry ? "new " : "old ",
		question, type, ttl,
		sizeof(*entry) + sizeof(*data) + data->data_len + qlen,
		data->data_len,
		srv->protocol == IPPROTO_TCP ?
			(unsigned int)(data->data[0] * 256 + data->data[1]) :
			data->data_len);

	return 0;
}

static int ns_resolv(struct server_data *server, struct request_data *req,
				gpointer request, gpointer name)
{
	GList *list;
	int sk, err, type = 0;
	char *dot, *lookup = (char *) name;
	struct cache_entry *entry;

	entry = cache_check(request, &type, req->protocol);
	if (entry) {
		int ttl_left = 0;
		struct cache_data *data;

		DBG("cache hit %s type %s", lookup, type == 1 ? "A" : "AAAA");
		if (type == 1)
			data = entry->ipv4;
		else
			data = entry->ipv6;

		if (data) {
			ttl_left = data->valid_until - time(NULL);
			entry->hits++;
		}

		if (data && req->protocol == IPPROTO_TCP) {
			send_cached_response(req->client_sk, data->data,
					data->data_len, NULL, 0, IPPROTO_TCP,
					req->srcid, data->answers, ttl_left);
			return 1;
		}

		if (data && req->protocol == IPPROTO_UDP) {
			int udp_sk = get_req_udp_socket(req);

			if (udp_sk < 0)
				return -EIO;

			send_cached_response(udp_sk, data->data,
				data->data_len, &req->sa, req->sa_len,
				IPPROTO_UDP, req->srcid, data->answers,
				ttl_left);
			return 1;
		}
	}

	sk = g_io_channel_unix_get_fd(server->channel);

	err = sendto(sk, request, req->request_len, MSG_NOSIGNAL,
			server->server_addr, server->server_addr_len);
	if (err < 0) {
		DBG("Cannot send message to server %s sock %d "
			"protocol %d (%s/%d)",
			server->server, sk, server->protocol,
			strerror(errno), errno);
		return -EIO;
	}

	req->numserv++;

	/* If we have more than one dot, we don't add domains */
	dot = strchr(lookup, '.');
	if (dot && dot != lookup + strlen(lookup) - 1)
		return 0;

	if (server->domains && server->domains->data)
		req->append_domain = true;

	for (list = server->domains; list; list = list->next) {
		char *domain;
		unsigned char alt[1024];
		struct domain_hdr *hdr = (void *) &alt;
		int altlen, domlen, offset;

		domain = list->data;

		if (!domain)
			continue;

		offset = protocol_offset(server->protocol);
		if (offset < 0)
			return offset;

		domlen = strlen(domain) + 1;
		if (domlen < 5)
			return -EINVAL;

		alt[offset] = req->altid & 0xff;
		alt[offset + 1] = req->altid >> 8;

		memcpy(alt + offset + 2, request + offset + 2, 10);
		hdr->qdcount = htons(1);

		altlen = append_query(alt + offset + 12, sizeof(alt) - 12,
					name, domain);
		if (altlen < 0)
			return -EINVAL;

		altlen += 12;

		memcpy(alt + offset + altlen,
			request + offset + altlen - domlen,
				req->request_len - altlen - offset + domlen);

		if (server->protocol == IPPROTO_TCP) {
			int req_len = req->request_len + domlen - 2;

			alt[0] = (req_len >> 8) & 0xff;
			alt[1] = req_len & 0xff;
		}

		DBG("req %p dstid 0x%04x altid 0x%04x", req, req->dstid,
				req->altid);

		err = send(sk, alt, req->request_len + domlen, MSG_NOSIGNAL);
		if (err < 0)
			return -EIO;

		req->numserv++;
	}

	return 0;
}

static char *convert_label(char *start, char *end, char *ptr, char *uptr,
			int remaining_len, int *used_comp, int *used_uncomp)
{
	int pos, comp_pos;
	char name[NS_MAXLABEL];

	pos = dn_expand((u_char *)start, (u_char *)end, (u_char *)ptr,
			name, NS_MAXLABEL);
	if (pos < 0) {
		DBG("uncompress error [%d/%s]", errno, strerror(errno));
		goto out;
	}

	/*
	 * We need to compress back the name so that we get back to internal
	 * label presentation.
	 */
	comp_pos = dn_comp(name, (u_char *)uptr, remaining_len, NULL, NULL);
	if (comp_pos < 0) {
		DBG("compress error [%d/%s]", errno, strerror(errno));
		goto out;
	}

	*used_comp = pos;
	*used_uncomp = comp_pos;

	return ptr;

out:
	return NULL;
}

static char *uncompress(int16_t field_count, char *start, char *end,
			char *ptr, char *uncompressed, int uncomp_len,
			char **uncompressed_ptr)
{
	char *uptr = *uncompressed_ptr; /* position in result buffer */

	DBG("count %d ptr %p end %p uptr %p", field_count, ptr, end, uptr);

	while (field_count-- > 0 && ptr < end) {
		int dlen;		/* data field length */
		int ulen;		/* uncompress length */
		int pos;		/* position in compressed string */
		char name[NS_MAXLABEL]; /* tmp label */
		uint16_t dns_type, dns_class;
		int comp_pos;

		if (!convert_label(start, end, ptr, name, NS_MAXLABEL,
					&pos, &comp_pos))
			goto out;

		/*
		 * Copy the uncompressed resource record, type, class and \0 to
		 * tmp buffer.
		 */

		ulen = strlen(name);
		strncpy(uptr, name, uncomp_len - (uptr - uncompressed));

		DBG("pos %d ulen %d left %d name %s", pos, ulen,
			(int)(uncomp_len - (uptr - uncompressed)), uptr);

		uptr += ulen;
		*uptr++ = '\0';

		ptr += pos;

		/*
		 * We copy also the fixed portion of the result (type, class,
		 * ttl, address length and the address)
		 */
		memcpy(uptr, ptr, NS_RRFIXEDSZ);

		dns_type = uptr[0] << 8 | uptr[1];
		dns_class = uptr[2] << 8 | uptr[3];

		if (dns_class != ns_c_in)
			goto out;

		ptr += NS_RRFIXEDSZ;
		uptr += NS_RRFIXEDSZ;

		/*
		 * Then the variable portion of the result (data length).
		 * Typically this portion is also compressed
		 * so we need to uncompress it also when necessary.
		 */
		if (dns_type == ns_t_cname) {
			if (!convert_label(start, end, ptr, uptr,
					uncomp_len - (uptr - uncompressed),
						&pos, &comp_pos))
				goto out;

			uptr[-2] = comp_pos << 8;
			uptr[-1] = comp_pos & 0xff;

			uptr += comp_pos;
			ptr += pos;

		} else if (dns_type == ns_t_a || dns_type == ns_t_aaaa) {
			dlen = uptr[-2] << 8 | uptr[-1];

			if (ptr + dlen > end) {
				DBG("data len %d too long", dlen);
				goto out;
			}

			memcpy(uptr, ptr, dlen);
			uptr += dlen;
			ptr += dlen;

		} else if (dns_type == ns_t_soa) {
			int total_len = 0;
			char *len_ptr;

			/* Primary name server expansion */
			if (!convert_label(start, end, ptr, uptr,
					uncomp_len - (uptr - uncompressed),
						&pos, &comp_pos))
				goto out;

			total_len += comp_pos;
			len_ptr = &uptr[-2];
			ptr += pos;
			uptr += comp_pos;

			/* Responsible authority's mailbox */
			if (!convert_label(start, end, ptr, uptr,
					uncomp_len - (uptr - uncompressed),
						&pos, &comp_pos))
				goto out;

			total_len += comp_pos;
			ptr += pos;
			uptr += comp_pos;

			/*
			 * Copy rest of the soa fields (serial number,
			 * refresh interval, retry interval, expiration
			 * limit and minimum ttl). They are 20 bytes long.
			 */
			memcpy(uptr, ptr, 20);
			uptr += 20;
			ptr += 20;
			total_len += 20;

			/*
			 * Finally fix the length of the data part
			 */
			len_ptr[0] = total_len << 8;
			len_ptr[1] = total_len & 0xff;
		}

		*uncompressed_ptr = uptr;
	}

	return ptr;

out:
	return NULL;
}

static int strip_domains(char *name, char *answers, int maxlen)
{
	uint16_t data_len;
	int name_len = strlen(name);
	char *ptr, *start = answers, *end = answers + maxlen;

	while (maxlen > 0) {
		ptr = strstr(answers, name);
		if (ptr) {
			char *domain = ptr + name_len;

			if (*domain) {
				int domain_len = strlen(domain);

				memmove(answers + name_len,
					domain + domain_len,
					end - (domain + domain_len));

				end -= domain_len;
				maxlen -= domain_len;
			}
		}

		answers += strlen(answers) + 1;
		answers += 2 + 2 + 4;  /* skip type, class and ttl fields */

		data_len = answers[0] << 8 | answers[1];
		answers += 2; /* skip the length field */

		if (answers + data_len > end)
			return -EINVAL;

		answers += data_len;
		maxlen -= answers - ptr;
	}

	return end - start;
}

static int forward_dns_reply(unsigned char *reply, int reply_len, int protocol,
				struct server_data *data)
{
	struct domain_hdr *hdr;
	struct request_data *req;
	int dns_id, sk, err, offset = protocol_offset(protocol);

	if (offset < 0)
		return offset;

	hdr = (void *)(reply + offset);
	dns_id = reply[offset] | reply[offset + 1] << 8;

	DBG("Received %d bytes (id 0x%04x)", reply_len, dns_id);

	req = find_request(dns_id);
	if (!req)
		return -EINVAL;

	DBG("req %p dstid 0x%04x altid 0x%04x rcode %d",
			req, req->dstid, req->altid, hdr->rcode);

	reply[offset] = req->srcid & 0xff;
	reply[offset + 1] = req->srcid >> 8;

	req->numresp++;

	if (hdr->rcode == ns_r_noerror || !req->resp) {
		unsigned char *new_reply = NULL;

		/*
		 * If the domain name was append
		 * remove it before forwarding the reply.
		 * If there were more than one question, then this
		 * domain name ripping can be hairy so avoid that
		 * and bail out in that that case.
		 *
		 * The reason we are doing this magic is that if the
		 * user's DNS client tries to resolv hostname without
		 * domain part, it also expects to get the result without
		 * a domain name part.
		 */
		if (req->append_domain && ntohs(hdr->qdcount) == 1) {
			uint16_t domain_len = 0;
			uint16_t header_len;
			uint16_t dns_type, dns_class;
			uint8_t host_len, dns_type_pos;
			char uncompressed[NS_MAXDNAME], *uptr;
			char *ptr, *eom = (char *)reply + reply_len;

			/*
			 * ptr points to the first char of the hostname.
			 * ->hostname.domain.net
			 */
			header_len = offset + sizeof(struct domain_hdr);
			ptr = (char *)reply + header_len;

			host_len = *ptr;
			if (host_len > 0)
				domain_len = strnlen(ptr + 1 + host_len,
						reply_len - header_len);

			/*
			 * If the query type is anything other than A or AAAA,
			 * then bail out and pass the message as is.
			 * We only want to deal with IPv4 or IPv6 addresses.
			 */
			dns_type_pos = host_len + 1 + domain_len + 1;

			dns_type = ptr[dns_type_pos] << 8 |
							ptr[dns_type_pos + 1];
			dns_class = ptr[dns_type_pos + 2] << 8 |
							ptr[dns_type_pos + 3];
			if (dns_type != ns_t_a && dns_type != ns_t_aaaa &&
					dns_class != ns_c_in) {
				DBG("Pass msg dns type %d class %d",
					dns_type, dns_class);
				goto pass;
			}

			/*
			 * Remove the domain name and replace it by the end
			 * of reply. Check if the domain is really there
			 * before trying to copy the data. We also need to
			 * uncompress the answers if necessary.
			 * The domain_len can be 0 because if the original
			 * query did not contain a domain name, then we are
			 * sending two packets, first without the domain name
			 * and the second packet with domain name.
			 * The append_domain is set to true even if we sent
			 * the first packet without domain name. In this
			 * case we end up in this branch.
			 */
			if (domain_len > 0) {
				int len = host_len + 1;
				int new_len, fixed_len;
				char *answers;

				/*
				 * First copy host (without domain name) into
				 * tmp buffer.
				 */
				uptr = &uncompressed[0];
				memcpy(uptr, ptr, len);

				uptr[len] = '\0'; /* host termination */
				uptr += len + 1;

				/*
				 * Copy type and class fields of the question.
				 */
				ptr += len + domain_len + 1;
				memcpy(uptr, ptr, NS_QFIXEDSZ);

				/*
				 * ptr points to answers after this
				 */
				ptr += NS_QFIXEDSZ;
				uptr += NS_QFIXEDSZ;
				answers = uptr;
				fixed_len = answers - uncompressed;

				/*
				 * We then uncompress the result to buffer
				 * so that we can rip off the domain name
				 * part from the question. First answers,
				 * then name server (authority) information,
				 * and finally additional record info.
				 */

				ptr = uncompress(ntohs(hdr->ancount),
						(char *)reply + offset, eom,
						ptr, uncompressed, NS_MAXDNAME,
						&uptr);
				if (ptr == NULL)
					goto out;

				ptr = uncompress(ntohs(hdr->nscount),
						(char *)reply + offset, eom,
						ptr, uncompressed, NS_MAXDNAME,
						&uptr);
				if (ptr == NULL)
					goto out;

				ptr = uncompress(ntohs(hdr->arcount),
						(char *)reply + offset, eom,
						ptr, uncompressed, NS_MAXDNAME,
						&uptr);
				if (ptr == NULL)
					goto out;

				/*
				 * The uncompressed buffer now contains almost
				 * valid response. Final step is to get rid of
				 * the domain name because at least glibc
				 * gethostbyname() implementation does extra
				 * checks and expects to find an answer without
				 * domain name if we asked a query without
				 * domain part. Note that glibc getaddrinfo()
				 * works differently and accepts FQDN in answer
				 */
				new_len = strip_domains(uncompressed, answers,
							uptr - answers);
				if (new_len < 0) {
					DBG("Corrupted packet");
					return -EINVAL;
				}

				/*
				 * Because we have now uncompressed the answers
				 * we might have to create a bigger buffer to
				 * hold all that data.
				 */

				reply_len = header_len + new_len + fixed_len;

				new_reply = g_try_malloc(reply_len);
				if (!new_reply)
					return -ENOMEM;

				memcpy(new_reply, reply, header_len);
				memcpy(new_reply + header_len, uncompressed,
					new_len + fixed_len);

				reply = new_reply;
			}
		}

	pass:
		g_free(req->resp);
		req->resplen = 0;

		req->resp = g_try_malloc(reply_len);
		if (!req->resp)
			return -ENOMEM;

		memcpy(req->resp, reply, reply_len);
		req->resplen = reply_len;

		cache_update(data, reply, reply_len);

		g_free(new_reply);
	}

out:
	if (req->numresp < req->numserv) {
		if (hdr->rcode > ns_r_noerror) {
			return -EINVAL;
		} else if (hdr->ancount == 0 && req->append_domain) {
			return -EINVAL;
		}
	}

	request_list = g_slist_remove(request_list, req);

	if (protocol == IPPROTO_UDP) {
		sk = get_req_udp_socket(req);
		if (sk < 0) {
			errno = -EIO;
			err = -EIO;
		} else
			err = sendto(sk, req->resp, req->resplen, 0,
				&req->sa, req->sa_len);
	} else {
		sk = req->client_sk;
		err = send(sk, req->resp, req->resplen, MSG_NOSIGNAL);
	}

	if (err < 0)
		DBG("Cannot send msg, sk %d proto %d errno %d/%s", sk,
			protocol, errno, strerror(errno));
	else
		DBG("proto %d sent %d bytes to %d", protocol, err, sk);

	destroy_request_data(req);

	return err;
}

static void server_destroy_socket(struct server_data *data)
{
	DBG("index %d server %s proto %d", data->index,
					data->server, data->protocol);

	if (data->watch > 0) {
		g_source_remove(data->watch);
		data->watch = 0;
	}

	if (data->timeout > 0) {
		g_source_remove(data->timeout);
		data->timeout = 0;
	}

	if (data->channel) {
		g_io_channel_shutdown(data->channel, TRUE, NULL);
		g_io_channel_unref(data->channel);
		data->channel = NULL;
	}

	g_free(data->incoming_reply);
	data->incoming_reply = NULL;
}

static void destroy_server(struct server_data *server)
{
	DBG("index %d server %s sock %d", server->index, server->server,
			server->channel ?
			g_io_channel_unix_get_fd(server->channel): -1);

	server_list = g_slist_remove(server_list, server);
	server_destroy_socket(server);

	if (server->protocol == IPPROTO_UDP && server->enabled)
		DBG("Removing DNS server %s", server->server);

	g_free(server->server);
	g_list_free_full(server->domains, g_free);
	g_free(server->server_addr);

	/*
	 * We do not remove cache right away but delay it few seconds.
	 * The idea is that when IPv6 DNS server is added via RDNSS, it has a
	 * lifetime. When the lifetime expires we decrease the refcount so it
	 * is possible that the cache is then removed. Because a new DNS server
	 * is usually created almost immediately we would then loose the cache
	 * without any good reason. The small delay allows the new RDNSS to
	 * create a new DNS server instance and the refcount does not go to 0.
	 */
	if (cache && !cache_timer)
		cache_timer = g_timeout_add_seconds(3, try_remove_cache, NULL);

	g_free(server);
}

static gboolean udp_server_event(GIOChannel *channel, GIOCondition condition,
							gpointer user_data)
{
	unsigned char buf[4096];
	int sk, err, len;
	struct server_data *data = user_data;

	if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		connman_error("Error with UDP server %s", data->server);
		server_destroy_socket(data);
		return FALSE;
	}

	sk = g_io_channel_unix_get_fd(channel);

	len = recv(sk, buf, sizeof(buf), 0);
	if (len < 12)
		return TRUE;

	err = forward_dns_reply(buf, len, IPPROTO_UDP, data);
	if (err < 0)
		return TRUE;

	return TRUE;
}

static gboolean tcp_server_event(GIOChannel *channel, GIOCondition condition,
							gpointer user_data)
{
	int sk;
	struct server_data *server = user_data;

	sk = g_io_channel_unix_get_fd(channel);
	if (sk == 0)
		return FALSE;

	if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		GSList *list;
hangup:
		DBG("TCP server channel closed, sk %d", sk);

		/*
		 * Discard any partial response which is buffered; better
		 * to get a proper response from a working server.
		 */
		g_free(server->incoming_reply);
		server->incoming_reply = NULL;

		for (list = request_list; list; list = list->next) {
			struct request_data *req = list->data;
			struct domain_hdr *hdr;

			if (req->protocol == IPPROTO_UDP)
				continue;

			if (!req->request)
				continue;

			/*
			 * If we're not waiting for any further response
			 * from another name server, then we send an error
			 * response to the client.
			 */
			if (req->numserv && --(req->numserv))
				continue;

			hdr = (void *) (req->request + 2);
			hdr->id = req->srcid;
			send_response(req->client_sk, req->request,
				req->request_len, NULL, 0, IPPROTO_TCP);

			request_list = g_slist_remove(request_list, req);
		}

		destroy_server(server);

		return FALSE;
	}

	if ((condition & G_IO_OUT) && !server->connected) {
		GSList *list;
		GList *domains;
		bool no_request_sent = true;
		struct server_data *udp_server;

		udp_server = find_server(server->index, server->server,
								IPPROTO_UDP);
		if (udp_server) {
			for (domains = udp_server->domains; domains;
						domains = domains->next) {
				char *dom = domains->data;

				DBG("Adding domain %s to %s",
						dom, server->server);

				server->domains = g_list_append(server->domains,
								g_strdup(dom));
			}
		}

		server->connected = true;
		server_list = g_slist_append(server_list, server);

		if (server->timeout > 0) {
			g_source_remove(server->timeout);
			server->timeout = 0;
		}

		for (list = request_list; list; ) {
			struct request_data *req = list->data;
			int status;

			if (req->protocol == IPPROTO_UDP) {
				list = list->next;
				continue;
			}

			DBG("Sending req %s over TCP", (char *)req->name);

			status = ns_resolv(server, req,
						req->request, req->name);
			if (status > 0) {
				/*
				 * A cached result was sent,
				 * so the request can be released
				 */
				list = list->next;
				request_list = g_slist_remove(request_list, req);
				destroy_request_data(req);
				continue;
			}

			if (status < 0) {
				list = list->next;
				continue;
			}

			no_request_sent = false;

			if (req->timeout > 0)
				g_source_remove(req->timeout);

			req->timeout = g_timeout_add_seconds(30,
						request_timeout, req);
			list = list->next;
		}

		if (no_request_sent) {
			destroy_server(server);
			return FALSE;
		}

	} else if (condition & G_IO_IN) {
		struct partial_reply *reply = server->incoming_reply;
		int bytes_recv;

		if (!reply) {
			unsigned char reply_len_buf[2];
			uint16_t reply_len;

			bytes_recv = recv(sk, reply_len_buf, 2, MSG_PEEK);
			if (!bytes_recv) {
				goto hangup;
			} else if (bytes_recv < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					return TRUE;

				connman_error("DNS proxy error %s",
						strerror(errno));
				goto hangup;
			} else if (bytes_recv < 2)
				return TRUE;

			reply_len = reply_len_buf[1] | reply_len_buf[0] << 8;
			reply_len += 2;

			DBG("TCP reply %d bytes from %d", reply_len, sk);

			reply = g_try_malloc(sizeof(*reply) + reply_len + 2);
			if (!reply)
				return TRUE;

			reply->len = reply_len;
			reply->received = 0;

			server->incoming_reply = reply;
		}

		while (reply->received < reply->len) {
			bytes_recv = recv(sk, reply->buf + reply->received,
					reply->len - reply->received, 0);
			if (!bytes_recv) {
				connman_error("DNS proxy TCP disconnect");
				break;
			} else if (bytes_recv < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					return TRUE;

				connman_error("DNS proxy error %s",
						strerror(errno));
				break;
			}
			reply->received += bytes_recv;
		}

		forward_dns_reply(reply->buf, reply->received, IPPROTO_TCP,
					server);

		g_free(reply);
		server->incoming_reply = NULL;

		destroy_server(server);

		return FALSE;
	}

	return TRUE;
}

static gboolean tcp_idle_timeout(gpointer user_data)
{
	struct server_data *server = user_data;

	DBG("");

	if (!server)
		return FALSE;

	destroy_server(server);

	return FALSE;
}

static int server_create_socket(struct server_data *data)
{
	int sk, err;
	char *interface;

	DBG("index %d server %s proto %d", data->index,
					data->server, data->protocol);

	sk = socket(data->server_addr->sa_family,
		data->protocol == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM,
		data->protocol);
	if (sk < 0) {
		err = errno;
		connman_error("Failed to create server %s socket",
							data->server);
		server_destroy_socket(data);
		return -err;
	}

	DBG("sk %d", sk);

	interface = connman_inet_ifname(data->index);
	if (interface) {
		if (setsockopt(sk, SOL_SOCKET, SO_BINDTODEVICE,
					interface,
					strlen(interface) + 1) < 0) {
			err = errno;
			connman_error("Failed to bind server %s "
						"to interface %s",
						data->server, interface);
			close(sk);
			server_destroy_socket(data);
			g_free(interface);
			return -err;
		}
		g_free(interface);
	}

	data->channel = g_io_channel_unix_new(sk);
	if (!data->channel) {
		connman_error("Failed to create server %s channel",
							data->server);
		close(sk);
		server_destroy_socket(data);
		return -ENOMEM;
	}

	g_io_channel_set_close_on_unref(data->channel, TRUE);

	if (data->protocol == IPPROTO_TCP) {
		g_io_channel_set_flags(data->channel, G_IO_FLAG_NONBLOCK, NULL);
		data->watch = g_io_add_watch(data->channel,
			G_IO_OUT | G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR,
						tcp_server_event, data);
		data->timeout = g_timeout_add_seconds(30, tcp_idle_timeout,
								data);
	} else
		data->watch = g_io_add_watch(data->channel,
			G_IO_IN | G_IO_NVAL | G_IO_ERR | G_IO_HUP,
						udp_server_event, data);

	if (connect(sk, data->server_addr, data->server_addr_len) < 0) {
		err = errno;

		if ((data->protocol == IPPROTO_TCP && errno != EINPROGRESS) ||
				data->protocol == IPPROTO_UDP) {

			connman_error("Failed to connect to server %s",
								data->server);
			server_destroy_socket(data);
			return -err;
		}
	}

	create_cache();

	return 0;
}

static struct server_data *create_server(int index,
					const char *domain, const char *server,
					int protocol)
{
	struct server_data *data;
	struct addrinfo hints, *rp;
	int ret;

	DBG("index %d server %s", index, server);

	data = g_try_new0(struct server_data, 1);
	if (!data) {
		connman_error("Failed to allocate server %s data", server);
		return NULL;
	}

	data->index = index;
	if (domain)
		data->domains = g_list_append(data->domains, g_strdup(domain));
	data->server = g_strdup(server);
	data->protocol = protocol;

	memset(&hints, 0, sizeof(hints));

	switch (protocol) {
	case IPPROTO_UDP:
		hints.ai_socktype = SOCK_DGRAM;
		break;

	case IPPROTO_TCP:
		hints.ai_socktype = SOCK_STREAM;
		break;

	default:
		destroy_server(data);
		return NULL;
	}
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICSERV | AI_NUMERICHOST;

	ret = getaddrinfo(data->server, "53", &hints, &rp);
	if (ret) {
		connman_error("Failed to parse server %s address: %s\n",
			      data->server, gai_strerror(ret));
		destroy_server(data);
		return NULL;
	}

	/* Do not blindly copy this code elsewhere; it doesn't loop over the
	   results using ->ai_next as it should. That's OK in *this* case
	   because it was a numeric lookup; we *know* there's only one. */

	data->server_addr_len = rp->ai_addrlen;

	switch (rp->ai_family) {
	case AF_INET:
		data->server_addr = (struct sockaddr *)
					g_try_new0(struct sockaddr_in, 1);
		break;
	case AF_INET6:
		data->server_addr = (struct sockaddr *)
					g_try_new0(struct sockaddr_in6, 1);
		break;
	default:
		connman_error("Wrong address family %d", rp->ai_family);
		break;
	}
	if (!data->server_addr) {
		freeaddrinfo(rp);
		destroy_server(data);
		return NULL;
	}
	memcpy(data->server_addr, rp->ai_addr, rp->ai_addrlen);
	freeaddrinfo(rp);

	if (server_create_socket(data) != 0) {
		destroy_server(data);
		return NULL;
	}

	if (protocol == IPPROTO_UDP) {
		if (__connman_service_index_is_default(data->index) ||
				__connman_service_index_is_split_routing(
								data->index)) {
			data->enabled = true;
			DBG("Adding DNS server %s", data->server);
		}

		server_list = g_slist_append(server_list, data);
	}

	return data;
}

static bool resolv(struct request_data *req,
				gpointer request, gpointer name)
{
	GSList *list;

	for (list = server_list; list; list = list->next) {
		struct server_data *data = list->data;

		if (data->protocol == IPPROTO_TCP) {
			DBG("server %s ignored proto TCP", data->server);
			continue;
		}

		DBG("server %s enabled %d", data->server, data->enabled);

		if (!data->enabled)
			continue;

		if (!data->channel && data->protocol == IPPROTO_UDP) {
			if (server_create_socket(data) < 0) {
				DBG("socket creation failed while resolving");
				continue;
			}
		}

		if (ns_resolv(data, req, request, name) > 0)
			return true;
	}

	return false;
}

static void append_domain(int index, const char *domain)
{
	GSList *list;

	DBG("index %d domain %s", index, domain);

	if (!domain)
		return;

	for (list = server_list; list; list = list->next) {
		struct server_data *data = list->data;
		GList *dom_list;
		char *dom;
		bool dom_found = false;

		if (data->index < 0)
			continue;

		if (data->index != index)
			continue;

		for (dom_list = data->domains; dom_list;
				dom_list = dom_list->next) {
			dom = dom_list->data;

			if (g_str_equal(dom, domain)) {
				dom_found = true;
				break;
			}
		}

		if (!dom_found) {
			data->domains =
				g_list_append(data->domains, g_strdup(domain));
		}
	}
}

int __connman_dnsproxy_append(int index, const char *domain,
							const char *server)
{
	struct server_data *data;

	DBG("index %d server %s", index, server);

	if (!server && !domain)
		return -EINVAL;

	if (!server) {
		append_domain(index, domain);

		return 0;
	}

	if (g_str_equal(server, "127.0.0.1"))
		return -ENODEV;

	if (g_str_equal(server, "::1"))
		return -ENODEV;

	data = find_server(index, server, IPPROTO_UDP);
	if (data) {
		append_domain(index, domain);
		return 0;
	}

	data = create_server(index, domain, server, IPPROTO_UDP);
	if (!data)
		return -EIO;

	return 0;
}

static void remove_server(int index, const char *domain,
			const char *server, int protocol)
{
	struct server_data *data;

	data = find_server(index, server, protocol);
	if (!data)
		return;

	destroy_server(data);
}

int __connman_dnsproxy_remove(int index, const char *domain,
							const char *server)
{
	DBG("index %d server %s", index, server);

	if (!server)
		return -EINVAL;

	if (g_str_equal(server, "127.0.0.1"))
		return -ENODEV;

	if (g_str_equal(server, "::1"))
		return -ENODEV;

	remove_server(index, domain, server, IPPROTO_UDP);
	remove_server(index, domain, server, IPPROTO_TCP);

	return 0;
}

void __connman_dnsproxy_flush(void)
{
	GSList *list;

	list = request_list;
	while (list) {
		struct request_data *req = list->data;

		list = list->next;

		if (resolv(req, req->request, req->name)) {
			/*
			 * A cached result was sent,
			 * so the request can be released
			 */
			request_list =
				g_slist_remove(request_list, req);
			destroy_request_data(req);
			continue;
		}

		if (req->timeout > 0)
			g_source_remove(req->timeout);
		req->timeout = g_timeout_add_seconds(5, request_timeout, req);
	}
}

static void dnsproxy_offline_mode(bool enabled)
{
	GSList *list;

	DBG("enabled %d", enabled);

	for (list = server_list; list; list = list->next) {
		struct server_data *data = list->data;

		if (!enabled) {
			DBG("Enabling DNS server %s", data->server);
			data->enabled = true;
			cache_invalidate();
			cache_refresh();
		} else {
			DBG("Disabling DNS server %s", data->server);
			data->enabled = false;
			cache_invalidate();
		}
	}
}

static void dnsproxy_default_changed(struct connman_service *service)
{
	GSList *list;
	int index;

	DBG("service %p", service);

	/* DNS has changed, invalidate the cache */
	cache_invalidate();

	if (!service) {
		/* When no services are active, then disable DNS proxying */
		dnsproxy_offline_mode(true);
		return;
	}

	index = __connman_service_get_index(service);
	if (index < 0)
		return;

	for (list = server_list; list; list = list->next) {
		struct server_data *data = list->data;

		if (data->index == index) {
			DBG("Enabling DNS server %s", data->server);
			data->enabled = true;
		} else {
			DBG("Disabling DNS server %s", data->server);
			data->enabled = false;
		}
	}

	cache_refresh();
}

static struct connman_notifier dnsproxy_notifier = {
	.name			= "dnsproxy",
	.default_changed	= dnsproxy_default_changed,
	.offline_mode		= dnsproxy_offline_mode,
};

static unsigned char opt_edns0_type[2] = { 0x00, 0x29 };

static int parse_request(unsigned char *buf, int len,
					char *name, unsigned int size)
{
	struct domain_hdr *hdr = (void *) buf;
	uint16_t qdcount = ntohs(hdr->qdcount);
	uint16_t arcount = ntohs(hdr->arcount);
	unsigned char *ptr;
	char *last_label = NULL;
	unsigned int remain, used = 0;

	if (len < 12)
		return -EINVAL;

	DBG("id 0x%04x qr %d opcode %d qdcount %d arcount %d",
					hdr->id, hdr->qr, hdr->opcode,
							qdcount, arcount);

	if (hdr->qr != 0 || qdcount != 1)
		return -EINVAL;

	name[0] = '\0';

	ptr = buf + sizeof(struct domain_hdr);
	remain = len - sizeof(struct domain_hdr);

	while (remain > 0) {
		uint8_t label_len = *ptr;

		if (label_len == 0x00) {
			last_label = (char *) (ptr + 1);
			break;
		}

		if (used + label_len + 1 > size)
			return -ENOBUFS;

		strncat(name, (char *) (ptr + 1), label_len);
		strcat(name, ".");

		used += label_len + 1;

		ptr += label_len + 1;
		remain -= label_len + 1;
	}

	if (last_label && arcount && remain >= 9 && last_label[4] == 0 &&
				!memcmp(last_label + 5, opt_edns0_type, 2)) {
		uint16_t edns0_bufsize;

		edns0_bufsize = last_label[7] << 8 | last_label[8];

		DBG("EDNS0 buffer size %u", edns0_bufsize);

		/* This is an evil hack until full TCP support has been
		 * implemented.
		 *
		 * Somtimes the EDNS0 request gets send with a too-small
		 * buffer size. Since glibc doesn't seem to crash when it
		 * gets a response biffer then it requested, just bump
		 * the buffer size up to 4KiB.
		 */
		if (edns0_bufsize < 0x1000) {
			last_label[7] = 0x10;
			last_label[8] = 0x00;
		}
	}

	DBG("query %s", name);

	return 0;
}

static void client_reset(struct tcp_partial_client_data *client)
{
	if (!client)
		return;

	if (client->channel) {
		DBG("client %d closing",
			g_io_channel_unix_get_fd(client->channel));

		g_io_channel_unref(client->channel);
		client->channel = NULL;
	}

	if (client->watch > 0) {
		g_source_remove(client->watch);
		client->watch = 0;
	}

	if (client->timeout > 0) {
		g_source_remove(client->timeout);
		client->timeout = 0;
	}

	g_free(client->buf);
	client->buf = NULL;

	client->buf_end = 0;
}

static unsigned int get_msg_len(unsigned char *buf)
{
	return buf[0]<<8 | buf[1];
}

static bool read_tcp_data(struct tcp_partial_client_data *client,
				void *client_addr, socklen_t client_addr_len,
				int read_len)
{
	char query[TCP_MAX_BUF_LEN];
	struct request_data *req;
	int client_sk, err;
	unsigned int msg_len;
	GSList *list;
	bool waiting_for_connect = false;
	int qtype = 0;
	struct cache_entry *entry;

	client_sk = g_io_channel_unix_get_fd(client->channel);

	if (read_len == 0) {
		DBG("client %d closed, pending %d bytes",
			client_sk, client->buf_end);
		g_hash_table_remove(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));
		return false;
	}

	DBG("client %d received %d bytes", client_sk, read_len);

	client->buf_end += read_len;

	if (client->buf_end < 2)
		return true;

	msg_len = get_msg_len(client->buf);
	if (msg_len > TCP_MAX_BUF_LEN) {
		DBG("client %d sent too much data %d", client_sk, msg_len);
		g_hash_table_remove(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));
		return false;
	}

read_another:
	DBG("client %d msg len %d end %d past end %d", client_sk, msg_len,
		client->buf_end, client->buf_end - (msg_len + 2));

	if (client->buf_end < (msg_len + 2)) {
		DBG("client %d still missing %d bytes",
			client_sk,
			msg_len + 2 - client->buf_end);
		return true;
	}

	DBG("client %d all data %d received", client_sk, msg_len);

	err = parse_request(client->buf + 2, msg_len,
			query, sizeof(query));
	if (err < 0 || (g_slist_length(server_list) == 0)) {
		send_response(client_sk, client->buf, msg_len + 2,
			NULL, 0, IPPROTO_TCP);
		return true;
	}

	req = g_try_new0(struct request_data, 1);
	if (!req)
		return true;

	memcpy(&req->sa, client_addr, client_addr_len);
	req->sa_len = client_addr_len;
	req->client_sk = client_sk;
	req->protocol = IPPROTO_TCP;
	req->family = client->family;

	req->srcid = client->buf[2] | (client->buf[3] << 8);
	req->dstid = get_id();
	req->altid = get_id();
	req->request_len = msg_len + 2;

	client->buf[2] = req->dstid & 0xff;
	client->buf[3] = req->dstid >> 8;

	req->numserv = 0;
	req->ifdata = client->ifdata;
	req->append_domain = false;

	/*
	 * Check if the answer is found in the cache before
	 * creating sockets to the server.
	 */
	entry = cache_check(client->buf, &qtype, IPPROTO_TCP);
	if (entry) {
		int ttl_left = 0;
		struct cache_data *data;

		DBG("cache hit %s type %s", query, qtype == 1 ? "A" : "AAAA");
		if (qtype == 1)
			data = entry->ipv4;
		else
			data = entry->ipv6;

		if (data) {
			ttl_left = data->valid_until - time(NULL);
			entry->hits++;

			send_cached_response(client_sk, data->data,
					data->data_len, NULL, 0, IPPROTO_TCP,
					req->srcid, data->answers, ttl_left);

			g_free(req);
			goto out;
		} else
			DBG("data missing, ignoring cache for this query");
	}

	for (list = server_list; list; list = list->next) {
		struct server_data *data = list->data;

		if (data->protocol != IPPROTO_UDP || !data->enabled)
			continue;

		if (!create_server(data->index, NULL, data->server,
					IPPROTO_TCP))
			continue;

		waiting_for_connect = true;
	}

	if (!waiting_for_connect) {
		/* No server is waiting for connect */
		send_response(client_sk, client->buf,
			req->request_len, NULL, 0, IPPROTO_TCP);
		g_free(req);
		return true;
	}

	/*
	 * The server is not connected yet.
	 * Copy the relevant buffers.
	 * The request will actually be sent once we're
	 * properly connected over TCP to the nameserver.
	 */
	req->request = g_try_malloc0(req->request_len);
	if (!req->request) {
		send_response(client_sk, client->buf,
			req->request_len, NULL, 0, IPPROTO_TCP);
		g_free(req);
		goto out;
	}
	memcpy(req->request, client->buf, req->request_len);

	req->name = g_try_malloc0(sizeof(query));
	if (!req->name) {
		send_response(client_sk, client->buf,
			req->request_len, NULL, 0, IPPROTO_TCP);
		g_free(req->request);
		g_free(req);
		goto out;
	}
	memcpy(req->name, query, sizeof(query));

	req->timeout = g_timeout_add_seconds(30, request_timeout, req);

	request_list = g_slist_append(request_list, req);

out:
	if (client->buf_end > (msg_len + 2)) {
		DBG("client %d buf %p -> %p end %d len %d new %d",
			client_sk,
			client->buf + msg_len + 2,
			client->buf, client->buf_end,
			TCP_MAX_BUF_LEN - client->buf_end,
			client->buf_end - (msg_len + 2));
		memmove(client->buf, client->buf + msg_len + 2,
			TCP_MAX_BUF_LEN - client->buf_end);
		client->buf_end = client->buf_end - (msg_len + 2);

		/*
		 * If we have a full message waiting, just read it
		 * immediately.
		 */
		msg_len = get_msg_len(client->buf);
		if ((msg_len + 2) == client->buf_end) {
			DBG("client %d reading another %d bytes", client_sk,
								msg_len + 2);
			goto read_another;
		}
	} else {
		DBG("client %d clearing reading buffer", client_sk);

		client->buf_end = 0;
		memset(client->buf, 0, TCP_MAX_BUF_LEN);

		/*
		 * We received all the packets from client so we must also
		 * remove the timeout handler here otherwise we might get
		 * timeout while waiting the results from server.
		 */
		g_source_remove(client->timeout);
		client->timeout = 0;
	}

	return true;
}

static gboolean tcp_client_event(GIOChannel *channel, GIOCondition condition,
				gpointer user_data)
{
	struct tcp_partial_client_data *client = user_data;
	struct sockaddr_in6 client_addr6;
	socklen_t client_addr6_len = sizeof(client_addr6);
	struct sockaddr_in client_addr4;
	socklen_t client_addr4_len = sizeof(client_addr4);
	void *client_addr;
	socklen_t *client_addr_len;
	int len, client_sk;

	client_sk = g_io_channel_unix_get_fd(channel);

	if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		g_hash_table_remove(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));

		connman_error("Error with TCP client %d channel", client_sk);
		return FALSE;
	}

	switch (client->family) {
	case AF_INET:
		client_addr = &client_addr4;
		client_addr_len = &client_addr4_len;
		break;
	case AF_INET6:
		client_addr = &client_addr6;
		client_addr_len = &client_addr6_len;
		break;
	default:
		g_hash_table_remove(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));
		connman_error("client %p corrupted", client);
		return FALSE;
	}

	len = recvfrom(client_sk, client->buf + client->buf_end,
			TCP_MAX_BUF_LEN - client->buf_end, 0,
			client_addr, client_addr_len);
	if (len < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return TRUE;

		DBG("client %d cannot read errno %d/%s", client_sk, -errno,
			strerror(errno));
		g_hash_table_remove(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));
		return FALSE;
	}

	return read_tcp_data(client, client_addr, *client_addr_len, len);
}

static gboolean client_timeout(gpointer user_data)
{
	struct tcp_partial_client_data *client = user_data;
	int sock;

	sock = g_io_channel_unix_get_fd(client->channel);

	DBG("client %d timeout pending %d bytes", sock, client->buf_end);

	g_hash_table_remove(partial_tcp_req_table, GINT_TO_POINTER(sock));

	return FALSE;
}

static bool tcp_listener_event(GIOChannel *channel, GIOCondition condition,
				struct listener_data *ifdata, int family,
				guint *listener_watch)
{
	int sk, client_sk, len;
	unsigned int msg_len;
	struct tcp_partial_client_data *client;
	struct sockaddr_in6 client_addr6;
	socklen_t client_addr6_len = sizeof(client_addr6);
	struct sockaddr_in client_addr4;
	socklen_t client_addr4_len = sizeof(client_addr4);
	void *client_addr;
	socklen_t *client_addr_len;
	struct timeval tv;
	fd_set readfds;

	DBG("condition 0x%02x channel %p ifdata %p family %d",
		condition, channel, ifdata, family);

	if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		if (*listener_watch > 0)
			g_source_remove(*listener_watch);
		*listener_watch = 0;

		connman_error("Error with TCP listener channel");

		return false;
	}

	sk = g_io_channel_unix_get_fd(channel);

	if (family == AF_INET) {
		client_addr = &client_addr4;
		client_addr_len = &client_addr4_len;
	} else {
		client_addr = &client_addr6;
		client_addr_len = &client_addr6_len;
	}

	tv.tv_sec = tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(sk, &readfds);

	select(sk + 1, &readfds, NULL, NULL, &tv);
	if (FD_ISSET(sk, &readfds)) {
		client_sk = accept(sk, client_addr, client_addr_len);
		DBG("client %d accepted", client_sk);
	} else {
		DBG("No data to read from master %d, waiting.", sk);
		return true;
	}

	if (client_sk < 0) {
		connman_error("Accept failure on TCP listener");
		*listener_watch = 0;
		return false;
	}

	fcntl(client_sk, F_SETFL, O_NONBLOCK);

	client = g_hash_table_lookup(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));
	if (!client) {
		client = g_try_new0(struct tcp_partial_client_data, 1);
		if (!client) {
			close(client_sk);
			return false;
		}

		g_hash_table_insert(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk),
					client);

		client->channel = g_io_channel_unix_new(client_sk);
		g_io_channel_set_close_on_unref(client->channel, TRUE);

		client->watch = g_io_add_watch(client->channel,
						G_IO_IN, tcp_client_event,
						(gpointer)client);

		client->ifdata = ifdata;

		DBG("client %d created %p", client_sk, client);
	} else {
		DBG("client %d already exists %p", client_sk, client);
	}

	if (!client->buf) {
		client->buf = g_try_malloc(TCP_MAX_BUF_LEN);
		if (!client->buf)
			return false;
	}
	memset(client->buf, 0, TCP_MAX_BUF_LEN);
	client->buf_end = 0;
	client->family = family;

	if (client->timeout == 0)
		client->timeout = g_timeout_add_seconds(2, client_timeout,
							client);

	/*
	 * Check how much data there is. If all is there, then we can
	 * proceed normally, otherwise read the bits until everything
	 * is received or timeout occurs.
	 */
	len = recv(client_sk, client->buf, TCP_MAX_BUF_LEN, 0);
	if (len < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			DBG("client %d no data to read, waiting", client_sk);
			return true;
		}

		DBG("client %d cannot read errno %d/%s", client_sk, -errno,
			strerror(errno));
		g_hash_table_remove(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));
		return true;
	}

	if (len < 2) {
		DBG("client %d not enough data to read, waiting", client_sk);
		client->buf_end += len;
		return true;
	}

	msg_len = get_msg_len(client->buf);
	if (msg_len > TCP_MAX_BUF_LEN) {
		DBG("client %d invalid message length %u ignoring packet",
			client_sk, msg_len);
		g_hash_table_remove(partial_tcp_req_table,
					GINT_TO_POINTER(client_sk));
		return true;
	}

	/*
	 * The packet length bytes do not contain the total message length,
	 * that is the reason to -2 below.
	 */
	if (msg_len != (unsigned int)(len - 2)) {
		DBG("client %d sent %d bytes but expecting %u pending %d",
			client_sk, len, msg_len + 2, msg_len + 2 - len);

		client->buf_end += len;
		return true;
	}

	return read_tcp_data(client, client_addr, *client_addr_len, len);
}

static gboolean tcp4_listener_event(GIOChannel *channel, GIOCondition condition,
				gpointer user_data)
{
	struct listener_data *ifdata = user_data;

	return tcp_listener_event(channel, condition, ifdata, AF_INET,
				&ifdata->tcp4_listener_watch);
}

static gboolean tcp6_listener_event(GIOChannel *channel, GIOCondition condition,
				gpointer user_data)
{
	struct listener_data *ifdata = user_data;

	return tcp_listener_event(channel, condition, user_data, AF_INET6,
				&ifdata->tcp6_listener_watch);
}

static bool udp_listener_event(GIOChannel *channel, GIOCondition condition,
				struct listener_data *ifdata, int family,
				guint *listener_watch)
{
	unsigned char buf[768];
	char query[512];
	struct request_data *req;
	struct sockaddr_in6 client_addr6;
	socklen_t client_addr6_len = sizeof(client_addr6);
	struct sockaddr_in client_addr4;
	socklen_t client_addr4_len = sizeof(client_addr4);
	void *client_addr;
	socklen_t *client_addr_len;
	int sk, err, len;

	if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		connman_error("Error with UDP listener channel");
		*listener_watch = 0;
		return false;
	}

	sk = g_io_channel_unix_get_fd(channel);

	if (family == AF_INET) {
		client_addr = &client_addr4;
		client_addr_len = &client_addr4_len;
	} else {
		client_addr = &client_addr6;
		client_addr_len = &client_addr6_len;
	}

	memset(client_addr, 0, *client_addr_len);
	len = recvfrom(sk, buf, sizeof(buf), 0, client_addr, client_addr_len);
	if (len < 2)
		return true;

	DBG("Received %d bytes (id 0x%04x)", len, buf[0] | buf[1] << 8);

	err = parse_request(buf, len, query, sizeof(query));
	if (err < 0 || (g_slist_length(server_list) == 0)) {
		send_response(sk, buf, len, client_addr,
				*client_addr_len, IPPROTO_UDP);
		return true;
	}

	req = g_try_new0(struct request_data, 1);
	if (!req)
		return true;

	memcpy(&req->sa, client_addr, *client_addr_len);
	req->sa_len = *client_addr_len;
	req->client_sk = 0;
	req->protocol = IPPROTO_UDP;
	req->family = family;

	req->srcid = buf[0] | (buf[1] << 8);
	req->dstid = get_id();
	req->altid = get_id();
	req->request_len = len;

	buf[0] = req->dstid & 0xff;
	buf[1] = req->dstid >> 8;

	req->numserv = 0;
	req->ifdata = ifdata;
	req->append_domain = false;

	if (resolv(req, buf, query)) {
		/* a cached result was sent, so the request can be released */
	        g_free(req);
		return true;
	}

	req->name = g_strdup(query);
	req->request = g_malloc(len);
	memcpy(req->request, buf, len);
	req->timeout = g_timeout_add_seconds(5, request_timeout, req);
	request_list = g_slist_append(request_list, req);

	return true;
}

static gboolean udp4_listener_event(GIOChannel *channel, GIOCondition condition,
				gpointer user_data)
{
	struct listener_data *ifdata = user_data;

	return udp_listener_event(channel, condition, ifdata, AF_INET,
				&ifdata->udp4_listener_watch);
}

static gboolean udp6_listener_event(GIOChannel *channel, GIOCondition condition,
				gpointer user_data)
{
	struct listener_data *ifdata = user_data;

	return udp_listener_event(channel, condition, user_data, AF_INET6,
				&ifdata->udp6_listener_watch);
}

static GIOChannel *get_listener(int family, int protocol, int index)
{
	GIOChannel *channel;
	const char *proto;
	union {
		struct sockaddr sa;
		struct sockaddr_in6 sin6;
		struct sockaddr_in sin;
	} s;
	socklen_t slen;
	int sk, type;
	char *interface;

	DBG("family %d protocol %d index %d", family, protocol, index);

	switch (protocol) {
	case IPPROTO_UDP:
		proto = "UDP";
		type = SOCK_DGRAM | SOCK_CLOEXEC;
		break;

	case IPPROTO_TCP:
		proto = "TCP";
		type = SOCK_STREAM | SOCK_CLOEXEC;
		break;

	default:
		return NULL;
	}

	sk = socket(family, type, protocol);
	if (sk < 0 && family == AF_INET6 && errno == EAFNOSUPPORT) {
		connman_error("No IPv6 support");
		return NULL;
	}

	if (sk < 0) {
		connman_error("Failed to create %s listener socket", proto);
		return NULL;
	}

	interface = connman_inet_ifname(index);
	if (!interface || setsockopt(sk, SOL_SOCKET, SO_BINDTODEVICE,
					interface,
					strlen(interface) + 1) < 0) {
		connman_error("Failed to bind %s listener interface "
			"for %s (%d/%s)",
			proto, family == AF_INET ? "IPv4" : "IPv6",
			-errno, strerror(errno));
		close(sk);
		g_free(interface);
		return NULL;
	}
	g_free(interface);

	if (family == AF_INET6) {
		memset(&s.sin6, 0, sizeof(s.sin6));
		s.sin6.sin6_family = AF_INET6;
		s.sin6.sin6_port = htons(53);
		slen = sizeof(s.sin6);

		if (__connman_inet_get_interface_address(index,
						AF_INET6,
						&s.sin6.sin6_addr) < 0) {
			/* So we could not find suitable IPv6 address for
			 * the interface. This could happen if we have
			 * disabled IPv6 for the interface.
			 */
			close(sk);
			return NULL;
		}

	} else if (family == AF_INET) {
		memset(&s.sin, 0, sizeof(s.sin));
		s.sin.sin_family = AF_INET;
		s.sin.sin_port = htons(53);
		slen = sizeof(s.sin);

		if (__connman_inet_get_interface_address(index,
						AF_INET,
						&s.sin.sin_addr) < 0) {
			close(sk);
			return NULL;
		}
	} else {
		close(sk);
		return NULL;
	}

	if (bind(sk, &s.sa, slen) < 0) {
		connman_error("Failed to bind %s listener socket", proto);
		close(sk);
		return NULL;
	}

	if (protocol == IPPROTO_TCP) {

		if (listen(sk, 10) < 0) {
			connman_error("Failed to listen on TCP socket %d/%s",
				-errno, strerror(errno));
			close(sk);
			return NULL;
		}

		fcntl(sk, F_SETFL, O_NONBLOCK);
	}

	channel = g_io_channel_unix_new(sk);
	if (!channel) {
		connman_error("Failed to create %s listener channel", proto);
		close(sk);
		return NULL;
	}

	g_io_channel_set_close_on_unref(channel, TRUE);

	return channel;
}

#define UDP_IPv4_FAILED 0x01
#define TCP_IPv4_FAILED 0x02
#define UDP_IPv6_FAILED 0x04
#define TCP_IPv6_FAILED 0x08
#define UDP_FAILED (UDP_IPv4_FAILED | UDP_IPv6_FAILED)
#define TCP_FAILED (TCP_IPv4_FAILED | TCP_IPv6_FAILED)
#define IPv6_FAILED (UDP_IPv6_FAILED | TCP_IPv6_FAILED)
#define IPv4_FAILED (UDP_IPv4_FAILED | TCP_IPv4_FAILED)

static int create_dns_listener(int protocol, struct listener_data *ifdata)
{
	int ret = 0;

	if (protocol == IPPROTO_TCP) {
		ifdata->tcp4_listener_channel = get_listener(AF_INET, protocol,
							ifdata->index);
		if (ifdata->tcp4_listener_channel)
			ifdata->tcp4_listener_watch =
				g_io_add_watch(ifdata->tcp4_listener_channel,
					G_IO_IN, tcp4_listener_event,
					(gpointer)ifdata);
		else
			ret |= TCP_IPv4_FAILED;

		ifdata->tcp6_listener_channel = get_listener(AF_INET6, protocol,
							ifdata->index);
		if (ifdata->tcp6_listener_channel)
			ifdata->tcp6_listener_watch =
				g_io_add_watch(ifdata->tcp6_listener_channel,
					G_IO_IN, tcp6_listener_event,
					(gpointer)ifdata);
		else
			ret |= TCP_IPv6_FAILED;
	} else {
		ifdata->udp4_listener_channel = get_listener(AF_INET, protocol,
							ifdata->index);
		if (ifdata->udp4_listener_channel)
			ifdata->udp4_listener_watch =
				g_io_add_watch(ifdata->udp4_listener_channel,
					G_IO_IN, udp4_listener_event,
					(gpointer)ifdata);
		else
			ret |= UDP_IPv4_FAILED;

		ifdata->udp6_listener_channel = get_listener(AF_INET6, protocol,
							ifdata->index);
		if (ifdata->udp6_listener_channel)
			ifdata->udp6_listener_watch =
				g_io_add_watch(ifdata->udp6_listener_channel,
					G_IO_IN, udp6_listener_event,
					(gpointer)ifdata);
		else
			ret |= UDP_IPv6_FAILED;
	}

	return ret;
}

static void destroy_udp_listener(struct listener_data *ifdata)
{
	DBG("index %d", ifdata->index);

	if (ifdata->udp4_listener_watch > 0)
		g_source_remove(ifdata->udp4_listener_watch);

	if (ifdata->udp6_listener_watch > 0)
		g_source_remove(ifdata->udp6_listener_watch);

	if (ifdata->udp4_listener_channel)
		g_io_channel_unref(ifdata->udp4_listener_channel);
	if (ifdata->udp6_listener_channel)
		g_io_channel_unref(ifdata->udp6_listener_channel);
}

static void destroy_tcp_listener(struct listener_data *ifdata)
{
	DBG("index %d", ifdata->index);

	if (ifdata->tcp4_listener_watch > 0)
		g_source_remove(ifdata->tcp4_listener_watch);
	if (ifdata->tcp6_listener_watch > 0)
		g_source_remove(ifdata->tcp6_listener_watch);

	if (ifdata->tcp4_listener_channel)
		g_io_channel_unref(ifdata->tcp4_listener_channel);
	if (ifdata->tcp6_listener_channel)
		g_io_channel_unref(ifdata->tcp6_listener_channel);
}

static int create_listener(struct listener_data *ifdata)
{
	int err, index;

	err = create_dns_listener(IPPROTO_UDP, ifdata);
	if ((err & UDP_FAILED) == UDP_FAILED)
		return -EIO;

	err |= create_dns_listener(IPPROTO_TCP, ifdata);
	if ((err & TCP_FAILED) == TCP_FAILED) {
		destroy_udp_listener(ifdata);
		return -EIO;
	}

	index = connman_inet_ifindex("lo");
	if (ifdata->index == index) {
		if ((err & IPv6_FAILED) != IPv6_FAILED)
			__connman_resolvfile_append(index, NULL, "::1");

		if ((err & IPv4_FAILED) != IPv4_FAILED)
			__connman_resolvfile_append(index, NULL, "127.0.0.1");
	}

	return 0;
}

static void destroy_listener(struct listener_data *ifdata)
{
	int index;
	GSList *list;

	index = connman_inet_ifindex("lo");
	if (ifdata->index == index) {
		__connman_resolvfile_remove(index, NULL, "127.0.0.1");
		__connman_resolvfile_remove(index, NULL, "::1");
	}

	for (list = request_list; list; list = list->next) {
		struct request_data *req = list->data;

		DBG("Dropping request (id 0x%04x -> 0x%04x)",
						req->srcid, req->dstid);
		destroy_request_data(req);
		list->data = NULL;
	}

	g_slist_free(request_list);
	request_list = NULL;

	destroy_tcp_listener(ifdata);
	destroy_udp_listener(ifdata);
}

int __connman_dnsproxy_add_listener(int index)
{
	struct listener_data *ifdata;
	int err;

	DBG("index %d", index);

	if (index < 0)
		return -EINVAL;

	if (!listener_table)
		return -ENOENT;

	if (g_hash_table_lookup(listener_table, GINT_TO_POINTER(index)))
		return 0;

	ifdata = g_try_new0(struct listener_data, 1);
	if (!ifdata)
		return -ENOMEM;

	ifdata->index = index;
	ifdata->udp4_listener_channel = NULL;
	ifdata->udp4_listener_watch = 0;
	ifdata->tcp4_listener_channel = NULL;
	ifdata->tcp4_listener_watch = 0;
	ifdata->udp6_listener_channel = NULL;
	ifdata->udp6_listener_watch = 0;
	ifdata->tcp6_listener_channel = NULL;
	ifdata->tcp6_listener_watch = 0;

	err = create_listener(ifdata);
	if (err < 0) {
		connman_error("Couldn't create listener for index %d err %d",
				index, err);
		g_free(ifdata);
		return err;
	}
	g_hash_table_insert(listener_table, GINT_TO_POINTER(ifdata->index),
			ifdata);
	return 0;
}

void __connman_dnsproxy_remove_listener(int index)
{
	struct listener_data *ifdata;

	DBG("index %d", index);

	if (!listener_table)
		return;

	ifdata = g_hash_table_lookup(listener_table, GINT_TO_POINTER(index));
	if (!ifdata)
		return;

	destroy_listener(ifdata);

	g_hash_table_remove(listener_table, GINT_TO_POINTER(index));
}

static void remove_listener(gpointer key, gpointer value, gpointer user_data)
{
	int index = GPOINTER_TO_INT(key);
	struct listener_data *ifdata = value;

	DBG("index %d", index);

	destroy_listener(ifdata);
}

static void free_partial_reqs(gpointer value)
{
	struct tcp_partial_client_data *data = value;

	client_reset(data);
	g_free(data);
}

int __connman_dnsproxy_init(void)
{
	int err, index;

	DBG("");

	listener_table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, g_free);

	partial_tcp_req_table = g_hash_table_new_full(g_direct_hash,
							g_direct_equal,
							NULL,
							free_partial_reqs);

	index = connman_inet_ifindex("lo");
	err = __connman_dnsproxy_add_listener(index);
	if (err < 0)
		return err;

	err = connman_notifier_register(&dnsproxy_notifier);
	if (err < 0)
		goto destroy;

	return 0;

destroy:
	__connman_dnsproxy_remove_listener(index);
	g_hash_table_destroy(listener_table);
	g_hash_table_destroy(partial_tcp_req_table);

	return err;
}

void __connman_dnsproxy_cleanup(void)
{
	DBG("");

	if (cache_timer) {
		g_source_remove(cache_timer);
		cache_timer = 0;
	}

	if (cache) {
		g_hash_table_destroy(cache);
		cache = NULL;
	}

	connman_notifier_unregister(&dnsproxy_notifier);

	g_hash_table_foreach(listener_table, remove_listener, NULL);

	g_hash_table_destroy(listener_table);

	g_hash_table_destroy(partial_tcp_req_table);
}
