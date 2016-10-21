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

#include <errno.h>

#include "pacrunner.h"
#include "js.h"

static GSList *js_driver_list = NULL;

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct pacrunner_js_driver *driver1 = a;
	const struct pacrunner_js_driver *driver2 = b;

	return driver2->priority - driver1->priority;
}

int pacrunner_js_driver_register(struct pacrunner_js_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	js_driver_list = g_slist_insert_sorted(js_driver_list, driver,
							compare_priority);

	return 0;
}

void pacrunner_js_driver_unregister(struct pacrunner_js_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	js_driver_list = g_slist_remove(js_driver_list, driver);
}

int __pacrunner_js_set_proxy(struct pacrunner_proxy *proxy)
{
	GSList *list;

	for (list = js_driver_list; list; list = list->next) {
		struct pacrunner_js_driver *driver = list->data;

		if (driver->set_proxy)
			return driver->set_proxy(proxy);
	}

	return -ENXIO;
}

int __pacrunner_js_clear_proxy(struct pacrunner_proxy *proxy)
{
	GSList *list;

	for (list = js_driver_list; list; list = list->next) {
		struct pacrunner_js_driver *driver = list->data;

		if (driver->clear_proxy)
			return driver->clear_proxy(proxy);
		else
			return 0;
	}

	return -ENXIO;
}

char *__pacrunner_js_execute(struct pacrunner_proxy *proxy, const char *url,
			     const char *host)
{
	GSList *list;

	for (list = js_driver_list; list; list = list->next) {
		struct pacrunner_js_driver *driver = list->data;

		if (driver->execute)
			return driver->execute(proxy, url, host);
	}

	return NULL;
}

int __pacrunner_js_init(void)
{
	DBG("");

	return 0;
}

void __pacrunner_js_cleanup(void)
{
	DBG("");
}
