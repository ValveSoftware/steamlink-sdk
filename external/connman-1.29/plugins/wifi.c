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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#include <net/ethernet.h>

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP	0x10000
#endif

#include <dbus/dbus.h>
#include <glib.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/inet.h>
#include <connman/device.h>
#include <connman/rtnl.h>
#include <connman/technology.h>
#include <connman/service.h>
#include <connman/peer.h>
#include <connman/log.h>
#include <connman/option.h>
#include <connman/storage.h>
#include <include/setting.h>
#include <connman/provision.h>
#include <connman/utsname.h>
#include <connman/machine.h>

#include <gsupplicant/gsupplicant.h>

#define CLEANUP_TIMEOUT   8	/* in seconds */
#define INACTIVE_TIMEOUT  12	/* in seconds */
#define FAVORITE_MAXIMUM_RETRIES 2

#define BGSCAN_DEFAULT "simple:30:-45:300"
#define AUTOSCAN_DEFAULT "exponential:3:300"

#define P2P_FIND_TIMEOUT 30
#define P2P_CONNECTION_TIMEOUT 100
#define P2P_LISTEN_PERIOD 500
#define P2P_LISTEN_INTERVAL 2000

static struct connman_technology *wifi_technology = NULL;
static struct connman_technology *p2p_technology = NULL;

struct hidden_params {
	char ssid[32];
	unsigned int ssid_len;
	char *identity;
	char *passphrase;
	char *security;
	GSupplicantScanParams *scan_params;
	gpointer user_data;
};

/**
 * Used for autoscan "emulation".
 * Should be removed when wpa_s autoscan support will be by default.
 */
struct autoscan_params {
	int base;
	int limit;
	int interval;
	unsigned int timeout;
};

struct wifi_data {
	char *identifier;
	struct connman_device *device;
	struct connman_network *network;
	struct connman_network *pending_network;
	GSList *networks;
	GSupplicantInterface *interface;
	GSupplicantState state;
	bool connected;
	bool disconnecting;
	bool tethering;
	bool bridged;
	bool interface_ready;
	const char *bridge;
	int index;
	unsigned flags;
	unsigned int watch;
	int retries;
	struct hidden_params *hidden;
	bool postpone_hidden;
	/**
	 * autoscan "emulation".
	 */
	struct autoscan_params *autoscan;

	GSupplicantScanParams *scan_params;
	unsigned int p2p_find_timeout;
	unsigned int p2p_connection_timeout;
	struct connman_peer *pending_peer;
	GSupplicantPeer *peer;
	bool p2p_connecting;
	bool p2p_device;
	int servicing;
};

static GList *iface_list = NULL;

static GList *pending_wifi_device = NULL;
static GList *p2p_iface_list = NULL;
bool wfd_service_registered = false;

static void start_autoscan(struct connman_device *device);

static int p2p_tech_probe(struct connman_technology *technology)
{
	p2p_technology = technology;

	return 0;
}

static void p2p_tech_remove(struct connman_technology *technology)
{
	p2p_technology = NULL;
}

static struct connman_technology_driver p2p_tech_driver = {
	.name		= "p2p",
	.type		= CONNMAN_SERVICE_TYPE_P2P,
	.probe		= p2p_tech_probe,
	.remove		= p2p_tech_remove,
};

static bool is_p2p_connecting(void)
{
	GList *list;

	for (list = iface_list; list; list = list->next) {
		struct wifi_data *wifi = list->data;

		if (wifi->p2p_connecting)
			return true;
	}

	return false;
}

static void add_pending_wifi_device(struct wifi_data *wifi)
{
	if (g_list_find(pending_wifi_device, wifi))
		return;

	pending_wifi_device = g_list_append(pending_wifi_device, wifi);
}

static struct wifi_data *get_pending_wifi_data(const char *ifname)
{
	GList *list;

	for (list = pending_wifi_device; list; list = list->next) {
		struct wifi_data *wifi;
		const char *dev_name;

		wifi = list->data;
		if (!wifi || !wifi->device)
			continue;

		dev_name = connman_device_get_string(wifi->device, "Interface");
		if (!g_strcmp0(ifname, dev_name)) {
			pending_wifi_device = g_list_delete_link(
						pending_wifi_device, list);
			return wifi;
		}
	}

	return NULL;
}

static void remove_pending_wifi_device(struct wifi_data *wifi)
{
	GList *link;

	link = g_list_find(pending_wifi_device, wifi);

	if (!link)
		return;

	pending_wifi_device = g_list_delete_link(pending_wifi_device, link);
}

static void peer_cancel_timeout(struct wifi_data *wifi)
{
	if (wifi->p2p_connection_timeout > 0)
		g_source_remove(wifi->p2p_connection_timeout);

	wifi->p2p_connection_timeout = 0;
	wifi->p2p_connecting = false;

	if (wifi->pending_peer) {
		connman_peer_unref(wifi->pending_peer);
		wifi->pending_peer = NULL;
	}

	wifi->peer = NULL;
}

static gboolean peer_connect_timeout(gpointer data)
{
	struct wifi_data *wifi = data;

	DBG("");

	if (wifi->p2p_connecting) {
		enum connman_peer_state state = CONNMAN_PEER_STATE_FAILURE;

		if (g_supplicant_peer_has_requested_connection(wifi->peer))
			state = CONNMAN_PEER_STATE_IDLE;

		connman_peer_set_state(wifi->pending_peer, state);
	}

	peer_cancel_timeout(wifi);

	return FALSE;
}

static void peer_connect_callback(int result, GSupplicantInterface *interface,
							void *user_data)
{
	struct wifi_data *wifi = user_data;
	struct connman_peer *peer = wifi->pending_peer;

	DBG("peer %p - %d", peer, result);

	if (!peer)
		return;

	if (result < 0) {
		peer_connect_timeout(wifi);
		return;
	}

	connman_peer_set_state(peer, CONNMAN_PEER_STATE_ASSOCIATION);

	wifi->p2p_connection_timeout = g_timeout_add_seconds(
						P2P_CONNECTION_TIMEOUT,
						peer_connect_timeout, wifi);
}

static int peer_connect(struct connman_peer *peer,
			enum connman_peer_wps_method wps_method,
			const char *wps_pin)
{
	struct connman_device *device = connman_peer_get_device(peer);
	GSupplicantPeerParams *peer_params;
	GSupplicantPeer *gs_peer;
	struct wifi_data *wifi;
	bool pbc, pin;
	int ret;

	DBG("peer %p", peer);

	if (!device)
		return -ENODEV;

	wifi = connman_device_get_data(device);
	if (!wifi)
		return -ENODEV;

	if (wifi->p2p_connecting)
		return -EBUSY;

	wifi->peer = NULL;

	gs_peer = g_supplicant_interface_peer_lookup(wifi->interface,
					connman_peer_get_identifier(peer));
	if (!gs_peer)
		return -EINVAL;

	pbc = g_supplicant_peer_is_wps_pbc(gs_peer);
	pin = g_supplicant_peer_is_wps_pin(gs_peer);

	switch (wps_method) {
	case CONNMAN_PEER_WPS_UNKNOWN:
		if ((pbc && pin) || pin)
			return -ENOKEY;
		break;
	case CONNMAN_PEER_WPS_PBC:
		if (!pbc)
			return -EINVAL;
		wps_pin = NULL;
		break;
	case CONNMAN_PEER_WPS_PIN:
		if (!pin || !wps_pin)
			return -EINVAL;
		break;
	}

	peer_params = g_try_malloc0(sizeof(GSupplicantPeerParams));
	if (!peer_params)
		return -ENOMEM;

	peer_params->path = g_strdup(g_supplicant_peer_get_path(gs_peer));
	if (wps_pin)
		peer_params->wps_pin = g_strdup(wps_pin);

	peer_params->master = connman_peer_service_is_master();

	ret = g_supplicant_interface_p2p_connect(wifi->interface, peer_params,
						peer_connect_callback, wifi);
	if (ret == -EINPROGRESS) {
		wifi->pending_peer = connman_peer_ref(peer);
		wifi->peer = gs_peer;
		wifi->p2p_connecting = true;
	} else if (ret < 0)
		g_free(peer_params);

	return ret;
}

static int peer_disconnect(struct connman_peer *peer)
{
	struct connman_device *device = connman_peer_get_device(peer);
	GSupplicantPeerParams peer_params = {};
	GSupplicantPeer *gs_peer;
	struct wifi_data *wifi;
	int ret;

	DBG("peer %p", peer);

	if (!device)
		return -ENODEV;

	wifi = connman_device_get_data(device);
	if (!wifi)
		return -ENODEV;

	gs_peer = g_supplicant_interface_peer_lookup(wifi->interface,
					connman_peer_get_identifier(peer));
	if (!gs_peer)
		return -EINVAL;

	peer_params.path = g_strdup(g_supplicant_peer_get_path(gs_peer));

	ret = g_supplicant_interface_p2p_disconnect(wifi->interface,
							&peer_params);
	g_free(peer_params.path);

	if (ret == -EINPROGRESS)
		peer_cancel_timeout(wifi);

	return ret;
}

struct peer_service_registration {
	peer_service_registration_cb_t callback;
	void *user_data;
};

static bool is_service_wfd(const unsigned char *specs, int length)
{
	if (length < 9 || specs[0] != 0 || specs[1] != 0 || specs[2] != 6)
		return false;

	return true;
}

static void apply_p2p_listen_on_iface(gpointer data, gpointer user_data)
{
	struct wifi_data *wifi = data;

	if (!wifi->interface ||
			!g_supplicant_interface_has_p2p(wifi->interface))
		return;

	if (!wifi->servicing) {
		g_supplicant_interface_p2p_listen(wifi->interface,
				P2P_LISTEN_PERIOD, P2P_LISTEN_INTERVAL);
	}

	wifi->servicing++;
}

static void register_wfd_service_cb(int result,
				GSupplicantInterface *iface, void *user_data)
{
	struct peer_service_registration *reg_data = user_data;

	DBG("");

	if (result == 0)
		g_list_foreach(iface_list, apply_p2p_listen_on_iface, NULL);

	if (reg_data && reg_data->callback) {
		reg_data->callback(result, reg_data->user_data);
		g_free(reg_data);
	}
}

static GSupplicantP2PServiceParams *fill_in_peer_service_params(
				const unsigned char *spec,
				int spec_length, const unsigned char *query,
				int query_length, int version)
{
	GSupplicantP2PServiceParams *params;

	params = g_try_malloc0(sizeof(GSupplicantP2PServiceParams));
	if (!params)
		return NULL;

	if (version > 0) {
		params->version = version;
		params->service = g_memdup(spec, spec_length);
	} else if (query_length > 0 && spec_length > 0) {
		params->query = g_memdup(query, query_length);
		params->query_length = query_length;

		params->response = g_memdup(spec, spec_length);
		params->response_length = spec_length;
	} else {
		params->wfd_ies = g_memdup(spec, spec_length);
		params->wfd_ies_length = spec_length;
	}

	return params;
}

static void free_peer_service_params(GSupplicantP2PServiceParams *params)
{
	if (!params)
		return;

	g_free(params->service);
	g_free(params->query);
	g_free(params->response);
	g_free(params->wfd_ies);

	g_free(params);
}

static int peer_register_wfd_service(const unsigned char *specification,
				int specification_length,
				peer_service_registration_cb_t callback,
				void *user_data)
{
	struct peer_service_registration *reg_data = NULL;
	static GSupplicantP2PServiceParams *params;
	int ret;

	DBG("");

	if (wfd_service_registered)
		return -EBUSY;

	params = fill_in_peer_service_params(specification,
					specification_length, NULL, 0, 0);
	if (!params)
		return -ENOMEM;

	reg_data = g_try_malloc0(sizeof(*reg_data));
	if (!reg_data) {
		ret = -ENOMEM;
		goto error;
	}

	reg_data->callback = callback;
	reg_data->user_data = user_data;

	ret = g_supplicant_set_widi_ies(params,
					register_wfd_service_cb, reg_data);
	if (ret < 0 && ret != -EINPROGRESS)
		goto error;

	wfd_service_registered = true;

	return ret;
error:
	free_peer_service_params(params);
	g_free(reg_data);

	return ret;
}

static void register_peer_service_cb(int result,
				GSupplicantInterface *iface, void *user_data)
{
	struct wifi_data *wifi = g_supplicant_interface_get_data(iface);
	struct peer_service_registration *reg_data = user_data;

	DBG("");

	if (result == 0)
		apply_p2p_listen_on_iface(wifi, NULL);

	if (reg_data->callback)
		reg_data->callback(result, reg_data->user_data);

	g_free(reg_data);
}

static int peer_register_service(const unsigned char *specification,
				int specification_length,
				const unsigned char *query,
				int query_length, int version,
				peer_service_registration_cb_t callback,
				void *user_data)
{
	struct peer_service_registration *reg_data;
	GSupplicantP2PServiceParams *params;
	bool found = false;
	int ret, ret_f;
	GList *list;

	DBG("");

	if (specification && !version && !query &&
			is_service_wfd(specification, specification_length)) {
		return peer_register_wfd_service(specification,
				specification_length, callback, user_data);
	}

	reg_data = g_try_malloc0(sizeof(*reg_data));
	if (!reg_data)
		return -ENOMEM;

	reg_data->callback = callback;
	reg_data->user_data = user_data;

	ret_f = -EOPNOTSUPP;

	for (list = iface_list; list; list = list->next) {
		struct wifi_data *wifi = list->data;
		GSupplicantInterface *iface = wifi->interface;

		if (!g_supplicant_interface_has_p2p(iface))
			continue;

		params = fill_in_peer_service_params(specification,
						specification_length, query,
						query_length, version);
		if (!params) {
			ret = -ENOMEM;
			continue;
		}

		if (!found) {
			ret_f = g_supplicant_interface_p2p_add_service(iface,
				register_peer_service_cb, params, reg_data);
			if (ret_f == 0 || ret_f == -EINPROGRESS)
				found = true;
			ret = ret_f;
		} else
			ret = g_supplicant_interface_p2p_add_service(iface,
				register_peer_service_cb, params, NULL);
		if (ret != 0 && ret != -EINPROGRESS)
			free_peer_service_params(params);
	}

	if (ret_f != 0 && ret_f != -EINPROGRESS)
		g_free(reg_data);

	return ret_f;
}

static int peer_unregister_wfd_service(void)
{
	GSupplicantP2PServiceParams *params;
	GList *list;

	if (!wfd_service_registered)
		return -EALREADY;

	params = fill_in_peer_service_params(NULL, 0, NULL, 0, 0);
	if (!params)
		return -ENOMEM;

	wfd_service_registered = false;

	g_supplicant_set_widi_ies(params, NULL, NULL);

	for (list = iface_list; list; list = list->next) {
		struct wifi_data *wifi = list->data;

		if (!g_supplicant_interface_has_p2p(wifi->interface))
			continue;

		wifi->servicing--;
		if (!wifi->servicing || wifi->servicing < 0) {
			g_supplicant_interface_p2p_listen(wifi->interface,
									0, 0);
			wifi->servicing = 0;
		}
	}

	return 0;
}

static int peer_unregister_service(const unsigned char *specification,
						int specification_length,
						const unsigned char *query,
						int query_length, int version)
{
	GSupplicantP2PServiceParams *params;
	bool wfd = false;
	GList *list;
	int ret;

	if (specification && !version && !query &&
			is_service_wfd(specification, specification_length)) {
		ret = peer_unregister_wfd_service();
		if (ret != 0 && ret != -EINPROGRESS)
			return ret;
		wfd = true;
	}

	for (list = iface_list; list; list = list->next) {
		struct wifi_data *wifi = list->data;
		GSupplicantInterface *iface = wifi->interface;

		if (wfd)
			goto stop_listening;

		if (!g_supplicant_interface_has_p2p(iface))
			continue;

		params = fill_in_peer_service_params(specification,
						specification_length, query,
						query_length, version);
		if (!params) {
			ret = -ENOMEM;
			continue;
		}

		ret = g_supplicant_interface_p2p_del_service(iface, params);
		if (ret != 0 && ret != -EINPROGRESS)
			free_peer_service_params(params);
stop_listening:
		wifi->servicing--;
		if (!wifi->servicing || wifi->servicing < 0) {
			g_supplicant_interface_p2p_listen(iface, 0, 0);
			wifi->servicing = 0;
		}
	}

	return 0;
}

static struct connman_peer_driver peer_driver = {
	.connect    = peer_connect,
	.disconnect = peer_disconnect,
	.register_service = peer_register_service,
	.unregister_service = peer_unregister_service,
};

static void handle_tethering(struct wifi_data *wifi)
{
	if (!wifi->tethering)
		return;

	if (!wifi->bridge)
		return;

	if (wifi->bridged)
		return;

	DBG("index %d bridge %s", wifi->index, wifi->bridge);

	if (connman_inet_add_to_bridge(wifi->index, wifi->bridge) < 0)
		return;

	wifi->bridged = true;
}

static void wifi_newlink(unsigned flags, unsigned change, void *user_data)
{
	struct connman_device *device = user_data;
	struct wifi_data *wifi = connman_device_get_data(device);

	if (!wifi)
		return;

	DBG("index %d flags %d change %d", wifi->index, flags, change);

	if ((wifi->flags & IFF_UP) != (flags & IFF_UP)) {
		if (flags & IFF_UP)
			DBG("interface up");
		else
			DBG("interface down");
	}

	if ((wifi->flags & IFF_LOWER_UP) != (flags & IFF_LOWER_UP)) {
		if (flags & IFF_LOWER_UP) {
			DBG("carrier on");

			handle_tethering(wifi);
		} else
			DBG("carrier off");
	}

	wifi->flags = flags;
}

static int wifi_probe(struct connman_device *device)
{
	struct wifi_data *wifi;

	DBG("device %p", device);

	wifi = g_try_new0(struct wifi_data, 1);
	if (!wifi)
		return -ENOMEM;

	wifi->state = G_SUPPLICANT_STATE_INACTIVE;

	connman_device_set_data(device, wifi);
	wifi->device = connman_device_ref(device);

	wifi->index = connman_device_get_index(device);
	wifi->flags = 0;

	wifi->watch = connman_rtnl_add_newlink_watch(wifi->index,
							wifi_newlink, device);
	if (is_p2p_connecting())
		add_pending_wifi_device(wifi);
	else
		iface_list = g_list_append(iface_list, wifi);

	return 0;
}

static void remove_networks(struct connman_device *device,
				struct wifi_data *wifi)
{
	GSList *list;

	for (list = wifi->networks; list; list = list->next) {
		struct connman_network *network = list->data;

		connman_device_remove_network(device, network);
		connman_network_unref(network);
	}

	g_slist_free(wifi->networks);
	wifi->networks = NULL;
}

static void reset_autoscan(struct connman_device *device)
{
	struct wifi_data *wifi = connman_device_get_data(device);
	struct autoscan_params *autoscan;

	DBG("");

	if (!wifi || !wifi->autoscan)
		return;

	autoscan = wifi->autoscan;

	if (autoscan->timeout == 0 && autoscan->interval == 0)
		return;

	g_source_remove(autoscan->timeout);

	autoscan->timeout = 0;
	autoscan->interval = 0;

	connman_device_unref(device);
}

static void stop_autoscan(struct connman_device *device)
{
	const struct wifi_data *wifi = connman_device_get_data(device);

	if (!wifi || !wifi->autoscan)
		return;

	reset_autoscan(device);

	connman_device_set_scanning(device, CONNMAN_SERVICE_TYPE_WIFI, false);
}

static void check_p2p_technology(void)
{
	bool p2p_exists = false;
	GList *list;

	for (list = iface_list; list; list = list->next) {
		struct wifi_data *w = list->data;

		if (w->interface &&
				g_supplicant_interface_has_p2p(w->interface))
			p2p_exists = true;
	}

	if (!p2p_exists) {
		connman_technology_driver_unregister(&p2p_tech_driver);
		connman_peer_driver_unregister(&peer_driver);
	}
}

static void wifi_remove(struct connman_device *device)
{
	struct wifi_data *wifi = connman_device_get_data(device);

	DBG("device %p wifi %p", device, wifi);

	if (!wifi)
		return;

	stop_autoscan(device);

	if (wifi->p2p_device)
		p2p_iface_list = g_list_remove(p2p_iface_list, wifi);
	else
		iface_list = g_list_remove(iface_list, wifi);

	check_p2p_technology();

	remove_pending_wifi_device(wifi);

	if (wifi->p2p_find_timeout) {
		g_source_remove(wifi->p2p_find_timeout);
		connman_device_unref(wifi->device);
	}

	if (wifi->p2p_connection_timeout)
		g_source_remove(wifi->p2p_connection_timeout);

	remove_networks(device, wifi);

	connman_device_set_powered(device, false);
	connman_device_set_data(device, NULL);
	connman_device_unref(wifi->device);
	connman_rtnl_remove_watch(wifi->watch);

	g_supplicant_interface_set_data(wifi->interface, NULL);

	g_supplicant_interface_cancel(wifi->interface);

	if (wifi->scan_params)
		g_supplicant_free_scan_params(wifi->scan_params);

	g_free(wifi->autoscan);
	g_free(wifi->identifier);
	g_free(wifi);
}

static bool is_duplicate(GSList *list, gchar *ssid, int ssid_len)
{
	GSList *iter;

	for (iter = list; iter; iter = g_slist_next(iter)) {
		struct scan_ssid *scan_ssid = iter->data;

		if (ssid_len == scan_ssid->ssid_len &&
				memcmp(ssid, scan_ssid->ssid, ssid_len) == 0)
			return true;
	}

	return false;
}

static int add_scan_param(gchar *hex_ssid, char *raw_ssid, int ssid_len,
			int freq, GSupplicantScanParams *scan_data,
			int driver_max_scan_ssids, char *ssid_name)
{
	unsigned int i;
	struct scan_ssid *scan_ssid;

	if ((driver_max_scan_ssids == 0 ||
			driver_max_scan_ssids > scan_data->num_ssids) &&
			(hex_ssid || raw_ssid)) {
		gchar *ssid;
		unsigned int j = 0, hex;

		if (hex_ssid) {
			size_t hex_ssid_len = strlen(hex_ssid);

			ssid = g_try_malloc0(hex_ssid_len / 2);
			if (!ssid)
				return -ENOMEM;

			for (i = 0; i < hex_ssid_len; i += 2) {
				sscanf(hex_ssid + i, "%02x", &hex);
				ssid[j++] = hex;
			}
		} else {
			ssid = raw_ssid;
			j = ssid_len;
		}

		/*
		 * If we have already added hidden AP to the list,
		 * then do not do it again. This might happen if you have
		 * used or are using multiple wifi cards, so in that case
		 * you might have multiple service files for same AP.
		 */
		if (is_duplicate(scan_data->ssids, ssid, j)) {
			if (hex_ssid)
				g_free(ssid);
			return 0;
		}

		scan_ssid = g_try_new(struct scan_ssid, 1);
		if (!scan_ssid) {
			if (hex_ssid)
				g_free(ssid);
			return -ENOMEM;
		}

		memcpy(scan_ssid->ssid, ssid, j);
		scan_ssid->ssid_len = j;
		scan_data->ssids = g_slist_prepend(scan_data->ssids,
								scan_ssid);

		scan_data->num_ssids++;

		DBG("SSID %s added to scanned list of %d entries", ssid_name,
							scan_data->num_ssids);

		if (hex_ssid)
			g_free(ssid);
	} else
		return -EINVAL;

	scan_data->ssids = g_slist_reverse(scan_data->ssids);

	if (!scan_data->freqs) {
		scan_data->freqs = g_try_malloc0(sizeof(uint16_t));
		if (!scan_data->freqs) {
			g_slist_free_full(scan_data->ssids, g_free);
			return -ENOMEM;
		}

		scan_data->num_freqs = 1;
		scan_data->freqs[0] = freq;
	} else {
		bool duplicate = false;

		/* Don't add duplicate entries */
		for (i = 0; i < scan_data->num_freqs; i++) {
			if (scan_data->freqs[i] == freq) {
				duplicate = true;
				break;
			}
		}

		if (!duplicate) {
			scan_data->num_freqs++;
			scan_data->freqs = g_try_realloc(scan_data->freqs,
				sizeof(uint16_t) * scan_data->num_freqs);
			if (!scan_data->freqs) {
				g_slist_free_full(scan_data->ssids, g_free);
				return -ENOMEM;
			}
			scan_data->freqs[scan_data->num_freqs - 1] = freq;
		}
	}

	return 1;
}

static int get_hidden_connections(GSupplicantScanParams *scan_data)
{
	struct connman_config_entry **entries;
	GKeyFile *keyfile;
	gchar **services;
	char *ssid, *name;
	int i, ret;
	bool value;
	int num_ssids = 0, add_param_failed = 0;

	services = connman_storage_get_services();
	for (i = 0; services && services[i]; i++) {
		if (strncmp(services[i], "wifi_", 5) != 0)
			continue;

		keyfile = connman_storage_load_service(services[i]);
		if (!keyfile)
			continue;

		value = g_key_file_get_boolean(keyfile,
					services[i], "Hidden", NULL);
		if (!value) {
			g_key_file_free(keyfile);
			continue;
		}

		value = g_key_file_get_boolean(keyfile,
					services[i], "Favorite", NULL);
		if (!value) {
			g_key_file_free(keyfile);
			continue;
		}

		ssid = g_key_file_get_string(keyfile,
					services[i], "SSID", NULL);

		name = g_key_file_get_string(keyfile, services[i], "Name",
								NULL);

		ret = add_scan_param(ssid, NULL, 0, 0, scan_data, 0, name);
		if (ret < 0)
			add_param_failed++;
		else if (ret > 0)
			num_ssids++;

		g_free(ssid);
		g_free(name);
		g_key_file_free(keyfile);
	}

	/*
	 * Check if there are any hidden AP that needs to be provisioned.
	 */
	entries = connman_config_get_entries("wifi");
	for (i = 0; entries && entries[i]; i++) {
		int len;

		if (!entries[i]->hidden)
			continue;

		if (!entries[i]->ssid) {
			ssid = entries[i]->name;
			len = strlen(ssid);
		} else {
			ssid = entries[i]->ssid;
			len = entries[i]->ssid_len;
		}

		if (!ssid)
			continue;

		ret = add_scan_param(NULL, ssid, len, 0, scan_data, 0, ssid);
		if (ret < 0)
			add_param_failed++;
		else if (ret > 0)
			num_ssids++;
	}

	connman_config_free_entries(entries);

	if (add_param_failed > 0)
		DBG("Unable to scan %d out of %d SSIDs",
					add_param_failed, num_ssids);

	g_strfreev(services);

	return num_ssids;
}

static int get_hidden_connections_params(struct wifi_data *wifi,
					GSupplicantScanParams *scan_params)
{
	int driver_max_ssids, i;
	GSupplicantScanParams *orig_params;

	/*
	 * Scan hidden networks so that we can autoconnect to them.
	 * We will assume 1 as a default number of ssid to scan.
	 */
	driver_max_ssids = g_supplicant_interface_get_max_scan_ssids(
							wifi->interface);
	if (driver_max_ssids == 0)
		driver_max_ssids = 1;

	DBG("max ssids %d", driver_max_ssids);

	if (!wifi->scan_params) {
		wifi->scan_params = g_try_malloc0(sizeof(GSupplicantScanParams));
		if (!wifi->scan_params)
			return 0;

		if (get_hidden_connections(wifi->scan_params) == 0) {
			g_supplicant_free_scan_params(wifi->scan_params);
			wifi->scan_params = NULL;

			return 0;
		}
	}

	orig_params = wifi->scan_params;

	/* Let's transfer driver_max_ssids params */
	for (i = 0; i < driver_max_ssids; i++) {
		struct scan_ssid *ssid;

		if (!wifi->scan_params->ssids)
			break;

		ssid = orig_params->ssids->data;
		orig_params->ssids = g_slist_remove(orig_params->ssids, ssid);
		scan_params->ssids = g_slist_prepend(scan_params->ssids, ssid);
	}

	if (i > 0) {
		scan_params->num_ssids = i;
		scan_params->ssids = g_slist_reverse(scan_params->ssids);

		scan_params->freqs = g_memdup(orig_params->freqs,
				sizeof(uint16_t) * orig_params->num_freqs);
		if (!scan_params->freqs)
			goto err;

		scan_params->num_freqs = orig_params->num_freqs;

	} else
		goto err;

	orig_params->num_ssids -= scan_params->num_ssids;

	return scan_params->num_ssids;

err:
	g_slist_free_full(scan_params->ssids, g_free);
	g_supplicant_free_scan_params(wifi->scan_params);
	wifi->scan_params = NULL;

	return 0;
}

static int throw_wifi_scan(struct connman_device *device,
			GSupplicantInterfaceCallback callback)
{
	struct wifi_data *wifi = connman_device_get_data(device);
	int ret;

	if (!wifi)
		return -ENODEV;

	DBG("device %p %p", device, wifi->interface);

	if (wifi->tethering)
		return -EBUSY;

	if (connman_device_get_scanning(device))
		return -EALREADY;

	connman_device_ref(device);

	ret = g_supplicant_interface_scan(wifi->interface, NULL,
						callback, device);
	if (ret == 0) {
		connman_device_set_scanning(device,
				CONNMAN_SERVICE_TYPE_WIFI, true);
	} else
		connman_device_unref(device);

	return ret;
}

static void hidden_free(struct hidden_params *hidden)
{
	if (!hidden)
		return;

	if (hidden->scan_params)
		g_supplicant_free_scan_params(hidden->scan_params);
	g_free(hidden->identity);
	g_free(hidden->passphrase);
	g_free(hidden->security);
	g_free(hidden);
}

static void scan_callback(int result, GSupplicantInterface *interface,
						void *user_data)
{
	struct connman_device *device = user_data;
	struct wifi_data *wifi = connman_device_get_data(device);
	bool scanning;

	DBG("result %d wifi %p", result, wifi);

	if (wifi) {
		if (wifi->hidden && !wifi->postpone_hidden) {
			connman_network_clear_hidden(wifi->hidden->user_data);
			hidden_free(wifi->hidden);
			wifi->hidden = NULL;
		}

		if (wifi->scan_params) {
			g_supplicant_free_scan_params(wifi->scan_params);
			wifi->scan_params = NULL;
		}
	}

	if (result < 0)
		connman_device_reset_scanning(device);

	/* User is connecting to a hidden AP, let's wait for finished event */
	if (wifi && wifi->hidden && wifi->postpone_hidden) {
		GSupplicantScanParams *scan_params;
		int ret;

		wifi->postpone_hidden = false;
		scan_params = wifi->hidden->scan_params;
		wifi->hidden->scan_params = NULL;

		reset_autoscan(device);

		ret = g_supplicant_interface_scan(wifi->interface, scan_params,
							scan_callback, device);
		if (ret == 0)
			return;

		/* On error, let's recall scan_callback, which will cleanup */
		return scan_callback(ret, interface, user_data);
	}

	scanning = connman_device_get_scanning(device);

	if (scanning) {
		connman_device_set_scanning(device,
				CONNMAN_SERVICE_TYPE_WIFI, false);
	}

	if (result != -ENOLINK)
		start_autoscan(device);

	/*
	 * If we are here then we were scanning; however, if we are
	 * also mid-flight disabling the interface, then wifi_disable
	 * has already cleared the device scanning state and
	 * unreferenced the device, obviating the need to do it here.
	 */

	if (scanning)
		connman_device_unref(device);
}

static void scan_callback_hidden(int result,
			GSupplicantInterface *interface, void *user_data)
{
	struct connman_device *device = user_data;
	struct wifi_data *wifi = connman_device_get_data(device);
	GSupplicantScanParams *scan_params;
	int ret;

	DBG("result %d wifi %p", result, wifi);

	if (!wifi)
		goto out;

	/* User is trying to connect to a hidden AP */
	if (wifi->hidden && wifi->postpone_hidden)
		goto out;

	scan_params = g_try_malloc0(sizeof(GSupplicantScanParams));
	if (!scan_params)
		goto out;

	if (get_hidden_connections_params(wifi, scan_params) > 0) {
		ret = g_supplicant_interface_scan(wifi->interface,
							scan_params,
							scan_callback_hidden,
							device);
		if (ret == 0)
			return;
	}

	g_supplicant_free_scan_params(scan_params);

out:
	scan_callback(result, interface, user_data);
}

static gboolean autoscan_timeout(gpointer data)
{
	struct connman_device *device = data;
	struct wifi_data *wifi = connman_device_get_data(device);
	struct autoscan_params *autoscan;
	int interval;

	if (!wifi)
		return FALSE;

	autoscan = wifi->autoscan;

	if (autoscan->interval <= 0) {
		interval = autoscan->base;
		goto set_interval;
	} else
		interval = autoscan->interval * autoscan->base;

	if (interval > autoscan->limit)
		interval = autoscan->limit;

	throw_wifi_scan(wifi->device, scan_callback_hidden);

set_interval:
	DBG("interval %d", interval);

	autoscan->interval = interval;

	autoscan->timeout = g_timeout_add_seconds(interval,
						autoscan_timeout, device);

	return FALSE;
}

static void start_autoscan(struct connman_device *device)
{
	struct wifi_data *wifi = connman_device_get_data(device);
	struct autoscan_params *autoscan;

	DBG("");

	if (!wifi)
		return;

	if (wifi->p2p_device)
		return;

	if (wifi->connected)
		return;

	autoscan = wifi->autoscan;
	if (!autoscan)
		return;

	if (autoscan->timeout > 0 || autoscan->interval > 0)
		return;

	connman_device_ref(device);

	autoscan_timeout(device);
}

static struct autoscan_params *parse_autoscan_params(const char *params)
{
	struct autoscan_params *autoscan;
	char **list_params;
	int limit;
	int base;

	DBG("Emulating autoscan");

	list_params = g_strsplit(params, ":", 0);
	if (list_params == 0)
		return NULL;

	if (g_strv_length(list_params) < 3) {
		g_strfreev(list_params);
		return NULL;
	}

	base = atoi(list_params[1]);
	limit = atoi(list_params[2]);

	g_strfreev(list_params);

	autoscan = g_try_malloc0(sizeof(struct autoscan_params));
	if (!autoscan) {
		DBG("Could not allocate memory for autoscan");
		return NULL;
	}

	DBG("base %d - limit %d", base, limit);
	autoscan->base = base;
	autoscan->limit = limit;

	return autoscan;
}

static void setup_autoscan(struct wifi_data *wifi)
{
	if (!wifi->autoscan)
		wifi->autoscan = parse_autoscan_params(AUTOSCAN_DEFAULT);

	start_autoscan(wifi->device);
}

static void finalize_interface_creation(struct wifi_data *wifi)
{
	DBG("interface is ready wifi %p tethering %d", wifi, wifi->tethering);

	if (!wifi->device) {
		connman_error("WiFi device not set");
		return;
	}

	connman_device_set_powered(wifi->device, true);

	if (!connman_setting_get_bool("BackgroundScanning"))
		return;

	if (wifi->p2p_device)
		return;

	setup_autoscan(wifi);
}

static void interface_create_callback(int result,
					GSupplicantInterface *interface,
							void *user_data)
{
	struct wifi_data *wifi = user_data;

	DBG("result %d ifname %s, wifi %p", result,
				g_supplicant_interface_get_ifname(interface),
				wifi);

	if (result < 0 || !wifi)
		return;

	wifi->interface = interface;
	g_supplicant_interface_set_data(interface, wifi);

	if (g_supplicant_interface_get_ready(interface)) {
		wifi->interface_ready = true;
		finalize_interface_creation(wifi);
	}
}

static int wifi_enable(struct connman_device *device)
{
	struct wifi_data *wifi = connman_device_get_data(device);
	int index;
	char *interface;
	const char *driver = connman_option_get_string("wifi");
	int ret;

	DBG("device %p %p", device, wifi);

	index = connman_device_get_index(device);
	if (!wifi || index < 0)
		return -ENODEV;

	if (is_p2p_connecting())
		return -EINPROGRESS;

	interface = connman_inet_ifname(index);
	ret = g_supplicant_interface_create(interface, driver, NULL,
						interface_create_callback,
							wifi);
	g_free(interface);

	if (ret < 0)
		return ret;

	return -EINPROGRESS;
}

static int wifi_disable(struct connman_device *device)
{
	struct wifi_data *wifi = connman_device_get_data(device);
	int ret;

	DBG("device %p wifi %p", device, wifi);

	if (!wifi)
		return -ENODEV;

	wifi->connected = false;
	wifi->disconnecting = false;

	if (wifi->pending_network)
		wifi->pending_network = NULL;

	stop_autoscan(device);

	if (wifi->p2p_find_timeout) {
		g_source_remove(wifi->p2p_find_timeout);
		wifi->p2p_find_timeout = 0;
		connman_device_set_scanning(device, CONNMAN_SERVICE_TYPE_P2P, false);
		connman_device_unref(wifi->device);
	}

	/* In case of a user scan, device is still referenced */
	if (connman_device_get_scanning(device)) {
		connman_device_set_scanning(device,
				CONNMAN_SERVICE_TYPE_WIFI, false);
		connman_device_unref(wifi->device);
	}

	remove_networks(device, wifi);

	ret = g_supplicant_interface_remove(wifi->interface, NULL, NULL);
	if (ret < 0)
		return ret;

	return -EINPROGRESS;
}

struct last_connected {
	GTimeVal modified;
	gchar *ssid;
	int freq;
};

static gint sort_entry(gconstpointer a, gconstpointer b, gpointer user_data)
{
	GTimeVal *aval = (GTimeVal *)a;
	GTimeVal *bval = (GTimeVal *)b;

	/* Note that the sort order is descending */
	if (aval->tv_sec < bval->tv_sec)
		return 1;

	if (aval->tv_sec > bval->tv_sec)
		return -1;

	return 0;
}

static void free_entry(gpointer data)
{
	struct last_connected *entry = data;

	g_free(entry->ssid);
	g_free(entry);
}

static int get_latest_connections(int max_ssids,
				GSupplicantScanParams *scan_data)
{
	GSequenceIter *iter;
	GSequence *latest_list;
	struct last_connected *entry;
	GKeyFile *keyfile;
	GTimeVal modified;
	gchar **services;
	gchar *str;
	char *ssid;
	int i, freq;
	int num_ssids = 0;

	latest_list = g_sequence_new(free_entry);
	if (!latest_list)
		return -ENOMEM;

	services = connman_storage_get_services();
	for (i = 0; services && services[i]; i++) {
		if (strncmp(services[i], "wifi_", 5) != 0)
			continue;

		keyfile = connman_storage_load_service(services[i]);
		if (!keyfile)
			continue;

		str = g_key_file_get_string(keyfile,
					services[i], "Favorite", NULL);
		if (!str || g_strcmp0(str, "true")) {
			g_free(str);
			g_key_file_free(keyfile);
			continue;
		}
		g_free(str);

		str = g_key_file_get_string(keyfile,
					services[i], "AutoConnect", NULL);
		if (!str || g_strcmp0(str, "true")) {
			g_free(str);
			g_key_file_free(keyfile);
			continue;
		}
		g_free(str);

		str = g_key_file_get_string(keyfile,
					services[i], "Modified", NULL);
		if (!str) {
			g_key_file_free(keyfile);
			continue;
		}
		g_time_val_from_iso8601(str, &modified);
		g_free(str);

		ssid = g_key_file_get_string(keyfile,
					services[i], "SSID", NULL);

		freq = g_key_file_get_integer(keyfile, services[i],
					"Frequency", NULL);
		if (freq) {
			entry = g_try_new(struct last_connected, 1);
			if (!entry) {
				g_sequence_free(latest_list);
				g_key_file_free(keyfile);
				g_free(ssid);
				return -ENOMEM;
			}

			entry->ssid = ssid;
			entry->modified = modified;
			entry->freq = freq;

			g_sequence_insert_sorted(latest_list, entry,
						sort_entry, NULL);
			num_ssids++;
		} else
			g_free(ssid);

		g_key_file_free(keyfile);
	}

	g_strfreev(services);

	num_ssids = num_ssids > max_ssids ? max_ssids : num_ssids;

	iter = g_sequence_get_begin_iter(latest_list);

	for (i = 0; i < num_ssids; i++) {
		entry = g_sequence_get(iter);

		DBG("ssid %s freq %d modified %lu", entry->ssid, entry->freq,
						entry->modified.tv_sec);

		add_scan_param(entry->ssid, NULL, 0, entry->freq, scan_data,
						max_ssids, entry->ssid);

		iter = g_sequence_iter_next(iter);
	}

	g_sequence_free(latest_list);
	return num_ssids;
}

static int wifi_scan_simple(struct connman_device *device)
{
	reset_autoscan(device);

	return throw_wifi_scan(device, scan_callback_hidden);
}

static gboolean p2p_find_stop(gpointer data)
{
	struct connman_device *device = data;
	struct wifi_data *wifi = connman_device_get_data(device);

	DBG("");

	wifi->p2p_find_timeout = 0;

	connman_device_set_scanning(device, CONNMAN_SERVICE_TYPE_P2P, false);

	g_supplicant_interface_p2p_stop_find(wifi->interface);

	connman_device_unref(device);
	reset_autoscan(device);

	return FALSE;
}

static void p2p_find_callback(int result, GSupplicantInterface *interface,
							void *user_data)
{
	struct connman_device *device = user_data;
	struct wifi_data *wifi = connman_device_get_data(device);

	DBG("result %d wifi %p", result, wifi);

	if (wifi->p2p_find_timeout) {
		g_source_remove(wifi->p2p_find_timeout);
		wifi->p2p_find_timeout = 0;
	}

	if (result)
		goto error;

	wifi->p2p_find_timeout = g_timeout_add_seconds(P2P_FIND_TIMEOUT,
							p2p_find_stop, device);
	if (!wifi->p2p_find_timeout)
		goto error;

	return;
error:
	p2p_find_stop(device);
}

static int p2p_find(struct connman_device *device)
{
	struct wifi_data *wifi;
	int ret;

	DBG("");

	if (!p2p_technology)
		return -ENOTSUP;

	wifi = connman_device_get_data(device);

	if (g_supplicant_interface_is_p2p_finding(wifi->interface))
		return -EALREADY;

	reset_autoscan(device);
	connman_device_ref(device);

	ret = g_supplicant_interface_p2p_find(wifi->interface,
						p2p_find_callback, device);
	if (ret) {
		connman_device_unref(device);
		start_autoscan(device);
	} else {
		connman_device_set_scanning(device,
				CONNMAN_SERVICE_TYPE_P2P, true);
	}

	return ret;
}

/*
 * Note that the hidden scan is only used when connecting to this specific
 * hidden AP first time. It is not used when system autoconnects to hidden AP.
 */
static int wifi_scan(enum connman_service_type type,
			struct connman_device *device,
			const char *ssid, unsigned int ssid_len,
			const char *identity, const char* passphrase,
			const char *security, void *user_data)
{
	struct wifi_data *wifi = connman_device_get_data(device);
	GSupplicantScanParams *scan_params = NULL;
	struct scan_ssid *scan_ssid;
	struct hidden_params *hidden;
	int ret;
	int driver_max_ssids = 0;
	bool do_hidden;
	bool scanning;

	if (!wifi)
		return -ENODEV;

	if (wifi->p2p_device)
		return 0;

	if (type == CONNMAN_SERVICE_TYPE_P2P)
		return p2p_find(device);

	DBG("device %p wifi %p hidden ssid %s", device, wifi->interface, ssid);

	if (wifi->tethering)
		return 0;

	scanning = connman_device_get_scanning(device);

	if (!ssid || ssid_len == 0 || ssid_len > 32) {
		if (scanning)
			return -EALREADY;

		driver_max_ssids = g_supplicant_interface_get_max_scan_ssids(
							wifi->interface);
		DBG("max ssids %d", driver_max_ssids);
		if (driver_max_ssids == 0)
			return wifi_scan_simple(device);

		do_hidden = false;
	} else {
		if (scanning && wifi->hidden && wifi->postpone_hidden)
			return -EALREADY;

		do_hidden = true;
	}

	scan_params = g_try_malloc0(sizeof(GSupplicantScanParams));
	if (!scan_params)
		return -ENOMEM;

	if (do_hidden) {
		scan_ssid = g_try_new(struct scan_ssid, 1);
		if (!scan_ssid) {
			g_free(scan_params);
			return -ENOMEM;
		}

		memcpy(scan_ssid->ssid, ssid, ssid_len);
		scan_ssid->ssid_len = ssid_len;
		scan_params->ssids = g_slist_prepend(scan_params->ssids,
								scan_ssid);
		scan_params->num_ssids = 1;

		hidden = g_try_new0(struct hidden_params, 1);
		if (!hidden) {
			g_supplicant_free_scan_params(scan_params);
			return -ENOMEM;
		}

		if (wifi->hidden) {
			hidden_free(wifi->hidden);
			wifi->hidden = NULL;
		}

		memcpy(hidden->ssid, ssid, ssid_len);
		hidden->ssid_len = ssid_len;
		hidden->identity = g_strdup(identity);
		hidden->passphrase = g_strdup(passphrase);
		hidden->security = g_strdup(security);
		hidden->user_data = user_data;
		wifi->hidden = hidden;

		if (scanning) {
			/* Let's keep this active scan for later,
			 * when current scan will be over. */
			wifi->postpone_hidden = TRUE;
			hidden->scan_params = scan_params;

			return 0;
		}
	} else if (wifi->connected) {
		g_supplicant_free_scan_params(scan_params);
		return wifi_scan_simple(device);
	} else {
		ret = get_latest_connections(driver_max_ssids, scan_params);
		if (ret <= 0) {
			g_supplicant_free_scan_params(scan_params);
			return wifi_scan_simple(device);
		}
	}

	connman_device_ref(device);

	reset_autoscan(device);

	ret = g_supplicant_interface_scan(wifi->interface, scan_params,
						scan_callback, device);
	if (ret == 0) {
		connman_device_set_scanning(device,
				CONNMAN_SERVICE_TYPE_WIFI, true);
	} else {
		g_supplicant_free_scan_params(scan_params);
		connman_device_unref(device);

		if (do_hidden) {
			hidden_free(wifi->hidden);
			wifi->hidden = NULL;
		}
	}

	return ret;
}

static void wifi_regdom_callback(int result,
					const char *alpha2,
						void *user_data)
{
	struct connman_device *device = user_data;

	connman_device_regdom_notify(device, result, alpha2);

	connman_device_unref(device);
}

static int wifi_set_regdom(struct connman_device *device, const char *alpha2)
{
	struct wifi_data *wifi = connman_device_get_data(device);
	int ret;

	if (!wifi)
		return -EINVAL;

	connman_device_ref(device);

	ret = g_supplicant_interface_set_country(wifi->interface,
						wifi_regdom_callback,
							alpha2, device);
	if (ret != 0)
		connman_device_unref(device);

	return ret;
}

static struct connman_device_driver wifi_ng_driver = {
	.name		= "wifi",
	.type		= CONNMAN_DEVICE_TYPE_WIFI,
	.priority	= CONNMAN_DEVICE_PRIORITY_LOW,
	.probe		= wifi_probe,
	.remove		= wifi_remove,
	.enable		= wifi_enable,
	.disable	= wifi_disable,
	.scan		= wifi_scan,
	.set_regdom	= wifi_set_regdom,
};

static void system_ready(void)
{
	DBG("");

	if (connman_device_driver_register(&wifi_ng_driver) < 0)
		connman_error("Failed to register WiFi driver");
}

static void system_killed(void)
{
	DBG("");

	connman_device_driver_unregister(&wifi_ng_driver);
}

static int network_probe(struct connman_network *network)
{
	DBG("network %p", network);

	return 0;
}

static void network_remove(struct connman_network *network)
{
	struct connman_device *device = connman_network_get_device(network);
	struct wifi_data *wifi;

	DBG("network %p", network);

	wifi = connman_device_get_data(device);
	if (!wifi)
		return;

	if (wifi->network != network)
		return;

	wifi->network = NULL;
}

static void connect_callback(int result, GSupplicantInterface *interface,
							void *user_data)
{
	struct connman_network *network = user_data;

	DBG("network %p result %d", network, result);

	if (result == -ENOKEY) {
		connman_network_set_error(network,
					CONNMAN_NETWORK_ERROR_INVALID_KEY);
	} else if (result < 0) {
		connman_network_set_error(network,
					CONNMAN_NETWORK_ERROR_CONFIGURE_FAIL);
	}

	connman_network_unref(network);
}

static GSupplicantSecurity network_security(const char *security)
{
	if (g_str_equal(security, "none"))
		return G_SUPPLICANT_SECURITY_NONE;
	else if (g_str_equal(security, "wep"))
		return G_SUPPLICANT_SECURITY_WEP;
	else if (g_str_equal(security, "psk"))
		return G_SUPPLICANT_SECURITY_PSK;
	else if (g_str_equal(security, "wpa"))
		return G_SUPPLICANT_SECURITY_PSK;
	else if (g_str_equal(security, "rsn"))
		return G_SUPPLICANT_SECURITY_PSK;
	else if (g_str_equal(security, "ieee8021x"))
		return G_SUPPLICANT_SECURITY_IEEE8021X;

	return G_SUPPLICANT_SECURITY_UNKNOWN;
}

static void ssid_init(GSupplicantSSID *ssid, struct connman_network *network)
{
	const char *security;

	memset(ssid, 0, sizeof(*ssid));
	ssid->mode = G_SUPPLICANT_MODE_INFRA;
	ssid->ssid = connman_network_get_blob(network, "WiFi.SSID",
						&ssid->ssid_len);
	ssid->scan_ssid = 1;
	security = connman_network_get_string(network, "WiFi.Security");
	ssid->security = network_security(security);
	ssid->passphrase = connman_network_get_string(network,
						"WiFi.Passphrase");

	ssid->eap = connman_network_get_string(network, "WiFi.EAP");

	/*
	 * If our private key password is unset,
	 * we use the supplied passphrase. That is needed
	 * for PEAP where 2 passphrases (identity and client
	 * cert may have to be provided.
	 */
	if (!connman_network_get_string(network, "WiFi.PrivateKeyPassphrase"))
		connman_network_set_string(network,
						"WiFi.PrivateKeyPassphrase",
						ssid->passphrase);
	/* We must have an identity for both PEAP and TLS */
	ssid->identity = connman_network_get_string(network, "WiFi.Identity");

	/* Use agent provided identity as a fallback */
	if (!ssid->identity || strlen(ssid->identity) == 0)
		ssid->identity = connman_network_get_string(network,
							"WiFi.AgentIdentity");

	ssid->ca_cert_path = connman_network_get_string(network,
							"WiFi.CACertFile");
	ssid->client_cert_path = connman_network_get_string(network,
							"WiFi.ClientCertFile");
	ssid->private_key_path = connman_network_get_string(network,
							"WiFi.PrivateKeyFile");
	ssid->private_key_passphrase = connman_network_get_string(network,
						"WiFi.PrivateKeyPassphrase");
	ssid->phase2_auth = connman_network_get_string(network, "WiFi.Phase2");

	ssid->use_wps = connman_network_get_bool(network, "WiFi.UseWPS");
	ssid->pin_wps = connman_network_get_string(network, "WiFi.PinWPS");

	if (connman_setting_get_bool("BackgroundScanning"))
		ssid->bgscan = BGSCAN_DEFAULT;
}

static int network_connect(struct connman_network *network)
{
	struct connman_device *device = connman_network_get_device(network);
	struct wifi_data *wifi;
	GSupplicantInterface *interface;
	GSupplicantSSID *ssid;

	DBG("network %p", network);

	if (!device)
		return -ENODEV;

	wifi = connman_device_get_data(device);
	if (!wifi)
		return -ENODEV;

	ssid = g_try_malloc0(sizeof(GSupplicantSSID));
	if (!ssid)
		return -ENOMEM;

	interface = wifi->interface;

	ssid_init(ssid, network);

	if (wifi->disconnecting) {
		wifi->pending_network = network;
		g_free(ssid);
	} else {
		wifi->network = connman_network_ref(network);
		wifi->retries = 0;

		return g_supplicant_interface_connect(interface, ssid,
						connect_callback, network);
	}

	return -EINPROGRESS;
}

static void disconnect_callback(int result, GSupplicantInterface *interface,
								void *user_data)
{
	struct wifi_data *wifi = user_data;

	DBG("result %d supplicant interface %p wifi %p",
			result, interface, wifi);

	if (result == -ECONNABORTED) {
		DBG("wifi interface no longer available");
		return;
	}

	if (wifi->network) {
		/*
		 * if result < 0 supplican return an error because
		 * the network is not current.
		 * we wont receive G_SUPPLICANT_STATE_DISCONNECTED since it
		 * failed, call connman_network_set_connected to report
		 * disconnect is completed.
		 */
		if (result < 0)
			connman_network_set_connected(wifi->network, false);
	}

	wifi->network = NULL;

	wifi->disconnecting = false;

	if (wifi->pending_network) {
		network_connect(wifi->pending_network);
		wifi->pending_network = NULL;
	}

	start_autoscan(wifi->device);
}

static int network_disconnect(struct connman_network *network)
{
	struct connman_device *device = connman_network_get_device(network);
	struct wifi_data *wifi;
	int err;

	DBG("network %p", network);

	wifi = connman_device_get_data(device);
	if (!wifi || !wifi->interface)
		return -ENODEV;

	connman_network_set_associating(network, false);

	if (wifi->disconnecting)
		return -EALREADY;

	wifi->disconnecting = true;

	err = g_supplicant_interface_disconnect(wifi->interface,
						disconnect_callback, wifi);
	if (err < 0)
		wifi->disconnecting = false;

	return err;
}

static struct connman_network_driver network_driver = {
	.name		= "wifi",
	.type		= CONNMAN_NETWORK_TYPE_WIFI,
	.priority	= CONNMAN_NETWORK_PRIORITY_LOW,
	.probe		= network_probe,
	.remove		= network_remove,
	.connect	= network_connect,
	.disconnect	= network_disconnect,
};

static void interface_added(GSupplicantInterface *interface)
{
	const char *ifname = g_supplicant_interface_get_ifname(interface);
	const char *driver = g_supplicant_interface_get_driver(interface);
	struct wifi_data *wifi;

	wifi = g_supplicant_interface_get_data(interface);
	if (!wifi) {
		wifi = get_pending_wifi_data(ifname);
		if (!wifi)
			return;

		g_supplicant_interface_set_data(interface, wifi);
		p2p_iface_list = g_list_append(p2p_iface_list, wifi);
		wifi->p2p_device = true;
	}

	DBG("ifname %s driver %s wifi %p tethering %d",
			ifname, driver, wifi, wifi->tethering);

	if (!wifi->device) {
		connman_error("WiFi device not set");
		return;
	}

	connman_device_set_powered(wifi->device, true);
}

static bool is_idle(struct wifi_data *wifi)
{
	DBG("state %d", wifi->state);

	switch (wifi->state) {
	case G_SUPPLICANT_STATE_UNKNOWN:
	case G_SUPPLICANT_STATE_DISABLED:
	case G_SUPPLICANT_STATE_DISCONNECTED:
	case G_SUPPLICANT_STATE_INACTIVE:
	case G_SUPPLICANT_STATE_SCANNING:
		return true;

	case G_SUPPLICANT_STATE_AUTHENTICATING:
	case G_SUPPLICANT_STATE_ASSOCIATING:
	case G_SUPPLICANT_STATE_ASSOCIATED:
	case G_SUPPLICANT_STATE_4WAY_HANDSHAKE:
	case G_SUPPLICANT_STATE_GROUP_HANDSHAKE:
	case G_SUPPLICANT_STATE_COMPLETED:
		return false;
	}

	return false;
}

static bool is_idle_wps(GSupplicantInterface *interface,
						struct wifi_data *wifi)
{
	/* First, let's check if WPS processing did not went wrong */
	if (g_supplicant_interface_get_wps_state(interface) ==
		G_SUPPLICANT_WPS_STATE_FAIL)
		return false;

	/* Unlike normal connection, being associated while processing wps
	 * actually means that we are idling. */
	switch (wifi->state) {
	case G_SUPPLICANT_STATE_UNKNOWN:
	case G_SUPPLICANT_STATE_DISABLED:
	case G_SUPPLICANT_STATE_DISCONNECTED:
	case G_SUPPLICANT_STATE_INACTIVE:
	case G_SUPPLICANT_STATE_SCANNING:
	case G_SUPPLICANT_STATE_ASSOCIATED:
		return true;
	case G_SUPPLICANT_STATE_AUTHENTICATING:
	case G_SUPPLICANT_STATE_ASSOCIATING:
	case G_SUPPLICANT_STATE_4WAY_HANDSHAKE:
	case G_SUPPLICANT_STATE_GROUP_HANDSHAKE:
	case G_SUPPLICANT_STATE_COMPLETED:
		return false;
	}

	return false;
}

static bool handle_wps_completion(GSupplicantInterface *interface,
					struct connman_network *network,
					struct connman_device *device,
					struct wifi_data *wifi)
{
	bool wps;

	wps = connman_network_get_bool(network, "WiFi.UseWPS");
	if (wps) {
		const unsigned char *ssid, *wps_ssid;
		unsigned int ssid_len, wps_ssid_len;
		const char *wps_key;

		/* Checking if we got associated with requested
		 * network */
		ssid = connman_network_get_blob(network, "WiFi.SSID",
						&ssid_len);

		wps_ssid = g_supplicant_interface_get_wps_ssid(
			interface, &wps_ssid_len);

		if (!wps_ssid || wps_ssid_len != ssid_len ||
				memcmp(ssid, wps_ssid, ssid_len) != 0) {
			connman_network_set_associating(network, false);
			g_supplicant_interface_disconnect(wifi->interface,
						disconnect_callback, wifi);
			return false;
		}

		wps_key = g_supplicant_interface_get_wps_key(interface);
		connman_network_set_string(network, "WiFi.Passphrase",
					wps_key);

		connman_network_set_string(network, "WiFi.PinWPS", NULL);
	}

	return true;
}

static bool handle_4way_handshake_failure(GSupplicantInterface *interface,
					struct connman_network *network,
					struct wifi_data *wifi)
{
	struct connman_service *service;

	if (wifi->state != G_SUPPLICANT_STATE_4WAY_HANDSHAKE)
		return false;

	service = connman_service_lookup_from_network(network);
	if (!service)
		return false;

	wifi->retries++;

	if (connman_service_get_favorite(service)) {
		if (wifi->retries < FAVORITE_MAXIMUM_RETRIES)
			return true;
	}

	wifi->retries = 0;
	connman_network_set_error(network, CONNMAN_NETWORK_ERROR_INVALID_KEY);

	return false;
}

static void interface_state(GSupplicantInterface *interface)
{
	struct connman_network *network;
	struct connman_device *device;
	struct wifi_data *wifi;
	GSupplicantState state = g_supplicant_interface_get_state(interface);
	bool wps;

	wifi = g_supplicant_interface_get_data(interface);

	DBG("wifi %p interface state %d", wifi, state);

	if (!wifi)
		return;

	device = wifi->device;
	if (!device)
		return;

	if (g_supplicant_interface_get_ready(interface) &&
					!wifi->interface_ready) {
		wifi->interface_ready = true;
		finalize_interface_creation(wifi);
	}

	network = wifi->network;
	if (!network)
		return;

	switch (state) {
	case G_SUPPLICANT_STATE_SCANNING:
		break;

	case G_SUPPLICANT_STATE_AUTHENTICATING:
	case G_SUPPLICANT_STATE_ASSOCIATING:
		stop_autoscan(device);

		if (!wifi->connected)
			connman_network_set_associating(network, true);

		break;

	case G_SUPPLICANT_STATE_COMPLETED:
		/* though it should be already stopped: */
		stop_autoscan(device);

		if (!handle_wps_completion(interface, network, device, wifi))
			break;

		connman_network_set_connected(network, true);
		break;

	case G_SUPPLICANT_STATE_DISCONNECTED:
		/*
		 * If we're in one of the idle modes, we have
		 * not started association yet and thus setting
		 * those ones to FALSE could cancel an association
		 * in progress.
		 */
		wps = connman_network_get_bool(network, "WiFi.UseWPS");
		if (wps)
			if (is_idle_wps(interface, wifi))
				break;

		if (is_idle(wifi))
			break;

		/* If previous state was 4way-handshake, then
		 * it's either: psk was incorrect and thus we retry
		 * or if we reach the maximum retries we declare the
		 * psk as wrong */
		if (handle_4way_handshake_failure(interface,
						network, wifi))
			break;

		/* We disable the selected network, if not then
		 * wpa_supplicant will loop retrying */
		if (g_supplicant_interface_enable_selected_network(interface,
						FALSE) != 0)
			DBG("Could not disables selected network");

		connman_network_set_connected(network, false);
		connman_network_set_associating(network, false);
		wifi->disconnecting = false;

		start_autoscan(device);

		break;

	case G_SUPPLICANT_STATE_INACTIVE:
		connman_network_set_associating(network, false);
		start_autoscan(device);

		break;

	case G_SUPPLICANT_STATE_UNKNOWN:
	case G_SUPPLICANT_STATE_DISABLED:
	case G_SUPPLICANT_STATE_ASSOCIATED:
	case G_SUPPLICANT_STATE_4WAY_HANDSHAKE:
	case G_SUPPLICANT_STATE_GROUP_HANDSHAKE:
		break;
	}

	wifi->state = state;

	/* Saving wpa_s state policy:
	 * If connected and if the state changes are roaming related:
	 * --> We stay connected
	 * If completed
	 * --> We are connected
	 * All other case:
	 * --> We are not connected
	 * */
	switch (state) {
	case G_SUPPLICANT_STATE_AUTHENTICATING:
	case G_SUPPLICANT_STATE_ASSOCIATING:
	case G_SUPPLICANT_STATE_ASSOCIATED:
	case G_SUPPLICANT_STATE_4WAY_HANDSHAKE:
	case G_SUPPLICANT_STATE_GROUP_HANDSHAKE:
		if (wifi->connected)
			connman_warn("Probably roaming right now!"
						" Staying connected...");
		else
			wifi->connected = false;
		break;
	case G_SUPPLICANT_STATE_COMPLETED:
		wifi->connected = true;
		break;
	default:
		wifi->connected = false;
		break;
	}

	DBG("DONE");
}

static void interface_removed(GSupplicantInterface *interface)
{
	const char *ifname = g_supplicant_interface_get_ifname(interface);
	struct wifi_data *wifi;

	DBG("ifname %s", ifname);

	wifi = g_supplicant_interface_get_data(interface);

	if (wifi)
		wifi->interface = NULL;

	if (wifi && wifi->tethering)
		return;

	if (!wifi || !wifi->device) {
		DBG("wifi interface already removed");
		return;
	}

	connman_device_set_powered(wifi->device, false);

	check_p2p_technology();
}

static void set_device_type(const char *type, char dev_type[17])
{
	const char *oui = "0050F204";
	const char *category = "0100";
	const char *sub_category = "0000";

	if (!g_strcmp0(type, "handset")) {
		category = "0A00";
		sub_category = "0500";
	} else if (!g_strcmp0(type, "vm") || !g_strcmp0(type, "container"))
		sub_category = "0100";
	else if (!g_strcmp0(type, "server"))
		sub_category = "0200";
	else if (!g_strcmp0(type, "laptop"))
		sub_category = "0500";
	else if (!g_strcmp0(type, "desktop"))
		sub_category = "0600";
	else if (!g_strcmp0(type, "tablet"))
		sub_category = "0900";
	else if (!g_strcmp0(type, "watch"))
		category = "FF00";

	snprintf(dev_type, 17, "%s%s%s", category, oui, sub_category);
}

static void p2p_support(GSupplicantInterface *interface)
{
	char dev_type[17] = {};
	const char *hostname;

	DBG("");

	if (!g_supplicant_interface_has_p2p(interface))
		return;

	if (connman_technology_driver_register(&p2p_tech_driver) < 0) {
		DBG("Could not register P2P technology driver");
		return;
	}

	hostname = connman_utsname_get_hostname();
	if (!hostname)
		hostname = "ConnMan";

	set_device_type(connman_machine_get_type(), dev_type);
	g_supplicant_interface_set_p2p_device_config(interface,
							hostname, dev_type);
	connman_peer_driver_register(&peer_driver);
}

static void scan_started(GSupplicantInterface *interface)
{
	DBG("");
}

static void scan_finished(GSupplicantInterface *interface)
{
	DBG("");
}

static unsigned char calculate_strength(GSupplicantNetwork *supplicant_network)
{
	unsigned char strength;

	strength = 120 + g_supplicant_network_get_signal(supplicant_network);
	if (strength > 100)
		strength = 100;

	return strength;
}

static void network_added(GSupplicantNetwork *supplicant_network)
{
	struct connman_network *network;
	GSupplicantInterface *interface;
	struct wifi_data *wifi;
	const char *name, *identifier, *security, *group, *mode;
	const unsigned char *ssid;
	unsigned int ssid_len;
	bool wps;
	bool wps_pbc;
	bool wps_ready;
	bool wps_advertizing;

	mode = g_supplicant_network_get_mode(supplicant_network);
	identifier = g_supplicant_network_get_identifier(supplicant_network);

	DBG("%s", identifier);

	if (!g_strcmp0(mode, "adhoc"))
		return;

	interface = g_supplicant_network_get_interface(supplicant_network);
	wifi = g_supplicant_interface_get_data(interface);
	name = g_supplicant_network_get_name(supplicant_network);
	security = g_supplicant_network_get_security(supplicant_network);
	group = g_supplicant_network_get_identifier(supplicant_network);
	wps = g_supplicant_network_get_wps(supplicant_network);
	wps_pbc = g_supplicant_network_is_wps_pbc(supplicant_network);
	wps_ready = g_supplicant_network_is_wps_active(supplicant_network);
	wps_advertizing = g_supplicant_network_is_wps_advertizing(
							supplicant_network);

	if (!wifi)
		return;

	ssid = g_supplicant_network_get_ssid(supplicant_network, &ssid_len);

	network = connman_device_get_network(wifi->device, identifier);

	if (!network) {
		network = connman_network_create(identifier,
						CONNMAN_NETWORK_TYPE_WIFI);
		if (!network)
			return;

		connman_network_set_index(network, wifi->index);

		if (connman_device_add_network(wifi->device, network) < 0) {
			connman_network_unref(network);
			return;
		}

		wifi->networks = g_slist_prepend(wifi->networks, network);
	}

	if (name && name[0] != '\0')
		connman_network_set_name(network, name);

	connman_network_set_blob(network, "WiFi.SSID",
						ssid, ssid_len);
	connman_network_set_string(network, "WiFi.Security", security);
	connman_network_set_strength(network,
				calculate_strength(supplicant_network));
	connman_network_set_bool(network, "WiFi.WPS", wps);

	if (wps) {
		/* Is AP advertizing for WPS association?
		 * If so, we decide to use WPS by default */
		if (wps_ready && wps_pbc &&
						wps_advertizing)
			connman_network_set_bool(network, "WiFi.UseWPS", true);
	}

	connman_network_set_frequency(network,
			g_supplicant_network_get_frequency(supplicant_network));

	connman_network_set_available(network, true);
	connman_network_set_string(network, "WiFi.Mode", mode);

	if (ssid)
		connman_network_set_group(network, group);

	if (wifi->hidden && ssid) {
		if (!g_strcmp0(wifi->hidden->security, security) &&
				wifi->hidden->ssid_len == ssid_len &&
				!memcmp(wifi->hidden->ssid, ssid, ssid_len)) {
			connman_network_connect_hidden(network,
					wifi->hidden->identity,
					wifi->hidden->passphrase,
					wifi->hidden->user_data);
			wifi->hidden->user_data = NULL;
			hidden_free(wifi->hidden);
			wifi->hidden = NULL;
		}
	}
}

static void network_removed(GSupplicantNetwork *network)
{
	GSupplicantInterface *interface;
	struct wifi_data *wifi;
	const char *name, *identifier;
	struct connman_network *connman_network;

	interface = g_supplicant_network_get_interface(network);
	wifi = g_supplicant_interface_get_data(interface);
	identifier = g_supplicant_network_get_identifier(network);
	name = g_supplicant_network_get_name(network);

	DBG("name %s", name);

	if (!wifi)
		return;

	connman_network = connman_device_get_network(wifi->device, identifier);
	if (!connman_network)
		return;

	wifi->networks = g_slist_remove(wifi->networks, connman_network);

	connman_device_remove_network(wifi->device, connman_network);
	connman_network_unref(connman_network);
}

static void network_changed(GSupplicantNetwork *network, const char *property)
{
	GSupplicantInterface *interface;
	struct wifi_data *wifi;
	const char *name, *identifier;
	struct connman_network *connman_network;

	interface = g_supplicant_network_get_interface(network);
	wifi = g_supplicant_interface_get_data(interface);
	identifier = g_supplicant_network_get_identifier(network);
	name = g_supplicant_network_get_name(network);

	DBG("name %s", name);

	if (!wifi)
		return;

	connman_network = connman_device_get_network(wifi->device, identifier);
	if (!connman_network)
		return;

	if (g_str_equal(property, "Signal")) {
	       connman_network_set_strength(connman_network,
					calculate_strength(network));
	       connman_network_update(connman_network);
	}

	if (g_str_equal(property, "WiFi.WPS")) {
		dbus_bool_t wps = g_supplicant_network_get_wps(network);
		DBG("changing network WiFi.WPS to %d", wps);
		connman_network_set_bool(connman_network, "WiFi.WPS", wps);
		connman_network_update(connman_network);
	}
}

static void apply_peer_services(GSupplicantPeer *peer,
				struct connman_peer *connman_peer)
{
	const unsigned char *data;
	int length;

	DBG("");

	connman_peer_reset_services(connman_peer);

	data = g_supplicant_peer_get_widi_ies(peer, &length);
	if (data) {
		connman_peer_add_service(connman_peer,
			CONNMAN_PEER_SERVICE_WIFI_DISPLAY, data, length);
	}
}

static void peer_found(GSupplicantPeer *peer)
{
	GSupplicantInterface *iface = g_supplicant_peer_get_interface(peer);
	struct wifi_data *wifi = g_supplicant_interface_get_data(iface);
	struct connman_peer *connman_peer;
	const char *identifier, *name;
	int ret;

	identifier = g_supplicant_peer_get_identifier(peer);
	name = g_supplicant_peer_get_name(peer);

	DBG("ident: %s", identifier);

	connman_peer = connman_peer_get(wifi->device, identifier);
	if (connman_peer)
		return;

	connman_peer = connman_peer_create(identifier);
	connman_peer_set_name(connman_peer, name);
	connman_peer_set_device(connman_peer, wifi->device);
	apply_peer_services(peer, connman_peer);

	ret = connman_peer_register(connman_peer);
	if (ret < 0 && ret != -EALREADY)
		connman_peer_unref(connman_peer);
}

static void peer_lost(GSupplicantPeer *peer)
{
	GSupplicantInterface *iface = g_supplicant_peer_get_interface(peer);
	struct wifi_data *wifi = g_supplicant_interface_get_data(iface);
	struct connman_peer *connman_peer;
	const char *identifier;

	if (!wifi)
		return;

	identifier = g_supplicant_peer_get_identifier(peer);

	DBG("ident: %s", identifier);

	connman_peer = connman_peer_get(wifi->device, identifier);
	if (connman_peer) {
		if (wifi->p2p_connecting &&
				wifi->pending_peer == connman_peer) {
			peer_connect_timeout(wifi);
		}
		connman_peer_unregister(connman_peer);
		connman_peer_unref(connman_peer);
	}
}

static void peer_changed(GSupplicantPeer *peer, GSupplicantPeerState state)
{
	GSupplicantInterface *iface = g_supplicant_peer_get_interface(peer);
	struct wifi_data *wifi = g_supplicant_interface_get_data(iface);
	enum connman_peer_state p_state = CONNMAN_PEER_STATE_UNKNOWN;
	struct connman_peer *connman_peer;
	const char *identifier;

	identifier = g_supplicant_peer_get_identifier(peer);

	DBG("ident: %s", identifier);

	connman_peer = connman_peer_get(wifi->device, identifier);
	if (!connman_peer)
		return;

	switch (state) {
	case G_SUPPLICANT_PEER_SERVICES_CHANGED:
		apply_peer_services(peer, connman_peer);
		connman_peer_services_changed(connman_peer);
		return;
	case G_SUPPLICANT_PEER_GROUP_CHANGED:
		if (!g_supplicant_peer_is_in_a_group(peer))
			p_state = CONNMAN_PEER_STATE_IDLE;
		else
			p_state = CONNMAN_PEER_STATE_CONFIGURATION;
		break;
	case G_SUPPLICANT_PEER_GROUP_STARTED:
		break;
	case G_SUPPLICANT_PEER_GROUP_FINISHED:
		p_state = CONNMAN_PEER_STATE_IDLE;
		break;
	case G_SUPPLICANT_PEER_GROUP_JOINED:
		connman_peer_set_iface_address(connman_peer,
				g_supplicant_peer_get_iface_address(peer));
		break;
	case G_SUPPLICANT_PEER_GROUP_DISCONNECTED:
		p_state = CONNMAN_PEER_STATE_IDLE;
		break;
	case G_SUPPLICANT_PEER_GROUP_FAILED:
		if (g_supplicant_peer_has_requested_connection(peer))
			p_state = CONNMAN_PEER_STATE_IDLE;
		else
			p_state = CONNMAN_PEER_STATE_FAILURE;
		break;
	}

	if (p_state == CONNMAN_PEER_STATE_CONFIGURATION ||
					p_state == CONNMAN_PEER_STATE_FAILURE) {
		if (wifi->p2p_connecting
				&& connman_peer == wifi->pending_peer)
			peer_cancel_timeout(wifi);
		else
			p_state = CONNMAN_PEER_STATE_UNKNOWN;
	}

	if (p_state == CONNMAN_PEER_STATE_UNKNOWN)
		return;

	if (p_state == CONNMAN_PEER_STATE_CONFIGURATION) {
		GSupplicantInterface *g_iface;
		struct wifi_data *g_wifi;

		g_iface = g_supplicant_peer_get_group_interface(peer);
		if (!g_iface)
			return;

		g_wifi = g_supplicant_interface_get_data(g_iface);
		if (!g_wifi)
			return;

		connman_peer_set_as_master(connman_peer,
					!g_supplicant_peer_is_client(peer));
		connman_peer_set_sub_device(connman_peer, g_wifi->device);
	}

	connman_peer_set_state(connman_peer, p_state);
}

static void peer_request(GSupplicantPeer *peer)
{
	GSupplicantInterface *iface = g_supplicant_peer_get_interface(peer);
	struct wifi_data *wifi = g_supplicant_interface_get_data(iface);
	struct connman_peer *connman_peer;
	const char *identifier;

	identifier = g_supplicant_peer_get_identifier(peer);

	DBG("ident: %s", identifier);

	connman_peer = connman_peer_get(wifi->device, identifier);
	if (!connman_peer)
		return;

	connman_peer_request_connection(connman_peer);
}

static void debug(const char *str)
{
	if (getenv("CONNMAN_SUPPLICANT_DEBUG"))
		connman_debug("%s", str);
}

static const GSupplicantCallbacks callbacks = {
	.system_ready		= system_ready,
	.system_killed		= system_killed,
	.interface_added	= interface_added,
	.interface_state	= interface_state,
	.interface_removed	= interface_removed,
	.p2p_support		= p2p_support,
	.scan_started		= scan_started,
	.scan_finished		= scan_finished,
	.network_added		= network_added,
	.network_removed	= network_removed,
	.network_changed	= network_changed,
	.peer_found		= peer_found,
	.peer_lost		= peer_lost,
	.peer_changed		= peer_changed,
	.peer_request		= peer_request,
	.debug			= debug,
};


static int tech_probe(struct connman_technology *technology)
{
	wifi_technology = technology;

	return 0;
}

static void tech_remove(struct connman_technology *technology)
{
	wifi_technology = NULL;
}

struct wifi_tethering_info {
	struct wifi_data *wifi;
	struct connman_technology *technology;
	char *ifname;
	GSupplicantSSID *ssid;
};

static GSupplicantSSID *ssid_ap_init(const char *ssid, const char *passphrase)
{
	GSupplicantSSID *ap;

	ap = g_try_malloc0(sizeof(GSupplicantSSID));
	if (!ap)
		return NULL;

	ap->mode = G_SUPPLICANT_MODE_MASTER;
	ap->ssid = ssid;
	ap->ssid_len = strlen(ssid);
	ap->scan_ssid = 0;
	ap->freq = 2412;

	if (!passphrase || strlen(passphrase) == 0) {
		ap->security = G_SUPPLICANT_SECURITY_NONE;
		ap->passphrase = NULL;
	} else {
	       ap->security = G_SUPPLICANT_SECURITY_PSK;
	       ap->protocol = G_SUPPLICANT_PROTO_RSN;
	       ap->pairwise_cipher = G_SUPPLICANT_PAIRWISE_CCMP;
	       ap->group_cipher = G_SUPPLICANT_GROUP_CCMP;
	       ap->passphrase = passphrase;
	}

	return ap;
}

static void ap_start_callback(int result, GSupplicantInterface *interface,
							void *user_data)
{
	struct wifi_tethering_info *info = user_data;

	DBG("result %d index %d bridge %s",
		result, info->wifi->index, info->wifi->bridge);

	if (result < 0) {
		connman_inet_remove_from_bridge(info->wifi->index,
							info->wifi->bridge);
		connman_technology_tethering_notify(info->technology, false);
	}

	g_free(info->ifname);
	g_free(info);
}

static void ap_create_callback(int result,
				GSupplicantInterface *interface,
					void *user_data)
{
	struct wifi_tethering_info *info = user_data;

	DBG("result %d ifname %s", result,
				g_supplicant_interface_get_ifname(interface));

	if (result < 0) {
		connman_inet_remove_from_bridge(info->wifi->index,
							info->wifi->bridge);
		connman_technology_tethering_notify(info->technology, false);

		g_free(info->ifname);
		g_free(info->ssid);
		g_free(info);
		return;
	}

	info->wifi->interface = interface;
	g_supplicant_interface_set_data(interface, info->wifi);

	if (g_supplicant_interface_set_apscan(interface, 2) < 0)
		connman_error("Failed to set interface ap_scan property");

	g_supplicant_interface_connect(interface, info->ssid,
						ap_start_callback, info);
}

static void sta_remove_callback(int result,
				GSupplicantInterface *interface,
					void *user_data)
{
	struct wifi_tethering_info *info = user_data;
	const char *driver = connman_option_get_string("wifi");

	DBG("ifname %s result %d ", info->ifname, result);

	if (result < 0) {
		info->wifi->tethering = true;

		g_free(info->ifname);
		g_free(info->ssid);
		g_free(info);
		return;
	}

	info->wifi->interface = NULL;

	connman_technology_tethering_notify(info->technology, true);

	g_supplicant_interface_create(info->ifname, driver, info->wifi->bridge,
						ap_create_callback,
							info);
}

static int tech_set_tethering(struct connman_technology *technology,
				const char *identifier, const char *passphrase,
				const char *bridge, bool enabled)
{
	GList *list;
	GSupplicantInterface *interface;
	struct wifi_data *wifi;
	struct wifi_tethering_info *info;
	const char *ifname;
	unsigned int mode;
	int err;

	DBG("");

	if (!enabled) {
		for (list = iface_list; list; list = list->next) {
			wifi = list->data;

			if (wifi->tethering) {
				wifi->tethering = false;

				connman_inet_remove_from_bridge(wifi->index,
									bridge);
				wifi->bridged = false;
			}
		}

		connman_technology_tethering_notify(technology, false);

		return 0;
	}

	for (list = iface_list; list; list = list->next) {
		wifi = list->data;

		interface = wifi->interface;

		if (!interface)
			continue;

		ifname = g_supplicant_interface_get_ifname(wifi->interface);

		mode = g_supplicant_interface_get_mode(interface);
		if ((mode & G_SUPPLICANT_CAPABILITY_MODE_AP) == 0) {
			DBG("%s does not support AP mode", ifname);
			continue;
		}

		info = g_try_malloc0(sizeof(struct wifi_tethering_info));
		if (!info)
			return -ENOMEM;

		info->wifi = wifi;
		info->technology = technology;
		info->wifi->bridge = bridge;
		info->ssid = ssid_ap_init(identifier, passphrase);
		if (!info->ssid) {
			g_free(info);
			continue;
		}
		info->ifname = g_strdup(ifname);
		if (!info->ifname) {
			g_free(info->ssid);
			g_free(info);
			continue;
		}

		info->wifi->tethering = true;

		err = g_supplicant_interface_remove(interface,
						sta_remove_callback,
							info);
		if (err == 0)
			return err;
	}

	return -EOPNOTSUPP;
}

static void regdom_callback(int result, const char *alpha2, void *user_data)
{
	DBG("");

	if (!wifi_technology)
		return;

	if (result != 0)
		alpha2 = NULL;

	connman_technology_regdom_notify(wifi_technology, alpha2);
}

static int tech_set_regdom(struct connman_technology *technology, const char *alpha2)
{
	return g_supplicant_set_country(alpha2, regdom_callback, NULL);
}

static struct connman_technology_driver tech_driver = {
	.name		= "wifi",
	.type		= CONNMAN_SERVICE_TYPE_WIFI,
	.probe		= tech_probe,
	.remove		= tech_remove,
	.set_tethering	= tech_set_tethering,
	.set_regdom	= tech_set_regdom,
};

static int wifi_init(void)
{
	int err;

	err = connman_network_driver_register(&network_driver);
	if (err < 0)
		return err;

	err = g_supplicant_register(&callbacks);
	if (err < 0) {
		connman_network_driver_unregister(&network_driver);
		return err;
	}

	err = connman_technology_driver_register(&tech_driver);
	if (err < 0) {
		g_supplicant_unregister(&callbacks);
		connman_network_driver_unregister(&network_driver);
		return err;
	}

	return 0;
}

static void wifi_exit(void)
{
	DBG();

	connman_technology_driver_unregister(&tech_driver);

	g_supplicant_unregister(&callbacks);

	connman_network_driver_unregister(&network_driver);
}

CONNMAN_PLUGIN_DEFINE(wifi, "WiFi interface plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, wifi_init, wifi_exit)
