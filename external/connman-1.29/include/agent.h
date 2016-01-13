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

#ifndef __CONNMAN_AGENT_H
#define __CONNMAN_AGENT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:agent
 * @title: agent primitives
 * @short_description: Functions for handling generic agent details
 */

struct connman_agent;

struct connman_agent_driver {
	const char *name;
	const char *interface;
	int priority;
	int (*probe) (struct connman_agent *agent);
	void (*remove) (struct connman_agent *agent);
	void * (*context_ref) (void *user_context);
	void (*context_unref) (void *user_context);
};

int connman_agent_driver_register(struct connman_agent_driver *driver);
void connman_agent_driver_unregister(struct connman_agent_driver *driver);

typedef void (* report_error_cb_t) (void *user_context,
				bool retry, void *user_data);

int connman_agent_report_error_full(void *user_context, const char *path,
				const char *method, const char *error,
				report_error_cb_t callback,
				const char *dbus_sender, void *user_data);
int connman_agent_report_error(void *user_context, const char *path,
				const char *error,
				report_error_cb_t callback,
				const char *dbus_sender, void *user_data);

int connman_agent_register(const char *sender, const char *path);
int connman_agent_unregister(const char *sender, const char *path);
void connman_agent_cancel(void *user_context);

typedef void (*agent_queue_cb)(DBusMessage *reply, void *user_data);

int connman_agent_queue_message(void *user_context,
				DBusMessage *msg, int timeout,
				agent_queue_cb callback, void *user_data,
				void *agent_data);

void *connman_agent_get_info(const char *dbus_sender, const char **sender,
							const char **path);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_TECHNOLOGY_H */
