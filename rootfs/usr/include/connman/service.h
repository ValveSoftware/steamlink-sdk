/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2014  Intel Corporation. All rights reserved.
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

#ifndef __CONNMAN_SERVICE_H
#define __CONNMAN_SERVICE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:service
 * @title: Service premitives
 * @short_description: Functions for handling services
 */

enum connman_service_type {
	CONNMAN_SERVICE_TYPE_UNKNOWN   = 0,
	CONNMAN_SERVICE_TYPE_SYSTEM    = 1,
	CONNMAN_SERVICE_TYPE_ETHERNET  = 2,
	CONNMAN_SERVICE_TYPE_WIFI      = 3,
	CONNMAN_SERVICE_TYPE_BLUETOOTH = 4,
	CONNMAN_SERVICE_TYPE_CELLULAR  = 5,
	CONNMAN_SERVICE_TYPE_GPS       = 6,
	CONNMAN_SERVICE_TYPE_VPN       = 7,
	CONNMAN_SERVICE_TYPE_GADGET    = 8,
	CONNMAN_SERVICE_TYPE_P2P       = 9,
};
#define	MAX_CONNMAN_SERVICE_TYPES        10


enum connman_service_security {
	CONNMAN_SERVICE_SECURITY_UNKNOWN = 0,
	CONNMAN_SERVICE_SECURITY_NONE    = 1,
	CONNMAN_SERVICE_SECURITY_WEP     = 2,
	CONNMAN_SERVICE_SECURITY_PSK     = 3,
	CONNMAN_SERVICE_SECURITY_8021X   = 4,
	CONNMAN_SERVICE_SECURITY_WPA     = 8,
	CONNMAN_SERVICE_SECURITY_RSN     = 9,
};

enum connman_service_state {
	CONNMAN_SERVICE_STATE_UNKNOWN       = 0,
	CONNMAN_SERVICE_STATE_IDLE          = 1,
	CONNMAN_SERVICE_STATE_ASSOCIATION   = 2,
	CONNMAN_SERVICE_STATE_CONFIGURATION = 3,
	CONNMAN_SERVICE_STATE_READY         = 4,
	CONNMAN_SERVICE_STATE_ONLINE        = 5,
	CONNMAN_SERVICE_STATE_DISCONNECT    = 6,
	CONNMAN_SERVICE_STATE_FAILURE       = 7,
};

enum connman_service_error {
	CONNMAN_SERVICE_ERROR_UNKNOWN        = 0,
	CONNMAN_SERVICE_ERROR_OUT_OF_RANGE   = 1,
	CONNMAN_SERVICE_ERROR_PIN_MISSING    = 2,
	CONNMAN_SERVICE_ERROR_DHCP_FAILED    = 3,
	CONNMAN_SERVICE_ERROR_CONNECT_FAILED = 4,
	CONNMAN_SERVICE_ERROR_LOGIN_FAILED  = 5,
	CONNMAN_SERVICE_ERROR_AUTH_FAILED    = 6,
	CONNMAN_SERVICE_ERROR_INVALID_KEY    = 7,
};

enum connman_service_proxy_method {
	CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN     = 0,
	CONNMAN_SERVICE_PROXY_METHOD_DIRECT      = 1,
	CONNMAN_SERVICE_PROXY_METHOD_MANUAL      = 2,
	CONNMAN_SERVICE_PROXY_METHOD_AUTO        = 3,
};

enum connman_service_connect_reason {
	CONNMAN_SERVICE_CONNECT_REASON_NONE	= 0,
	CONNMAN_SERVICE_CONNECT_REASON_AUTO	= 1,
	CONNMAN_SERVICE_CONNECT_REASON_USER	= 2,
	CONNMAN_SERVICE_CONNECT_REASON_SESSION	= 3,
};

struct connman_service;
struct connman_network;

struct connman_service *connman_service_create(void);

#define connman_service_ref(service) \
	connman_service_ref_debug(service, __FILE__, __LINE__, __func__)

#define connman_service_unref(service) \
	connman_service_unref_debug(service, __FILE__, __LINE__, __func__)

struct connman_service *
connman_service_ref_debug(struct connman_service *service,
			const char *file, int line, const char *caller);
void connman_service_unref_debug(struct connman_service *service,
			const char *file, int line, const char *caller);

enum connman_service_type connman_service_get_type(struct connman_service *service);
char *connman_service_get_interface(struct connman_service *service);

const char *connman_service_get_domainname(struct connman_service *service);
char **connman_service_get_nameservers(struct connman_service *service);
char **connman_service_get_timeservers_config(struct connman_service *service);
char **connman_service_get_timeservers(struct connman_service *service);
void connman_service_set_proxy_method(struct connman_service *service, enum connman_service_proxy_method method);
enum connman_service_proxy_method connman_service_get_proxy_method(struct connman_service *service);
char **connman_service_get_proxy_servers(struct connman_service *service);
char **connman_service_get_proxy_excludes(struct connman_service *service);
const char *connman_service_get_proxy_url(struct connman_service *service);
const char *connman_service_get_proxy_autoconfig(struct connman_service *service);
bool connman_service_get_favorite(struct connman_service *service);
bool connman_service_get_autoconnect(struct connman_service *service);

struct connman_service *connman_service_lookup_from_network(struct connman_network *network);

void connman_service_create_ip4config(struct connman_service *service,
								int index);
void connman_service_create_ip6config(struct connman_service *service,
								int index);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_SERVICE_H */
