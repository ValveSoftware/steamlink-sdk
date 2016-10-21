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

static GSList *driver_list = NULL;

int pacrunner_download_driver_register(struct pacrunner_download_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_append(driver_list, driver);

	return 0;
}

void pacrunner_download_driver_unregister(struct pacrunner_download_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_remove(driver_list, driver);
}

int __pacrunner_download_update(const char *interface, const char *url,
			pacrunner_download_cb callback, void *user_data)
{
	GSList *list;

	DBG("url %s", url);

	for (list = driver_list; list; list = list->next) {
		struct pacrunner_download_driver *driver = list->data;

		if (driver->download)
			return driver->download(interface, url,
							callback, user_data);
	}

	return -ENXIO;
}

int __pacrunner_download_init(void)
{
	DBG("");

	return 0;
}

void __pacrunner_download_cleanup(void)
{
	DBG("");
}
