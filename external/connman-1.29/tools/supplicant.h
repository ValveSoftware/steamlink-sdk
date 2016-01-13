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

#define SUPPLICANT_EAP_METHOD_MD5	(1 << 0)
#define SUPPLICANT_EAP_METHOD_TLS	(1 << 1)
#define SUPPLICANT_EAP_METHOD_MSCHAPV2	(1 << 2)
#define SUPPLICANT_EAP_METHOD_PEAP	(1 << 3)
#define SUPPLICANT_EAP_METHOD_TTLS	(1 << 4)
#define SUPPLICANT_EAP_METHOD_GTC	(1 << 5)
#define SUPPLICANT_EAP_METHOD_OTP	(1 << 6)
#define SUPPLICANT_EAP_METHOD_LEAP	(1 << 7)
#define SUPPLICANT_EAP_METHOD_WSC	(1 << 8)

#define SUPPLICANT_CAPABILITY_KEYMGMT_NONE	(1 << 0)
#define SUPPLICANT_CAPABILITY_KEYMGMT_IEEE8021X	(1 << 1)
#define SUPPLICANT_CAPABILITY_KEYMGMT_WPA_NONE	(1 << 2)
#define SUPPLICANT_CAPABILITY_KEYMGMT_WPA_PSK	(1 << 3)
#define SUPPLICANT_CAPABILITY_KEYMGMT_WPA_EAP	(1 << 4)
#define SUPPLICANT_CAPABILITY_KEYMGMT_WPS	(1 << 5)

#define SUPPLICANT_CAPABILITY_AUTHALG_OPEN	(1 << 0)
#define SUPPLICANT_CAPABILITY_AUTHALG_SHARED	(1 << 1)
#define SUPPLICANT_CAPABILITY_AUTHALG_LEAP	(1 << 2)

#define SUPPLICANT_CAPABILITY_PROTO_WPA		(1 << 0)
#define SUPPLICANT_CAPABILITY_PROTO_RSN		(1 << 1)

#define SUPPLICANT_CAPABILITY_GROUP_WEP40	(1 << 0)
#define SUPPLICANT_CAPABILITY_GROUP_WEP104	(1 << 1)
#define SUPPLICANT_CAPABILITY_GROUP_TKIP	(1 << 2)
#define SUPPLICANT_CAPABILITY_GROUP_CCMP	(1 << 3)

#define SUPPLICANT_CAPABILITY_PAIRWISE_NONE	(1 << 0)
#define SUPPLICANT_CAPABILITY_PAIRWISE_TKIP	(1 << 1)
#define SUPPLICANT_CAPABILITY_PAIRWISE_CCMP	(1 << 2)

#define SUPPLICANT_CAPABILITY_SCAN_ACTIVE	(1 << 0)
#define SUPPLICANT_CAPABILITY_SCAN_PASSIVE	(1 << 1)
#define SUPPLICANT_CAPABILITY_SCAN_SSID		(1 << 2)

#define SUPPLICANT_CAPABILITY_MODE_INFRA	(1 << 0)
#define SUPPLICANT_CAPABILITY_MODE_IBSS		(1 << 1)
#define SUPPLICANT_CAPABILITY_MODE_AP		(1 << 2)

enum supplicant_mode {
	SUPPLICANT_MODE_UNKNOWN,
	SUPPLICANT_MODE_INFRA,
	SUPPLICANT_MODE_IBSS,
};

enum supplicant_security {
	SUPPLICANT_SECURITY_UNKNOWN,
	SUPPLICANT_SECURITY_NONE,
	SUPPLICANT_SECURITY_WEP,
	SUPPLICANT_SECURITY_PSK,
	SUPPLICANT_SECURITY_IEEE8021X,
};

enum supplicant_state {
	SUPPLICANT_STATE_UNKNOWN,
	SUPPLICANT_STATE_DISCONNECTED,
	SUPPLICANT_STATE_INACTIVE,
	SUPPLICANT_STATE_SCANNING,
	SUPPLICANT_STATE_AUTHENTICATING,
	SUPPLICANT_STATE_ASSOCIATING,
	SUPPLICANT_STATE_ASSOCIATED,
	SUPPLICANT_STATE_4WAY_HANDSHAKE,
	SUPPLICANT_STATE_GROUP_HANDSHAKE,
	SUPPLICANT_STATE_COMPLETED,
};

struct supplicant_interface;

typedef void (* supplicant_interface_create_callback) (int result,
				struct supplicant_interface *interface,
							void *user_data);
typedef void (* supplicant_interface_remove_callback) (int result,
							void *user_data);
typedef void (* supplicant_interface_scan_callback) (int result,
							void *user_data);
typedef void (* supplicant_interface_disconnect_callback) (int result,
							void *user_data);

int supplicant_interface_create(const char *ifname, const char *driver,
			supplicant_interface_create_callback callback,
							void *user_data);
int supplicant_interface_remove(struct supplicant_interface *interface,
			supplicant_interface_remove_callback callback,
							void *user_data);
int supplicant_interface_scan(struct supplicant_interface *interface,
			supplicant_interface_scan_callback callback,
							void *user_data);
int supplicant_interface_disconnect(struct supplicant_interface *interface,
			supplicant_interface_disconnect_callback callback,
							void *user_data);

const char *supplicant_interface_get_ifname(struct supplicant_interface *interface);
const char *supplicant_interface_get_driver(struct supplicant_interface *interface);

struct supplicant_network;

struct supplicant_interface *supplicant_network_get_interface(struct supplicant_network *network);
const char *supplicant_network_get_name(struct supplicant_network *network);
const char *supplicant_network_get_identifier(struct supplicant_network *network);
enum supplicant_mode supplicant_network_get_mode(struct supplicant_network *network);

struct supplicant_callbacks {
	void (*system_ready) (void);
	void (*system_killed) (void);
	void (*interface_added) (struct supplicant_interface *interface);
	void (*interface_removed) (struct supplicant_interface *interface);
	void (*scan_started) (struct supplicant_interface *interface);
	void (*scan_finished) (struct supplicant_interface *interface);
	void (*network_added) (struct supplicant_network *network);
	void (*network_removed) (struct supplicant_network *network);
};

int supplicant_register(const struct supplicant_callbacks *callbacks);
void supplicant_unregister(const struct supplicant_callbacks *callbacks);

void supplicant_set_debug_level(unsigned int level);
