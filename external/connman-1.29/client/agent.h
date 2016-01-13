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

#ifndef __CONNMANCTL_AGENT_H
#define __CONNMANCTL_AGENT_H

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#endif

int __connmanctl_agent_register(DBusConnection *connection);
int __connmanctl_agent_unregister(DBusConnection *connection);

int __connmanctl_vpn_agent_register(DBusConnection *connection);
int __connmanctl_vpn_agent_unregister(DBusConnection *connection);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMANCTL_AGENT_H */
