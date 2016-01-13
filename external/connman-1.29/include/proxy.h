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

#ifndef __CONNMAN_PROXY_H
#define __CONNMAN_PROXY_H

#include <connman/service.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:proxy
 * @title: Proxy premitives
 * @short_description: Functions for handling proxy configuration
 */

typedef void (*connman_proxy_lookup_cb) (const char *proxy, void *user_data);

unsigned int connman_proxy_lookup(const char *interface, const char *url,
				struct connman_service *service,
				connman_proxy_lookup_cb cb, void *user_data);
void connman_proxy_lookup_cancel(unsigned int token);

void connman_proxy_driver_lookup_notify(struct connman_service *service,
					const char *url, const char *result);

#define CONNMAN_PROXY_PRIORITY_LOW      -100
#define CONNMAN_PROXY_PRIORITY_DEFAULT     0
#define CONNMAN_PROXY_PRIORITY_HIGH      100

struct connman_proxy_driver {
	const char *name;
	int priority;
	int (*request_lookup) (struct connman_service *service,
							const char *url);
	void (*cancel_lookup) (struct connman_service *service,
							const char *url);
};

int connman_proxy_driver_register(struct connman_proxy_driver *driver);
void connman_proxy_driver_unregister(struct connman_proxy_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_PROXY_H */
