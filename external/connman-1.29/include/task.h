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

#ifndef __CONNMAN_TASK_H
#define __CONNMAN_TASK_H

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:task
 * @title: Task premitives
 * @short_description: Functions for handling tasks
 */

struct connman_task;

typedef void (* connman_task_exit_t) (struct connman_task *task,
						int exit_code, void *user_data);

typedef DBusMessage * (* connman_task_notify_t) (struct connman_task *task,
				DBusMessage *message, void *user_data);

struct connman_task *connman_task_create(const char *program);
void connman_task_destroy(struct connman_task *task);

const char *connman_task_get_path(struct connman_task *task);

int connman_task_add_argument(struct connman_task *task,
				const char *name, const char *format, ...);
int connman_task_add_variable(struct connman_task *task,
				const char *key, const char *format, ...);

int connman_task_set_notify(struct connman_task *task, const char *member,
			connman_task_notify_t function, void *user_data);

int connman_task_run(struct connman_task *task,
			connman_task_exit_t function, void *user_data,
			int *stdin_fd, int *stdout_fd, int *stderr_fd);
int connman_task_stop(struct connman_task *task);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_TASK_H */
