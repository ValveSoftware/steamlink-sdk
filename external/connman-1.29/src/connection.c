/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
 *  Copyright (C) 2011-2014  BMW Car IT GmbH.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <net/if.h>

#include <gdbus.h>

#include "connman.h"

struct gateway_config {
	bool active;
	char *gateway;

	/* VPN extra data */
	bool vpn;
	char *vpn_ip;
	int vpn_phy_index;
	char *vpn_phy_ip;
};

struct gateway_data {
	int index;
	struct connman_service *service;
	unsigned int order;
	struct gateway_config *ipv4_gateway;
	struct gateway_config *ipv6_gateway;
	bool default_checked;
};

static GHashTable *gateway_hash = NULL;

static struct gateway_config *find_gateway(int index, const char *gateway)
{
	GHashTableIter iter;
	gpointer value, key;

	if (!gateway)
		return NULL;

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		if (data->ipv4_gateway && data->index == index &&
				g_str_equal(data->ipv4_gateway->gateway,
					gateway))
			return data->ipv4_gateway;

		if (data->ipv6_gateway && data->index == index &&
				g_str_equal(data->ipv6_gateway->gateway,
					gateway))
			return data->ipv6_gateway;
	}

	return NULL;
}

static struct gateway_data *lookup_gateway_data(struct gateway_config *config)
{
	GHashTableIter iter;
	gpointer value, key;

	if (!config)
		return NULL;

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		if (data->ipv4_gateway &&
				data->ipv4_gateway == config)
			return data;

		if (data->ipv6_gateway &&
				data->ipv6_gateway == config)
			return data;
	}

	return NULL;
}

static struct gateway_data *find_vpn_gateway(int index, const char *gateway)
{
	GHashTableIter iter;
	gpointer value, key;

	if (!gateway)
		return NULL;

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		if (data->ipv4_gateway && data->index == index &&
				g_str_equal(data->ipv4_gateway->gateway,
					gateway))
			return data;

		if (data->ipv6_gateway && data->index == index &&
				g_str_equal(data->ipv6_gateway->gateway,
					gateway))
			return data;
	}

	return NULL;
}

struct get_gateway_params {
	char *vpn_gateway;
	int vpn_index;
};

static void get_gateway_cb(const char *gateway, int index, void *user_data)
{
	struct gateway_config *config;
	struct gateway_data *data;
	struct get_gateway_params *params = user_data;
	int family;

	if (index < 0)
		goto out;

	DBG("phy index %d phy gw %s vpn index %d vpn gw %s", index, gateway,
		params->vpn_index, params->vpn_gateway);

	data = find_vpn_gateway(params->vpn_index, params->vpn_gateway);
	if (!data) {
		DBG("Cannot find VPN link route, index %d addr %s",
			params->vpn_index, params->vpn_gateway);
		goto out;
	}

	family = connman_inet_check_ipaddress(params->vpn_gateway);

	if (family == AF_INET)
		config = data->ipv4_gateway;
	else if (family == AF_INET6)
		config = data->ipv6_gateway;
	else
		goto out;

	config->vpn_phy_index = index;

	DBG("vpn %s phy index %d", config->vpn_ip, config->vpn_phy_index);

out:
	g_free(params->vpn_gateway);
	g_free(params);
}

static void set_vpn_routes(struct gateway_data *new_gateway,
			struct connman_service *service,
			const char *gateway,
			enum connman_ipconfig_type type,
			const char *peer,
			struct gateway_data *active_gateway)
{
	struct gateway_config *config;
	struct connman_ipconfig *ipconfig;
	char *dest;

	DBG("new %p service %p gw %s type %d peer %s active %p",
		new_gateway, service, gateway, type, peer, active_gateway);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4) {
		ipconfig = __connman_service_get_ip4config(service);
		config = new_gateway->ipv4_gateway;
	} else if (type == CONNMAN_IPCONFIG_TYPE_IPV6) {
		ipconfig = __connman_service_get_ip6config(service);
		config = new_gateway->ipv6_gateway;
	} else
		return;

	if (config) {
		int index = __connman_ipconfig_get_index(ipconfig);
		struct get_gateway_params *params;

		config->vpn = true;
		if (peer)
			config->vpn_ip = g_strdup(peer);
		else if (gateway)
			config->vpn_ip = g_strdup(gateway);

		params = g_try_malloc(sizeof(struct get_gateway_params));
		if (!params)
			return;

		params->vpn_index = index;
		params->vpn_gateway = g_strdup(gateway);

		/*
		 * Find the gateway that is serving the VPN link
		 */
		__connman_inet_get_route(gateway, get_gateway_cb, params);
	}

	if (!active_gateway)
		return;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4) {
		/*
		 * Special route to VPN server via gateway. This
		 * is needed so that we can access hosts behind
		 * the VPN. The route might already exist depending
		 * on network topology.
		 */
		if (!active_gateway->ipv4_gateway)
			return;

		DBG("active gw %s", active_gateway->ipv4_gateway->gateway);

		if (g_strcmp0(active_gateway->ipv4_gateway->gateway,
							"0.0.0.0") != 0)
			dest = active_gateway->ipv4_gateway->gateway;
		else
			dest = NULL;

		connman_inet_add_host_route(active_gateway->index, gateway,
									dest);

	} else if (type == CONNMAN_IPCONFIG_TYPE_IPV6) {

		if (!active_gateway->ipv6_gateway)
			return;

		DBG("active gw %s", active_gateway->ipv6_gateway->gateway);

		if (g_strcmp0(active_gateway->ipv6_gateway->gateway,
								"::") != 0)
			dest = active_gateway->ipv6_gateway->gateway;
		else
			dest = NULL;

		connman_inet_add_ipv6_host_route(active_gateway->index,
								gateway, dest);
	}
}

static int del_routes(struct gateway_data *data,
			enum connman_ipconfig_type type)
{
	int status4 = 0, status6 = 0;
	bool do_ipv4 = false, do_ipv6 = false;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		do_ipv4 = true;
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		do_ipv6 = true;
	else
		do_ipv4 = do_ipv6 = true;

	if (do_ipv4 && data->ipv4_gateway) {
		if (data->ipv4_gateway->vpn) {
			status4 = connman_inet_clear_gateway_address(
						data->index,
						data->ipv4_gateway->vpn_ip);

		} else if (g_strcmp0(data->ipv4_gateway->gateway,
							"0.0.0.0") == 0) {
			status4 = connman_inet_clear_gateway_interface(
								data->index);
		} else {
			connman_inet_del_host_route(data->index,
						data->ipv4_gateway->gateway);
			status4 = connman_inet_clear_gateway_address(
						data->index,
						data->ipv4_gateway->gateway);
		}
	}

	if (do_ipv6 && data->ipv6_gateway) {
		if (data->ipv6_gateway->vpn) {
			status6 = connman_inet_clear_ipv6_gateway_address(
						data->index,
						data->ipv6_gateway->vpn_ip);

		} else if (g_strcmp0(data->ipv6_gateway->gateway, "::") == 0) {
			status6 = connman_inet_clear_ipv6_gateway_interface(
								data->index);
		} else {
			connman_inet_del_ipv6_host_route(data->index,
						data->ipv6_gateway->gateway);
			status6 = connman_inet_clear_ipv6_gateway_address(
						data->index,
						data->ipv6_gateway->gateway);
		}
	}

	return (status4 < 0 ? status4 : status6);
}

static int disable_gateway(struct gateway_data *data,
			enum connman_ipconfig_type type)
{
	bool active = false;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4) {
		if (data->ipv4_gateway)
			active = data->ipv4_gateway->active;
	} else if (type == CONNMAN_IPCONFIG_TYPE_IPV6) {
		if (data->ipv6_gateway)
			active = data->ipv6_gateway->active;
	} else
		active = true;

	DBG("type %d active %d", type, active);

	if (active)
		return del_routes(data, type);

	return 0;
}

static struct gateway_data *add_gateway(struct connman_service *service,
					int index, const char *gateway,
					enum connman_ipconfig_type type)
{
	struct gateway_data *data, *old;
	struct gateway_config *config;

	if (!gateway || strlen(gateway) == 0)
		return NULL;

	data = g_try_new0(struct gateway_data, 1);
	if (!data)
		return NULL;

	data->index = index;

	config = g_try_new0(struct gateway_config, 1);
	if (!config) {
		g_free(data);
		return NULL;
	}

	config->gateway = g_strdup(gateway);
	config->vpn_ip = NULL;
	config->vpn_phy_ip = NULL;
	config->vpn = false;
	config->vpn_phy_index = -1;
	config->active = false;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		data->ipv4_gateway = config;
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		data->ipv6_gateway = config;
	else {
		g_free(config->gateway);
		g_free(config);
		g_free(data);
		return NULL;
	}

	data->service = service;

	data->order = __connman_service_get_order(service);

	/*
	 * If the service is already in the hash, then we
	 * must not replace it blindly but disable the gateway
	 * of the type we are replacing and take the other type
	 * from old gateway settings.
	 */
	old = g_hash_table_lookup(gateway_hash, service);
	if (old) {
		DBG("Replacing gw %p ipv4 %p ipv6 %p", old,
			old->ipv4_gateway, old->ipv6_gateway);
		disable_gateway(old, type);
		if (type == CONNMAN_IPCONFIG_TYPE_IPV4) {
			data->ipv6_gateway = old->ipv6_gateway;
			old->ipv6_gateway = NULL;
		} else if (type == CONNMAN_IPCONFIG_TYPE_IPV6) {
			data->ipv4_gateway = old->ipv4_gateway;
			old->ipv4_gateway = NULL;
		}
	}

	connman_service_ref(data->service);
	g_hash_table_replace(gateway_hash, service, data);

	return data;
}

static void set_default_gateway(struct gateway_data *data,
				enum connman_ipconfig_type type)
{
	int index;
	int status4 = 0, status6 = 0;
	bool do_ipv4 = false, do_ipv6 = false;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		do_ipv4 = true;
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		do_ipv6 = true;
	else
		do_ipv4 = do_ipv6 = true;

	DBG("type %d gateway ipv4 %p ipv6 %p", type, data->ipv4_gateway,
						data->ipv6_gateway);

	if (do_ipv4 && data->ipv4_gateway &&
					data->ipv4_gateway->vpn) {
		connman_inet_set_gateway_interface(data->index);
		data->ipv4_gateway->active = true;

		DBG("set %p index %d vpn %s index %d phy %s",
			data, data->index, data->ipv4_gateway->vpn_ip,
			data->ipv4_gateway->vpn_phy_index,
			data->ipv4_gateway->vpn_phy_ip);

		__connman_service_indicate_default(data->service);

		return;
	}

	if (do_ipv6 && data->ipv6_gateway &&
					data->ipv6_gateway->vpn) {
		connman_inet_set_ipv6_gateway_interface(data->index);
		data->ipv6_gateway->active = true;

		DBG("set %p index %d vpn %s index %d phy %s",
			data, data->index, data->ipv6_gateway->vpn_ip,
			data->ipv6_gateway->vpn_phy_index,
			data->ipv6_gateway->vpn_phy_ip);

		__connman_service_indicate_default(data->service);

		return;
	}

	index = __connman_service_get_index(data->service);

	if (do_ipv4 && data->ipv4_gateway &&
			g_strcmp0(data->ipv4_gateway->gateway,
							"0.0.0.0") == 0) {
		if (connman_inet_set_gateway_interface(index) < 0)
			return;
		goto done;
	}

	if (do_ipv6 && data->ipv6_gateway &&
			g_strcmp0(data->ipv6_gateway->gateway,
							"::") == 0) {
		if (connman_inet_set_ipv6_gateway_interface(index) < 0)
			return;
		goto done;
	}

	if (do_ipv6 && data->ipv6_gateway)
		status6 = __connman_inet_add_default_to_table(RT_TABLE_MAIN,
					index, data->ipv6_gateway->gateway);

	if (do_ipv4 && data->ipv4_gateway)
		status4 = __connman_inet_add_default_to_table(RT_TABLE_MAIN,
					index, data->ipv4_gateway->gateway);

	if (status4 < 0 || status6 < 0)
		return;

done:
	__connman_service_indicate_default(data->service);
}

static void unset_default_gateway(struct gateway_data *data,
				enum connman_ipconfig_type type)
{
	int index;
	bool do_ipv4 = false, do_ipv6 = false;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		do_ipv4 = true;
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		do_ipv6 = true;
	else
		do_ipv4 = do_ipv6 = true;

	DBG("type %d gateway ipv4 %p ipv6 %p", type, data->ipv4_gateway,
						data->ipv6_gateway);

	if (do_ipv4 && data->ipv4_gateway &&
					data->ipv4_gateway->vpn) {
		connman_inet_clear_gateway_interface(data->index);
		data->ipv4_gateway->active = false;

		DBG("unset %p index %d vpn %s index %d phy %s",
			data, data->index, data->ipv4_gateway->vpn_ip,
			data->ipv4_gateway->vpn_phy_index,
			data->ipv4_gateway->vpn_phy_ip);

		return;
	}

	if (do_ipv6 && data->ipv6_gateway &&
					data->ipv6_gateway->vpn) {
		connman_inet_clear_ipv6_gateway_interface(data->index);
		data->ipv6_gateway->active = false;

		DBG("unset %p index %d vpn %s index %d phy %s",
			data, data->index, data->ipv6_gateway->vpn_ip,
			data->ipv6_gateway->vpn_phy_index,
			data->ipv6_gateway->vpn_phy_ip);

		return;
	}

	index = __connman_service_get_index(data->service);

	if (do_ipv4 && data->ipv4_gateway &&
			g_strcmp0(data->ipv4_gateway->gateway,
							"0.0.0.0") == 0) {
		connman_inet_clear_gateway_interface(index);
		return;
	}

	if (do_ipv6 && data->ipv6_gateway &&
			g_strcmp0(data->ipv6_gateway->gateway,
							"::") == 0) {
		connman_inet_clear_ipv6_gateway_interface(index);
		return;
	}

	if (do_ipv6 && data->ipv6_gateway)
		connman_inet_clear_ipv6_gateway_address(index,
						data->ipv6_gateway->gateway);

	if (do_ipv4 && data->ipv4_gateway)
		connman_inet_clear_gateway_address(index,
						data->ipv4_gateway->gateway);
}

static struct gateway_data *find_default_gateway(void)
{
	struct gateway_data *found = NULL;
	unsigned int order = 0;
	GHashTableIter iter;
	gpointer value, key;

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		if (!found || data->order > order) {
			found = data;
			order = data->order;

			DBG("default %p order %d", found, order);
		}
	}

	return found;
}

static bool choose_default_gateway(struct gateway_data *data,
					struct gateway_data *candidate)
{
	bool downgraded = false;

	/*
	 * If the current default is not active, then we mark
	 * this one as default. If the other one is already active
	 * we mark this one as non default.
	 */
	if (data->ipv4_gateway) {
		if (candidate->ipv4_gateway &&
				!candidate->ipv4_gateway->active) {
			DBG("ipv4 downgrading %p", candidate);
			unset_default_gateway(candidate,
						CONNMAN_IPCONFIG_TYPE_IPV4);
		}
		if (candidate->ipv4_gateway &&
				candidate->ipv4_gateway->active &&
				candidate->order > data->order) {
			DBG("ipv4 downgrading this %p", data);
			unset_default_gateway(data,
						CONNMAN_IPCONFIG_TYPE_IPV4);
			downgraded = true;
		}
	}

	if (data->ipv6_gateway) {
		if (candidate->ipv6_gateway &&
				!candidate->ipv6_gateway->active) {
			DBG("ipv6 downgrading %p", candidate);
			unset_default_gateway(candidate,
						CONNMAN_IPCONFIG_TYPE_IPV6);
		}

		if (candidate->ipv6_gateway &&
				candidate->ipv6_gateway->active &&
				candidate->order > data->order) {
			DBG("ipv6 downgrading this %p", data);
			unset_default_gateway(data,
						CONNMAN_IPCONFIG_TYPE_IPV6);
			downgraded = true;
		}
	}

	return downgraded;
}

static void connection_newgateway(int index, const char *gateway)
{
	struct gateway_config *config;
	struct gateway_data *data;
	GHashTableIter iter;
	gpointer value, key;
	bool found = false;

	DBG("index %d gateway %s", index, gateway);

	config = find_gateway(index, gateway);
	if (!config)
		return;

	config->active = true;

	/*
	 * It is possible that we have two default routes atm
	 * if there are two gateways waiting rtnl activation at the
	 * same time.
	 */
	data = lookup_gateway_data(config);
	if (!data)
		return;

	if (data->default_checked)
		return;

	/*
	 * The next checks are only done once, otherwise setting
	 * the default gateway could lead into rtnl forever loop.
	 */

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *candidate = value;

		if (candidate == data)
			continue;

		found = choose_default_gateway(data, candidate);
		if (found)
			break;
	}

	if (!found) {
		if (data->ipv4_gateway)
			set_default_gateway(data, CONNMAN_IPCONFIG_TYPE_IPV4);

		if (data->ipv6_gateway)
			set_default_gateway(data, CONNMAN_IPCONFIG_TYPE_IPV6);
	}

	data->default_checked = true;
}

static void remove_gateway(gpointer user_data)
{
	struct gateway_data *data = user_data;

	DBG("gateway ipv4 %p ipv6 %p", data->ipv4_gateway, data->ipv6_gateway);

	if (data->ipv4_gateway) {
		g_free(data->ipv4_gateway->gateway);
		g_free(data->ipv4_gateway->vpn_ip);
		g_free(data->ipv4_gateway->vpn_phy_ip);
		g_free(data->ipv4_gateway);
	}

	if (data->ipv6_gateway) {
		g_free(data->ipv6_gateway->gateway);
		g_free(data->ipv6_gateway->vpn_ip);
		g_free(data->ipv6_gateway->vpn_phy_ip);
		g_free(data->ipv6_gateway);
	}

	connman_service_unref(data->service);

	g_free(data);
}

static void connection_delgateway(int index, const char *gateway)
{
	struct gateway_config *config;
	struct gateway_data *data;

	DBG("index %d gateway %s", index, gateway);

	config = find_gateway(index, gateway);
	if (config)
		config->active = false;

	data = find_default_gateway();
	if (data)
		set_default_gateway(data, CONNMAN_IPCONFIG_TYPE_ALL);
}

static struct connman_rtnl connection_rtnl = {
	.name		= "connection",
	.newgateway	= connection_newgateway,
	.delgateway	= connection_delgateway,
};

static struct gateway_data *find_active_gateway(void)
{
	GHashTableIter iter;
	gpointer value, key;

	DBG("");

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		if (data->ipv4_gateway &&
				data->ipv4_gateway->active)
			return data;

		if (data->ipv6_gateway &&
				data->ipv6_gateway->active)
			return data;
	}

	return NULL;
}

static void update_order(void)
{
	GHashTableIter iter;
	gpointer value, key;

	DBG("");

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		data->order = __connman_service_get_order(data->service);
	}
}

void __connman_connection_gateway_activate(struct connman_service *service,
					enum connman_ipconfig_type type)
{
	struct gateway_data *data = NULL;

	data = g_hash_table_lookup(gateway_hash, service);
	if (!data)
		return;

	DBG("gateway %p/%p type %d", data->ipv4_gateway,
					data->ipv6_gateway, type);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		data->ipv4_gateway->active = true;
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		data->ipv6_gateway->active = true;
}

static void add_host_route(int family, int index, const char *gateway,
			enum connman_service_type service_type)
{
	switch (family) {
	case AF_INET:
		if (g_strcmp0(gateway, "0.0.0.0") != 0) {
			/*
			 * We must not set route to the phy dev gateway in
			 * VPN link. The packets to VPN link might be routed
			 * back to itself and not routed into phy link gateway.
			 */
			if (service_type != CONNMAN_SERVICE_TYPE_VPN)
				connman_inet_add_host_route(index, gateway,
									NULL);
		} else {
			/*
			 * Add host route to P-t-P link so that services can
			 * be moved around and we can have some link to P-t-P
			 * network (although those P-t-P links have limited
			 * usage if default route is not directed to them)
			 */
			char *dest;
			if (connman_inet_get_dest_addr(index, &dest) == 0) {
				connman_inet_add_host_route(index, dest, NULL);
				g_free(dest);
			}
		}
		break;

	case AF_INET6:
		if (g_strcmp0(gateway, "::") != 0) {
			if (service_type != CONNMAN_SERVICE_TYPE_VPN)
				connman_inet_add_ipv6_host_route(index,
								gateway, NULL);
		} else {
			/* P-t-P link, add route to destination */
			char *dest;
			if (connman_inet_ipv6_get_dest_addr(index,
								&dest) == 0) {
				connman_inet_add_ipv6_host_route(index, dest,
								NULL);
				g_free(dest);
			}
		}
		break;
	}
}

int __connman_connection_gateway_add(struct connman_service *service,
					const char *gateway,
					enum connman_ipconfig_type type,
					const char *peer)
{
	struct gateway_data *active_gateway = NULL;
	struct gateway_data *new_gateway = NULL;
	enum connman_ipconfig_type type4 = CONNMAN_IPCONFIG_TYPE_UNKNOWN,
		type6 = CONNMAN_IPCONFIG_TYPE_UNKNOWN;
	enum connman_service_type service_type =
					connman_service_get_type(service);
	int index;

	index = __connman_service_get_index(service);

	/*
	 * If gateway is NULL, it's a point to point link and the default
	 * gateway for ipv4 is 0.0.0.0 and for ipv6 is ::, meaning the
	 * interface
	 */
	if (!gateway && type == CONNMAN_IPCONFIG_TYPE_IPV4)
		gateway = "0.0.0.0";

	if (!gateway && type == CONNMAN_IPCONFIG_TYPE_IPV6)
		gateway = "::";

	DBG("service %p index %d gateway %s vpn ip %s type %d",
		service, index, gateway, peer, type);

	new_gateway = add_gateway(service, index, gateway, type);
	if (!new_gateway)
		return -EINVAL;

	active_gateway = find_active_gateway();

	DBG("active %p index %d new %p", active_gateway,
		active_gateway ? active_gateway->index : -1, new_gateway);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4 &&
				new_gateway->ipv4_gateway) {
		add_host_route(AF_INET, index, gateway, service_type);
		__connman_service_nameserver_add_routes(service,
					new_gateway->ipv4_gateway->gateway);
		type4 = CONNMAN_IPCONFIG_TYPE_IPV4;
	}

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6 &&
				new_gateway->ipv6_gateway) {
		add_host_route(AF_INET6, index, gateway, service_type);
		__connman_service_nameserver_add_routes(service,
					new_gateway->ipv6_gateway->gateway);
		type6 = CONNMAN_IPCONFIG_TYPE_IPV6;
	}

	if (service_type == CONNMAN_SERVICE_TYPE_VPN) {

		set_vpn_routes(new_gateway, service, gateway, type, peer,
							active_gateway);

	} else {
		if (type == CONNMAN_IPCONFIG_TYPE_IPV4 &&
					new_gateway->ipv4_gateway)
			new_gateway->ipv4_gateway->vpn = false;

		if (type == CONNMAN_IPCONFIG_TYPE_IPV6 &&
					new_gateway->ipv6_gateway)
			new_gateway->ipv6_gateway->vpn = false;
	}

	if (!active_gateway) {
		set_default_gateway(new_gateway, type);
		goto done;
	}

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4 &&
				new_gateway->ipv4_gateway &&
				new_gateway->ipv4_gateway->vpn) {
		if (!__connman_service_is_split_routing(new_gateway->service))
			connman_inet_clear_gateway_address(
					active_gateway->index,
					active_gateway->ipv4_gateway->gateway);
	}

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6 &&
				new_gateway->ipv6_gateway &&
				new_gateway->ipv6_gateway->vpn) {
		if (!__connman_service_is_split_routing(new_gateway->service))
			connman_inet_clear_ipv6_gateway_address(
					active_gateway->index,
					active_gateway->ipv6_gateway->gateway);
	}

done:
	if (type4 == CONNMAN_IPCONFIG_TYPE_IPV4)
		__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_READY,
						CONNMAN_IPCONFIG_TYPE_IPV4);

	if (type6 == CONNMAN_IPCONFIG_TYPE_IPV6)
		__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_READY,
						CONNMAN_IPCONFIG_TYPE_IPV6);
	return 0;
}

void __connman_connection_gateway_remove(struct connman_service *service,
					enum connman_ipconfig_type type)
{
	struct gateway_data *data = NULL;
	bool set_default4 = false, set_default6 = false;
        bool do_ipv4 = false, do_ipv6 = false;
	int err;

	DBG("service %p type %d", service, type);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		do_ipv4 = true;
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		do_ipv6 = true;
	else
		do_ipv4 = do_ipv6 = true;

	__connman_service_nameserver_del_routes(service, type);

	data = g_hash_table_lookup(gateway_hash, service);
	if (!data)
		return;

	if (do_ipv4 && data->ipv4_gateway)
		set_default4 = data->ipv4_gateway->vpn;

	if (do_ipv6 && data->ipv6_gateway)
		set_default6 = data->ipv6_gateway->vpn;

	DBG("ipv4 gateway %s ipv6 gateway %s vpn %d/%d",
		data->ipv4_gateway ? data->ipv4_gateway->gateway : "<null>",
		data->ipv6_gateway ? data->ipv6_gateway->gateway : "<null>",
		set_default4, set_default6);

	if (do_ipv4 && data->ipv4_gateway &&
			data->ipv4_gateway->vpn && data->index >= 0)
		connman_inet_del_host_route(data->ipv4_gateway->vpn_phy_index,
						data->ipv4_gateway->gateway);

	if (do_ipv6 && data->ipv6_gateway &&
			data->ipv6_gateway->vpn && data->index >= 0)
		connman_inet_del_ipv6_host_route(
					data->ipv6_gateway->vpn_phy_index,
						data->ipv6_gateway->gateway);

	err = disable_gateway(data, type);

	/*
	 * We remove the service from the hash only if all the gateway
	 * settings are to be removed.
	 */
	if (do_ipv4 == do_ipv6 ||
		(data->ipv4_gateway && !data->ipv6_gateway
			&& do_ipv4) ||
		(data->ipv6_gateway && !data->ipv4_gateway
			&& do_ipv6)) {
		g_hash_table_remove(gateway_hash, service);
	} else
		DBG("Not yet removing gw ipv4 %p/%d ipv6 %p/%d",
			data->ipv4_gateway, do_ipv4,
			data->ipv6_gateway, do_ipv6);

	/* with vpn this will be called after the network was deleted,
	 * we need to call set_default here because we will not recieve any
	 * gateway delete notification.
	 * We hit the same issue if remove_gateway() fails.
	 */
	if (set_default4 || set_default6 || err < 0) {
		data = find_default_gateway();
		if (data)
			set_default_gateway(data, type);
	}
}

bool __connman_connection_update_gateway(void)
{
	struct gateway_data *default_gateway;
	bool updated = false;
	GHashTableIter iter;
	gpointer value, key;

	if (!gateway_hash)
		return updated;

	update_order();

	default_gateway = find_default_gateway();

	__connman_service_update_ordering();

	DBG("default %p", default_gateway);

	/*
	 * There can be multiple active gateways so we need to
	 * check them all.
	 */
	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *active_gateway = value;

		if (active_gateway == default_gateway)
			continue;

		if (active_gateway->ipv4_gateway &&
				active_gateway->ipv4_gateway->active) {

			unset_default_gateway(active_gateway,
						CONNMAN_IPCONFIG_TYPE_IPV4);
			updated = true;
		}

		if (active_gateway->ipv6_gateway &&
				active_gateway->ipv6_gateway->active) {

			unset_default_gateway(active_gateway,
						CONNMAN_IPCONFIG_TYPE_IPV6);
			updated = true;
		}
	}

	if (updated && default_gateway) {
		if (default_gateway->ipv4_gateway)
			set_default_gateway(default_gateway,
					CONNMAN_IPCONFIG_TYPE_IPV4);

		if (default_gateway->ipv6_gateway)
			set_default_gateway(default_gateway,
					CONNMAN_IPCONFIG_TYPE_IPV6);
	}

	return updated;
}

int __connman_connection_get_vpn_index(int phy_index)
{
	GHashTableIter iter;
	gpointer value, key;

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		if (data->ipv4_gateway &&
				data->ipv4_gateway->vpn_phy_index == phy_index)
			return data->index;

		if (data->ipv6_gateway &&
				data->ipv6_gateway->vpn_phy_index == phy_index)
			return data->index;
	}

	return -1;
}

int __connman_connection_init(void)
{
	int err;

	DBG("");

	gateway_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, remove_gateway);

	err = connman_rtnl_register(&connection_rtnl);
	if (err < 0)
		connman_error("Failed to setup RTNL gateway driver");

	return err;
}

void __connman_connection_cleanup(void)
{
	GHashTableIter iter;
	gpointer value, key;

	DBG("");

	connman_rtnl_unregister(&connection_rtnl);

	g_hash_table_iter_init(&iter, gateway_hash);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct gateway_data *data = value;

		disable_gateway(data, CONNMAN_IPCONFIG_TYPE_ALL);
	}

	g_hash_table_destroy(gateway_hash);
	gateway_hash = NULL;
}
