/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
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

#ifndef __CONNMAN_TECHNOLOGY_H
#define __CONNMAN_TECHNOLOGY_H

#include <connman/service.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:technology
 * @title: technology premitives
 * @short_description: Functions for handling technology details
 */

struct connman_technology;

void connman_technology_tethering_notify(struct connman_technology *technology,
							bool enabled);
int connman_technology_set_regdom(const char *alpha2);
void connman_technology_regdom_notify(struct connman_technology *technology,
							const char *alpha2);

bool connman_technology_get_wifi_tethering(const char **ssid,
							const char **psk);
bool connman_technology_is_tethering_allowed(enum connman_service_type type);

struct connman_technology_driver {
	const char *name;
	enum connman_service_type type;
	int priority;
	int (*probe) (struct connman_technology *technology);
	void (*remove) (struct connman_technology *technology);
	void (*add_interface) (struct connman_technology *technology,
						int index, const char *name,
							const char *ident);
	void (*remove_interface) (struct connman_technology *technology,
								int index);
	int (*set_tethering) (struct connman_technology *technology,
				const char *identifier, const char *passphrase,
				const char *bridge, bool enabled);
	int (*set_regdom) (struct connman_technology *technology,
						const char *alpha2);
};

int connman_technology_driver_register(struct connman_technology_driver *driver);
void connman_technology_driver_unregister(struct connman_technology_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_TECHNOLOGY_H */
