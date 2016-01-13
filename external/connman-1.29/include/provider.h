/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
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

#ifndef __CONNMAN_PROVIDER_H
#define __CONNMAN_PROVIDER_H

#include <stdbool.h>

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:provider
 * @title: Provider premitives
 * @short_description: Functions for handling providers
 */

enum connman_provider_type {
	CONNMAN_PROVIDER_TYPE_UNKNOWN = 0,
	CONNMAN_PROVIDER_TYPE_VPN     = 1,
};

enum connman_provider_state {
	CONNMAN_PROVIDER_STATE_UNKNOWN       = 0,
	CONNMAN_PROVIDER_STATE_IDLE          = 1,
	CONNMAN_PROVIDER_STATE_CONNECT       = 2,
	CONNMAN_PROVIDER_STATE_READY         = 3,
	CONNMAN_PROVIDER_STATE_DISCONNECT    = 4,
	CONNMAN_PROVIDER_STATE_FAILURE       = 5,
};

enum connman_provider_error {
	CONNMAN_PROVIDER_ERROR_UNKNOWN		= 0,
	CONNMAN_PROVIDER_ERROR_CONNECT_FAILED	= 1,
	CONNMAN_PROVIDER_ERROR_LOGIN_FAILED	= 2,
	CONNMAN_PROVIDER_ERROR_AUTH_FAILED	= 3,
};

enum connman_provider_route_type {
	CONNMAN_PROVIDER_ROUTE_UNKNOWN = 0,
	CONNMAN_PROVIDER_ROUTE_ALL = 0,
	CONNMAN_PROVIDER_ROUTE_USER = 1,
	CONNMAN_PROVIDER_ROUTE_SERVER = 2,
};

struct connman_provider;
struct connman_ipaddress;

#define connman_provider_ref(provider) \
	connman_provider_ref_debug(provider, __FILE__, __LINE__, __func__)

#define connman_provider_unref(provider) \
	connman_provider_unref_debug(provider, __FILE__, __LINE__, __func__)

struct connman_provider *
connman_provider_ref_debug(struct connman_provider *provider,
			const char *file, int line, const char *caller);
void connman_provider_unref_debug(struct connman_provider *provider,
			const char *file, int line, const char *caller);

int connman_provider_disconnect(struct connman_provider *provider);
int connman_provider_remove(struct connman_provider *provider);

int connman_provider_set_string(struct connman_provider *provider,
					const char *key, const char *value);
const char *connman_provider_get_string(struct connman_provider *provider,
							const char *key);

int connman_provider_set_state(struct connman_provider *provider,
					enum connman_provider_state state);

int connman_provider_indicate_error(struct connman_provider *provider,
					enum connman_provider_error error);

void connman_provider_set_index(struct connman_provider *provider, int index);
int connman_provider_get_index(struct connman_provider *provider);

void connman_provider_set_data(struct connman_provider *provider, void *data);
void *connman_provider_get_data(struct connman_provider *provider);
int connman_provider_set_ipaddress(struct connman_provider *provider,
					struct connman_ipaddress *ipaddress);
int connman_provider_set_pac(struct connman_provider *provider,
				const char *pac);
int connman_provider_create_service(struct connman_provider *provider);
int connman_provider_set_immutable(struct connman_provider *provider,
						bool immutable);
struct connman_provider *connman_provider_get(const char *identifier);
void connman_provider_put(struct connman_provider *provider);
int connman_provider_set_domain(struct connman_provider *provider,
				const char *domain);
int connman_provider_set_nameservers(struct connman_provider *provider,
					char * const *nameservers);
int connman_provider_append_route(struct connman_provider *provider,
					const char *key, const char *value);

const char *connman_provider_get_driver_name(struct connman_provider *provider);
const char *connman_provider_get_save_group(struct connman_provider *provider);
typedef void (* connection_ready_cb) (DBusMessage *msg, int error_code,
					void *user_data);

struct connman_provider_driver {
	const char *name;
	enum connman_provider_type type;
	int (*probe) (struct connman_provider *provider);
	int (*remove) (struct connman_provider *provider);
	int (*connect) (struct connman_provider *provider);
	int (*disconnect) (struct connman_provider *provider);
	int (*save) (struct connman_provider *provider, GKeyFile *keyfile);
	int (*set_property) (struct connman_provider *provider,
				const char *key, const char *value);
	const char * (*get_property) (struct connman_provider *provider,
				const char *key);
	int (*create) (DBusMessage *msg, connection_ready_cb callback);
	int (*set_routes) (struct connman_provider *provider,
				enum connman_provider_route_type type);
	bool (*check_routes) (struct connman_provider *provider);
};

int connman_provider_driver_register(struct connman_provider_driver *driver);
void connman_provider_driver_unregister(struct connman_provider_driver *driver);
void connman_provider_set_driver(struct connman_provider *provider,
				struct connman_provider_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_PROVIDER_H */
