/*
 *
 *  ConnMan VPN daemon
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

#ifndef __VPN_DBUS_H
#define __VPN_DBUS_H

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VPN_SERVICE			"net.connman.vpn"
#define VPN_PATH			"/net/connman/vpn"

#define VPN_ERROR_INTERFACE		VPN_SERVICE ".Error"

#define VPN_MANAGER_INTERFACE		VPN_SERVICE ".Manager"
#define VPN_MANAGER_PATH		"/"

#define VPN_CONNECTION_INTERFACE	VPN_SERVICE ".Connection"
#define VPN_AGENT_INTERFACE		VPN_SERVICE ".Agent"
#define VPN_TASK_INTERFACE		VPN_SERVICE ".Task"

#define VPN_PRIVILEGE_MODIFY		1
#define VPN_PRIVILEGE_SECRET		2

#define CONNECTION_ADDED		"ConnectionAdded"
#define CONNECTION_REMOVED		"ConnectionRemoved"
#define PROPERTY_CHANGED		"PropertyChanged"
#define GET_CONNECTIONS			"GetConnections"
#define VPN_CONNECT			"Connect"
#define VPN_DISCONNECT			"Disconnect"
#define VPN_REMOVE			"Remove"

#ifdef __cplusplus
}
#endif

#endif /* __VPN_DBUS_H */
