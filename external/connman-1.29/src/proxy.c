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

#include <glib.h>

#include "connman.h"

static unsigned int next_lookup_token = 1;

static GSList *driver_list = NULL;
static GSList *lookup_list = NULL;

struct proxy_lookup {
	unsigned int token;
	connman_proxy_lookup_cb cb;
	void *user_data;
	struct connman_service *service;
	char *url;
	guint watch;
	struct connman_proxy_driver *proxy;
};

static void remove_lookup(struct proxy_lookup *lookup)
{
	if (lookup->watch > 0)
		g_source_remove(lookup->watch);

	lookup_list = g_slist_remove(lookup_list, lookup);

	connman_service_unref(lookup->service);
	g_free(lookup->url);
	g_free(lookup);
}

static void remove_lookups(GSList *lookups)
{
	GSList *list;

	for (list = lookups; list; list = list->next) {
		struct proxy_lookup *lookup = list->data;

		remove_lookup(lookup);
	}

	g_slist_free(lookups);
}

static gboolean lookup_callback(gpointer user_data)
{
	struct proxy_lookup *lookup = user_data;
	GSList *list;

	if (!lookup)
		return FALSE;

	lookup->watch = 0;

	for (list = driver_list; list; list = list->next) {
		struct connman_proxy_driver *proxy = list->data;

		if (!proxy->request_lookup)
			continue;

		lookup->proxy = proxy;
		break;
	}

	if (!lookup->proxy ||
		lookup->proxy->request_lookup(lookup->service,
						lookup->url) < 0) {

		if (lookup->cb)
			lookup->cb(NULL, lookup->user_data);

		remove_lookup(lookup);
	}

	return FALSE;
}

unsigned int connman_proxy_lookup(const char *interface, const char *url,
					struct connman_service *service,
					connman_proxy_lookup_cb cb,
					void *user_data)
{
	struct proxy_lookup *lookup;

	DBG("interface %s url %s", interface, url);

	if (!interface)
		return 0;

	lookup = g_try_new0(struct proxy_lookup, 1);
	if (!lookup)
		return 0;

	lookup->token = next_lookup_token++;

	lookup->cb = cb;
	lookup->user_data = user_data;
	lookup->url = g_strdup(url);
	lookup->service = connman_service_ref(service);

	lookup->watch = g_timeout_add_seconds(0, lookup_callback, lookup);
	if (lookup->watch == 0) {
		g_free(lookup->url);
		g_free(lookup);
		return 0;
	}

	DBG("token %u", lookup->token);
	lookup_list = g_slist_prepend(lookup_list, lookup);

	return lookup->token;
}

void connman_proxy_lookup_cancel(unsigned int token)
{
	GSList *list;
	struct proxy_lookup *lookup = NULL;

	DBG("token %u", token);

	for (list = lookup_list; list; list = list->next) {
		lookup = list->data;

		if (lookup->token == token)
			break;

		lookup = NULL;
	}

	if (lookup) {
		if (lookup->proxy &&
					lookup->proxy->cancel_lookup)
			lookup->proxy->cancel_lookup(lookup->service,
								lookup->url);

		remove_lookup(lookup);
	}
}

void connman_proxy_driver_lookup_notify(struct connman_service *service,
					const char *url, const char *result)
{
	GSList *list, *matches = NULL;

	DBG("service %p url %s result %s", service, url, result);

	for (list = lookup_list; list; list = list->next) {
		struct proxy_lookup *lookup = list->data;

		if (service != lookup->service)
			continue;

		if (g_strcmp0(lookup->url, url) == 0) {
			if (lookup->cb)
				lookup->cb(result, lookup->user_data);

			matches = g_slist_prepend(matches, lookup);
		}
	}

	if (matches)
		remove_lookups(matches);
}

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_proxy_driver *driver1 = a;
	const struct connman_proxy_driver *driver2 = b;

	return driver2->priority - driver1->priority;
}

/**
 * connman_proxy_driver_register:
 * @driver: Proxy driver definition
 *
 * Register a new proxy driver
 *
 * Returns: %0 on success
 */
int connman_proxy_driver_register(struct connman_proxy_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_insert_sorted(driver_list, driver,
							compare_priority);

	return 0;
}

/**
 * connman_proxy_driver_unregister:
 * @driver: Proxy driver definition
 *
 * Remove a previously registered proxy driver
 */
void connman_proxy_driver_unregister(struct connman_proxy_driver *driver)
{
	GSList *list;

	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_remove(driver_list, driver);

	for (list = lookup_list; list; list = list->next) {
		struct proxy_lookup *lookup = list->data;

		if (lookup->proxy == driver)
			lookup->proxy = NULL;
	}

}

int __connman_proxy_init(void)
{
	DBG("");

	return 0;
}

void __connman_proxy_cleanup(void)
{
	DBG("");

	remove_lookups(lookup_list);
}
