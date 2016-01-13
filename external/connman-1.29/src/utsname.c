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

static GSList *driver_list = NULL;

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_utsname_driver *driver1 = a;
	const struct connman_utsname_driver *driver2 = b;

	return driver2->priority - driver1->priority;
}

/**
 * connman_utsname_driver_register:
 * @driver: utsname driver definition
 *
 * Register a new utsname driver
 *
 * Returns: %0 on success
 */
int connman_utsname_driver_register(struct connman_utsname_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_insert_sorted(driver_list, driver,
							compare_priority);

	return 0;
}

/**
 * connman_utsname_driver_unregister:
 * @driver: utsname driver definition
 *
 * Remove a previously registered utsname driver
 */
void connman_utsname_driver_unregister(struct connman_utsname_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_remove(driver_list, driver);
}

/**
 * connman_utsname_get_hostname:
 *
 * Returns current hostname
 */
const char *connman_utsname_get_hostname(void)
{
	GSList *list;

	DBG("");

	for (list = driver_list; list; list = list->next) {
		struct connman_utsname_driver *driver = list->data;
		const char *hostname;

		DBG("driver %p name %s", driver, driver->name);

		if (!driver->get_hostname)
			continue;

		hostname = driver->get_hostname();
		if (hostname)
			return hostname;
	}

	return NULL;
}

int __connman_utsname_set_hostname(const char *hostname)
{
	GSList *list;

	DBG("hostname %s", hostname);

	for (list = driver_list; list; list = list->next) {
		struct connman_utsname_driver *driver = list->data;

		DBG("driver %p name %s", driver, driver->name);

		if (!driver->set_hostname)
			continue;

		if (driver->set_hostname(hostname) == 0)
			break;
	}

	return 0;
}

int __connman_utsname_set_domainname(const char *domainname)
{
	GSList *list;

	DBG("domainname %s", domainname);

	for (list = driver_list; list; list = list->next) {
		struct connman_utsname_driver *driver = list->data;

		DBG("driver %p name %s", driver, driver->name);

		if (!driver->set_domainname)
			continue;

		if (driver->set_domainname(domainname) == 0)
			break;
	}

	return 0;
}
