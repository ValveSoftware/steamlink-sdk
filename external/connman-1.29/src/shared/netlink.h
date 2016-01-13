/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2011-2012  Intel Corporation. All rights reserved.
 *  Copyright (C) 2013  BMW Car IT GbmH.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdint.h>
#include <stdbool.h>

typedef void (*netlink_debug_func_t) (const char *str, void *user_data);
typedef void (*netlink_command_func_t) (unsigned int error,
						uint16_t type, const void *data,
						uint32_t len, void *user_data);
typedef void (*netlink_notify_func_t) (uint16_t type, const void *data,
						uint32_t len, void *user_data);
typedef void (*netlink_destroy_func_t) (void *user_data);

struct netlink_info;

struct netlink_info *netlink_new(int protocol);
void netlink_destroy(struct netlink_info *netlink);

unsigned int netlink_send(struct netlink_info *netlink,
			uint16_t type, uint16_t flags, const void *data,
			uint32_t len, netlink_command_func_t function,
			void *user_data, netlink_destroy_func_t destroy);
bool netlink_cancel(struct netlink_info *netlink, unsigned int id);

unsigned int netlink_register(struct netlink_info *netlink,
			uint32_t group, netlink_notify_func_t function,
			void *user_data, netlink_destroy_func_t destroy);
bool netlink_unregister(struct netlink_info *netlink, unsigned int id);

bool netlink_set_debug(struct netlink_info *netlink,
			netlink_debug_func_t function,
			void *user_data, netlink_destroy_func_t destroy);
