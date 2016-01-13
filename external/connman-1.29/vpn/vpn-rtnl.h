/*
 *
 *  ConnMan VPN daemon
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
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

#ifndef __VPN_RTNL_H
#define __VPN_RTNL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:rtnl
 * @title: RTNL premitives
 * @short_description: Functions for registering RTNL modules
 */

typedef void (* vpn_rtnl_link_cb_t) (unsigned flags, unsigned change,
							void *user_data);

unsigned int vpn_rtnl_add_newlink_watch(int index,
			vpn_rtnl_link_cb_t callback, void *user_data);

void vpn_rtnl_remove_watch(unsigned int id);

#define VPN_RTNL_PRIORITY_LOW      -100
#define VPN_RTNL_PRIORITY_DEFAULT     0
#define VPN_RTNL_PRIORITY_HIGH      100

struct vpn_rtnl {
	const char *name;
	int priority;
	void (*newlink) (unsigned short type, int index,
					unsigned flags, unsigned change);
	void (*dellink) (unsigned short type, int index,
					unsigned flags, unsigned change);
	void (*newgateway) (int index, const char *gateway);
	void (*delgateway) (int index, const char *gateway);
};

int vpn_rtnl_register(struct vpn_rtnl *rtnl);
void vpn_rtnl_unregister(struct vpn_rtnl *rtnl);

#ifdef __cplusplus
}
#endif

#endif /* __VPN_RTNL_H */
