/*
 *
 *  ConnMan VPN daemon
 *
 *  Copyright (C) 2012-2013  Intel Corporation. All rights reserved.
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

#include <stdbool.h>

#include <glib.h>

#define VPN_API_SUBJECT_TO_CHANGE

#include <connman/dbus.h>

int __vpn_manager_init(void);
void __vpn_manager_cleanup(void);

struct vpn_ipconfig;

struct connman_ipaddress *__vpn_ipconfig_get_address(struct vpn_ipconfig *ipconfig);
unsigned short __vpn_ipconfig_get_type_from_index(int index);
unsigned int __vpn_ipconfig_get_flags_from_index(int index);
void __vpn_ipconfig_foreach(void (*function) (int index,
				    void *user_data), void *user_data);
void __vpn_ipconfig_set_local(struct vpn_ipconfig *ipconfig,
							const char *address);
const char *__vpn_ipconfig_get_local(struct vpn_ipconfig *ipconfig);
void __vpn_ipconfig_set_peer(struct vpn_ipconfig *ipconfig,
						const char *address);
const char *__vpn_ipconfig_get_peer(struct vpn_ipconfig *ipconfig);
void __vpn_ipconfig_set_broadcast(struct vpn_ipconfig *ipconfig,
						const char *broadcast);
void __vpn_ipconfig_set_gateway(struct vpn_ipconfig *ipconfig,
						const char *gateway);
const char *__vpn_ipconfig_get_gateway(struct vpn_ipconfig *ipconfig);
void __vpn_ipconfig_set_prefixlen(struct vpn_ipconfig *ipconfig,
						unsigned char prefixlen);
unsigned char __vpn_ipconfig_get_prefixlen(struct vpn_ipconfig *ipconfig);
int __vpn_ipconfig_address_add(struct vpn_ipconfig *ipconfig, int family);
int __vpn_ipconfig_gateway_add(struct vpn_ipconfig *ipconfig, int family);
void __vpn_ipconfig_unref_debug(struct vpn_ipconfig *ipconfig,
			const char *file, int line, const char *caller);
#define __vpn_ipconfig_unref(ipconfig) \
	__vpn_ipconfig_unref_debug(ipconfig, __FILE__, __LINE__, __func__)
struct vpn_ipconfig *__vpn_ipconfig_create(int index, int family);
void __vpn_ipconfig_set_index(struct vpn_ipconfig *ipconfig,
								int index);
struct rtnl_link_stats;

void __vpn_ipconfig_newlink(int index, unsigned short type,
				unsigned int flags, const char *address,
				unsigned short mtu,
				struct rtnl_link_stats *stats);
void __vpn_ipconfig_dellink(int index, struct rtnl_link_stats *stats);
int __vpn_ipconfig_init(void);
void __vpn_ipconfig_cleanup(void);

#include "vpn-provider.h"

char *__vpn_provider_create_identifier(const char *host, const char *domain);
bool __vpn_provider_check_routes(struct vpn_provider *provider);
int __vpn_provider_append_user_route(struct vpn_provider *provider,
				int family, const char *network,
				const char *netmask, const char *gateway);
void __vpn_provider_append_properties(struct vpn_provider *provider, DBusMessageIter *iter);
void __vpn_provider_list(DBusMessageIter *iter, void *user_data);
int __vpn_provider_create(DBusMessage *msg);
int __vpn_provider_create_from_config(GHashTable *settings,
			const char *config_ident, const char *config_entry);
int __vpn_provider_set_string_immutable(struct vpn_provider *provider,
					const char *key, const char *value);
DBusMessage *__vpn_provider_get_connections(DBusMessage *msg);
const char * __vpn_provider_get_ident(struct vpn_provider *provider);
struct vpn_provider *__vpn_provider_lookup(const char *identifier);
int __vpn_provider_indicate_state(struct vpn_provider *provider,
					enum vpn_provider_state state);
int __vpn_provider_indicate_error(struct vpn_provider *provider,
					enum vpn_provider_error error);
int __vpn_provider_connect(struct vpn_provider *provider, DBusMessage *msg);
int __vpn_provider_connect_path(const char *path);
int __vpn_provider_disconnect(struct vpn_provider *provider);
int __vpn_provider_remove(const char *path);
int __vpn_provider_delete(struct vpn_provider *provider);
void __vpn_provider_cleanup(void);
int __vpn_provider_init(bool handle_routes);

#include "vpn-rtnl.h"

int __vpn_rtnl_init(void);
void __vpn_rtnl_start(void);
void __vpn_rtnl_cleanup(void);

unsigned int __vpn_rtnl_update_interval_add(unsigned int interval);
unsigned int __vpn_rtnl_update_interval_remove(unsigned int interval);
int __vpn_rtnl_request_update(void);
int __vpn_rtnl_send(const void *buf, size_t len);

int __vpn_config_init(void);
void __vpn_config_cleanup(void);
char *__vpn_config_get_string(GKeyFile *key_file,
        const char *group_name, const char *key, GError **error);
char **__vpn_config_get_string_list(GKeyFile *key_file,
        const char *group_name, const char *key, gsize *length, GError **error);
