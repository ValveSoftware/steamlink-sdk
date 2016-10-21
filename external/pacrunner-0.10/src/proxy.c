/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#include "pacrunner.h"

struct pacrunner_proxy {
	gint refcount;

	char *interface;
	enum pacrunner_proxy_method method;
	char *url;
	char *script;
	GList **servers;
	GList **excludes;
	gboolean browser_only;
	GList *domains;
	void *jsctx;
};

struct proxy_domain {
	char *domain;
	int proto;
	union {
		struct in_addr ip4;
		struct in6_addr ip6;
	} addr;
	int mask;
};

static GList *proxy_list = NULL;
static pthread_mutex_t proxy_mutex;
static pthread_cond_t proxy_cond;
static int timeout_source = 0;
static gint proxy_updating = -1; /* -1 for 'never set', with timeout */

struct pacrunner_proxy *pacrunner_proxy_create(const char *interface)
{
	struct pacrunner_proxy *proxy;

	DBG("interface %s", interface);

	proxy = g_try_new0(struct pacrunner_proxy, 1);
	if (!proxy)
		return NULL;

	proxy->refcount = 1;

	proxy->interface = g_strdup(interface);
	proxy->method = PACRUNNER_PROXY_METHOD_UNKNOWN;

	DBG("proxy %p", proxy);

	return proxy;
}

struct pacrunner_proxy *pacrunner_proxy_ref(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	if (!proxy)
		return NULL;

	g_atomic_int_inc(&proxy->refcount);

	return proxy;
}

static void proxy_domain_destroy(gpointer data)
{
	struct proxy_domain *domain = data;
	g_return_if_fail(domain != NULL);

	g_free(domain->domain);
	g_free(domain);
}

static void reset_proxy(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	g_free(proxy->url);
	proxy->url = NULL;

	g_free(proxy->script);
	proxy->script = NULL;

	__pacrunner_manual_destroy_servers(proxy->servers);
	proxy->servers = NULL;

	__pacrunner_manual_destroy_excludes(proxy->excludes);
	proxy->excludes = NULL;
}

void pacrunner_proxy_unref(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	if (!proxy)
		return;

	if (!g_atomic_int_dec_and_test(&proxy->refcount))
		return;

	__pacrunner_js_clear_proxy(proxy);

	reset_proxy(proxy);

	g_list_free_full(proxy->domains, proxy_domain_destroy);
	proxy->domains = NULL;

	g_free(proxy->interface);
	g_free(proxy);
}

const char *pacrunner_proxy_get_interface(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	if (!proxy)
		return NULL;

	return proxy->interface;
}

const char *pacrunner_proxy_get_script(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	if (!proxy)
		return NULL;

	return proxy->script;
}

static gboolean check_browser_protocol(const char *url)
{
	static const char *browser_schemes[] = {
		"http://",
		"https://",
		"ftp://",
		"nntp://",
		"nntps://",
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS(browser_schemes); i++) {
		if (strncmp(browser_schemes[i], url,
				strlen(browser_schemes[i])) == 0)
			return TRUE;
	}

	return FALSE;
}

int pacrunner_proxy_set_domains(struct pacrunner_proxy *proxy, char **domains,
					gboolean browser_only)
{
	int len;
	char *slash, **domain;
	char ip[INET6_ADDRSTRLEN + 1];

	DBG("proxy %p domains %p browser-only %u", proxy,
					domains, browser_only);

	proxy->browser_only = browser_only;

	if (!proxy)
		return -EINVAL;

	g_list_free_full(proxy->domains, proxy_domain_destroy);
	proxy->domains = NULL;

	if (!domains)
		return 0;

	for (domain = domains; *domain; domain++) {
		struct proxy_domain *data;

		data = g_malloc0(sizeof(struct proxy_domain));

		DBG("proxy %p domain %s", proxy, *domain);

		slash = strchr(*domain, '/');
		if (!slash) {
			data->domain = g_strdup(*domain);
			data->proto = 0;

			proxy->domains = g_list_append(proxy->domains, data);
			continue;
		}

		len = slash - *domain;
		if (len > INET6_ADDRSTRLEN) {
			g_free(data);
			continue;
		}

		strncpy(ip, *domain, len);
		ip[len] = '\0';

		if (inet_pton(AF_INET, ip, &(data->addr.ip4)) == 1) {
			data->domain = NULL;
			data->proto = 4;

			errno = 0;
			data->mask = strtol(slash + 1, NULL, 10);
			if (errno || data->mask < 0 || data->mask > 32) {
				g_free(data);
				continue;
			}

			proxy->domains = g_list_append(proxy->domains, data);
		} else if (inet_pton(AF_INET6, ip, &(data->addr.ip6)) == 1) {
			data->domain = NULL;
			data->proto = 6;

			errno = 0;
			data->mask = strtol(slash + 1, NULL, 10);
			if (errno || data->mask < 0 || data->mask > 128) {
				g_free(data);
				continue;
			}

			proxy->domains = g_list_append(proxy->domains, data);
		} else {
			g_free(data);
			continue;
		}
	}

	return 0;
}

static int set_method(struct pacrunner_proxy *proxy,
					enum pacrunner_proxy_method method)
{
	DBG("proxy %p method %d", proxy, method);

	if (!proxy)
		return -EINVAL;

	if (proxy->method == method)
		return 0;

	proxy->method = method;

	reset_proxy(proxy);

	return 0;
}

int pacrunner_proxy_set_direct(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	if (!proxy)
		return -EINVAL;

	pthread_mutex_lock(&proxy_mutex);
	if (proxy_updating == -1) {
		proxy_updating = 0;
		pthread_cond_broadcast(&proxy_cond);
	}
	pthread_mutex_unlock(&proxy_mutex);

	return set_method(proxy, PACRUNNER_PROXY_METHOD_DIRECT);
}

int pacrunner_proxy_set_manual(struct pacrunner_proxy *proxy,
					char **servers, char **excludes)
{
	int err;

	DBG("proxy %p servers %p excludes %p", proxy, servers, excludes);

	if (!proxy)
		return -EINVAL;

	if (!servers)
		return -EINVAL;

	err = set_method(proxy, PACRUNNER_PROXY_METHOD_MANUAL);
	if (err < 0)
		return err;

	proxy->servers = __pacrunner_manual_parse_servers(servers);
	if (!proxy->servers)
		return -EINVAL;

	proxy->excludes = __pacrunner_manual_parse_excludes(excludes);

	pacrunner_proxy_enable(proxy);

	return 0;
}

static void download_callback(char *content, void *user_data)
{
	struct pacrunner_proxy *proxy = user_data;

	DBG("url %s content %p", proxy->url, content);

	if (!content) {
		pacrunner_error("Failed to retrieve PAC script");
		goto done;
	}

	g_free(proxy->script);
	proxy->script = content;

	pacrunner_proxy_enable(proxy);

done:
	pthread_mutex_lock(&proxy_mutex);
	proxy_updating--;
	pthread_cond_broadcast(&proxy_cond);
	pthread_mutex_unlock(&proxy_mutex);
	pacrunner_proxy_unref(proxy);
}

int pacrunner_proxy_set_auto(struct pacrunner_proxy *proxy,
					const char *url, const char *script)
{
	int err;

	DBG("proxy %p url %s script %p", proxy, url, script);

	if (!proxy)
		return -EINVAL;

	err = set_method(proxy, PACRUNNER_PROXY_METHOD_AUTO);
	if (err < 0)
		return err;

	g_free(proxy->url);
	proxy->url = g_strdup(url);

	if (!proxy->url) {
		g_free(proxy->script);
		proxy->script = g_strdup(script);
	} else {
		g_free(proxy->script);
		proxy->script = NULL;
	}

	if (proxy->script) {
		pacrunner_proxy_enable(proxy);
		return 0;
	}

	pacrunner_proxy_ref(proxy);

	pthread_mutex_lock(&proxy_mutex);
	err = __pacrunner_download_update(proxy->interface, proxy->url,
						download_callback, proxy);
	if (err < 0) {
		pacrunner_proxy_unref(proxy);
		if (proxy_updating == -1) {
			proxy_updating = 0;
			pthread_cond_broadcast(&proxy_cond);
		}
		pthread_mutex_unlock(&proxy_mutex);
		return err;
	}
	if (proxy_updating == -1)
		proxy_updating = 1;
	else
		proxy_updating++;
	pthread_mutex_unlock(&proxy_mutex);

	return 0;
}

int pacrunner_proxy_enable(struct pacrunner_proxy *proxy)
{
	GList *list;

	DBG("proxy %p", proxy);

	if (!proxy)
		return -EINVAL;

	list = g_list_find(proxy_list, proxy);
	if (list)
		return -EEXIST;

	proxy = pacrunner_proxy_ref(proxy);
	if (!proxy)
		return -EIO;

	__pacrunner_js_set_proxy(proxy);

	pthread_mutex_lock(&proxy_mutex);
	if (proxy_updating == -1) {
		proxy_updating = 0;
		pthread_cond_broadcast(&proxy_cond);
	}
	proxy_list = g_list_append(proxy_list, proxy);
	pthread_mutex_unlock(&proxy_mutex);

	return 0;
}

int pacrunner_proxy_disable(struct pacrunner_proxy *proxy)
{
	GList *list;

	DBG("proxy %p", proxy);

	if (!proxy)
		return -EINVAL;

	list = g_list_find(proxy_list, proxy);
	if (!list)
		return -ENXIO;

	pthread_mutex_lock(&proxy_mutex);
	proxy_list = g_list_remove_link(proxy_list, list);
	pthread_mutex_unlock(&proxy_mutex);

	__pacrunner_js_set_proxy(NULL);

	pacrunner_proxy_unref(proxy);

	return 0;
}

static int compare_legacy_ip_in_net(struct in_addr *host,
					struct proxy_domain *match)
{
	if (ntohl(host->s_addr ^ match->addr.ip4.s_addr) >> (32 - match->mask))
		return -1;

	return 0;
}

static int compare_ipv6_in_net(struct in6_addr *host,
					struct proxy_domain *match)
{
	int i, shift;

	for (i = 0; i < (match->mask)/8; i++) {
		if (host->s6_addr[i] != match->addr.ip6.s6_addr[i])
			return -1;
	}

	if ((match->mask) % 8) {
		/**
		 * If mask bits are not multiple of 8 , 1-7 bits are left
		 * to be compared.
		 */
		shift = 8 - (match->mask - (i*8));

		if ((host->s6_addr[i] >> shift) !=
			(match->addr.ip6.s6_addr[i] >> shift))
			return -1;
	}

	return 0;
}

static int compare_host_in_domain(const char *host, struct proxy_domain *match)
{
	size_t hlen = strlen(host);
	size_t dlen = strlen(match->domain);

	if ((hlen >= dlen) && (strcmp(host + (hlen - dlen),
						match->domain) == 0)) {
		if (hlen == dlen || host[hlen - dlen - 1] == '.')
			return 0;
	}

	return -1;
}

/**
 * A request for a "browser" protocol would match the following configs
 * order of preference (if they exist):
 * • Matching "Domains", BrowserOnly==TRUE
 * • Matching "Domains", BrowserOnly==FALSE
 * • Domains==NULL, BrowserOnly==TRUE
 * • Domains==NULL, BrowserOnly==FALSE
 *
 * A request for a non-browser protocol would match the following :
 * • Matching "Domains", BrowserOnly==FALSE
 * • Domains==NULL, BrowserOnly==FALSE (except if a config exists with
 * Matching "Domains", BrowserOnly==TRUE, in which case we need to
 * return NULL).
 **/
char *pacrunner_proxy_lookup(const char *url, const char *host)
{
	GList *l, *list;
	int protocol = 0;
	struct in_addr ip4_addr;
	struct in6_addr ip6_addr;
	gboolean request_is_browser;
	struct pacrunner_proxy *proxy = NULL;

	/* Four classes of 'match' */
	struct pacrunner_proxy *alldomains_browseronly = NULL;
	struct pacrunner_proxy *alldomains_allprotos = NULL;
	struct pacrunner_proxy *domainmatch_browseronly = NULL;
	struct pacrunner_proxy *domainmatch_allprotos = NULL;

	DBG("url %s host %s", url, host);

	pthread_mutex_lock(&proxy_mutex);
	while (proxy_updating)
		pthread_cond_wait(&proxy_cond, &proxy_mutex);

	if (!proxy_list) {
		pthread_mutex_unlock(&proxy_mutex);
		return NULL;
	}

	if (inet_pton(AF_INET, host, &ip4_addr) == 1) {
		protocol = 4;
	} else if (inet_pton(AF_INET6, host, &ip6_addr) == 1) {
		protocol = 6;
	} else if (host[0] == '[') {
		char ip[INET6_ADDRSTRLEN + 1];
		int len = strlen(host);

		if (len < INET6_ADDRSTRLEN + 2 && host[len - 1] == ']') {
			strncpy(ip, host + 1, len - 2);
			ip[len - 2] = '\0';

			if (inet_pton(AF_INET6, ip, &ip6_addr) == 1)
				protocol = 6;
		}
	}

	request_is_browser = check_browser_protocol(url);

	for (list = g_list_first(proxy_list); list; list = g_list_next(list)) {
		proxy = list->data;

		if (!proxy->domains) {
			if (proxy->browser_only && !alldomains_browseronly)
				alldomains_browseronly = proxy;
			else if (!proxy->browser_only && !alldomains_allprotos)
				alldomains_allprotos = proxy;
			continue;
		}

		for (l = g_list_first(proxy->domains); l; l = g_list_next(l)) {
			struct proxy_domain *data = l->data;

			if (data->proto != protocol)
				continue;

			switch (protocol) {
			case 4:
				if (compare_legacy_ip_in_net(&ip4_addr,
								data) == 0) {
					DBG("match proxy %p Legacy IP range %s",
					    proxy, data->domain);
					goto matches;
				}
				break;
			case 6:
				if (compare_ipv6_in_net(&ip6_addr,
							data) == 0) {
					DBG("match proxy %p IPv6 range %s",
					    proxy, data->domain);
					goto matches;
				}
				break;
			default:
				if (compare_host_in_domain(host, data) == 0) {
					DBG("match proxy %p DNS domain %s",
					    proxy, data->domain);
					goto matches;
				}
				break;
			}
		}
		/* No match */
		continue;

	matches:
		if (proxy->browser_only == request_is_browser) {
			goto found;
		} else if (proxy->browser_only) {
			/* A non-browser request will return DIRECT instead of
			 * falling back to alldomains_* if this exists.
			 */
			if (!domainmatch_browseronly)
				domainmatch_browseronly = proxy;
		} else {
			/* We might fall back to this, for a browser request */
			if (!domainmatch_allprotos)
				domainmatch_allprotos = proxy;
		}
	}

	if (request_is_browser) {
		/* We'll have bailed out immediately if we found a domain match
		 * with proxy->browser_only==TRUE. Fallbacks in order of prefe-
		 * rence.
		 */
		proxy = domainmatch_allprotos;
		if (!proxy)
			proxy = alldomains_browseronly;
		if (!proxy)
			proxy = alldomains_allprotos;
	} else {
		if (!domainmatch_browseronly)
			proxy = alldomains_allprotos;
		else
			proxy = NULL;
	}

 found:
	pthread_mutex_unlock(&proxy_mutex);

	if (!proxy)
		return NULL;

	switch (proxy->method) {
	case PACRUNNER_PROXY_METHOD_UNKNOWN:
	case PACRUNNER_PROXY_METHOD_DIRECT:
		break;
	case PACRUNNER_PROXY_METHOD_MANUAL:
		return __pacrunner_manual_execute(url, host, proxy->servers,
						  proxy->excludes);
	case PACRUNNER_PROXY_METHOD_AUTO:
		return __pacrunner_js_execute(proxy, url, host);
	}

	return NULL;
}

static gboolean proxy_config_timeout(gpointer user_data)
{
	DBG("");

	/* If ConnMan/NetworkManager/whatever hasn't given us a config within
	   a reasonable length of time, start responding 'DIRECT'. */
	if (proxy_updating == -1) {
		proxy_updating = 0;
		pthread_cond_broadcast(&proxy_cond);
	}
	return FALSE;
}

void __pacrunner_proxy_set_jsctx(struct pacrunner_proxy *proxy, void *jsctx)
{
	proxy->jsctx = jsctx;
}

void *__pacrunner_proxy_get_jsctx(struct pacrunner_proxy *proxy)
{
	return proxy->jsctx;
}

int __pacrunner_proxy_init(void)
{
	DBG("");

	pthread_mutex_init(&proxy_mutex, NULL);
	pthread_cond_init(&proxy_cond, NULL);

	timeout_source = g_timeout_add_seconds(5, proxy_config_timeout, NULL);

	return 0;
}

void __pacrunner_proxy_cleanup(void)
{
	GList *list;

	DBG("");

	for (list = g_list_first(proxy_list); list; list = g_list_next(list)) {
		struct pacrunner_proxy *proxy = list->data;

		DBG("proxy %p", proxy);

		if (proxy)
			pacrunner_proxy_unref(proxy);
	}

	pthread_mutex_destroy(&proxy_mutex);
	pthread_cond_destroy(&proxy_cond);

	g_list_free(proxy_list);
	proxy_list = NULL;

	if (timeout_source) {
		g_source_remove(timeout_source);
		timeout_source = 0;
	}
}
