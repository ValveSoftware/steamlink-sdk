/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2013  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef __CONNMANCTL_COMMANDS_H
#define __CONNMANCTL_COMMANDS_H

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char *(*__connmanctl_lookup_cb)(const char *text, int state);

int __connmanctl_commands(DBusConnection *connection, char *argv[], int argc);
char *__connmanctl_lookup_command(const char *text, int state);
void __connmanctl_monitor_completions(DBusConnection *dbus_conn);
__connmanctl_lookup_cb __connmanctl_get_lookup_func(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMANCTL_COMMANDS_H */
