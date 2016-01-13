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
#include <stdlib.h>
#include <string.h>

#include <gweb/gresolv.h>

#include "connman.h"

struct connman_wpad {
	struct connman_service *service;
	GResolv *resolv;
	char *hostname;
	char **addrlist;
};

static GHashTable *wpad_list = NULL;

static void resolv_debug(const char *str, void *data)
{
	connman_info("%s: %s\n", (const char *) data, str);
}

static void free_wpad(gpointer data)
{
        struct connman_wpad *wpad = data;

	g_resolv_unref(wpad->resolv);

	g_strfreev(wpad->addrlist);
	g_free(wpad->hostname);
	g_free(wpad);
}

static void download_pac(struct connman_wpad *wpad, const char *target)
{
}

static void wpad_result(GResolvResultStatus status,
					char **results, gpointer user_data)
{
	struct connman_wpad *wpad = user_data;
	const char *ptr;
	char *hostname;

	DBG("status %d", status);

	if (status == G_RESOLV_RESULT_STATUS_SUCCESS) {
		char *url;

		if (!results || g_strv_length(results) == 0)
			goto failed;

		url = g_strdup_printf("http://%s/wpad.dat", wpad->hostname);

		__connman_service_set_proxy_autoconfig(wpad->service, url);

		wpad->addrlist = g_strdupv(results);
		if (wpad->addrlist)
			download_pac(wpad, "wpad.dat");

		g_free(url);

		__connman_wispr_start(wpad->service,
					CONNMAN_IPCONFIG_TYPE_IPV4);

		return;
	}

	hostname = wpad->hostname;

	if (strlen(hostname) < 6)
		goto failed;

	ptr = strchr(hostname + 5, '.');
	if (!ptr || strlen(ptr) < 2)
		goto failed;

	if (!strchr(ptr + 1, '.'))
		goto failed;

	wpad->hostname = g_strdup_printf("wpad.%s", ptr + 1);
	g_free(hostname);

	DBG("hostname %s", wpad->hostname);

	g_resolv_lookup_hostname(wpad->resolv, wpad->hostname,
							wpad_result, wpad);

	return;

failed:
	connman_service_set_proxy_method(wpad->service,
				CONNMAN_SERVICE_PROXY_METHOD_DIRECT);

	__connman_wispr_start(wpad->service,
					CONNMAN_IPCONFIG_TYPE_IPV4);
}

int __connman_wpad_start(struct connman_service *service)
{
	struct connman_wpad *wpad;
	const char *domainname;
	char **nameservers;
	int index;
	int i;

	DBG("service %p", service);

	if (!wpad_list)
		return -EINVAL;

	index = __connman_service_get_index(service);
	if (index < 0)
		return -EINVAL;

	domainname = connman_service_get_domainname(service);
	if (!domainname)
		return -EINVAL;

	nameservers = connman_service_get_nameservers(service);
	if (!nameservers)
		return -EINVAL;

	wpad = g_try_new0(struct connman_wpad, 1);
	if (!wpad) {
		g_strfreev(nameservers);
		return -ENOMEM;
	}

	wpad->service = service;
	wpad->resolv = g_resolv_new(index);
	if (!wpad->resolv) {
		g_strfreev(nameservers);
		g_free(wpad);
		return -ENOMEM;
	}

	if (getenv("CONNMAN_RESOLV_DEBUG"))
		g_resolv_set_debug(wpad->resolv, resolv_debug, "RESOLV");

	for (i = 0; nameservers[i]; i++)
		g_resolv_add_nameserver(wpad->resolv, nameservers[i], 53, 0);

	g_strfreev(nameservers);

	wpad->hostname = g_strdup_printf("wpad.%s", domainname);

	DBG("hostname %s", wpad->hostname);

	g_resolv_lookup_hostname(wpad->resolv, wpad->hostname,
							wpad_result, wpad);

	connman_service_ref(service);
	g_hash_table_replace(wpad_list, GINT_TO_POINTER(index), wpad);

	return 0;
}

void __connman_wpad_stop(struct connman_service *service)
{
	int index;

	DBG("service %p", service);

	if (!wpad_list)
		return;

	index = __connman_service_get_index(service);
	if (index < 0)
		return;

	if (g_hash_table_remove(wpad_list, GINT_TO_POINTER(index)))
		connman_service_unref(service);
}

int __connman_wpad_init(void)
{
	DBG("");

	wpad_list = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, free_wpad);

	return 0;
}

void __connman_wpad_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(wpad_list);
	wpad_list = NULL;
}
