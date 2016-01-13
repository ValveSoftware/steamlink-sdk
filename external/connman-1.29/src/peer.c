/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2014  Intel Corporation. All rights reserved.
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
#include <ctype.h>
#include <gdbus.h>
#include <gdhcp/gdhcp.h>
#include <netinet/if_ether.h>

#include <connman/agent.h>

#include "connman.h"

static DBusConnection *connection = NULL;

static GHashTable *peers_table = NULL;

static struct connman_peer_driver *peer_driver;

struct _peers_notify {
	int id;
	GHashTable *add;
	GHashTable *remove;
} *peers_notify;

struct _peer_service {
	enum connman_peer_service_type type;
	unsigned char *data;
	int length;
};

struct connman_peer {
	int refcount;
	struct connman_device *device;
	struct connman_device *sub_device;
	unsigned char *iface_address[ETH_ALEN];
	char *identifier;
	char *name;
	char *path;
	enum connman_peer_state state;
	struct connman_ipconfig *ipconfig;
	DBusMessage *pending;
	bool registered;
	bool connection_master;
	struct connman_ippool *ip_pool;
	GDHCPServer *dhcp_server;
	uint32_t lease_ip;
	GSList *services;
};

static void settings_changed(struct connman_peer *peer);

static void stop_dhcp_server(struct connman_peer *peer)
{
	DBG("");

	if (peer->dhcp_server)
		g_dhcp_server_unref(peer->dhcp_server);

	peer->dhcp_server = NULL;

	if (peer->ip_pool)
		__connman_ippool_unref(peer->ip_pool);
	peer->ip_pool = NULL;
	peer->lease_ip = 0;
}

static void dhcp_server_debug(const char *str, void *data)
{
	connman_info("%s: %s\n", (const char *) data, str);
}

static void lease_added(unsigned char *mac, uint32_t ip)
{
	GList *list, *start;

	start = list = g_hash_table_get_values(peers_table);
	for (; list; list = list->next) {
		struct connman_peer *temp = list->data;

		if (!memcmp(temp->iface_address, mac, ETH_ALEN)) {
			temp->lease_ip = ip;
			settings_changed(temp);
			break;
		}
	}

	g_list_free(start);
}

static gboolean dhcp_server_started(gpointer data)
{
	struct connman_peer *peer = data;

	connman_peer_set_state(peer, CONNMAN_PEER_STATE_READY);
	connman_peer_unref(peer);

	return FALSE;
}

static int start_dhcp_server(struct connman_peer *peer)
{
	const char *start_ip, *end_ip;
	GDHCPServerError dhcp_error;
	const char *broadcast;
	const char *gateway;
	const char *subnet;
	int prefixlen;
	int index;
	int err;

	DBG("");

	err = -ENOMEM;

	if (peer->sub_device)
		index = connman_device_get_index(peer->sub_device);
	else
		index = connman_device_get_index(peer->device);

	peer->ip_pool = __connman_ippool_create(index, 2, 1, NULL, NULL);
	if (!peer->ip_pool)
		goto error;

	gateway = __connman_ippool_get_gateway(peer->ip_pool);
	subnet = __connman_ippool_get_subnet_mask(peer->ip_pool);
	broadcast = __connman_ippool_get_broadcast(peer->ip_pool);
	start_ip = __connman_ippool_get_start_ip(peer->ip_pool);
	end_ip = __connman_ippool_get_end_ip(peer->ip_pool);

	prefixlen = connman_ipaddress_calc_netmask_len(subnet);

	err = __connman_inet_modify_address(RTM_NEWADDR,
				NLM_F_REPLACE | NLM_F_ACK, index, AF_INET,
				gateway, NULL, prefixlen, broadcast);
	if (err < 0)
		goto error;

	peer->dhcp_server = g_dhcp_server_new(G_DHCP_IPV4, index, &dhcp_error);
	if (!peer->dhcp_server)
		goto error;

	g_dhcp_server_set_debug(peer->dhcp_server,
					dhcp_server_debug, "Peer DHCP server");
	g_dhcp_server_set_lease_time(peer->dhcp_server, 3600);
	g_dhcp_server_set_option(peer->dhcp_server, G_DHCP_SUBNET, subnet);
	g_dhcp_server_set_option(peer->dhcp_server, G_DHCP_ROUTER, gateway);
	g_dhcp_server_set_option(peer->dhcp_server, G_DHCP_DNS_SERVER, NULL);
	g_dhcp_server_set_ip_range(peer->dhcp_server, start_ip, end_ip);

	g_dhcp_server_set_lease_added_cb(peer->dhcp_server, lease_added);

	err = g_dhcp_server_start(peer->dhcp_server);
	if (err < 0)
		goto error;

	g_timeout_add_seconds(0, dhcp_server_started, connman_peer_ref(peer));

	return 0;

error:
	stop_dhcp_server(peer);
	return err;
}

static void reply_pending(struct connman_peer *peer, int error)
{
	if (!peer->pending)
		return;

	connman_dbus_reply_pending(peer->pending, error, NULL);
	peer->pending = NULL;
}

static void peer_free(gpointer data)
{
	struct connman_peer *peer = data;

	reply_pending(peer, ENOENT);

	connman_peer_unregister(peer);

	if (peer->path) {
		g_free(peer->path);
		peer->path = NULL;
	}

	if (peer->ipconfig) {
		__connman_ipconfig_set_ops(peer->ipconfig, NULL);
		__connman_ipconfig_set_data(peer->ipconfig, NULL);
		__connman_ipconfig_unref(peer->ipconfig);
		peer->ipconfig = NULL;
	}

	stop_dhcp_server(peer);

	if (peer->device) {
		connman_device_unref(peer->device);
		peer->device = NULL;
	}

	if (peer->services)
		connman_peer_reset_services(peer);

	g_free(peer->identifier);
	g_free(peer->name);

	g_free(peer);
}

static const char *state2string(enum connman_peer_state state)
{
	switch (state) {
	case CONNMAN_PEER_STATE_UNKNOWN:
		break;
	case CONNMAN_PEER_STATE_IDLE:
		return "idle";
	case CONNMAN_PEER_STATE_ASSOCIATION:
		return "association";
	case CONNMAN_PEER_STATE_CONFIGURATION:
		return "configuration";
	case CONNMAN_PEER_STATE_READY:
		return "ready";
	case CONNMAN_PEER_STATE_DISCONNECT:
		return "disconnect";
	case CONNMAN_PEER_STATE_FAILURE:
		return "failure";
	}

	return NULL;
}

static bool is_connecting(struct connman_peer *peer)
{
	if (peer->state == CONNMAN_PEER_STATE_ASSOCIATION ||
			peer->state == CONNMAN_PEER_STATE_CONFIGURATION ||
			peer->pending)
		return true;

	return false;
}

static bool is_connected(struct connman_peer *peer)
{
	if (peer->state == CONNMAN_PEER_STATE_READY)
		return true;

	return false;
}

static bool allow_property_changed(struct connman_peer *peer)
{
	if (g_hash_table_lookup_extended(peers_notify->add, peer->path,
								NULL, NULL))
		return false;

	return true;
}

static void append_ipv4(DBusMessageIter *iter, void *user_data)
{
	struct connman_peer *peer = user_data;
	char trans[INET_ADDRSTRLEN+1] = {};
	const char *local = "";
	const char *remote = "";
	char *dhcp = NULL;

	if (!is_connected(peer))
		return;

	if (peer->connection_master) {
		struct in_addr addr;

		addr.s_addr = peer->lease_ip;
		inet_ntop(AF_INET, &addr, trans, INET_ADDRSTRLEN);

		local = __connman_ippool_get_gateway(peer->ip_pool);
		remote = trans;
	} else if (peer->ipconfig) {
		local = __connman_ipconfig_get_local(peer->ipconfig);

		remote = __connman_ipconfig_get_gateway(peer->ipconfig);
		if (!remote) {
			remote = dhcp = __connman_dhcp_get_server_address(
							peer->ipconfig);
			if (!dhcp)
				remote = "";
		}
	}

	connman_dbus_dict_append_basic(iter, "Local",
						DBUS_TYPE_STRING, &local);
	connman_dbus_dict_append_basic(iter, "Remote",
						DBUS_TYPE_STRING, &remote);
	if (dhcp)
		g_free(dhcp);
}

static void append_peer_service(DBusMessageIter *iter,
					struct _peer_service *service)
{
	DBusMessageIter dict;

	connman_dbus_dict_open(iter, &dict);

	switch (service->type) {
	case CONNMAN_PEER_SERVICE_UNKNOWN:
		/* Should never happen */
		break;
	case CONNMAN_PEER_SERVICE_WIFI_DISPLAY:
		connman_dbus_dict_append_fixed_array(&dict,
				"WiFiDisplayIEs", DBUS_TYPE_BYTE,
				&service->data, service->length);
		break;
	}

	connman_dbus_dict_close(iter, &dict);
}

static void append_peer_services(DBusMessageIter *iter, void *user_data)
{
	struct connman_peer *peer = user_data;
	DBusMessageIter container;
	GSList *list;

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT,
							NULL, &container);

	if (!peer->services) {
		DBusMessageIter dict;

		connman_dbus_dict_open(&container, &dict);
		connman_dbus_dict_close(&container, &dict);
	} else {
		for (list = peer->services; list; list = list->next)
			append_peer_service(&container, list->data);
	}

	dbus_message_iter_close_container(iter, &container);
}

static void append_properties(DBusMessageIter *iter, struct connman_peer *peer)
{
	const char *state = state2string(peer->state);
	DBusMessageIter dict;

	connman_dbus_dict_open(iter, &dict);

	connman_dbus_dict_append_basic(&dict, "State",
					DBUS_TYPE_STRING, &state);
	connman_dbus_dict_append_basic(&dict, "Name",
					DBUS_TYPE_STRING, &peer->name);
	connman_dbus_dict_append_dict(&dict, "IPv4", append_ipv4, peer);
	connman_dbus_dict_append_array(&dict, "Services",
					DBUS_TYPE_DICT_ENTRY,
					append_peer_services, peer);
	connman_dbus_dict_close(iter, &dict);
}

static void settings_changed(struct connman_peer *peer)
{
	if (!allow_property_changed(peer))
		return;

	connman_dbus_property_changed_dict(peer->path,
					CONNMAN_PEER_INTERFACE, "IPv4",
					append_ipv4, peer);
}

static DBusMessage *get_peer_properties(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	struct connman_peer *peer = data;
	DBusMessageIter dict;
	DBusMessage *reply;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &dict);
	append_properties(&dict, peer);

	return reply;
}

static void append_peer_struct(gpointer key, gpointer value,
						gpointer user_data)
{
	DBusMessageIter *array = user_data;
	struct connman_peer *peer = value;
	DBusMessageIter entry;

	dbus_message_iter_open_container(array, DBUS_TYPE_STRUCT,
							NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_OBJECT_PATH,
							&peer->path);
	append_properties(&entry, peer);
	dbus_message_iter_close_container(array, &entry);
}

static void state_changed(struct connman_peer *peer)
{
	const char *state;

	state = state2string(peer->state);
	if (!state || !allow_property_changed(peer))
		return;

	connman_dbus_property_changed_basic(peer->path,
					 CONNMAN_PEER_INTERFACE, "State",
					 DBUS_TYPE_STRING, &state);
}

static void append_existing_and_new_peers(gpointer key,
					gpointer value, gpointer user_data)
{
	struct connman_peer *peer = value;
	DBusMessageIter *iter = user_data;
	DBusMessageIter entry, dict;

	if (!peer || !peer->registered)
		return;

	if (g_hash_table_lookup(peers_notify->add, peer->path)) {
		DBG("new %s", peer->path);

		append_peer_struct(key, peer, iter);
		g_hash_table_remove(peers_notify->add, peer->path);
	} else if (!g_hash_table_lookup(peers_notify->remove, peer->path)) {
		DBG("existing %s", peer->path);

		dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT,
								NULL, &entry);
		dbus_message_iter_append_basic(&entry, DBUS_TYPE_OBJECT_PATH,
								&peer->path);
		connman_dbus_dict_open(&entry, &dict);
		connman_dbus_dict_close(&entry, &dict);

		dbus_message_iter_close_container(iter, &entry);
	}
}

static void peer_append_all(DBusMessageIter *iter, void *user_data)
{
	g_hash_table_foreach(peers_table, append_existing_and_new_peers, iter);
}

static void append_removed(gpointer key, gpointer value, gpointer user_data)
{
	DBusMessageIter *iter = user_data;
	char *objpath = key;

	DBG("removed %s", objpath);
	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &objpath);
}

static void peer_append_removed(DBusMessageIter *iter, void *user_data)
{
	g_hash_table_foreach(peers_notify->remove, append_removed, iter);
}

static gboolean peer_send_changed(gpointer data)
{
	DBusMessage *signal;

	DBG("");

	peers_notify->id = 0;

	signal = dbus_message_new_signal(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "PeersChanged");
	if (!signal)
		return FALSE;

	__connman_dbus_append_objpath_dict_array(signal,
						peer_append_all, NULL);
	__connman_dbus_append_objpath_array(signal,
						peer_append_removed, NULL);

	dbus_connection_send(connection, signal, NULL);
	dbus_message_unref(signal);

	g_hash_table_remove_all(peers_notify->remove);
	g_hash_table_remove_all(peers_notify->add);

	return FALSE;
}

static void peer_schedule_changed(void)
{
	if (peers_notify->id != 0)
		return;

	peers_notify->id = g_timeout_add(100, peer_send_changed, NULL);
}

static void peer_added(struct connman_peer *peer)
{
	DBG("peer %p", peer);

	g_hash_table_remove(peers_notify->remove, peer->path);
	g_hash_table_replace(peers_notify->add, peer->path, peer);

	peer_schedule_changed();
}

static void peer_removed(struct connman_peer *peer)
{
	DBG("peer %p", peer);

	g_hash_table_remove(peers_notify->add, peer->path);
	g_hash_table_replace(peers_notify->remove, g_strdup(peer->path), NULL);

	peer_schedule_changed();
}

static const char *get_dbus_sender(struct connman_peer *peer)
{
	if (!peer->pending)
		return NULL;

	return dbus_message_get_sender(peer->pending);
}

static enum connman_peer_wps_method check_wpspin(struct connman_peer *peer,
							const char *wpspin)
{
	int len, i;

	if (!wpspin)
		return CONNMAN_PEER_WPS_PBC;

	len = strlen(wpspin);
	if (len == 0)
		return CONNMAN_PEER_WPS_PBC;

	if (len != 8)
		return CONNMAN_PEER_WPS_UNKNOWN;
	for (i = 0; i < 8; i++) {
		if (!isdigit((unsigned char) wpspin[i]))
			return CONNMAN_PEER_WPS_UNKNOWN;
	}

	return CONNMAN_PEER_WPS_PIN;
}

static void request_authorization_cb(struct connman_peer *peer,
					bool choice_done, const char *wpspin,
					const char *error, void *user_data)
{
	enum connman_peer_wps_method wps_method;
	int err;

	DBG("RequestInput return, %p", peer);

	if (error) {
		if (g_strcmp0(error,
				"net.connman.Agent.Error.Canceled") == 0 ||
			g_strcmp0(error,
				"net.connman.Agent.Error.Rejected") == 0) {
			err = -EINVAL;
			goto out;
		}
	}

	if (!choice_done || !peer_driver->connect) {
		err = -EINVAL;
		goto out;
	}

	wps_method = check_wpspin(peer, wpspin);

	err = peer_driver->connect(peer, wps_method, wpspin);
	if (err == -EINPROGRESS)
		return;

out:
	reply_pending(peer, EIO);
	connman_peer_set_state(peer, CONNMAN_PEER_STATE_IDLE);
}

static int peer_connect(struct connman_peer *peer)
{
	int err = -ENOTSUP;

	if (peer_driver->connect)
		err = peer_driver->connect(peer,
					CONNMAN_PEER_WPS_UNKNOWN, NULL);

	if (err == -ENOKEY) {
		err = __connman_agent_request_peer_authorization(peer,
						request_authorization_cb, true,
						get_dbus_sender(peer), NULL);
	}

	return err;
}

static int peer_disconnect(struct connman_peer *peer)
{
	int err = -ENOTSUP;

	connman_agent_cancel(peer);
	reply_pending(peer, ECONNABORTED);

	connman_peer_set_state(peer, CONNMAN_PEER_STATE_DISCONNECT);

	if (peer->connection_master)
		stop_dhcp_server(peer);
	else
		__connman_dhcp_stop(peer->ipconfig);

	if (peer_driver->disconnect)
		err = peer_driver->disconnect(peer);

	connman_peer_set_state(peer, CONNMAN_PEER_STATE_IDLE);

	return err;
}

static DBusMessage *connect_peer(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_peer *peer = user_data;
	GList *list, *start;
	int err;

	DBG("peer %p", peer);

	if (peer->pending)
		return __connman_error_in_progress(msg);

	list = g_hash_table_get_values(peers_table);
	start = list;
	for (; list; list = list->next) {
		struct connman_peer *temp = list->data;

		if (temp == peer || temp->device != peer->device)
			continue;

		if (is_connecting(temp) || is_connected(temp)) {
			if (peer_disconnect(temp) == -EINPROGRESS) {
				g_list_free(start);
				return __connman_error_in_progress(msg);
			}
		}
	}

	g_list_free(start);

	peer->pending = dbus_message_ref(msg);

	err = peer_connect(peer);
	if (err == -EINPROGRESS)
		return NULL;

	if (err < 0) {
		dbus_message_unref(peer->pending);
		peer->pending = NULL;

		return __connman_error_failed(msg, -err);
	}

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static DBusMessage *disconnect_peer(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_peer *peer = user_data;
	int err;

	DBG("peer %p", peer);

	err = peer_disconnect(peer);
	if (err < 0 && err != -EINPROGRESS)
		return __connman_error_failed(msg, -err);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

struct connman_peer *connman_peer_create(const char *identifier)
{
	struct connman_peer *peer;

	peer = g_malloc0(sizeof(struct connman_peer));
	peer->identifier = g_strdup(identifier);
	peer->state = CONNMAN_PEER_STATE_IDLE;

	peer->refcount = 1;

	return peer;
}

struct connman_peer *connman_peer_ref_debug(struct connman_peer *peer,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", peer, peer->refcount + 1,
						file, line, caller);

	__sync_fetch_and_add(&peer->refcount, 1);

	return peer;
}

void connman_peer_unref_debug(struct connman_peer *peer,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", peer, peer->refcount - 1,
						file, line, caller);

	if (__sync_fetch_and_sub(&peer->refcount, 1) != 1)
		return;

	if (!peer->registered && !peer->path)
		return peer_free(peer);

	g_hash_table_remove(peers_table, peer->path);
}

const char *connman_peer_get_identifier(struct connman_peer *peer)
{
	if (!peer)
		return NULL;

	return peer->identifier;
}

void connman_peer_set_name(struct connman_peer *peer, const char *name)
{
	g_free(peer->name);
	peer->name = g_strdup(name);
}

void connman_peer_set_iface_address(struct connman_peer *peer,
					const unsigned char *iface_address)
{
	memset(peer->iface_address, 0, ETH_ALEN);
	memcpy(peer->iface_address, iface_address, ETH_ALEN);
}

void connman_peer_set_device(struct connman_peer *peer,
				struct connman_device *device)
{
	if (!peer || !device)
		return;

	peer->device = device;
	connman_device_ref(device);
}

struct connman_device *connman_peer_get_device(struct connman_peer *peer)
{
	if (!peer)
		return NULL;

	return peer->device;
}

void connman_peer_set_sub_device(struct connman_peer *peer,
					struct connman_device *device)
{
	if (!peer || !device || peer->sub_device)
		return;

	peer->sub_device = device;
}

void connman_peer_set_as_master(struct connman_peer *peer, bool master)
{
	if (!peer || !is_connecting(peer))
		return;

	peer->connection_master = master;
}

static void dhcp_callback(struct connman_ipconfig *ipconfig,
			struct connman_network *network,
			bool success, gpointer data)
{
	struct connman_peer *peer = data;
	int err;

	if (!success)
		goto error;

	DBG("lease acquired for ipconfig %p", ipconfig);

	err = __connman_ipconfig_address_add(ipconfig);
	if (err < 0)
		goto error;

	return;

error:
	__connman_ipconfig_address_remove(ipconfig);
	connman_peer_set_state(peer, CONNMAN_PEER_STATE_FAILURE);
}

static int start_dhcp_client(struct connman_peer *peer)
{
	if (peer->sub_device)
		__connman_ipconfig_set_index(peer->ipconfig,
				connman_device_get_index(peer->sub_device));

	__connman_ipconfig_enable(peer->ipconfig);

	return __connman_dhcp_start(peer->ipconfig, NULL, dhcp_callback, peer);
}

static void report_error_cb(void *user_context, bool retry, void *user_data)
{
	struct connman_peer *peer = user_context;

	if (retry) {
		int err;
		err = peer_connect(peer);

		if (err == 0 || err == -EINPROGRESS)
			return;
	}

	reply_pending(peer, ENOTCONN);

	peer_disconnect(peer);

	if (!peer->connection_master) {
		__connman_dhcp_stop(peer->ipconfig);
		__connman_ipconfig_disable(peer->ipconfig);
	} else
		stop_dhcp_server(peer);

	peer->connection_master = false;
	peer->sub_device = NULL;
}

static int manage_peer_error(struct connman_peer *peer)
{
	int err;

	err = __connman_agent_report_peer_error(peer, peer->path,
					"connect-failed", report_error_cb,
					get_dbus_sender(peer), NULL);
	if (err != -EINPROGRESS) {
		report_error_cb(peer, false, NULL);
		return err;
	}

	return 0;
}

int connman_peer_set_state(struct connman_peer *peer,
					enum connman_peer_state new_state)
{
	enum connman_peer_state old_state = peer->state;
	int err;

	DBG("peer (%s) old state %d new state %d", peer->name,
				old_state, new_state);

	if (old_state == new_state)
		return -EALREADY;

	switch (new_state) {
	case CONNMAN_PEER_STATE_UNKNOWN:
		return -EINVAL;
	case CONNMAN_PEER_STATE_IDLE:
		if (is_connecting(peer) || is_connected(peer))
			return peer_disconnect(peer);
		peer->sub_device = NULL;
		break;
	case CONNMAN_PEER_STATE_ASSOCIATION:
		break;
	case CONNMAN_PEER_STATE_CONFIGURATION:
		if (peer->connection_master)
			err = start_dhcp_server(peer);
		else
			err = start_dhcp_client(peer);
		if (err < 0)
			return connman_peer_set_state(peer,
						CONNMAN_PEER_STATE_FAILURE);
		break;
	case CONNMAN_PEER_STATE_READY:
		reply_pending(peer, 0);
		break;
	case CONNMAN_PEER_STATE_DISCONNECT:
		if (peer->connection_master)
			stop_dhcp_server(peer);
		peer->connection_master = false;
		peer->sub_device = NULL;

		break;
	case CONNMAN_PEER_STATE_FAILURE:
		if (manage_peer_error(peer) == 0)
			return 0;
		break;
	};

	peer->state = new_state;
	state_changed(peer);

	if (peer->state == CONNMAN_PEER_STATE_READY ||
				peer->state == CONNMAN_PEER_STATE_DISCONNECT)
		settings_changed(peer);

	return 0;
}

int connman_peer_request_connection(struct connman_peer *peer)
{
	return __connman_agent_request_peer_authorization(peer,
					request_authorization_cb, false,
					NULL, NULL);
}

static void peer_service_free(gpointer data)
{
	struct _peer_service *service = data;

	if (!service)
		return;

	g_free(service->data);
	g_free(service);
}

void connman_peer_reset_services(struct connman_peer *peer)
{
	if (!peer)
		return;

	g_slist_free_full(peer->services, peer_service_free);
	peer->services = NULL;
}

void connman_peer_services_changed(struct connman_peer *peer)
{
	if (!peer || !peer->registered || !allow_property_changed(peer))
		return;

	connman_dbus_property_changed_array(peer->path,
			CONNMAN_PEER_INTERFACE, "Services",
			DBUS_TYPE_DICT_ENTRY, append_peer_services, peer);
}

void connman_peer_add_service(struct connman_peer *peer,
				enum connman_peer_service_type type,
				const unsigned char *data, int data_length)
{
	struct _peer_service *service;

	if (!peer || !data || type == CONNMAN_PEER_SERVICE_UNKNOWN)
		return;

	service = g_malloc0(sizeof(struct _peer_service));
	service->type = type;
	service->data = g_memdup(data, data_length * sizeof(unsigned char));
	service->length = data_length;

	peer->services = g_slist_prepend(peer->services, service);
}

static void peer_up(struct connman_ipconfig *ipconfig, const char *ifname)
{
	DBG("%s up", ifname);
}

static void peer_down(struct connman_ipconfig *ipconfig, const char *ifname)
{
	DBG("%s down", ifname);
}

static void peer_lower_up(struct connman_ipconfig *ipconfig,
							const char *ifname)
{
	DBG("%s lower up", ifname);
}

static void peer_lower_down(struct connman_ipconfig *ipconfig,
							const char *ifname)
{
	struct connman_peer *peer = __connman_ipconfig_get_data(ipconfig);

	DBG("%s lower down", ifname);

	__connman_ipconfig_disable(ipconfig);
	connman_peer_set_state(peer, CONNMAN_PEER_STATE_DISCONNECT);
}

static void peer_ip_bound(struct connman_ipconfig *ipconfig,
							const char *ifname)
{
	struct connman_peer *peer = __connman_ipconfig_get_data(ipconfig);

	DBG("%s ip bound", ifname);

	if (peer->state == CONNMAN_PEER_STATE_READY)
		settings_changed(peer);
	connman_peer_set_state(peer, CONNMAN_PEER_STATE_READY);
}

static void peer_ip_release(struct connman_ipconfig *ipconfig,
							const char *ifname)
{
	struct connman_peer *peer = __connman_ipconfig_get_data(ipconfig);

	DBG("%s ip release", ifname);

	if (peer->state == CONNMAN_PEER_STATE_READY)
		settings_changed(peer);
}

static const struct connman_ipconfig_ops peer_ip_ops = {
	.up		= peer_up,
	.down		= peer_down,
	.lower_up	= peer_lower_up,
	.lower_down	= peer_lower_down,
	.ip_bound	= peer_ip_bound,
	.ip_release	= peer_ip_release,
	.route_set	= NULL,
	.route_unset	= NULL,
};

static struct connman_ipconfig *create_ipconfig(int index, void *user_data)
{
	struct connman_ipconfig *ipconfig;

	ipconfig = __connman_ipconfig_create(index,
						CONNMAN_IPCONFIG_TYPE_IPV4);
	if (!ipconfig)
		return NULL;

	__connman_ipconfig_set_method(ipconfig, CONNMAN_IPCONFIG_METHOD_DHCP);
	__connman_ipconfig_set_data(ipconfig, user_data);
	__connman_ipconfig_set_ops(ipconfig, &peer_ip_ops);

	return ipconfig;
}

static const GDBusMethodTable peer_methods[] = {
	{ GDBUS_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			get_peer_properties) },
	{ GDBUS_ASYNC_METHOD("Connect", NULL, NULL, connect_peer) },
	{ GDBUS_METHOD("Disconnect", NULL, NULL, disconnect_peer) },
	{ },
};

static const GDBusSignalTable peer_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" })) },
	{ },
};

static char *get_peer_path(struct connman_device *device,
					const char *identifier)
{
	return g_strdup_printf("%s/peer/peer_%s_%s", CONNMAN_PATH,
				connman_device_get_ident(device), identifier);
}

int connman_peer_register(struct connman_peer *peer)
{
	int index;

	DBG("peer %p", peer);

	if (peer->path && peer->registered)
		return -EALREADY;

	index = connman_device_get_index(peer->device);
	peer->ipconfig = create_ipconfig(index, peer);
	if (!peer->ipconfig)
		return -ENOMEM;

	peer->path = get_peer_path(peer->device, peer->identifier);
	DBG("path %s", peer->path);

	g_hash_table_insert(peers_table, peer->path, peer);

	g_dbus_register_interface(connection, peer->path,
					CONNMAN_PEER_INTERFACE,
					peer_methods, peer_signals,
					NULL, peer, NULL);
	peer->registered = true;
	peer_added(peer);

	return 0;
}

void connman_peer_unregister(struct connman_peer *peer)
{
	DBG("peer %p", peer);

	if (!peer->path || !peer->registered)
		return;

	connman_agent_cancel(peer);
	reply_pending(peer, EIO);

	g_dbus_unregister_interface(connection, peer->path,
					CONNMAN_PEER_INTERFACE);
	peer->registered = false;
	peer_removed(peer);
}

struct connman_peer *connman_peer_get(struct connman_device *device,
						const char *identifier)
{
	char *ident = get_peer_path(device, identifier);
	struct connman_peer *peer;

	peer = g_hash_table_lookup(peers_table, ident);
	g_free(ident);

	return peer;
}

int connman_peer_driver_register(struct connman_peer_driver *driver)
{
	if (peer_driver && peer_driver != driver)
		return -EINVAL;

	peer_driver = driver;

	__connman_peer_service_set_driver(driver);

	return 0;
}

void connman_peer_driver_unregister(struct connman_peer_driver *driver)
{
	if (peer_driver != driver)
		return;

	peer_driver = NULL;

	__connman_peer_service_set_driver(NULL);
}

void __connman_peer_list_struct(DBusMessageIter *array)
{
	g_hash_table_foreach(peers_table, append_peer_struct, array);
}

const char *__connman_peer_get_path(struct connman_peer *peer)
{
	if (!peer || !peer->registered)
		return NULL;

	return peer->path;
}

int __connman_peer_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();

	peers_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							NULL, peer_free);

	peers_notify = g_new0(struct _peers_notify, 1);
	peers_notify->add = g_hash_table_new(g_str_hash, g_str_equal);
	peers_notify->remove = g_hash_table_new_full(g_str_hash, g_str_equal,
								g_free, NULL);
	return 0;
}

void __connman_peer_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(peers_notify->remove);
	g_hash_table_destroy(peers_notify->add);
	g_free(peers_notify);

	g_hash_table_destroy(peers_table);
	peers_table = NULL;
	dbus_connection_unref(connection);
	connection = NULL;
}
