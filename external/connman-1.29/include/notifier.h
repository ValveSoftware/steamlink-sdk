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

#ifndef __CONNMAN_NOTIFIER_H
#define __CONNMAN_NOTIFIER_H

#include <connman/service.h>
#include <connman/ipconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:notifier
 * @title: Notifier premitives
 * @short_description: Functions for registering notifier modules
 */

#define CONNMAN_NOTIFIER_PRIORITY_LOW      -100
#define CONNMAN_NOTIFIER_PRIORITY_DEFAULT     0
#define CONNMAN_NOTIFIER_PRIORITY_HIGH      100

struct connman_notifier {
	const char *name;
	int priority;
	void (*default_changed) (struct connman_service *service);
	void (*service_add) (struct connman_service *service,
				const char *name);
	void (*service_remove) (struct connman_service *service);
	void (*service_enabled) (enum connman_service_type type,
						bool enabled);
	void (*offline_mode) (bool enabled);
	void (*proxy_changed) (struct connman_service *service);
	void (*service_state_changed) (struct connman_service *service,
					enum connman_service_state state);
	void (*ipconfig_changed) (struct connman_service *service,
					struct connman_ipconfig *ipconfig);
	void (*idle_state) (bool idle);
};

int connman_notifier_register(struct connman_notifier *notifier);
void connman_notifier_unregister(struct connman_notifier *notifier);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_NOTIFIER_H */
