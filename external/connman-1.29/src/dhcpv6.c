/*
 *
 *  Connection Manager
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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>

#include <connman/ipconfig.h>
#include <connman/storage.h>

#include <gdhcp/gdhcp.h>

#include <glib.h>

#include "connman.h"

/* Transmission params in msec, RFC 3315 chapter 5.5 */
#define INF_MAX_DELAY	(1 * 1000)
#define INF_TIMEOUT	(1 * 1000)
#define INF_MAX_RT	(120 * 1000)
#define SOL_MAX_DELAY   (1 * 1000)
#define SOL_TIMEOUT     (1 * 1000)
#define SOL_MAX_RT      (120 * 1000)
#define REQ_TIMEOUT	(1 * 1000)
#define REQ_MAX_RT	(30 * 1000)
#define REQ_MAX_RC	10
#define REN_TIMEOUT     (10 * 1000)
#define REN_MAX_RT      (600 * 1000)
#define REB_TIMEOUT     (10 * 1000)
#define REB_MAX_RT      (600 * 1000)
#define CNF_MAX_DELAY   (1 * 1000)
#define CNF_TIMEOUT	(1 * 1000)
#define CNF_MAX_RT	(4 * 1000)
#define CNF_MAX_RD	(10 * 1000)
#define DEC_TIMEOUT     (1 * 1000)
#define DEC_MAX_RC      5

enum request_type {
	REQ_REQUEST = 1,
	REQ_REBIND = 2,
	REQ_RENEW = 3,
};

struct connman_dhcpv6 {
	struct connman_network *network;
	dhcpv6_cb callback;

	char **nameservers;
	char **timeservers;

	GDHCPClient *dhcp_client;

	guint timeout;		/* operation timeout in msec */
	guint MRD;		/* max operation timeout in msec */
	guint RT;		/* in msec */
	bool use_ta;		/* set to TRUE if IPv6 privacy is enabled */
	GSList *prefixes;	/* network prefixes from radvd or dhcpv6 pd */
	int request_count;	/* how many times REQUEST have been sent */
	bool stateless;		/* TRUE if stateless DHCPv6 is used */
	bool started;		/* TRUE if we have DHCPv6 started */
};

static GHashTable *network_table;
static GHashTable *network_pd_table;

static int dhcpv6_request(struct connman_dhcpv6 *dhcp, bool add_addresses);
static int dhcpv6_pd_request(struct connman_dhcpv6 *dhcp);
static gboolean start_solicitation(gpointer user_data);
static int dhcpv6_renew(struct connman_dhcpv6 *dhcp);
static int dhcpv6_rebind(struct connman_dhcpv6 *dhcp);

static void clear_timer(struct connman_dhcpv6 *dhcp)
{
	if (dhcp->timeout > 0) {
		g_source_remove(dhcp->timeout);
		dhcp->timeout = 0;
	}

	if (dhcp->MRD > 0) {
		g_source_remove(dhcp->MRD);
		dhcp->MRD = 0;
	}
}

static inline guint get_random(void)
{
	uint64_t val;

	__connman_util_get_random(&val);

	/* Make sure the value is always positive so strip MSB */
	return ((uint32_t)val) >> 1;
}

static guint compute_random(guint val)
{
	return val - val / 10 +
		(get_random() % (2 * 1000)) * val / 10 / 1000;
}

/* Calculate a random delay, RFC 3315 chapter 14 */
/* RT and MRT are milliseconds */
static guint calc_delay(guint RT, guint MRT)
{
	if (MRT && (RT > MRT / 2))
		RT = compute_random(MRT);
	else
		RT += compute_random(RT);

	return RT;
}

static guint initial_rt(guint timeout)
{
	return compute_random(timeout);
}

static void free_prefix(gpointer data)
{
	g_free(data);
}

static void dhcpv6_free(struct connman_dhcpv6 *dhcp)
{
	g_strfreev(dhcp->nameservers);
	g_strfreev(dhcp->timeservers);

	dhcp->nameservers = NULL;
	dhcp->timeservers = NULL;
	dhcp->started = false;

	g_slist_free_full(dhcp->prefixes, free_prefix);
}

static bool compare_string_arrays(char **array_a, char **array_b)
{
	int i;

	if (!array_a || !array_b)
		return false;

	if (g_strv_length(array_a) != g_strv_length(array_b))
		return false;

	for (i = 0; array_a[i] && array_b[i]; i++)
		if (g_strcmp0(array_a[i], array_b[i]) != 0)
			return false;

	return true;
}

static void dhcpv6_debug(const char *str, void *data)
{
	connman_info("%s: %s\n", (const char *) data, str);
}

static gchar *convert_to_hex(unsigned char *buf, int len)
{
	gchar *ret = g_try_malloc(len * 2 + 1);
	int i;

	for (i = 0; ret && i < len; i++)
		g_snprintf(ret + i * 2, 3, "%02x", buf[i]);

	return ret;
}

/*
 * DUID should not change over time so save it to file.
 * See RFC 3315 chapter 9 for details.
 */
static int set_duid(struct connman_service *service,
			struct connman_network *network,
			GDHCPClient *dhcp_client, int index)
{
	GKeyFile *keyfile;
	const char *ident;
	char *hex_duid;
	unsigned char *duid;
	int duid_len;

	ident = __connman_service_get_ident(service);

	keyfile = connman_storage_load_service(ident);
	if (!keyfile)
		return -EINVAL;

	hex_duid = g_key_file_get_string(keyfile, ident, "IPv6.DHCP.DUID",
					NULL);
	if (hex_duid) {
		unsigned int i, j = 0, hex;
		size_t hex_duid_len = strlen(hex_duid);

		duid = g_try_malloc0(hex_duid_len / 2);
		if (!duid) {
			g_key_file_free(keyfile);
			g_free(hex_duid);
			return -ENOMEM;
		}

		for (i = 0; i < hex_duid_len; i += 2) {
			sscanf(hex_duid + i, "%02x", &hex);
			duid[j++] = hex;
		}

		duid_len = hex_duid_len / 2;
	} else {
		int ret;
		int type = __connman_ipconfig_get_type_from_index(index);

		ret = g_dhcpv6_create_duid(G_DHCPV6_DUID_LLT, index, type,
					&duid, &duid_len);
		if (ret < 0) {
			g_key_file_free(keyfile);
			return ret;
		}

		hex_duid = convert_to_hex(duid, duid_len);
		if (!hex_duid) {
			g_key_file_free(keyfile);
			return -ENOMEM;
		}

		g_key_file_set_string(keyfile, ident, "IPv6.DHCP.DUID",
				hex_duid);

		__connman_storage_save_service(keyfile, ident);
	}
	g_free(hex_duid);

	g_key_file_free(keyfile);

	g_dhcpv6_client_set_duid(dhcp_client, duid, duid_len);

	return 0;
}

static void clear_callbacks(GDHCPClient *dhcp_client)
{
	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_SOLICITATION,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_ADVERTISE,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_REQUEST,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_CONFIRM,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_RENEW,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_REBIND,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_RELEASE,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_DECLINE,
				NULL, NULL);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_INFORMATION_REQ,
				NULL, NULL);
}

static void info_req_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;
	struct connman_service *service;
	int entries, i;
	GList *option, *list;
	char **nameservers, **timeservers;

	DBG("dhcpv6 information-request %p", dhcp);

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		connman_error("Can not lookup service");
		return;
	}

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_DNS_SERVERS);
	entries = g_list_length(option);

	nameservers = g_try_new0(char *, entries + 1);
	if (nameservers) {
		for (i = 0, list = option; list; list = list->next, i++)
			nameservers[i] = g_strdup(list->data);
	}

	if (!compare_string_arrays(nameservers, dhcp->nameservers)) {
		if (dhcp->nameservers) {
			for (i = 0; dhcp->nameservers[i]; i++)
				__connman_service_nameserver_remove(service,
							dhcp->nameservers[i],
							false);
			g_strfreev(dhcp->nameservers);
		}

		dhcp->nameservers = nameservers;

		for (i = 0; dhcp->nameservers &&
					dhcp->nameservers[i]; i++)
			__connman_service_nameserver_append(service,
						dhcp->nameservers[i],
						false);
	} else
		g_strfreev(nameservers);


	option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_SNTP_SERVERS);
	entries = g_list_length(option);

	timeservers = g_try_new0(char *, entries + 1);
	if (timeservers) {
		for (i = 0, list = option; list; list = list->next, i++)
			timeservers[i] = g_strdup(list->data);
	}

	if (!compare_string_arrays(timeservers, dhcp->timeservers)) {
		if (dhcp->timeservers) {
			for (i = 0; dhcp->timeservers[i]; i++)
				__connman_service_timeserver_remove(service,
							dhcp->timeservers[i]);
			g_strfreev(dhcp->timeservers);
		}

		dhcp->timeservers = timeservers;

		for (i = 0; dhcp->timeservers &&
					dhcp->timeservers[i]; i++)
			__connman_service_timeserver_append(service,
							dhcp->timeservers[i]);
	} else
		g_strfreev(timeservers);


	if (dhcp->callback) {
		uint16_t status = g_dhcpv6_client_get_status(dhcp_client);
		dhcp->callback(dhcp->network, status == 0 ?
						CONNMAN_DHCPV6_STATUS_SUCCEED :
						CONNMAN_DHCPV6_STATUS_FAIL,
				NULL);
	}
}

static int dhcpv6_info_request(struct connman_dhcpv6 *dhcp)
{
	struct connman_service *service;
	GDHCPClient *dhcp_client;
	GDHCPClientError error;
	int index, ret;

	DBG("dhcp %p", dhcp);

	index = connman_network_get_index(dhcp->network);

	dhcp_client = g_dhcp_client_new(G_DHCP_IPV6, index, &error);
	if (error != G_DHCP_CLIENT_ERROR_NONE) {
		clear_timer(dhcp);
		return -EINVAL;
	}

	if (getenv("CONNMAN_DHCPV6_DEBUG"))
		g_dhcp_client_set_debug(dhcp_client, dhcpv6_debug, "DHCPv6");

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		clear_timer(dhcp);
		g_dhcp_client_unref(dhcp_client);
		return -EINVAL;
	}

	ret = set_duid(service, dhcp->network, dhcp_client, index);
	if (ret < 0) {
		clear_timer(dhcp);
		g_dhcp_client_unref(dhcp_client);
		return ret;
	}

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DOMAIN_LIST);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_set_oro(dhcp_client, 3, G_DHCPV6_DNS_SERVERS,
				G_DHCPV6_DOMAIN_LIST, G_DHCPV6_SNTP_SERVERS);

	g_dhcp_client_register_event(dhcp_client,
			G_DHCP_CLIENT_EVENT_INFORMATION_REQ, info_req_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static int check_ipv6_addr_prefix(GSList *prefixes, char *address)
{
	struct in6_addr addr_prefix, addr;
	GSList *list;
	int ret = 128, len;

	for (list = prefixes; list; list = list->next) {
		char *prefix = list->data;
		const char *slash = g_strrstr(prefix, "/");
		const unsigned char bits[] = { 0x00, 0xFE, 0xFC, 0xF8,
						0xF0, 0xE0, 0xC0, 0x80 };
		int left, count, i, plen;

		if (!slash)
			continue;

		prefix = g_strndup(prefix, slash - prefix);
		len = strtol(slash + 1, NULL, 10);
		if (len < 3 || len > 128)
			break;

		plen = 128 - len;

		count = plen / 8;
		left = plen % 8;
		i = 16 - count;

		inet_pton(AF_INET6, prefix, &addr_prefix);
		inet_pton(AF_INET6, address, &addr);

		memset(&addr_prefix.s6_addr[i], 0, count);
		memset(&addr.s6_addr[i], 0, count);

		if (left) {
			addr_prefix.s6_addr[i - 1] &= bits[left];
			addr.s6_addr[i - 1] &= bits[left];
		}

		g_free(prefix);

		if (memcmp(&addr_prefix, &addr, 16) == 0) {
			ret = len;
			break;
		}
	}

	return ret;
}

static int set_other_addresses(GDHCPClient *dhcp_client,
						struct connman_dhcpv6 *dhcp)
{
	struct connman_service *service;
	int entries, i;
	GList *option, *list;
	char **nameservers, **timeservers;

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		connman_error("Can not lookup service");
		return -EINVAL;
	}

	/*
	 * Check domains before nameservers so that the nameserver append
	 * function will update domain list in service.c
	 */
	option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_DOMAIN_LIST);
	entries = g_list_length(option);
	if (entries > 0) {
		char **domains = g_try_new0(char *, entries + 1);
		if (domains) {
			for (i = 0, list = option; list;
						list = list->next, i++)
				domains[i] = g_strdup(list->data);
			__connman_service_update_search_domains(service, domains);
			g_strfreev(domains);
		}
	}

	option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_DNS_SERVERS);
	entries = g_list_length(option);

	nameservers = g_try_new0(char *, entries + 1);
	if (nameservers) {
		for (i = 0, list = option; list; list = list->next, i++)
			nameservers[i] = g_strdup(list->data);
	}

	if (!compare_string_arrays(nameservers, dhcp->nameservers)) {
		if (dhcp->nameservers) {
			for (i = 0; dhcp->nameservers[i]; i++)
				__connman_service_nameserver_remove(service,
							dhcp->nameservers[i],
							false);
			g_strfreev(dhcp->nameservers);
		}

		dhcp->nameservers = nameservers;

		for (i = 0; dhcp->nameservers &&
					dhcp->nameservers[i]; i++)
			__connman_service_nameserver_append(service,
							dhcp->nameservers[i],
							false);
	} else
		g_strfreev(nameservers);


	option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_SNTP_SERVERS);
	entries = g_list_length(option);

	timeservers = g_try_new0(char *, entries + 1);
	if (timeservers) {
		for (i = 0, list = option; list; list = list->next, i++)
			timeservers[i] = g_strdup(list->data);
	}

	if (!compare_string_arrays(timeservers, dhcp->timeservers)) {
		if (dhcp->timeservers) {
			for (i = 0; dhcp->timeservers[i]; i++)
				__connman_service_timeserver_remove(service,
							dhcp->timeservers[i]);
			g_strfreev(dhcp->timeservers);
		}

		dhcp->timeservers = timeservers;

		for (i = 0; dhcp->timeservers &&
					dhcp->timeservers[i]; i++)
			__connman_service_timeserver_append(service,
							dhcp->timeservers[i]);
	} else
		g_strfreev(timeservers);

	return 0;
}

static GSList *copy_prefixes(GSList *prefixes)
{
	GSList *list, *copy = NULL;

	for (list = prefixes; list; list = list->next)
		copy = g_slist_prepend(copy, g_strdup(list->data));

	return copy;
}

/*
 * Helper struct for doing DAD (duplicate address detection).
 * It is refcounted and freed after all reply's to neighbor
 * discovery request are received.
 */
struct own_address {
	int refcount;

	int ifindex;
	GDHCPClient *dhcp_client;
	struct connman_ipconfig *ipconfig;
	GSList *prefixes;
	dhcpv6_cb callback;

	GSList *dad_failed;
	GSList *dad_succeed;
};

static void free_own_address(struct own_address *data)
{
	g_dhcp_client_unref(data->dhcp_client);
	__connman_ipconfig_unref(data->ipconfig);
	g_slist_free_full(data->prefixes, free_prefix);
	g_slist_free_full(data->dad_failed, g_free);
	g_slist_free_full(data->dad_succeed, g_free);

	g_free(data);
}

static struct own_address *ref_own_address(struct own_address *address)
{
	DBG("%p ref %d", address, address->refcount + 1);

	__sync_fetch_and_add(&address->refcount, 1);

	return address;
}

static void unref_own_address(struct own_address *address)
{
	if (!address)
		return;

	DBG("%p ref %d", address, address->refcount - 1);

	if (__sync_fetch_and_sub(&address->refcount, 1) != 1)
		return;

	free_own_address(address);
}

static void set_address(int ifindex, struct connman_ipconfig *ipconfig,
			GSList *prefixes, char *address)
{
	const char *c_address;

	c_address = __connman_ipconfig_get_local(ipconfig);

	if (address && ((c_address && g_strcmp0(address, c_address) != 0) ||
								!c_address)) {
		int prefix_len;

		/* Is this prefix part of the subnet we are suppose to use? */
		prefix_len = check_ipv6_addr_prefix(prefixes, address);

		__connman_ipconfig_address_remove(ipconfig);
		__connman_ipconfig_set_local(ipconfig, address);
		__connman_ipconfig_set_prefixlen(ipconfig, prefix_len);

		DBG("new address %s/%d", address, prefix_len);

		__connman_ipconfig_set_dhcp_address(ipconfig, address);
		__connman_service_save(
			__connman_service_lookup_from_index(ifindex));
	}
}


/*
 * Helper struct that is used when waiting a reply to DECLINE message.
 */
struct decline_cb_data {
	GDHCPClient *dhcp_client;
	dhcpv6_cb callback;
	int ifindex;
	guint timeout;
};

static void decline_reply_callback(struct decline_cb_data *data)
{
	struct connman_network *network;
	struct connman_service *service;

	service = __connman_service_lookup_from_index(data->ifindex);
	network = __connman_service_get_network(service);

	if (data->callback)
		data->callback(network, CONNMAN_DHCPV6_STATUS_RESTART, NULL);

	g_dhcp_client_unref(data->dhcp_client);
}

static gboolean decline_timeout(gpointer user_data)
{
	struct decline_cb_data *data = user_data;

	DBG("ifindex %d", data->ifindex);

	/*
	 * We ignore all DECLINE replies that are received after the timeout
	 */
	g_dhcp_client_register_event(data->dhcp_client,
					G_DHCP_CLIENT_EVENT_DECLINE,
					NULL, NULL);

	decline_reply_callback(data);

	g_free(data);

	return FALSE;
}

static void decline_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct decline_cb_data *data = user_data;

	DBG("ifindex %d", data->ifindex);

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	if (data->timeout)
		g_source_remove(data->timeout);

	decline_reply_callback(data);

	g_free(data);
}

static int dhcpv6_decline(GDHCPClient *dhcp_client, int ifindex,
			dhcpv6_cb callback, GSList *failed)
{
	struct decline_cb_data *data;
	GList *option;
	int code;

	DBG("dhcp_client %p", dhcp_client);

	g_dhcp_client_clear_requests(dhcp_client);

	g_dhcpv6_client_clear_send(dhcp_client, G_DHCPV6_ORO);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SERVERID);

	option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_IA_NA);
	if (!option) {
		option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_IA_TA);
		if (option)
			code = G_DHCPV6_IA_TA;
		else
			return -EINVAL;
	} else
		code = G_DHCPV6_IA_NA;

	g_dhcpv6_client_clear_send(dhcp_client, code);

	g_dhcpv6_client_set_ias(dhcp_client, ifindex, code, NULL, NULL,
				failed);

	clear_callbacks(dhcp_client);

	data = g_try_new(struct decline_cb_data, 1);
	if (!data)
		return -ENOMEM;

	data->ifindex = ifindex;
	data->callback = callback;
	data->dhcp_client = g_dhcp_client_ref(dhcp_client);
	data->timeout = g_timeout_add(DEC_TIMEOUT, decline_timeout, data);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_DECLINE,
				decline_cb, data);

	return g_dhcp_client_start(dhcp_client, NULL);
}

static void dad_reply(struct nd_neighbor_advert *reply,
		unsigned int length, struct in6_addr *addr, void *user_data)
{
	struct own_address *data = user_data;
	GSList *list;
	char address[INET6_ADDRSTRLEN];
	enum __connman_dhcpv6_status status = CONNMAN_DHCPV6_STATUS_FAIL;

	inet_ntop(AF_INET6, addr, address, INET6_ADDRSTRLEN);

	DBG("user %p reply %p len %d address %s index %d data %p", user_data,
		reply, length, address, data->ifindex, data);

	if (!reply) {
		if (length == 0)
			DBG("DAD succeed for %s", address);
		else
			DBG("DAD cannot be done for %s", address);

		data->dad_succeed = g_slist_prepend(data->dad_succeed,
						g_strdup(address));

	} else {
		DBG("DAD failed for %s", address);

		data->dad_failed = g_slist_prepend(data->dad_failed,
						g_strdup(address));
	}

	/*
	 * If refcount == 1 then we got the final reply and can continue.
	 */
	if (data->refcount > 1)
		return;

	for (list = data->dad_succeed; list; list = list->next)
		set_address(data->ifindex, data->ipconfig, data->prefixes,
								list->data);

	if (data->dad_failed) {
		dhcpv6_decline(data->dhcp_client, data->ifindex,
			data->callback, data->dad_failed);
	} else {
		if (data->dad_succeed)
			status = CONNMAN_DHCPV6_STATUS_SUCCEED;

		if (data->callback) {
			struct connman_network *network;
			struct connman_service *service;

			service = __connman_service_lookup_from_index(
								data->ifindex);
			network = __connman_service_get_network(service);
			if (network)
				data->callback(network, status, NULL);
		}
	}

	unref_own_address(data);
}

/*
 * Is the kernel configured to do DAD? If 0, then do not do DAD.
 * See also RFC 4862 chapter 5.4 about DupAddrDetectTransmits
 */
static int dad_transmits(int ifindex)
{
	char name[IF_NAMESIZE];
	gchar *path;
	int value = 1;
	FILE *f;

	if (!if_indextoname(ifindex, name))
		return value;

	path = g_strdup_printf("/proc/sys/net/ipv6/conf/%s/dad_transmits",
								name);

	if (!path)
		return value;

	f = fopen(path, "r");

	g_free(path);

	if (f) {
		if (fscanf(f, "%d", &value) < 0)
			value = 1;

		fclose(f);
	}

	return value;
}

static void do_dad(GDHCPClient *dhcp_client, struct connman_dhcpv6 *dhcp)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig;
	int ifindex;
	GList *option, *list;
	struct own_address *user_data;

	option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_IA_NA);
	if (!option)
		option = g_dhcp_client_get_option(dhcp_client, G_DHCPV6_IA_TA);

	/*
	 * Even if we didn't had any addresses, just try to set
	 * the other options.
	 */
	set_other_addresses(dhcp_client, dhcp);

	if (!option) {
		DBG("Skip DAD as no addresses found in reply");
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_SUCCEED, NULL);

		return;
	}

	ifindex = connman_network_get_index(dhcp->network);

	DBG("index %d", ifindex);

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		connman_error("Can not lookup service for index %d", ifindex);
		goto error;
	}

	ipconfig = __connman_service_get_ip6config(service);
	if (!ipconfig) {
		connman_error("Could not lookup ip6config for index %d",
								ifindex);
		goto error;
	}

	if (!dad_transmits(ifindex)) {
		DBG("Skip DAD because of kernel configuration");

		for (list = option; list; list = list->next)
			set_address(ifindex, ipconfig, dhcp->prefixes,
							option->data);

		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_SUCCEED, NULL);

		return;
	}

	if (g_list_length(option) == 0) {
		DBG("No addresses when doing DAD");
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_SUCCEED, NULL);

		return;
	}

	user_data = g_try_new0(struct own_address, 1);
	if (!user_data)
		goto error;

	user_data->refcount = 0;
	user_data->ifindex = ifindex;
	user_data->dhcp_client = g_dhcp_client_ref(dhcp_client);
	user_data->ipconfig = __connman_ipconfig_ref(ipconfig);
	user_data->prefixes = copy_prefixes(dhcp->prefixes);
	user_data->callback = dhcp->callback;

	/*
	 * We send one neighbor discovery request / address
	 * and after all checks are done, then report the status
	 * via dhcp callback.
	 */

	for (list = option; list; list = list->next) {
		char *address = option->data;
		struct in6_addr addr;
		int ret;

		ref_own_address(user_data);

		if (inet_pton(AF_INET6, address, &addr) < 0) {
			DBG("Invalid IPv6 address %s %d/%s", address,
				-errno, strerror(errno));
			goto fail;
		}

		DBG("user %p address %s client %p ipconfig %p prefixes %p",
			user_data, address,
			user_data->dhcp_client, user_data->ipconfig,
			user_data->prefixes);

		ret = __connman_inet_ipv6_do_dad(ifindex, 1000,
						&addr,
						dad_reply,
						user_data);
		if (ret < 0) {
			DBG("Could not send neighbor solicitation for %s",
								address);
			dad_reply(NULL, -1, &addr, user_data);
		} else {
			DBG("Sent neighbor solicitation %d bytes", ret);
		}
	}

	return;

fail:
	unref_own_address(user_data);

error:
	if (dhcp->callback)
		dhcp->callback(dhcp->network, CONNMAN_DHCPV6_STATUS_FAIL,
								NULL);
}

static gboolean timeout_request_resend(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (dhcp->request_count >= REQ_MAX_RC) {
		DBG("max request retry attempts %d", dhcp->request_count);
		dhcp->request_count = 0;
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		return FALSE;
	}

	dhcp->request_count++;

	dhcp->RT = calc_delay(dhcp->RT, REQ_MAX_RT);
	DBG("request resend RT timeout %d msec", dhcp->RT);
	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_request_resend, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static gboolean request_resend(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	dhcp->RT = calc_delay(dhcp->RT, REQ_MAX_RT);
	DBG("request resend RT timeout %d msec", dhcp->RT);
	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_request_resend, dhcp);

	dhcpv6_request(dhcp, true);

	return FALSE;
}

static void do_resend_request(struct connman_dhcpv6 *dhcp)
{
	if (dhcp->request_count >= REQ_MAX_RC) {
		DBG("max request retry attempts %d", dhcp->request_count);
		dhcp->request_count = 0;
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		return;
	}

	dhcp->request_count++;

	dhcp->RT = calc_delay(dhcp->RT, REQ_MAX_RT);
	DBG("resending request after %d msec", dhcp->RT);
	dhcp->timeout = g_timeout_add(dhcp->RT, request_resend, dhcp);
}

static void re_cb(enum request_type req_type, GDHCPClient *dhcp_client,
		gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;
	uint16_t status;

	clear_timer(dhcp);

	status = g_dhcpv6_client_get_status(dhcp_client);

	DBG("dhcpv6 cb msg %p status %d", dhcp, status);

	/*
	 * RFC 3315, 18.1.8 handle the resend if error
	 */
	if (status  == G_DHCPV6_ERROR_BINDING) {
		dhcpv6_request(dhcp, false);
	} else if (status  == G_DHCPV6_ERROR_MCAST) {
		switch (req_type) {
		case REQ_REQUEST:
			dhcpv6_request(dhcp, true);
			break;
		case REQ_REBIND:
			dhcpv6_rebind(dhcp);
			break;
		case REQ_RENEW:
			dhcpv6_renew(dhcp);
			break;
		}
	} else if (status  == G_DHCPV6_ERROR_LINK) {
		if (req_type == REQ_REQUEST) {
			g_dhcp_client_unref(dhcp->dhcp_client);
			start_solicitation(dhcp);
		} else {
			if (dhcp->callback)
				dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		}
	} else if (status  == G_DHCPV6_ERROR_FAILURE) {
		if (req_type == REQ_REQUEST) {
			/* Rate limit the resend of request message */
			do_resend_request(dhcp);
		} else {
			if (dhcp->callback)
				dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		}
	} else {

		/*
		 * If we did not got any addresses, then re-send
		 * a request.
		 */
		GList *option;

		option = g_dhcp_client_get_option(dhcp->dhcp_client,
						G_DHCPV6_IA_NA);
		if (!option) {
			option = g_dhcp_client_get_option(dhcp->dhcp_client,
							G_DHCPV6_IA_TA);
			if (!option) {
				switch (req_type) {
				case REQ_REQUEST:
					do_resend_request(dhcp);
					break;
				case REQ_REBIND:
					dhcpv6_rebind(dhcp);
					break;
				case REQ_RENEW:
					dhcpv6_renew(dhcp);
					break;
				}
				return;
			}
		}

		if (status == G_DHCPV6_ERROR_SUCCESS)
			do_dad(dhcp_client, dhcp);
		else if (dhcp->callback)
			dhcp->callback(dhcp->network,
				CONNMAN_DHCPV6_STATUS_FAIL, NULL);
	}
}

static void rebind_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	DBG("");

	g_dhcpv6_client_reset_request(dhcp_client);
	g_dhcpv6_client_clear_retransmit(dhcp_client);

	re_cb(REQ_REBIND, dhcp_client, user_data);
}

static int dhcpv6_rebind(struct connman_dhcpv6 *dhcp)
{
	GDHCPClient *dhcp_client;

	DBG("dhcp %p", dhcp);

	dhcp_client = dhcp->dhcp_client;

	g_dhcp_client_clear_requests(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DOMAIN_LIST);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_set_oro(dhcp_client, 3, G_DHCPV6_DNS_SERVERS,
				G_DHCPV6_DOMAIN_LIST, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_set_ia(dhcp_client,
			connman_network_get_index(dhcp->network),
			dhcp->use_ta ? G_DHCPV6_IA_TA : G_DHCPV6_IA_NA,
			NULL, NULL, TRUE, NULL);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_REBIND,
					rebind_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static gboolean dhcpv6_restart(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (dhcp->callback)
		dhcp->callback(dhcp->network, CONNMAN_DHCPV6_STATUS_FAIL,
								NULL);

	return FALSE;
}

/*
 * Check if we need to restart the solicitation procedure. This
 * is done if all the addresses have expired. RFC 3315, 18.1.4
 */
static int check_restart(struct connman_dhcpv6 *dhcp)
{
	time_t current, expired;

	g_dhcpv6_client_get_timeouts(dhcp->dhcp_client, NULL, NULL,
				NULL, &expired);
	current = time(NULL);

	if (current >= expired) {
		DBG("expired by %d secs", (int)(current - expired));

		g_timeout_add(0, dhcpv6_restart, dhcp);

		return -ETIMEDOUT;
	}

	return 0;
}

static gboolean timeout_rebind(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (check_restart(dhcp) < 0)
		return FALSE;

	dhcp->RT = calc_delay(dhcp->RT, REB_MAX_RT);

	DBG("rebind RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_rebind, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static gboolean start_rebind(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (check_restart(dhcp) < 0)
		return FALSE;

	dhcp->RT = initial_rt(REB_TIMEOUT);

	DBG("rebind initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_rebind, dhcp);

	dhcpv6_rebind(dhcp);

	return FALSE;
}

static void request_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	DBG("");

	g_dhcpv6_client_reset_request(dhcp_client);
	g_dhcpv6_client_clear_retransmit(dhcp_client);

	re_cb(REQ_REQUEST, dhcp_client, user_data);
}

static int dhcpv6_request(struct connman_dhcpv6 *dhcp,
			bool add_addresses)
{
	GDHCPClient *dhcp_client;
	uint32_t T1, T2;

	DBG("dhcp %p add %d", dhcp, add_addresses);

	dhcp_client = dhcp->dhcp_client;

	g_dhcp_client_clear_requests(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SERVERID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DOMAIN_LIST);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_set_oro(dhcp_client, 3, G_DHCPV6_DNS_SERVERS,
				G_DHCPV6_DOMAIN_LIST, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_get_timeouts(dhcp_client, &T1, &T2, NULL, NULL);
	g_dhcpv6_client_set_ia(dhcp_client,
			connman_network_get_index(dhcp->network),
			dhcp->use_ta ? G_DHCPV6_IA_TA : G_DHCPV6_IA_NA,
			&T1, &T2, add_addresses, NULL);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_REQUEST,
					request_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static gboolean timeout_request(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (dhcp->request_count >= REQ_MAX_RC) {
		DBG("max request retry attempts %d", dhcp->request_count);
		dhcp->request_count = 0;
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		return FALSE;
	}

	dhcp->request_count++;

	dhcp->RT = calc_delay(dhcp->RT, REQ_MAX_RT);
	DBG("request RT timeout %d msec", dhcp->RT);
	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_request, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static void renew_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	DBG("");

	g_dhcpv6_client_reset_request(dhcp_client);
	g_dhcpv6_client_clear_retransmit(dhcp_client);

	re_cb(REQ_RENEW, dhcp_client, user_data);
}

static int dhcpv6_renew(struct connman_dhcpv6 *dhcp)
{
	GDHCPClient *dhcp_client;
	uint32_t T1, T2;

	DBG("dhcp %p", dhcp);

	dhcp_client = dhcp->dhcp_client;

	g_dhcp_client_clear_requests(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SERVERID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DOMAIN_LIST);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_set_oro(dhcp_client, 3, G_DHCPV6_DNS_SERVERS,
				G_DHCPV6_DOMAIN_LIST, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_get_timeouts(dhcp_client, &T1, &T2, NULL, NULL);
	g_dhcpv6_client_set_ia(dhcp_client,
			connman_network_get_index(dhcp->network),
			dhcp->use_ta ? G_DHCPV6_IA_TA : G_DHCPV6_IA_NA,
			&T1, &T2, TRUE, NULL);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_RENEW,
					renew_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static gboolean timeout_renew(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;
	time_t last_rebind, current;
	uint32_t T2;

	if (check_restart(dhcp) < 0)
		return FALSE;

	g_dhcpv6_client_get_timeouts(dhcp->dhcp_client, NULL, &T2,
				&last_rebind, NULL);
	current = time(NULL);
	if ((unsigned)current >= (unsigned)last_rebind + T2) {
		/*
		 * Do rebind instead if past T2
		 */
		start_rebind(dhcp);
		return FALSE;
	}

	dhcp->RT = calc_delay(dhcp->RT, REN_MAX_RT);

	DBG("renew RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_renew, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static gboolean start_renew(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	dhcp->RT = initial_rt(REN_TIMEOUT);

	DBG("renew initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_renew, dhcp);

	dhcpv6_renew(dhcp);

	return FALSE;
}

int __connman_dhcpv6_start_renew(struct connman_network *network,
							dhcpv6_cb callback)
{
	struct connman_dhcpv6 *dhcp;
	uint32_t T1, T2, delta;
	time_t started, current, expired;

	dhcp = g_hash_table_lookup(network_table, network);
	if (!dhcp)
		return -ENOENT;

	DBG("network %p dhcp %p", network, dhcp);

	clear_timer(dhcp);

	g_dhcpv6_client_get_timeouts(dhcp->dhcp_client, &T1, &T2,
				&started, &expired);

	current = time(NULL);

	DBG("T1 %u T2 %u expires %lu current %lu started %lu", T1, T2,
		(unsigned long)expired, current, started);

	if (T1 == 0xffffffff)
		/* RFC 3315, 22.4 */
		return 0;

	if (T1 == 0) {
		/* RFC 3315, 22.4
		 * Client can choose the timeout.
		 */
		T1 = (expired - started) / 2;
		T2 = (expired - started) / 10 * 8;
	}

	dhcp->callback = callback;

	/* RFC 3315, 18.1.4, start solicit if expired */
	if (check_restart(dhcp) < 0)
		return 0;

	if (T2 != 0xffffffff && T2 > 0) {
		if ((unsigned)current >= (unsigned)started + T2) {
			/* RFC 3315, chapter 18.1.3, start rebind */
			DBG("start rebind immediately");

			dhcp->timeout = g_timeout_add_seconds(0, start_rebind,
							dhcp);

		} else if ((unsigned)current < (unsigned)started + T1) {
			delta = started + T1 - current;
			DBG("renew after %d secs", delta);

			dhcp->timeout = g_timeout_add_seconds(delta,
					start_renew, dhcp);
		} else {
			delta = started + T2 - current;
			DBG("rebind after %d secs", delta);

			dhcp->timeout = g_timeout_add_seconds(delta,
					start_rebind, dhcp);
		}
	}

	return 0;
}

static void release_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	DBG("");
}

int __connman_dhcpv6_start_release(struct connman_network *network,
				dhcpv6_cb callback)
{
	struct connman_dhcpv6 *dhcp;
	GDHCPClient *dhcp_client;

	if (!network_table)
		return 0;   /* we are already released */

	dhcp = g_hash_table_lookup(network_table, network);
	if (!dhcp)
		return -ENOENT;

	DBG("network %p dhcp %p client %p stateless %d", network, dhcp,
					dhcp->dhcp_client, dhcp->stateless);

	if (dhcp->stateless)
		return -EINVAL;

	clear_timer(dhcp);

	dhcp_client = dhcp->dhcp_client;
	if (!dhcp_client) {
		/*
		 * We had started the DHCPv6 handshaking i.e., we have called
		 * __connman_dhcpv6_start() but it has not yet sent
		 * a solicitation message to server. This means that we do not
		 * have DHCPv6 configured yet so we can just quit here.
		 */
		DBG("DHCPv6 was not started");
		return 0;
	}

	g_dhcp_client_clear_requests(dhcp_client);
	g_dhcp_client_clear_values(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SERVERID);

	g_dhcpv6_client_set_ia(dhcp_client,
			connman_network_get_index(dhcp->network),
			dhcp->use_ta ? G_DHCPV6_IA_TA : G_DHCPV6_IA_NA,
			NULL, NULL, TRUE, NULL);

	clear_callbacks(dhcp_client);

	/*
	 * Although we register a callback here we are really not interested in
	 * the answer because it might take too long time and network code
	 * might be in the middle of the disconnect.
	 * So we just inform the server that we are done with the addresses
	 * but ignore the reply from server. This is allowed by RFC 3315
	 * chapter 18.1.6.
	 */
	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_RELEASE,
				release_cb, NULL);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static int dhcpv6_release(struct connman_dhcpv6 *dhcp)
{
	DBG("dhcp %p", dhcp);

	clear_timer(dhcp);

	dhcpv6_free(dhcp);

	if (!dhcp->dhcp_client)
		return 0;

	g_dhcp_client_stop(dhcp->dhcp_client);
	g_dhcp_client_unref(dhcp->dhcp_client);

	dhcp->dhcp_client = NULL;

	return 0;
}

static void remove_network(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	DBG("dhcp %p", dhcp);

	dhcpv6_release(dhcp);

	g_free(dhcp);
}

static gboolean timeout_info_req(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	dhcp->RT = calc_delay(dhcp->RT, INF_MAX_RT);

	DBG("info RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_info_req, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static gboolean start_info_req(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	/* Set the retransmission timeout, RFC 3315 chapter 14 */
	dhcp->RT = initial_rt(INF_TIMEOUT);

	DBG("info initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_info_req, dhcp);

	dhcpv6_info_request(dhcp);

	return FALSE;
}

int __connman_dhcpv6_start_info(struct connman_network *network,
				dhcpv6_cb callback)
{
	struct connman_dhcpv6 *dhcp;
	int delay;
	uint64_t rand;

	DBG("");

	if (network_table) {
		dhcp = g_hash_table_lookup(network_table, network);
		if (dhcp && dhcp->started)
			return -EBUSY;
	}

	dhcp = g_try_new0(struct connman_dhcpv6, 1);
	if (!dhcp)
		return -ENOMEM;

	dhcp->network = network;
	dhcp->callback = callback;
	dhcp->stateless = true;
	dhcp->started = true;

	connman_network_ref(network);

	DBG("replace network %p dhcp %p", network, dhcp);

	g_hash_table_replace(network_table, network, dhcp);

	/* Initial timeout, RFC 3315, 18.1.5 */
	__connman_util_get_random(&rand);
	delay = rand % 1000;

	dhcp->timeout = g_timeout_add(delay, start_info_req, dhcp);

	return 0;
}

static void advertise_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	DBG("dhcpv6 advertise msg %p", dhcp);

	clear_timer(dhcp);

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	if (g_dhcpv6_client_get_status(dhcp_client) != 0) {
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		return;
	}

	dhcp->RT = initial_rt(REQ_TIMEOUT);
	DBG("request initial RT timeout %d msec", dhcp->RT);
	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_request, dhcp);

	dhcp->request_count = 1;

	dhcpv6_request(dhcp, true);
}

static void solicitation_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	/* We get here if server supports rapid commit */
	struct connman_dhcpv6 *dhcp = user_data;

	DBG("dhcpv6 solicitation msg %p", dhcp);

	clear_timer(dhcp);

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	if (g_dhcpv6_client_get_status(dhcp_client) != 0) {
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		return;
	}

	do_dad(dhcp_client, dhcp);
}

static gboolean timeout_solicitation(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	dhcp->RT = calc_delay(dhcp->RT, SOL_MAX_RT);

	DBG("solicit RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_solicitation, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static int dhcpv6_solicitation(struct connman_dhcpv6 *dhcp)
{
	struct connman_service *service;
	struct connman_ipconfig *ipconfig_ipv6;
	GDHCPClient *dhcp_client;
	GDHCPClientError error;
	int index, ret;

	DBG("dhcp %p", dhcp);

	index = connman_network_get_index(dhcp->network);

	dhcp_client = g_dhcp_client_new(G_DHCP_IPV6, index, &error);
	if (error != G_DHCP_CLIENT_ERROR_NONE) {
		clear_timer(dhcp);
		return -EINVAL;
	}

	if (getenv("CONNMAN_DHCPV6_DEBUG"))
		g_dhcp_client_set_debug(dhcp_client, dhcpv6_debug, "DHCPv6");

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		clear_timer(dhcp);
		g_dhcp_client_unref(dhcp_client);
		return -EINVAL;
	}

	ret = set_duid(service, dhcp->network, dhcp_client, index);
	if (ret < 0) {
		clear_timer(dhcp);
		g_dhcp_client_unref(dhcp_client);
		return ret;
	}

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_RAPID_COMMIT);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DOMAIN_LIST);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_set_oro(dhcp_client, 3, G_DHCPV6_DNS_SERVERS,
				G_DHCPV6_DOMAIN_LIST, G_DHCPV6_SNTP_SERVERS);

	ipconfig_ipv6 = __connman_service_get_ip6config(service);
	dhcp->use_ta = __connman_ipconfig_ipv6_privacy_enabled(ipconfig_ipv6);

	g_dhcpv6_client_set_ia(dhcp_client, index,
			dhcp->use_ta ? G_DHCPV6_IA_TA : G_DHCPV6_IA_NA,
			NULL, NULL, FALSE, NULL);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_SOLICITATION,
				solicitation_cb, dhcp);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_ADVERTISE,
				advertise_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static gboolean start_solicitation(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	/* Set the retransmission timeout, RFC 3315 chapter 14 */
	dhcp->RT = initial_rt(SOL_TIMEOUT);

	DBG("solicit initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_solicitation, dhcp);

	dhcpv6_solicitation(dhcp);

	return FALSE;
}

int __connman_dhcpv6_start(struct connman_network *network,
				GSList *prefixes, dhcpv6_cb callback)
{
	struct connman_service *service;
	struct connman_dhcpv6 *dhcp;
	int delay;
	uint64_t rand;

	DBG("");

	if (network_table) {
		dhcp = g_hash_table_lookup(network_table, network);
		if (dhcp && dhcp->started)
			return -EBUSY;
	}

	service = connman_service_lookup_from_network(network);
	if (!service)
		return -EINVAL;

	dhcp = g_try_new0(struct connman_dhcpv6, 1);
	if (!dhcp)
		return -ENOMEM;

	dhcp->network = network;
	dhcp->callback = callback;
	dhcp->prefixes = prefixes;
	dhcp->started = true;

	connman_network_ref(network);

	DBG("replace network %p dhcp %p", network, dhcp);

	g_hash_table_replace(network_table, network, dhcp);

	/* Initial timeout, RFC 3315, 17.1.2 */
	__connman_util_get_random(&rand);
	delay = rand % 1000;

	/*
	 * Start from scratch.
	 * RFC 3315, chapter 17.1.2 Solicitation message
	 *
	 * Note that we do not send CONFIRM message here as it does
	 * not make much sense because we do not save expiration time
	 * so we cannot really know how long the saved address is valid
	 * anyway. The reply to CONFIRM message does not send
	 * expiration times back to us. Because of this we need to
	 * start using SOLICITATION anyway.
	 */
	dhcp->timeout = g_timeout_add(delay, start_solicitation, dhcp);

	return 0;
}

void __connman_dhcpv6_stop(struct connman_network *network)
{
	DBG("");

	if (!network_table)
		return;

	if (g_hash_table_remove(network_table, network))
		connman_network_unref(network);
}

static int save_prefixes(struct connman_ipconfig *ipconfig,
			GSList *prefixes)
{
	GSList *list;
	int i = 0, count = g_slist_length(prefixes);
	char **array;

	if (count == 0)
		return 0;

	array = g_try_new0(char *, count + 1);
	if (!array)
		return -ENOMEM;

	for (list = prefixes; list; list = list->next) {
		char *elem, addr_str[INET6_ADDRSTRLEN];
		GDHCPIAPrefix *prefix = list->data;

		elem = g_strdup_printf("%s/%d", inet_ntop(AF_INET6,
				&prefix->prefix, addr_str, INET6_ADDRSTRLEN),
				prefix->prefixlen);
		if (!elem) {
			g_strfreev(array);
			return -ENOMEM;
		}

		array[i++] = elem;
	}

	__connman_ipconfig_set_dhcpv6_prefixes(ipconfig, array);
	return 0;
}

static GSList *load_prefixes(struct connman_ipconfig *ipconfig)
{
	int i;
	GSList *list = NULL;
	char **array =  __connman_ipconfig_get_dhcpv6_prefixes(ipconfig);

	if (!array)
		return NULL;

	for (i = 0; array[i]; i++) {
		GDHCPIAPrefix *prefix;
		long int value;
		char *ptr, **elems = g_strsplit(array[i], "/", 0);

		if (!elems)
			return list;

		value = strtol(elems[1], &ptr, 10);
		if (ptr != elems[1] && *ptr == '\0' && value <= 128) {
			struct in6_addr addr;

			if (inet_pton(AF_INET6, elems[0], &addr) == 1) {
				prefix = g_try_new0(GDHCPIAPrefix, 1);
				if (!prefix) {
					g_strfreev(elems);
					return list;
				}
				memcpy(&prefix->prefix, &addr,
					sizeof(struct in6_addr));
				prefix->prefixlen = value;

				list = g_slist_prepend(list, prefix);
			}
		}

		g_strfreev(elems);
	}

	return list;
}

static GDHCPIAPrefix *copy_prefix(gpointer data)
{
	GDHCPIAPrefix *copy, *prefix = data;

	copy = g_try_new(GDHCPIAPrefix, 1);
	if (!copy)
		return NULL;

	memcpy(copy, prefix, sizeof(GDHCPIAPrefix));

	return copy;
}

static GSList *copy_and_convert_prefixes(GList *prefixes)
{
	GSList *copy = NULL;
	GList *list;

	for (list = prefixes; list; list = list->next)
		copy = g_slist_prepend(copy, copy_prefix(list->data));

	return copy;
}

static int set_prefixes(GDHCPClient *dhcp_client, struct connman_dhcpv6 *dhcp)
{
	if (dhcp->prefixes)
		g_slist_free_full(dhcp->prefixes, free_prefix);

	dhcp->prefixes =
		copy_and_convert_prefixes(g_dhcp_client_get_option(dhcp_client,
							G_DHCPV6_IA_PD));

	DBG("Got %d prefix", g_slist_length(dhcp->prefixes));

	if (dhcp->callback) {
		uint16_t status = g_dhcpv6_client_get_status(dhcp_client);
		if (status == G_DHCPV6_ERROR_NO_PREFIX)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		else {
			struct connman_service *service;
			struct connman_ipconfig *ipconfig;
			int ifindex = connman_network_get_index(dhcp->network);

			service = __connman_service_lookup_from_index(ifindex);
			if (service) {
				ipconfig = __connman_service_get_ip6config(
								service);
				save_prefixes(ipconfig, dhcp->prefixes);
				__connman_service_save(service);
			}

			dhcp->callback(dhcp->network,
				CONNMAN_DHCPV6_STATUS_SUCCEED, dhcp->prefixes);
		}
	} else {
		g_slist_free_full(dhcp->prefixes, free_prefix);
		dhcp->prefixes = NULL;
	}

	return 0;
}

static void re_pd_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;
	uint16_t status;
	int ret;

	status = g_dhcpv6_client_get_status(dhcp_client);

	DBG("dhcpv6 cb msg %p status %d", dhcp, status);

	if (status  == G_DHCPV6_ERROR_BINDING) {
		/* RFC 3315, 18.1.8 */
		dhcpv6_pd_request(dhcp);
	} else {
		ret = set_prefixes(dhcp_client, dhcp);
		if (ret < 0) {
			if (dhcp->callback)
				dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
			return;
		}
	}
}

static void rebind_pd_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	DBG("");

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	re_pd_cb(dhcp_client, user_data);
}

static GDHCPClient *create_pd_client(struct connman_dhcpv6 *dhcp, int *err)
{
	GDHCPClient *dhcp_client;
	GDHCPClientError error;
	struct connman_service *service;
	int index, ret;
	uint32_t iaid;

	index = connman_network_get_index(dhcp->network);

	dhcp_client = g_dhcp_client_new(G_DHCP_IPV6, index, &error);
	if (error != G_DHCP_CLIENT_ERROR_NONE) {
		*err = -EINVAL;
		return NULL;
	}

	if (getenv("CONNMAN_DHCPV6_DEBUG"))
		g_dhcp_client_set_debug(dhcp_client, dhcpv6_debug, "DHCPv6:PD");

	service = connman_service_lookup_from_network(dhcp->network);
	if (!service) {
		g_dhcp_client_unref(dhcp_client);
		*err = -EINVAL;
		return NULL;
	}

	ret = set_duid(service, dhcp->network, dhcp_client, index);
	if (ret < 0) {
		g_dhcp_client_unref(dhcp_client);
		*err = ret;
		return NULL;
	}

	g_dhcpv6_client_create_iaid(dhcp_client, index, (unsigned char *)&iaid);
	g_dhcpv6_client_set_iaid(dhcp_client, iaid);

	return dhcp_client;
}

static int dhcpv6_pd_rebind(struct connman_dhcpv6 *dhcp)
{
	GDHCPClient *dhcp_client;
	uint32_t T1, T2;

	DBG("dhcp %p", dhcp);

	if (!dhcp->dhcp_client) {
		/*
		 * We skipped the solicitation phase
		 */
		int err;

		dhcp->dhcp_client = create_pd_client(dhcp, &err);
		if (!dhcp->dhcp_client) {
			clear_timer(dhcp);
			return err;
		}
	}

	dhcp_client = dhcp->dhcp_client;

	g_dhcp_client_clear_requests(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_get_timeouts(dhcp_client, &T1, &T2, NULL, NULL);
	g_dhcpv6_client_set_pd(dhcp_client, &T1, &T2, dhcp->prefixes);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_REBIND,
					rebind_pd_cb, dhcp);

	return g_dhcp_client_start(dhcp_client, NULL);
}

static void renew_pd_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	DBG("");

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	re_pd_cb(dhcp_client, user_data);
}

static int dhcpv6_pd_renew(struct connman_dhcpv6 *dhcp)
{
	GDHCPClient *dhcp_client;
	uint32_t T1, T2;

	DBG("dhcp %p", dhcp);

	dhcp_client = dhcp->dhcp_client;

	g_dhcp_client_clear_requests(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SERVERID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_get_timeouts(dhcp_client, &T1, &T2, NULL, NULL);
	g_dhcpv6_client_set_pd(dhcp_client, &T1, &T2, dhcp->prefixes);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_RENEW,
					renew_pd_cb, dhcp);

	return g_dhcp_client_start(dhcp_client, NULL);
}

/*
 * Check if we need to restart the solicitation procedure. This
 * is done if all the prefixes have expired.
 */
static int check_pd_restart(struct connman_dhcpv6 *dhcp)
{
	time_t current, expired;

	g_dhcpv6_client_get_timeouts(dhcp->dhcp_client, NULL, NULL,
				NULL, &expired);
	current = time(NULL);

	if (current > expired) {
		DBG("expired by %d secs", (int)(current - expired));

		g_timeout_add(0, dhcpv6_restart, dhcp);

		return -ETIMEDOUT;
	}

	return 0;
}

static gboolean timeout_pd_rebind(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (check_pd_restart(dhcp) < 0)
		return FALSE;

	dhcp->RT = calc_delay(dhcp->RT, REB_MAX_RT);

	DBG("rebind RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_pd_rebind, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static gboolean start_pd_rebind(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (check_pd_restart(dhcp) < 0)
		return FALSE;

	dhcp->RT = initial_rt(REB_TIMEOUT);

	DBG("rebind initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_pd_rebind, dhcp);

	dhcpv6_pd_rebind(dhcp);

	return FALSE;
}

static gboolean timeout_pd_rebind_confirm(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	dhcp->RT = calc_delay(dhcp->RT, CNF_MAX_RT);

	DBG("rebind with confirm RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT,
					timeout_pd_rebind_confirm, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static gboolean timeout_pd_max_confirm(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	dhcp->MRD = 0;

	clear_timer(dhcp);

	DBG("rebind with confirm max retransmit duration timeout");

	g_dhcpv6_client_clear_retransmit(dhcp->dhcp_client);

	if (dhcp->callback)
		dhcp->callback(dhcp->network,
			CONNMAN_DHCPV6_STATUS_FAIL, NULL);

	return FALSE;
}

static gboolean start_pd_rebind_with_confirm(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	dhcp->RT = initial_rt(CNF_TIMEOUT);

	DBG("rebind with confirm initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT,
					timeout_pd_rebind_confirm, dhcp);
	dhcp->MRD = g_timeout_add(CNF_MAX_RD, timeout_pd_max_confirm, dhcp);

	dhcpv6_pd_rebind(dhcp);

	return FALSE;
}

static gboolean timeout_pd_renew(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (check_pd_restart(dhcp) < 0)
		return FALSE;

	dhcp->RT = calc_delay(dhcp->RT, REN_MAX_RT);

	DBG("renew RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_renew, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static gboolean start_pd_renew(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	dhcp->RT = initial_rt(REN_TIMEOUT);

	DBG("renew initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_pd_renew, dhcp);

	dhcpv6_pd_renew(dhcp);

	return FALSE;
}

int __connman_dhcpv6_start_pd_renew(struct connman_network *network,
				dhcpv6_cb callback)
{
	struct connman_dhcpv6 *dhcp;
	uint32_t T1, T2;
	time_t started, current, expired;

	dhcp = g_hash_table_lookup(network_pd_table, network);
	if (!dhcp)
		return -ENOENT;

	DBG("network %p dhcp %p", network, dhcp);

	clear_timer(dhcp);

	g_dhcpv6_client_get_timeouts(dhcp->dhcp_client, &T1, &T2,
				&started, &expired);

	current = time(NULL);

	DBG("T1 %u T2 %u expires %lu current %lu started %lu", T1, T2,
		expired, current, started);

	if (T1 == 0xffffffff)
		/* RFC 3633, ch 9 */
		return 0;

	if (T1 == 0)
		/* RFC 3633, ch 9
		 * Client can choose the timeout.
		 */
		T1 = 120;

	dhcp->callback = callback;

	/* RFC 3315, 18.1.4, start solicit if expired */
	if (check_pd_restart(dhcp) < 0)
		return 0;

	if (T2 != 0xffffffff && T2 > 0) {
		if ((unsigned)current >= (unsigned)started + T2) {
			/* RFC 3315, chapter 18.1.3, start rebind */
			DBG("rebind after %d secs", T2);

			dhcp->timeout = g_timeout_add_seconds(T2,
							start_pd_rebind,
							dhcp);

		} else if ((unsigned)current < (unsigned)started + T1) {
			DBG("renew after %d secs", T1);

			dhcp->timeout = g_timeout_add_seconds(T1,
							start_pd_renew,
							dhcp);
		} else {
			DBG("rebind after %d secs", T2 - T1);

			dhcp->timeout = g_timeout_add_seconds(T2 - T1,
							start_pd_rebind,
							dhcp);
		}
	}

	return 0;
}

static void release_pd_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	DBG("");
}

int __connman_dhcpv6_start_pd_release(struct connman_network *network,
				dhcpv6_cb callback)
{
	struct connman_dhcpv6 *dhcp;
	GDHCPClient *dhcp_client;
	uint32_t T1, T2;

	if (!network_table)
		return 0;   /* we are already released */

	dhcp = g_hash_table_lookup(network_pd_table, network);
	if (!dhcp)
		return -ENOENT;

	DBG("network %p dhcp %p client %p", network, dhcp, dhcp->dhcp_client);

	clear_timer(dhcp);

	dhcp_client = dhcp->dhcp_client;
	if (!dhcp_client) {
		DBG("DHCPv6 PD was not started");
		return 0;
	}

	g_dhcp_client_clear_requests(dhcp_client);
	g_dhcp_client_clear_values(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SERVERID);

	g_dhcpv6_client_get_timeouts(dhcp_client, &T1, &T2, NULL, NULL);
	g_dhcpv6_client_set_pd(dhcp_client, &T1, &T2, dhcp->prefixes);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_RELEASE,
					release_pd_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static gboolean timeout_pd_request(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	if (dhcp->request_count >= REQ_MAX_RC) {
		DBG("max request retry attempts %d", dhcp->request_count);
		dhcp->request_count = 0;
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		return FALSE;
	}

	dhcp->request_count++;

	dhcp->RT = calc_delay(dhcp->RT, REQ_MAX_RT);
	DBG("request RT timeout %d msec", dhcp->RT);
	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_pd_request, dhcp);

	g_dhcpv6_client_set_retransmit(dhcp->dhcp_client);

	g_dhcp_client_start(dhcp->dhcp_client, NULL);

	return FALSE;
}

static void request_pd_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;
	uint16_t status;

	DBG("");

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	status = g_dhcpv6_client_get_status(dhcp_client);

	DBG("dhcpv6 pd cb msg %p status %d", dhcp, status);

	if (status  == G_DHCPV6_ERROR_BINDING) {
		/* RFC 3315, 18.1.8 */
		dhcpv6_pd_request(dhcp);
	} else {
		set_prefixes(dhcp_client, dhcp);
	}
}

static int dhcpv6_pd_request(struct connman_dhcpv6 *dhcp)
{
	GDHCPClient *dhcp_client;
	uint32_t T1 = 0, T2 = 0;

	DBG("dhcp %p", dhcp);

	dhcp_client = dhcp->dhcp_client;

	g_dhcp_client_clear_requests(dhcp_client);

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SERVERID);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_DNS_SERVERS);
	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_SNTP_SERVERS);

	g_dhcpv6_client_get_timeouts(dhcp_client, &T1, &T2, NULL, NULL);
	g_dhcpv6_client_set_pd(dhcp_client, &T1, &T2, dhcp->prefixes);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client, G_DHCP_CLIENT_EVENT_REQUEST,
					request_pd_cb, dhcp);

	return g_dhcp_client_start(dhcp_client, NULL);
}

static void advertise_pd_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	DBG("dhcpv6 advertise pd msg %p", dhcp);

	clear_timer(dhcp);

	g_dhcpv6_client_clear_retransmit(dhcp_client);

	if (g_dhcpv6_client_get_status(dhcp_client) != 0) {
		if (dhcp->callback)
			dhcp->callback(dhcp->network,
					CONNMAN_DHCPV6_STATUS_FAIL, NULL);
		return;
	}

	dhcp->RT = initial_rt(REQ_TIMEOUT);
	DBG("request initial RT timeout %d msec", dhcp->RT);
	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_pd_request, dhcp);

	dhcp->request_count = 1;

	dhcpv6_pd_request(dhcp);
}

static void solicitation_pd_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	/*
	 * This callback is here so that g_dhcp_client_start()
	 * will enter the proper L3 mode.
	 */
	DBG("DHCPv6 %p solicitation msg received, ignoring it", user_data);
}

static int dhcpv6_pd_solicitation(struct connman_dhcpv6 *dhcp)
{
	GDHCPClient *dhcp_client;
	int ret;

	DBG("dhcp %p", dhcp);

	dhcp_client = create_pd_client(dhcp, &ret);
	if (!dhcp_client) {
		clear_timer(dhcp);
		return ret;
	}

	g_dhcp_client_set_request(dhcp_client, G_DHCPV6_CLIENTID);

	g_dhcpv6_client_set_pd(dhcp_client, NULL, NULL, NULL);

	clear_callbacks(dhcp_client);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_ADVERTISE,
				advertise_pd_cb, dhcp);

	g_dhcp_client_register_event(dhcp_client,
				G_DHCP_CLIENT_EVENT_SOLICITATION,
				solicitation_pd_cb, dhcp);

	dhcp->dhcp_client = dhcp_client;

	return g_dhcp_client_start(dhcp_client, NULL);
}

static gboolean start_pd_solicitation(gpointer user_data)
{
	struct connman_dhcpv6 *dhcp = user_data;

	/* Set the retransmission timeout, RFC 3315 chapter 14 */
	dhcp->RT = initial_rt(SOL_TIMEOUT);

	DBG("solicit initial RT timeout %d msec", dhcp->RT);

	dhcp->timeout = g_timeout_add(dhcp->RT, timeout_solicitation, dhcp);

	dhcpv6_pd_solicitation(dhcp);

	return FALSE;
}

int __connman_dhcpv6_start_pd(int index, GSList *prefixes, dhcpv6_cb callback)
{
	struct connman_service *service;
	struct connman_network *network;
	struct connman_dhcpv6 *dhcp;

	if (index < 0)
		return 0;

	DBG("index %d", index);

	service = __connman_service_lookup_from_index(index);
	if (!service)
		return -EINVAL;

	network = __connman_service_get_network(service);
	if (!network)
		return -EINVAL;

	if (network_pd_table) {
		dhcp = g_hash_table_lookup(network_pd_table, network);
		if (dhcp && dhcp->started)
			return -EBUSY;
	}

	dhcp = g_try_new0(struct connman_dhcpv6, 1);
	if (!dhcp)
		return -ENOMEM;

	dhcp->network = network;
	dhcp->callback = callback;
	dhcp->started = true;

	if (!prefixes) {
		/*
		 * Try to load the earlier prefixes if caller did not supply
		 * any that we could use.
		 */
		struct connman_ipconfig *ipconfig;
		ipconfig = __connman_service_get_ip6config(service);

		dhcp->prefixes = load_prefixes(ipconfig);
	} else
		dhcp->prefixes = prefixes;

	connman_network_ref(network);

	DBG("replace network %p dhcp %p", network, dhcp);

	g_hash_table_replace(network_pd_table, network, dhcp);

	if (!dhcp->prefixes) {
		/*
		 * Refresh start, try to get prefixes.
		 */
		start_pd_solicitation(dhcp);
	} else {
		/*
		 * We used to have prefixes, try to use them again.
		 * We need to use timeouts from confirm msg, RFC 3633, ch 12.1
		 */
		start_pd_rebind_with_confirm(dhcp);
	}

	return 0;
}

void __connman_dhcpv6_stop_pd(int index)
{
	struct connman_service *service;
	struct connman_network *network;

	if (index < 0)
		return;

	DBG("index %d", index);

	if (!network_pd_table)
		return;

	service = __connman_service_lookup_from_index(index);
	if (!service)
		return;

	network = __connman_service_get_network(service);
	if (!network)
		return;

	__connman_dhcpv6_start_pd_release(network, NULL);

	if (g_hash_table_remove(network_pd_table, network))
		connman_network_unref(network);
}

int __connman_dhcpv6_init(void)
{
	DBG("");

	network_table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, remove_network);

	network_pd_table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, remove_network);

	return 0;
}

void __connman_dhcpv6_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(network_table);
	network_table = NULL;

	g_hash_table_destroy(network_pd_table);
	network_pd_table = NULL;
}
