/*
 *
 *  ConnMan VPN daemon
 *
 *  Copyright (C) 2010,2014  BMW Car IT GmbH.
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

#ifndef __CONNMAN_VPND_VPN_H
#define __CONNMAN_VPND_VPN_H

#include "../vpn-provider.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VPN_FLAG_NO_TUN	1

enum vpn_state {
	VPN_STATE_UNKNOWN       = 0,
	VPN_STATE_IDLE          = 1,
	VPN_STATE_CONNECT       = 2,
	VPN_STATE_READY         = 3,
	VPN_STATE_DISCONNECT    = 4,
	VPN_STATE_FAILURE       = 5,
	VPN_STATE_AUTH_FAILURE  = 6,
};

struct vpn_driver {
	int flags;
	int (*notify) (DBusMessage *msg, struct vpn_provider *provider);
	int (*connect) (struct vpn_provider *provider,
			struct connman_task *task, const char *if_name,
			vpn_provider_connect_cb_t cb, const char *dbus_sender,
			void *user_data);
	void (*disconnect) (struct vpn_provider *provider);
	int (*error_code) (struct vpn_provider *provider, int exit_code);
	int (*save) (struct vpn_provider *provider, GKeyFile *keyfile);
};

int vpn_register(const char *name, struct vpn_driver *driver,
			const char *program);
void vpn_unregister(const char *provider_name);
void vpn_died(struct connman_task *task, int exit_code, void *user_data);
int vpn_set_ifname(struct vpn_provider *provider, const char *ifname);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_VPND_VPN_H */
