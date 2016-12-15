/*
 *
 *  WPA supplicant library with GLib integration
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <syslog.h>
#include <ctype.h>
#include <stdbool.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <glib.h>
#include <gdbus.h>

#include "dbus.h"
#include "gsupplicant.h"

#define IEEE80211_CAP_ESS	0x0001
#define IEEE80211_CAP_IBSS	0x0002
#define IEEE80211_CAP_PRIVACY	0x0010

#define BSS_UNKNOWN_STRENGTH    -90

static DBusConnection *connection;

static const GSupplicantCallbacks *callbacks_pointer;

static dbus_bool_t system_available = FALSE;
static dbus_bool_t system_ready = FALSE;

static dbus_int32_t debug_level;
static dbus_bool_t debug_timestamp = FALSE;
static dbus_bool_t debug_showkeys = FALSE;

static const char *debug_strings[] = {
	"msgdump", "debug", "info", "warning", "error", NULL
};

static unsigned int eap_methods;

struct strvalmap {
	const char *str;
	unsigned int val;
};

static struct strvalmap eap_method_map[] = {
	{ "MD5",	G_SUPPLICANT_EAP_METHOD_MD5	},
	{ "TLS",	G_SUPPLICANT_EAP_METHOD_TLS	},
	{ "MSCHAPV2",	G_SUPPLICANT_EAP_METHOD_MSCHAPV2	},
	{ "PEAP",	G_SUPPLICANT_EAP_METHOD_PEAP	},
	{ "TTLS",	G_SUPPLICANT_EAP_METHOD_TTLS	},
	{ "GTC",	G_SUPPLICANT_EAP_METHOD_GTC	},
	{ "OTP",	G_SUPPLICANT_EAP_METHOD_OTP	},
	{ "LEAP",	G_SUPPLICANT_EAP_METHOD_LEAP	},
	{ "WSC",	G_SUPPLICANT_EAP_METHOD_WSC	},
	{ }
};

static struct strvalmap keymgmt_map[] = {
	{ "none",		G_SUPPLICANT_KEYMGMT_NONE		},
	{ "ieee8021x",		G_SUPPLICANT_KEYMGMT_IEEE8021X	},
	{ "wpa-none",		G_SUPPLICANT_KEYMGMT_WPA_NONE	},
	{ "wpa-psk",		G_SUPPLICANT_KEYMGMT_WPA_PSK	},
	{ "wpa-psk-sha256",	G_SUPPLICANT_KEYMGMT_WPA_PSK_256	},
	{ "wpa-ft-psk",		G_SUPPLICANT_KEYMGMT_WPA_FT_PSK	},
	{ "wpa-ft-eap",		G_SUPPLICANT_KEYMGMT_WPA_FT_EAP	},
	{ "wpa-eap",		G_SUPPLICANT_KEYMGMT_WPA_EAP	},
	{ "wpa-eap-sha256",	G_SUPPLICANT_KEYMGMT_WPA_EAP_256	},
	{ "wps",		G_SUPPLICANT_KEYMGMT_WPS		},
	{ }
};

static struct strvalmap authalg_capa_map[] = {
	{ "open",	G_SUPPLICANT_CAPABILITY_AUTHALG_OPEN	},
	{ "shared",	G_SUPPLICANT_CAPABILITY_AUTHALG_SHARED	},
	{ "leap",	G_SUPPLICANT_CAPABILITY_AUTHALG_LEAP	},
	{ }
};

static struct strvalmap proto_capa_map[] = {
	{ "wpa",	G_SUPPLICANT_CAPABILITY_PROTO_WPA		},
	{ "rsn",	G_SUPPLICANT_CAPABILITY_PROTO_RSN		},
	{ }
};

static struct strvalmap group_map[] = {
	{ "wep40",	G_SUPPLICANT_GROUP_WEP40	},
	{ "wep104",	G_SUPPLICANT_GROUP_WEP104	},
	{ "tkip",	G_SUPPLICANT_GROUP_TKIP	},
	{ "ccmp",	G_SUPPLICANT_GROUP_CCMP	},
	{ }
};

static struct strvalmap pairwise_map[] = {
	{ "none",	G_SUPPLICANT_PAIRWISE_NONE	},
	{ "tkip",	G_SUPPLICANT_PAIRWISE_TKIP	},
	{ "ccmp",	G_SUPPLICANT_PAIRWISE_CCMP	},
	{ }
};

static struct strvalmap scan_capa_map[] = {
	{ "active",	G_SUPPLICANT_CAPABILITY_SCAN_ACTIVE	},
	{ "passive",	G_SUPPLICANT_CAPABILITY_SCAN_PASSIVE	},
	{ "ssid",	G_SUPPLICANT_CAPABILITY_SCAN_SSID		},
	{ }
};

static struct strvalmap mode_capa_map[] = {
	{ "infrastructure",	G_SUPPLICANT_CAPABILITY_MODE_INFRA	},
	{ "ad-hoc",		G_SUPPLICANT_CAPABILITY_MODE_IBSS	},
	{ "ap",			G_SUPPLICANT_CAPABILITY_MODE_AP		},
	{ "p2p", 		G_SUPPLICANT_CAPABILITY_MODE_P2P	},
	{ }
};

static GHashTable *interface_table;
static GHashTable *bss_mapping;
static GHashTable *peer_mapping;
static GHashTable *group_mapping;
static GHashTable *pending_peer_connection;

struct _GSupplicantWpsCredentials {
	unsigned char ssid[32];
	unsigned int ssid_len;
	char *key;
};

struct _GSupplicantInterface {
	char *path;
	char *network_path;
	unsigned int keymgmt_capa;
	unsigned int authalg_capa;
	unsigned int proto_capa;
	unsigned int group_capa;
	unsigned int pairwise_capa;
	unsigned int scan_capa;
	unsigned int mode_capa;
	unsigned int max_scan_ssids;
	bool p2p_support;
	bool p2p_finding;
	dbus_bool_t ready;
	GSupplicantState state;
	dbus_bool_t scanning;
	GSupplicantInterfaceCallback scan_callback;
	void *scan_data;
	int apscan;
	char *ifname;
	char *driver;
	char *bridge;
	struct _GSupplicantWpsCredentials wps_cred;
	GSupplicantWpsState wps_state;
	GHashTable *network_table;
	GHashTable *peer_table;
	GHashTable *group_table;
	GHashTable *net_mapping;
	GHashTable *bss_mapping;
	void *data;
	const char *pending_peer_path;
};

struct g_supplicant_bss {
	GSupplicantInterface *interface;
	char *path;
	unsigned char bssid[6];
	unsigned char ssid[32];
	unsigned int ssid_len;
	dbus_uint16_t frequency;
	dbus_uint32_t maxrate;
	dbus_int16_t signal;
	GSupplicantMode mode;
	GSupplicantSecurity security;
	dbus_bool_t rsn_selected;
	unsigned int wpa_keymgmt;
	unsigned int wpa_pairwise;
	unsigned int wpa_group;
	unsigned int rsn_keymgmt;
	unsigned int rsn_pairwise;
	unsigned int rsn_group;
	unsigned int keymgmt;
	dbus_bool_t privacy;
	dbus_bool_t psk;
	dbus_bool_t ieee8021x;
	unsigned int wps_capabilities;
};

struct _GSupplicantNetwork {
	GSupplicantInterface *interface;
	char *path;
	char *group;
	char *name;
	unsigned char ssid[32];
	unsigned int ssid_len;
	dbus_int16_t signal;
	dbus_uint16_t frequency;
	struct g_supplicant_bss *best_bss;
	GSupplicantMode mode;
	GSupplicantSecurity security;
	dbus_bool_t wps;
	unsigned int wps_capabilities;
	GHashTable *bss_table;
	GHashTable *config_table;
};

struct _GSupplicantPeer {
	GSupplicantInterface *interface;
	char *path;
	unsigned char device_address[ETH_ALEN];
	unsigned char iface_address[ETH_ALEN];
	char *name;
	unsigned char *widi_ies;
	int widi_ies_length;
	char *identifier;
	unsigned int wps_capabilities;
	GSList *groups;
	const GSupplicantInterface *current_group_iface;
	bool connection_requested;
};

struct _GSupplicantGroup {
	GSupplicantInterface *interface;
	GSupplicantInterface *orig_interface;
	char *path;
	int role;
	GSList *members;
};

static inline void debug(const char *format, ...)
{
	char str[256];
	va_list ap;

	if (!callbacks_pointer->debug)
		return;

	va_start(ap, format);

	if (vsnprintf(str, sizeof(str), format, ap) > 0)
		callbacks_pointer->debug(str);

	va_end(ap);
}

#define SUPPLICANT_DBG(fmt, arg...) \
	debug("%s:%s() " fmt, __FILE__, __FUNCTION__ , ## arg);

static GSupplicantMode string2mode(const char *mode)
{
	if (!mode)
		return G_SUPPLICANT_MODE_UNKNOWN;

	if (g_str_equal(mode, "infrastructure"))
		return G_SUPPLICANT_MODE_INFRA;
	else if (g_str_equal(mode, "ad-hoc"))
		return G_SUPPLICANT_MODE_IBSS;

	return G_SUPPLICANT_MODE_UNKNOWN;
}

static const char *mode2string(GSupplicantMode mode)
{
	switch (mode) {
	case G_SUPPLICANT_MODE_UNKNOWN:
		break;
	case G_SUPPLICANT_MODE_INFRA:
		return "managed";
	case G_SUPPLICANT_MODE_IBSS:
		return "adhoc";
	case G_SUPPLICANT_MODE_MASTER:
		return "ap";
	}

	return NULL;
}

static const char *security2string(GSupplicantSecurity security)
{
	switch (security) {
	case G_SUPPLICANT_SECURITY_UNKNOWN:
		break;
	case G_SUPPLICANT_SECURITY_NONE:
		return "none";
	case G_SUPPLICANT_SECURITY_WEP:
		return "wep";
	case G_SUPPLICANT_SECURITY_PSK:
		return "psk";
	case G_SUPPLICANT_SECURITY_IEEE8021X:
		return "ieee8021x";
	}

	return NULL;
}

static GSupplicantState string2state(const char *state)
{
	if (!state)
		return G_SUPPLICANT_STATE_UNKNOWN;

	if (g_str_equal(state, "unknown"))
		return G_SUPPLICANT_STATE_UNKNOWN;
	else if (g_str_equal(state, "interface_disabled"))
		return G_SUPPLICANT_STATE_DISABLED;
	else if (g_str_equal(state, "disconnected"))
		return G_SUPPLICANT_STATE_DISCONNECTED;
	else if (g_str_equal(state, "inactive"))
		return G_SUPPLICANT_STATE_INACTIVE;
	else if (g_str_equal(state, "scanning"))
		return G_SUPPLICANT_STATE_SCANNING;
	else if (g_str_equal(state, "authenticating"))
		return G_SUPPLICANT_STATE_AUTHENTICATING;
	else if (g_str_equal(state, "associating"))
		return G_SUPPLICANT_STATE_ASSOCIATING;
	else if (g_str_equal(state, "associated"))
		return G_SUPPLICANT_STATE_ASSOCIATED;
	else if (g_str_equal(state, "group_handshake"))
		return G_SUPPLICANT_STATE_GROUP_HANDSHAKE;
	else if (g_str_equal(state, "4way_handshake"))
		return G_SUPPLICANT_STATE_4WAY_HANDSHAKE;
	else if (g_str_equal(state, "completed"))
		return G_SUPPLICANT_STATE_COMPLETED;

	return G_SUPPLICANT_STATE_UNKNOWN;
}

static void callback_system_ready(void)
{
	if (system_ready)
		return;

	system_ready = TRUE;

	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->system_ready)
		return;

	callbacks_pointer->system_ready();
}

static void callback_system_killed(void)
{
	system_ready = FALSE;

	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->system_killed)
		return;

	callbacks_pointer->system_killed();
}

static void callback_interface_added(GSupplicantInterface *interface)
{
	SUPPLICANT_DBG("");

	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->interface_added)
		return;

	callbacks_pointer->interface_added(interface);
}

static void callback_interface_state(GSupplicantInterface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->interface_state)
		return;

	callbacks_pointer->interface_state(interface);
}

static void callback_interface_removed(GSupplicantInterface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->interface_removed)
		return;

	callbacks_pointer->interface_removed(interface);
}

static void callback_p2p_support(GSupplicantInterface *interface)
{
	SUPPLICANT_DBG("");

	if (!interface->p2p_support)
		return;

	if (callbacks_pointer && callbacks_pointer->p2p_support)
		callbacks_pointer->p2p_support(interface);
}

static void callback_scan_started(GSupplicantInterface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->scan_started)
		return;

	callbacks_pointer->scan_started(interface);
}

static void callback_scan_finished(GSupplicantInterface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->scan_finished)
		return;

	callbacks_pointer->scan_finished(interface);
}

static void callback_network_added(GSupplicantNetwork *network)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->network_added)
		return;

	callbacks_pointer->network_added(network);
}

static void callback_network_removed(GSupplicantNetwork *network)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->network_removed)
		return;

	callbacks_pointer->network_removed(network);
}

static void callback_network_changed(GSupplicantNetwork *network,
					const char *property)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->network_changed)
		return;

	callbacks_pointer->network_changed(network, property);
}

static void callback_peer_found(GSupplicantPeer *peer)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->peer_found)
		return;

	callbacks_pointer->peer_found(peer);
}

static void callback_peer_lost(GSupplicantPeer *peer)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->peer_lost)
		return;

	callbacks_pointer->peer_lost(peer);
}

static void callback_peer_changed(GSupplicantPeer *peer,
						GSupplicantPeerState state)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->peer_changed)
		return;

	callbacks_pointer->peer_changed(peer, state);
}

static void callback_peer_request(GSupplicantPeer *peer)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->peer_request)
		return;

	peer->connection_requested = true;

	callbacks_pointer->peer_request(peer);
}

static void remove_group(gpointer data)
{
	GSupplicantGroup *group = data;

	if (group->members)
		g_slist_free_full(group->members, g_free);

	g_free(group->path);
	g_free(group);
}

static void remove_interface(gpointer data)
{
	GSupplicantInterface *interface = data;

	g_hash_table_destroy(interface->bss_mapping);
	g_hash_table_destroy(interface->net_mapping);
	g_hash_table_destroy(interface->network_table);
	g_hash_table_destroy(interface->peer_table);
	g_hash_table_destroy(interface->group_table);

	if (interface->scan_callback) {
		SUPPLICANT_DBG("call interface %p callback %p scanning %d",
				interface, interface->scan_callback,
				interface->scanning);

		interface->scan_callback(-EIO, interface, interface->scan_data);
                interface->scan_callback = NULL;
                interface->scan_data = NULL;

		if (interface->scanning) {
			interface->scanning = FALSE;
			callback_scan_finished(interface);
		}
	}

	callback_interface_removed(interface);

	g_free(interface->wps_cred.key);
	g_free(interface->path);
	g_free(interface->network_path);
	g_free(interface->ifname);
	g_free(interface->driver);
	g_free(interface->bridge);
	g_free(interface);
}

static void remove_network(gpointer data)
{
	GSupplicantNetwork *network = data;

	g_hash_table_destroy(network->bss_table);

	callback_network_removed(network);

	g_hash_table_destroy(network->config_table);

	g_free(network->path);
	g_free(network->group);
	g_free(network->name);
	g_free(network);
}

static void remove_bss(gpointer data)
{
	struct g_supplicant_bss *bss = data;

	g_free(bss->path);
	g_free(bss);
}

static void remove_peer(gpointer data)
{
	GSupplicantPeer *peer = data;

	callback_peer_lost(peer);

	if (peer->groups)
		g_slist_free_full(peer->groups, g_free);

	if (peer_mapping)
		g_hash_table_remove(peer_mapping, peer->path);

	if (pending_peer_connection)
		g_hash_table_remove(pending_peer_connection, peer->path);

	g_free(peer->path);
	g_free(peer->name);
	g_free(peer->identifier);

	g_free(peer);
}

static void debug_strvalmap(const char *label, struct strvalmap *map,
							unsigned int val)
{
	int i;

	for (i = 0; map[i].str; i++) {
		if (val & map[i].val)
			SUPPLICANT_DBG("%s: %s", label, map[i].str);
	}
}

static void interface_capability_keymgmt(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; keymgmt_map[i].str; i++)
		if (strcmp(str, keymgmt_map[i].str) == 0) {
			interface->keymgmt_capa |= keymgmt_map[i].val;
			break;
		}
}

static void interface_capability_authalg(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; authalg_capa_map[i].str; i++)
		if (strcmp(str, authalg_capa_map[i].str) == 0) {
			interface->authalg_capa |= authalg_capa_map[i].val;
			break;
		}
}

static void interface_capability_proto(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; proto_capa_map[i].str; i++)
		if (strcmp(str, proto_capa_map[i].str) == 0) {
			interface->proto_capa |= proto_capa_map[i].val;
			break;
		}
}

static void interface_capability_pairwise(DBusMessageIter *iter,
							void *user_data)
{
	GSupplicantInterface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; pairwise_map[i].str; i++)
		if (strcmp(str, pairwise_map[i].str) == 0) {
			interface->pairwise_capa |= pairwise_map[i].val;
			break;
		}
}

static void interface_capability_group(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; group_map[i].str; i++)
		if (strcmp(str, group_map[i].str) == 0) {
			interface->group_capa |= group_map[i].val;
			break;
		}
}

static void interface_capability_scan(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; scan_capa_map[i].str; i++)
		if (strcmp(str, scan_capa_map[i].str) == 0) {
			interface->scan_capa |= scan_capa_map[i].val;
			break;
		}
}

static void interface_capability_mode(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; mode_capa_map[i].str; i++)
		if (strcmp(str, mode_capa_map[i].str) == 0) {
			interface->mode_capa |= mode_capa_map[i].val;
			break;
		}
}

static void interface_capability(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	GSupplicantInterface *interface = user_data;

	if (!key)
		return;

	if (g_strcmp0(key, "KeyMgmt") == 0)
		supplicant_dbus_array_foreach(iter,
				interface_capability_keymgmt, interface);
	else if (g_strcmp0(key, "AuthAlg") == 0)
		supplicant_dbus_array_foreach(iter,
				interface_capability_authalg, interface);
	else if (g_strcmp0(key, "Protocol") == 0)
		supplicant_dbus_array_foreach(iter,
				interface_capability_proto, interface);
	else if (g_strcmp0(key, "Pairwise") == 0)
		supplicant_dbus_array_foreach(iter,
				interface_capability_pairwise, interface);
	else if (g_strcmp0(key, "Group") == 0)
		supplicant_dbus_array_foreach(iter,
				interface_capability_group, interface);
	else if (g_strcmp0(key, "Scan") == 0)
		supplicant_dbus_array_foreach(iter,
				interface_capability_scan, interface);
	else if (g_strcmp0(key, "Modes") == 0)
		supplicant_dbus_array_foreach(iter,
				interface_capability_mode, interface);
	else if (g_strcmp0(key, "MaxScanSSID") == 0) {
		dbus_int32_t max_scan_ssid;

		dbus_message_iter_get_basic(iter, &max_scan_ssid);
		if (max_scan_ssid < 2)
			max_scan_ssid = 0;
		interface->max_scan_ssids = max_scan_ssid;

	} else
		SUPPLICANT_DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static void set_apscan(DBusMessageIter *iter, void *user_data)
{
	unsigned int ap_scan = *(unsigned int *)user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &ap_scan);
}

int g_supplicant_interface_set_apscan(GSupplicantInterface *interface,
							unsigned int ap_scan)
{
	return supplicant_dbus_property_set(interface->path,
			SUPPLICANT_INTERFACE ".Interface",
				"ApScan", DBUS_TYPE_UINT32_AS_STRING,
					set_apscan, NULL, &ap_scan, NULL);
}

void g_supplicant_interface_set_data(GSupplicantInterface *interface,
								void *data)
{
	if (!interface)
		return;

	interface->data = data;

	if (!data)
		interface->scan_callback = NULL;
}

void *g_supplicant_interface_get_data(GSupplicantInterface *interface)
{
	if (!interface)
		return NULL;

	return interface->data;
}

const char *g_supplicant_interface_get_ifname(GSupplicantInterface *interface)
{
	if (!interface)
		return NULL;

	return interface->ifname;
}

const char *g_supplicant_interface_get_driver(GSupplicantInterface *interface)
{
	if (!interface)
		return NULL;

	return interface->driver;
}

GSupplicantState g_supplicant_interface_get_state(
					GSupplicantInterface *interface)
{
	if (!interface)
		return G_SUPPLICANT_STATE_UNKNOWN;

	return interface->state;
}

const char *g_supplicant_interface_get_wps_key(GSupplicantInterface *interface)
{
	if (!interface)
		return NULL;

	return (const char *)interface->wps_cred.key;
}

const void *g_supplicant_interface_get_wps_ssid(GSupplicantInterface *interface,
							unsigned int *ssid_len)
{
	if (!ssid_len)
		return NULL;

	if (!interface || interface->wps_cred.ssid_len == 0) {
		*ssid_len = 0;
		return NULL;
	}

	*ssid_len = interface->wps_cred.ssid_len;
	return interface->wps_cred.ssid;
}

GSupplicantWpsState g_supplicant_interface_get_wps_state(
					GSupplicantInterface *interface)
{
	if (!interface)
		return G_SUPPLICANT_WPS_STATE_UNKNOWN;

	return interface->wps_state;
}

unsigned int g_supplicant_interface_get_mode(GSupplicantInterface *interface)
{
	if (!interface)
		return 0;

	return interface->mode_capa;
}

unsigned int g_supplicant_interface_get_max_scan_ssids(
				GSupplicantInterface *interface)
{
	if (!interface)
		return 0;

	return interface->max_scan_ssids;
}

static void set_network_enabled(DBusMessageIter *iter, void *user_data)
{
	dbus_bool_t enable = *(dbus_bool_t *)user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &enable);
}

int g_supplicant_interface_enable_selected_network(GSupplicantInterface *interface,
							dbus_bool_t enable)
{
	if (!interface)
		return -1;

	if (!interface->network_path)
		return -1;

	SUPPLICANT_DBG(" ");
	return supplicant_dbus_property_set(interface->network_path,
				SUPPLICANT_INTERFACE ".Network",
				"Enabled", DBUS_TYPE_BOOLEAN_AS_STRING,
				set_network_enabled, NULL, &enable, NULL);
}

dbus_bool_t g_supplicant_interface_get_ready(GSupplicantInterface *interface)
{
	if (!interface)
		return FALSE;

	return interface->ready;
}

GSupplicantInterface *g_supplicant_network_get_interface(
					GSupplicantNetwork *network)
{
	if (!network)
		return NULL;

	return network->interface;
}

const char *g_supplicant_network_get_name(GSupplicantNetwork *network)
{
	if (!network || !network->name)
		return "";

	return network->name;
}

const char *g_supplicant_network_get_identifier(GSupplicantNetwork *network)
{
	if (!network || !network->group)
		return "";

	return network->group;
}

const char *g_supplicant_network_get_path(GSupplicantNetwork *network)
{
	if (!network || !network->path)
		return NULL;

	return network->path;
}

const char *g_supplicant_network_get_mode(GSupplicantNetwork *network)
{
	if (!network)
		return G_SUPPLICANT_MODE_UNKNOWN;

	return mode2string(network->mode);
}

const char *g_supplicant_network_get_security(GSupplicantNetwork *network)
{
	if (!network)
		return G_SUPPLICANT_SECURITY_UNKNOWN;

	return security2string(network->security);
}

const void *g_supplicant_network_get_ssid(GSupplicantNetwork *network,
						unsigned int *ssid_len)
{
	if (!network) {
		*ssid_len = 0;
		return NULL;
	}

	*ssid_len = network->ssid_len;
	return network->ssid;
}

dbus_int16_t g_supplicant_network_get_signal(GSupplicantNetwork *network)
{
	if (!network)
		return 0;

	return network->signal;
}

dbus_uint16_t g_supplicant_network_get_frequency(GSupplicantNetwork *network)
{
	if (!network)
		return 0;

	return network->frequency;
}

dbus_bool_t g_supplicant_network_get_wps(GSupplicantNetwork *network)
{
	if (!network)
		return FALSE;

	return network->wps;
}

dbus_bool_t g_supplicant_network_is_wps_active(GSupplicantNetwork *network)
{
	if (!network)
		return FALSE;

	if (network->wps_capabilities & G_SUPPLICANT_WPS_CONFIGURED)
		return TRUE;

	return FALSE;
}

dbus_bool_t g_supplicant_network_is_wps_pbc(GSupplicantNetwork *network)
{
	if (!network)
		return FALSE;

	if (network->wps_capabilities & G_SUPPLICANT_WPS_PBC)
		return TRUE;

	return FALSE;
}

dbus_bool_t g_supplicant_network_is_wps_advertizing(GSupplicantNetwork *network)
{
	if (!network)
		return FALSE;

	if (network->wps_capabilities & G_SUPPLICANT_WPS_REGISTRAR)
		return TRUE;

	return FALSE;
}

GSupplicantInterface *g_supplicant_peer_get_interface(GSupplicantPeer *peer)
{
	if (!peer)
		return NULL;

	return peer->interface;
}

const char *g_supplicant_peer_get_path(GSupplicantPeer *peer)
{
	if (!peer)
		return NULL;

	return peer->path;
}

const char *g_supplicant_peer_get_identifier(GSupplicantPeer *peer)
{
	if (!peer)
		return NULL;

	return peer->identifier;
}

const void *g_supplicant_peer_get_device_address(GSupplicantPeer *peer)
{
	if (!peer)
		return NULL;

	return peer->device_address;
}

const void *g_supplicant_peer_get_iface_address(GSupplicantPeer *peer)
{
	if (!peer)
		return NULL;

	return peer->iface_address;
}

const char *g_supplicant_peer_get_name(GSupplicantPeer *peer)
{
	if (!peer)
		return NULL;

	return peer->name;
}

const unsigned char *g_supplicant_peer_get_widi_ies(GSupplicantPeer *peer,
								int *length)
{
	if (!peer || !length)
		return NULL;

	*length = peer->widi_ies_length;
	return peer->widi_ies;
}

bool g_supplicant_peer_is_wps_pbc(GSupplicantPeer *peer)
{
	if (!peer)
		return false;

	if (peer->wps_capabilities & G_SUPPLICANT_WPS_PBC)
		return true;

	return false;
}

bool g_supplicant_peer_is_wps_pin(GSupplicantPeer *peer)
{
	if (!peer)
		return false;

	if (peer->wps_capabilities & G_SUPPLICANT_WPS_PIN)
		return true;

	return false;
}

bool g_supplicant_peer_is_in_a_group(GSupplicantPeer *peer)
{
	if (!peer || !peer->groups)
		return false;

	return true;
}

GSupplicantInterface *g_supplicant_peer_get_group_interface(GSupplicantPeer *peer)
{
	if (!peer)
		return NULL;

	return (GSupplicantInterface *) peer->current_group_iface;
}

bool g_supplicant_peer_is_client(GSupplicantPeer *peer)
{
	GSupplicantGroup *group;
	GSList *list;

	if (!peer)
		return false;

	for (list = peer->groups; list; list = list->next) {
		const char *path = list->data;

		group = g_hash_table_lookup(group_mapping, path);
		if (!group)
			continue;

		if (group->role != G_SUPPLICANT_GROUP_ROLE_CLIENT ||
				group->orig_interface != peer->interface)
			continue;

		if (group->interface == peer->current_group_iface)
			return true;
	}

	return false;
}

bool g_supplicant_peer_has_requested_connection(GSupplicantPeer *peer)
{
	if (!peer)
		return false;

	return peer->connection_requested;
}

static void merge_network(GSupplicantNetwork *network)
{
	GString *str;
	const char *ssid, *mode, *key_mgmt;
	unsigned int i, ssid_len;
	char *group;

	ssid = g_hash_table_lookup(network->config_table, "ssid");
	mode = g_hash_table_lookup(network->config_table, "mode");
	key_mgmt = g_hash_table_lookup(network->config_table, "key_mgmt");

	SUPPLICANT_DBG("ssid %s mode %s", ssid, mode);

	if (ssid)
		ssid_len = strlen(ssid);
	else
		ssid_len = 0;

	str = g_string_sized_new((ssid_len * 2) + 24);
	if (!str)
		return;

	for (i = 0; i < ssid_len; i++)
		g_string_append_printf(str, "%02x", ssid[i]);

	if (g_strcmp0(mode, "0") == 0)
		g_string_append_printf(str, "_managed");
	else if (g_strcmp0(mode, "1") == 0)
		g_string_append_printf(str, "_adhoc");

	if (g_strcmp0(key_mgmt, "WPA-PSK") == 0)
		g_string_append_printf(str, "_psk");

	group = g_string_free(str, FALSE);

	SUPPLICANT_DBG("%s", group);

	g_free(group);

	g_hash_table_destroy(network->config_table);

	g_free(network->path);
	g_free(network);
}

static void network_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	GSupplicantNetwork *network = user_data;

	if (!network->interface)
		return;

	if (!key) {
		merge_network(network);
		return;
	}

	if (g_strcmp0(key, "Enabled") == 0) {
		dbus_bool_t enabled = FALSE;

		dbus_message_iter_get_basic(iter, &enabled);
	} else if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str) {
			g_hash_table_replace(network->config_table,
						g_strdup(key), g_strdup(str));
		}
	} else
		SUPPLICANT_DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static void interface_network_added(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	GSupplicantNetwork *network;
	const char *path = NULL;

	SUPPLICANT_DBG("");

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	if (g_strcmp0(path, "/") == 0)
		return;

	network = g_hash_table_lookup(interface->net_mapping, path);
	if (network)
		return;

	network = g_try_new0(GSupplicantNetwork, 1);
	if (!network)
		return;

	network->interface = interface;
	network->path = g_strdup(path);

	network->config_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							g_free, g_free);

	dbus_message_iter_next(iter);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {
		supplicant_dbus_property_foreach(iter, network_property,
								network);
		network_property(NULL, NULL, network);
		return;
	}

	supplicant_dbus_property_get_all(path,
				SUPPLICANT_INTERFACE ".Network",
					network_property, network, NULL);
}

static void interface_network_removed(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	GSupplicantNetwork *network;
	const char *path = NULL;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	network = g_hash_table_lookup(interface->net_mapping, path);
	if (!network)
		return;

	g_hash_table_remove(interface->net_mapping, path);
}

static char *create_name(unsigned char *ssid, int ssid_len)
{
	GString *string;
	const gchar *remainder, *invalid;
	int valid_bytes, remaining_bytes;

	if (ssid_len < 1 || ssid[0] == '\0')
		return g_strdup("");

	string = NULL;
	remainder = (const gchar *)ssid;
	remaining_bytes = ssid_len;

	while (remaining_bytes != 0) {
		if (g_utf8_validate(remainder, remaining_bytes,
					&invalid)) {
			break;
		}

		valid_bytes = invalid - remainder;

		if (!string)
			string = g_string_sized_new(remaining_bytes);

		g_string_append_len(string, remainder, valid_bytes);

		/* append U+FFFD REPLACEMENT CHARACTER */
		g_string_append(string, "\357\277\275");

		remaining_bytes -= valid_bytes + 1;
		remainder = invalid + 1;
	}

	if (!string)
		return g_strndup((const gchar *)ssid, ssid_len + 1);

	g_string_append(string, remainder);

	return g_string_free(string, FALSE);
}

static char *create_group(struct g_supplicant_bss *bss)
{
	GString *str;
	unsigned int i;
	const char *mode, *security;

	str = g_string_sized_new((bss->ssid_len * 2) + 24);
	if (!str)
		return NULL;

	if (bss->ssid_len > 0 && bss->ssid[0] != '\0') {
		for (i = 0; i < bss->ssid_len; i++)
			g_string_append_printf(str, "%02x", bss->ssid[i]);
	} else
		g_string_append_printf(str, "hidden");

	mode = mode2string(bss->mode);
	if (mode)
		g_string_append_printf(str, "_%s", mode);

	security = security2string(bss->security);
	if (security)
		g_string_append_printf(str, "_%s", security);

	return g_string_free(str, FALSE);
}

static void add_or_replace_bss_to_network(struct g_supplicant_bss *bss)
{
	GSupplicantInterface *interface = bss->interface;
	GSupplicantNetwork *network;
	char *group;

	group = create_group(bss);
	SUPPLICANT_DBG("New group created: %s", group);

	if (!group)
		return;

	network = g_hash_table_lookup(interface->network_table, group);
	if (network) {
		g_free(group);
		SUPPLICANT_DBG("Network %s already exist", network->name);

		goto done;
	}

	network = g_try_new0(GSupplicantNetwork, 1);
	if (!network) {
		g_free(group);
		return;
	}

	network->interface = interface;
	if (!network->path)
		network->path = g_strdup(bss->path);
	network->group = group;
	network->name = create_name(bss->ssid, bss->ssid_len);
	network->mode = bss->mode;
	network->security = bss->security;
	network->ssid_len = bss->ssid_len;
	memcpy(network->ssid, bss->ssid, bss->ssid_len);
	network->signal = bss->signal;
	network->frequency = bss->frequency;
	network->best_bss = bss;

	SUPPLICANT_DBG("New network %s created", network->name);

	network->bss_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							NULL, remove_bss);

	network->config_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							g_free, g_free);

	g_hash_table_replace(interface->network_table,
						network->group, network);

	callback_network_added(network);

done:
	/* We update network's WPS properties if only bss provides WPS. */
	if ((bss->keymgmt & G_SUPPLICANT_KEYMGMT_WPS) != 0) {
		network->wps = TRUE;
		network->wps_capabilities |= bss->wps_capabilities;
	}

	if (bss->signal > network->signal) {
		network->signal = bss->signal;
		network->best_bss = bss;
		callback_network_changed(network, "Signal");
	}

	g_hash_table_replace(interface->bss_mapping, bss->path, network);
	g_hash_table_replace(network->bss_table, bss->path, bss);

	g_hash_table_replace(bss_mapping, bss->path, interface);
}

static void bss_rates(DBusMessageIter *iter, void *user_data)
{
	struct g_supplicant_bss *bss = user_data;
	dbus_uint32_t rate = 0;

	dbus_message_iter_get_basic(iter, &rate);
	if (rate == 0)
		return;

	if (rate > bss->maxrate)
		bss->maxrate = rate;
}

static void bss_keymgmt(DBusMessageIter *iter, void *user_data)
{
	unsigned int *keymgmt = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; keymgmt_map[i].str; i++)
		if (strcmp(str, keymgmt_map[i].str) == 0) {
			SUPPLICANT_DBG("Keymgmt: %s", str);
			*keymgmt |= keymgmt_map[i].val;
			break;
		}
}

static void bss_group(DBusMessageIter *iter, void *user_data)
{
	unsigned int *group = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; group_map[i].str; i++)
		if (strcmp(str, group_map[i].str) == 0) {
			SUPPLICANT_DBG("Group: %s", str);
			*group |= group_map[i].val;
			break;
		}
}

static void bss_pairwise(DBusMessageIter *iter, void *user_data)
{
	unsigned int *pairwise = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; pairwise_map[i].str; i++)
		if (strcmp(str, pairwise_map[i].str) == 0) {
			SUPPLICANT_DBG("Pairwise: %s", str);
			*pairwise |= pairwise_map[i].val;
			break;
		}
}

static void bss_wpa(const char *key, DBusMessageIter *iter,
			void *user_data)
{
	struct g_supplicant_bss *bss = user_data;
	unsigned int value = 0;

	SUPPLICANT_DBG("Key: %s", key);

	if (g_strcmp0(key, "KeyMgmt") == 0) {
		supplicant_dbus_array_foreach(iter, bss_keymgmt, &value);

		if (bss->rsn_selected)
			bss->rsn_keymgmt = value;
		else
			bss->wpa_keymgmt = value;
	} else if (g_strcmp0(key, "Group") == 0) {
		supplicant_dbus_array_foreach(iter, bss_group, &value);

		if (bss->rsn_selected)
			bss->rsn_group = value;
		else
			bss->wpa_group = value;
	} else if (g_strcmp0(key, "Pairwise") == 0) {
		supplicant_dbus_array_foreach(iter, bss_pairwise, &value);

		if (bss->rsn_selected)
			bss->rsn_pairwise = value;
		else
			bss->wpa_pairwise = value;
	}
}

static unsigned int get_tlv(unsigned char *ie, unsigned int ie_size,
							unsigned int type)
{
	unsigned int len = 0;

	while (len + 4 < ie_size) {
		unsigned int hi = ie[len];
		unsigned int lo = ie[len + 1];
		unsigned int tmp_type = (hi << 8) + lo;
		unsigned int v_len = 0;

		/* hi and lo are used to recreate an unsigned int
		 * based on 2 8bits length unsigned int. */

		hi = ie[len + 2];
		lo = ie[len + 3];
		v_len = (hi << 8) + lo;

		if (tmp_type == type) {
			unsigned int ret_value = 0;
			unsigned char *value = (unsigned char *)&ret_value;

			SUPPLICANT_DBG("IE: match type 0x%x", type);

			/* Verifying length relevance */
			if (v_len > sizeof(unsigned int) ||
				len + 4 + v_len > ie_size)
				break;

			memcpy(value, ie + len + 4, v_len);

			SUPPLICANT_DBG("returning 0x%x", ret_value);
			return ret_value;
		}

		len += v_len + 4;
	}

	SUPPLICANT_DBG("returning 0");
	return 0;
}

static void bss_process_ies(DBusMessageIter *iter, void *user_data)
{
	struct g_supplicant_bss *bss = user_data;
	const unsigned char WPS_OUI[] = { 0x00, 0x50, 0xf2, 0x04 };
	unsigned char *ie, *ie_end;
	DBusMessageIter array;
	unsigned int value;
	int ie_len;

#define WMM_WPA1_WPS_INFO 221
#define WPS_INFO_MIN_LEN  6
#define WPS_VERSION_TLV   0x104A
#define WPS_STATE_TLV     0x1044
#define WPS_METHODS_TLV   0x1012
#define WPS_REGISTRAR_TLV 0x1041
#define WPS_VERSION       0x10
#define WPS_PBC           0x04
#define WPS_PIN           0x00
#define WPS_CONFIGURED    0x02

	dbus_message_iter_recurse(iter, &array);
	dbus_message_iter_get_fixed_array(&array, &ie, &ie_len);

	if (!ie || ie_len < 2) {
		SUPPLICANT_DBG("IE: TOO SHORT");
		return;
	}

	bss->wps_capabilities = 0;
	bss->keymgmt = 0;

	for (ie_end = ie + ie_len; ie < ie_end && ie + ie[1] + 1 <= ie_end;
							ie += ie[1] + 2) {

		if (ie[0] != WMM_WPA1_WPS_INFO || ie[1] < WPS_INFO_MIN_LEN ||
			memcmp(ie+2, WPS_OUI, sizeof(WPS_OUI)) != 0)
			continue;

		SUPPLICANT_DBG("IE: match WPS_OUI");

		value = get_tlv(&ie[6], ie[1], WPS_STATE_TLV);
		if (get_tlv(&ie[6], ie[1], WPS_VERSION_TLV) == WPS_VERSION &&
								value != 0) {
			bss->keymgmt |= G_SUPPLICANT_KEYMGMT_WPS;

			if (value == WPS_CONFIGURED)
				bss->wps_capabilities |=
					G_SUPPLICANT_WPS_CONFIGURED;
		}

		value = get_tlv(&ie[6], ie[1], WPS_METHODS_TLV);
		if (value != 0) {
			if (GUINT16_FROM_BE(value) == WPS_PBC)
				bss->wps_capabilities |= G_SUPPLICANT_WPS_PBC;
			if (GUINT16_FROM_BE(value) == WPS_PIN)
				bss->wps_capabilities |= G_SUPPLICANT_WPS_PIN;
		} else
			bss->wps_capabilities |=
				G_SUPPLICANT_WPS_PBC | G_SUPPLICANT_WPS_PIN;

		/* If the AP sends this it means it's advertizing
		 * as a registrar and the WPS process is launched
		 * on its side */
		if (get_tlv(&ie[6], ie[1], WPS_REGISTRAR_TLV) != 0)
			bss->wps_capabilities |= G_SUPPLICANT_WPS_REGISTRAR;

		SUPPLICANT_DBG("WPS Methods 0x%x", bss->wps_capabilities);
	}
}

static void bss_compute_security(struct g_supplicant_bss *bss)
{
	/*
	 * Combining RSN and WPA keymgmt
	 * We combine it since parsing IEs might have set something for WPS. */
	bss->keymgmt |= bss->rsn_keymgmt | bss->wpa_keymgmt;

	bss->ieee8021x = FALSE;
	bss->psk = FALSE;

	if (bss->keymgmt &
			(G_SUPPLICANT_KEYMGMT_WPA_EAP |
				G_SUPPLICANT_KEYMGMT_WPA_FT_EAP |
				G_SUPPLICANT_KEYMGMT_WPA_EAP_256))
		bss->ieee8021x = TRUE;

	if (bss->keymgmt &
			(G_SUPPLICANT_KEYMGMT_WPA_PSK |
				G_SUPPLICANT_KEYMGMT_WPA_FT_PSK |
				G_SUPPLICANT_KEYMGMT_WPA_PSK_256))
		bss->psk = TRUE;

	if (bss->ieee8021x)
		bss->security = G_SUPPLICANT_SECURITY_IEEE8021X;
	else if (bss->psk)
		bss->security = G_SUPPLICANT_SECURITY_PSK;
	else if (bss->privacy)
		bss->security = G_SUPPLICANT_SECURITY_WEP;
	else
		bss->security = G_SUPPLICANT_SECURITY_NONE;
}


static void bss_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	struct g_supplicant_bss *bss = user_data;

	if (!bss->interface)
		return;

	SUPPLICANT_DBG("key %s", key);

	if (!key)
		return;

	if (g_strcmp0(key, "BSSID") == 0) {
		DBusMessageIter array;
		unsigned char *addr;
		int addr_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &addr, &addr_len);

		if (addr_len == 6)
			memcpy(bss->bssid, addr, addr_len);
	} else if (g_strcmp0(key, "SSID") == 0) {
		DBusMessageIter array;
		unsigned char *ssid;
		int ssid_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &ssid, &ssid_len);

		if (ssid_len > 0 && ssid_len < 33) {
			memcpy(bss->ssid, ssid, ssid_len);
			bss->ssid_len = ssid_len;
		} else {
			memset(bss->ssid, 0, sizeof(bss->ssid));
			bss->ssid_len = 0;
		}
	} else if (g_strcmp0(key, "Capabilities") == 0) {
		dbus_uint16_t capabilities = 0x0000;

		dbus_message_iter_get_basic(iter, &capabilities);

		if (capabilities & IEEE80211_CAP_ESS)
			bss->mode = G_SUPPLICANT_MODE_INFRA;
		else if (capabilities & IEEE80211_CAP_IBSS)
			bss->mode = G_SUPPLICANT_MODE_IBSS;

		if (capabilities & IEEE80211_CAP_PRIVACY)
			bss->privacy = TRUE;
	} else if (g_strcmp0(key, "Mode") == 0) {
		const char *mode = NULL;

		dbus_message_iter_get_basic(iter, &mode);
		bss->mode = string2mode(mode);
	} else if (g_strcmp0(key, "Frequency") == 0) {
		dbus_uint16_t frequency = 0;

		dbus_message_iter_get_basic(iter, &frequency);
		bss->frequency = frequency;
	} else if (g_strcmp0(key, "Signal") == 0) {
		dbus_int16_t signal = 0;

		dbus_message_iter_get_basic(iter, &signal);

		bss->signal = signal;
		if (!bss->signal)
			bss->signal = BSS_UNKNOWN_STRENGTH;

	} else if (g_strcmp0(key, "Level") == 0) {
		dbus_int32_t level = 0;

		dbus_message_iter_get_basic(iter, &level);
	} else if (g_strcmp0(key, "Rates") == 0) {
		supplicant_dbus_array_foreach(iter, bss_rates, bss);
	} else if (g_strcmp0(key, "MaxRate") == 0) {
		dbus_uint32_t maxrate = 0;

		dbus_message_iter_get_basic(iter, &maxrate);
		if (maxrate != 0)
			bss->maxrate = maxrate;
	} else if (g_strcmp0(key, "Privacy") == 0) {
		dbus_bool_t privacy = FALSE;

		dbus_message_iter_get_basic(iter, &privacy);
		bss->privacy = privacy;
	} else if (g_strcmp0(key, "RSN") == 0) {
		bss->rsn_selected = TRUE;

		supplicant_dbus_property_foreach(iter, bss_wpa, bss);
	} else if (g_strcmp0(key, "WPA") == 0) {
		bss->rsn_selected = FALSE;

		supplicant_dbus_property_foreach(iter, bss_wpa, bss);
	} else if (g_strcmp0(key, "IEs") == 0)
		bss_process_ies(iter, bss);
	else
		SUPPLICANT_DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static struct g_supplicant_bss *interface_bss_added(DBusMessageIter *iter,
							void *user_data)
{
	GSupplicantInterface *interface = user_data;
	GSupplicantNetwork *network;
	struct g_supplicant_bss *bss;
	const char *path = NULL;

	SUPPLICANT_DBG("");

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return NULL;

	if (g_strcmp0(path, "/") == 0)
		return NULL;

	SUPPLICANT_DBG("%s", path);

	network = g_hash_table_lookup(interface->bss_mapping, path);
	if (network) {
		bss = g_hash_table_lookup(network->bss_table, path);
		if (bss)
			return NULL;
	}

	bss = g_try_new0(struct g_supplicant_bss, 1);
	if (!bss)
		return NULL;

	bss->interface = interface;
	bss->path = g_strdup(path);
	bss->signal = BSS_UNKNOWN_STRENGTH;

	return bss;
}

static void interface_bss_added_with_keys(DBusMessageIter *iter,
						void *user_data)
{
	struct g_supplicant_bss *bss;

	SUPPLICANT_DBG("");

	bss = interface_bss_added(iter, user_data);
	if (!bss)
		return;

	dbus_message_iter_next(iter);

	if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_INVALID)
		return;

	supplicant_dbus_property_foreach(iter, bss_property, bss);

	bss_compute_security(bss);
	add_or_replace_bss_to_network(bss);
}

static void interface_bss_added_without_keys(DBusMessageIter *iter,
						void *user_data)
{
	struct g_supplicant_bss *bss;

	SUPPLICANT_DBG("");

	bss = interface_bss_added(iter, user_data);
	if (!bss)
		return;

	supplicant_dbus_property_get_all(bss->path,
					SUPPLICANT_INTERFACE ".BSS",
					bss_property, bss, NULL);

	bss_compute_security(bss);
	add_or_replace_bss_to_network(bss);
}

static void update_signal(gpointer key, gpointer value,
						gpointer user_data)
{
	struct g_supplicant_bss *bss = value;
	GSupplicantNetwork *network = user_data;

	if (bss->signal > network->signal) {
		network->signal = bss->signal;
		network->best_bss = bss;
	}
}

static void update_network_signal(GSupplicantNetwork *network)
{
	if (g_hash_table_size(network->bss_table) <= 1 && network->best_bss)
		return;

	g_hash_table_foreach(network->bss_table,
				update_signal, network);

	SUPPLICANT_DBG("New network signal %d", network->signal);
}

static void interface_bss_removed(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	GSupplicantNetwork *network;
	struct g_supplicant_bss *bss = NULL;
	const char *path = NULL;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	network = g_hash_table_lookup(interface->bss_mapping, path);
	if (!network)
		return;

	bss = g_hash_table_lookup(network->bss_table, path);
	if (network->best_bss == bss) {
		network->best_bss = NULL;
		network->signal = BSS_UNKNOWN_STRENGTH;
	}

	g_hash_table_remove(bss_mapping, path);

	g_hash_table_remove(interface->bss_mapping, path);
	g_hash_table_remove(network->bss_table, path);

	update_network_signal(network);

	if (g_hash_table_size(network->bss_table) == 0)
		g_hash_table_remove(interface->network_table, network->group);
}

static void set_config_methods(DBusMessageIter *iter, void *user_data)
{
	const char *config_methods = "virtual_push_button virtual_display";

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
							&config_methods);
}

static void interface_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	GSupplicantInterface *interface = user_data;

	if (!interface)
		return;

	SUPPLICANT_DBG("%s", key);

	if (!key) {
		debug_strvalmap("KeyMgmt capability", keymgmt_map,
						interface->keymgmt_capa);
		debug_strvalmap("AuthAlg capability", authalg_capa_map,
						interface->authalg_capa);
		debug_strvalmap("Protocol capability", proto_capa_map,
						interface->proto_capa);
		debug_strvalmap("Pairwise capability", pairwise_map,
						interface->pairwise_capa);
		debug_strvalmap("Group capability", group_map,
						interface->group_capa);
		debug_strvalmap("Scan capability", scan_capa_map,
						interface->scan_capa);
		debug_strvalmap("Mode capability", mode_capa_map,
						interface->mode_capa);


		supplicant_dbus_property_set(interface->path,
				SUPPLICANT_INTERFACE ".Interface.WPS",
				"ConfigMethods", DBUS_TYPE_STRING_AS_STRING,
				set_config_methods, NULL, NULL, NULL);

		if (interface->ready)
			callback_interface_added(interface);

		return;
	}

	if (g_strcmp0(key, "Capabilities") == 0) {
		supplicant_dbus_property_foreach(iter, interface_capability,
								interface);
		if (interface->mode_capa & G_SUPPLICANT_CAPABILITY_MODE_P2P)
			interface->p2p_support = true;
	} else if (g_strcmp0(key, "State") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str)
			if (string2state(str) != interface->state) {
				interface->state = string2state(str);
				callback_interface_state(interface);
			}
		if (interface->state == G_SUPPLICANT_STATE_DISABLED)
			interface->ready = FALSE;
		else
			interface->ready = TRUE;

		SUPPLICANT_DBG("state %s (%d)", str, interface->state);
	} else if (g_strcmp0(key, "Scanning") == 0) {
		dbus_bool_t scanning = FALSE;

		dbus_message_iter_get_basic(iter, &scanning);
		interface->scanning = scanning;

		if (interface->ready) {
			if (interface->scanning)
				callback_scan_started(interface);
			else
				callback_scan_finished(interface);
		}
	} else if (g_strcmp0(key, "ApScan") == 0) {
		int apscan = 1;

		dbus_message_iter_get_basic(iter, &apscan);
		interface->apscan = apscan;
	} else if (g_strcmp0(key, "Ifname") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str) {
			g_free(interface->ifname);
			interface->ifname = g_strdup(str);
		}
	} else if (g_strcmp0(key, "Driver") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str) {
			g_free(interface->driver);
			interface->driver = g_strdup(str);
		}
	} else if (g_strcmp0(key, "BridgeIfname") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str) {
			g_free(interface->bridge);
			interface->bridge = g_strdup(str);
		}
	} else if (g_strcmp0(key, "CurrentBSS") == 0) {
		interface_bss_added_without_keys(iter, interface);
	} else if (g_strcmp0(key, "CurrentNetwork") == 0) {
		interface_network_added(iter, interface);
	} else if (g_strcmp0(key, "BSSs") == 0) {
		supplicant_dbus_array_foreach(iter,
					interface_bss_added_without_keys,
					interface);
	} else if (g_strcmp0(key, "Blobs") == 0) {
		/* Nothing */
	} else if (g_strcmp0(key, "Networks") == 0) {
		supplicant_dbus_array_foreach(iter, interface_network_added,
								interface);
	} else
		SUPPLICANT_DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static void scan_network_update(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface = user_data;
	GSupplicantNetwork *network;
	char *path;

	if (!iter)
		return;

	dbus_message_iter_get_basic(iter, &path);

	if (!path)
		return;

	if (g_strcmp0(path, "/") == 0)
		return;

	/* Update the network details based on scan BSS data */
	network = g_hash_table_lookup(interface->bss_mapping, path);
	if (network)
		callback_network_added(network);
}

static void scan_bss_data(const char *key, DBusMessageIter *iter,
				void *user_data)
{
	GSupplicantInterface *interface = user_data;

	if (iter)
		supplicant_dbus_array_foreach(iter, scan_network_update,
						interface);

	if (interface->scan_callback)
		interface->scan_callback(0, interface, interface->scan_data);

	interface->scan_callback = NULL;
	interface->scan_data = NULL;
}

static GSupplicantInterface *interface_alloc(const char *path)
{
	GSupplicantInterface *interface;

	interface = g_try_new0(GSupplicantInterface, 1);
	if (!interface)
		return NULL;

	interface->path = g_strdup(path);

	interface->network_table = g_hash_table_new_full(g_str_hash,
					g_str_equal, NULL, remove_network);
	interface->peer_table = g_hash_table_new_full(g_str_hash,
					g_str_equal, NULL, remove_peer);
	interface->group_table = g_hash_table_new_full(g_str_hash,
					g_str_equal, NULL, remove_group);
	interface->net_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);
	interface->bss_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);

	g_hash_table_replace(interface_table, interface->path, interface);

	return interface;
}

static void interface_added(DBusMessageIter *iter, void *user_data)
{
	GSupplicantInterface *interface;
	const char *path = NULL;

	SUPPLICANT_DBG("");

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	if (g_strcmp0(path, "/") == 0)
		return;

	interface = g_hash_table_lookup(interface_table, path);
	if (interface)
		return;

	interface = interface_alloc(path);
	if (!interface)
		return;

	dbus_message_iter_next(iter);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {
		supplicant_dbus_property_foreach(iter, interface_property,
								interface);
		interface_property(NULL, NULL, interface);
		return;
	}

	supplicant_dbus_property_get_all(path,
					SUPPLICANT_INTERFACE ".Interface",
					interface_property, interface,
					interface);
}

static void interface_removed(DBusMessageIter *iter, void *user_data)
{
	const char *path = NULL;
	GSupplicantInterface *interface = user_data;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	interface = g_hash_table_lookup(interface_table, path);
	g_supplicant_interface_cancel(interface);

	g_hash_table_remove(interface_table, path);
}

static void eap_method(DBusMessageIter *iter, void *user_data)
{
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; eap_method_map[i].str; i++)
		if (strcmp(str, eap_method_map[i].str) == 0) {
			eap_methods |= eap_method_map[i].val;
			break;
		}
}

static void service_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	if (!key) {
		callback_system_ready();
		return;
	}

	if (g_strcmp0(key, "DebugLevel") == 0) {
		const char *str = NULL;
		int i;

		dbus_message_iter_get_basic(iter, &str);
		for (i = 0; debug_strings[i]; i++)
			if (g_strcmp0(debug_strings[i], str) == 0) {
				debug_level = i;
				break;
			}
		SUPPLICANT_DBG("Debug level %d", debug_level);
	} else if (g_strcmp0(key, "DebugTimestamp") == 0) {
		dbus_message_iter_get_basic(iter, &debug_timestamp);
		SUPPLICANT_DBG("Debug timestamp %u", debug_timestamp);
	} else if (g_strcmp0(key, "DebugShowKeys") == 0) {
		dbus_message_iter_get_basic(iter, &debug_showkeys);
		SUPPLICANT_DBG("Debug show keys %u", debug_showkeys);
	} else if (g_strcmp0(key, "Interfaces") == 0) {
		supplicant_dbus_array_foreach(iter, interface_added, NULL);
	} else if (g_strcmp0(key, "EapMethods") == 0) {
		supplicant_dbus_array_foreach(iter, eap_method, NULL);
		debug_strvalmap("EAP method", eap_method_map, eap_methods);
	} else if (g_strcmp0(key, "Country") == 0) {
		const char *country = NULL;

		dbus_message_iter_get_basic(iter, &country);
		SUPPLICANT_DBG("Country %s", country);
	} else
		SUPPLICANT_DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static void signal_name_owner_changed(const char *path, DBusMessageIter *iter)
{
	const char *name = NULL, *old = NULL, *new = NULL;

	SUPPLICANT_DBG("");

	if (g_strcmp0(path, DBUS_PATH_DBUS) != 0)
		return;

	dbus_message_iter_get_basic(iter, &name);
	if (!name)
		return;

	if (g_strcmp0(name, SUPPLICANT_SERVICE) != 0)
		return;

	dbus_message_iter_next(iter);
	dbus_message_iter_get_basic(iter, &old);
	dbus_message_iter_next(iter);
	dbus_message_iter_get_basic(iter, &new);

	if (!old || !new)
		return;

	if (strlen(old) > 0 && strlen(new) == 0) {
		system_available = FALSE;
		g_hash_table_remove_all(bss_mapping);
		g_hash_table_remove_all(peer_mapping);
		g_hash_table_remove_all(group_mapping);
		g_hash_table_remove_all(interface_table);
		callback_system_killed();
	}

	if (strlen(new) > 0 && strlen(old) == 0) {
		system_available = TRUE;
		supplicant_dbus_property_get_all(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						service_property, NULL, NULL);
	}
}

static void signal_properties_changed(const char *path, DBusMessageIter *iter)
{
	SUPPLICANT_DBG("");

	if (g_strcmp0(path, SUPPLICANT_PATH) != 0)
		return;

	supplicant_dbus_property_foreach(iter, service_property, NULL);
}

static void signal_interface_added(const char *path, DBusMessageIter *iter)
{
	SUPPLICANT_DBG("path %s %s", path, SUPPLICANT_PATH);

	if (g_strcmp0(path, SUPPLICANT_PATH) == 0)
		interface_added(iter, NULL);
}

static void signal_interface_removed(const char *path, DBusMessageIter *iter)
{
	SUPPLICANT_DBG("");

	if (g_strcmp0(path, SUPPLICANT_PATH) == 0)
		interface_removed(iter, NULL);
}

static void signal_interface_changed(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	supplicant_dbus_property_foreach(iter, interface_property, interface);
}

static void signal_scan_done(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	dbus_bool_t success = FALSE;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	dbus_message_iter_get_basic(iter, &success);

	if (interface->scanning) {
		callback_scan_finished(interface);
		interface->scanning = FALSE;
	}

	/*
	 * If scan is unsuccessful return -EIO else get the scanned BSSs
	 * and update the network details accordingly
	 */
	if (!success) {
		if (interface->scan_callback)
			interface->scan_callback(-EIO, interface,
						interface->scan_data);

		interface->scan_callback = NULL;
		interface->scan_data = NULL;

		return;
	}

	supplicant_dbus_property_get(path, SUPPLICANT_INTERFACE ".Interface",
				"BSSs", scan_bss_data, interface, interface);
}

static void signal_bss_added(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_bss_added_with_keys(iter, interface);
}

static void signal_bss_removed(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_bss_removed(iter, interface);
}

static void signal_network_added(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_network_added(iter, interface);
}

static void signal_network_removed(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_network_removed(iter, interface);
}

static void signal_bss_changed(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	GSupplicantNetwork *network;
	GSupplicantSecurity old_security;
	struct g_supplicant_bss *bss;
	unsigned int old_keymgmt;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(bss_mapping, path);
	if (!interface)
		return;

	network = g_hash_table_lookup(interface->bss_mapping, path);
	if (!network)
		return;

	bss = g_hash_table_lookup(network->bss_table, path);
	if (!bss)
		return;

	old_keymgmt = bss->keymgmt;
	supplicant_dbus_property_foreach(iter, bss_property, bss);

	old_security = network->security;
	bss_compute_security(bss);

	if (old_security != bss->security) {
		struct g_supplicant_bss *new_bss;

		SUPPLICANT_DBG("New network security for %s", bss->ssid);

		/* Security change policy:
		 * - we first copy the current bss into a new one with
		 * its own pointer (path)
		 * - we remove the current bss related network which will
		 * tell the plugin about such removal. This is done due
		 * to the fact that a security change means a group change
		 * so a complete network change.
		 * (current bss becomes invalid as well)
		 * - we add the new bss: it adds new network and tell the
		 * plugin about it. */

		new_bss = g_try_new0(struct g_supplicant_bss, 1);
		if (!new_bss)
			return;

		memcpy(new_bss, bss, sizeof(struct g_supplicant_bss));
		new_bss->path = g_strdup(bss->path);

		g_hash_table_remove(interface->network_table, network->group);

		add_or_replace_bss_to_network(new_bss);

		return;
	}

	if (bss->signal != network->signal)
	{
		/*
		 * If the new signal is lower than the SSID signal, we need
		 * to check for the new maximum.
		 */
		if (bss->signal < network->signal) {
			if (bss == network->best_bss)
			{
				network->signal = bss->signal;
				update_network_signal(network);
			}
		} else {
			network->signal = bss->signal;
			network->best_bss = bss;
		}

		SUPPLICANT_DBG("New network signal for %s %d dBm", network->ssid,
					   network->signal);

		callback_network_changed(network, "Signal");
	}

	/* Check if WPS was enabled some time after the network as added */
	if (old_keymgmt != bss->keymgmt &&
			(bss->keymgmt & G_SUPPLICANT_KEYMGMT_WPS) != 0) {
		network->wps = TRUE;
		network->wps_capabilities |= bss->wps_capabilities;

		SUPPLICANT_DBG("WPS became enabled after the network was added");
		callback_network_changed(network, "WiFi.WPS");
	}
}

static void wps_credentials(const char *key, DBusMessageIter *iter,
			void *user_data)
{
	GSupplicantInterface *interface = user_data;

	if (!key)
		return;

	SUPPLICANT_DBG("key %s", key);

	if (g_strcmp0(key, "Key") == 0) {
		DBusMessageIter array;
		unsigned char *key_val;
		int key_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &key_val, &key_len);

		g_free(interface->wps_cred.key);
		interface->wps_cred.key = g_try_malloc0(
						sizeof(char) * key_len + 1);

		if (!interface->wps_cred.key)
			return;

		memcpy(interface->wps_cred.key, key_val,
						sizeof(char) * key_len);

		SUPPLICANT_DBG("WPS key present");
	} else if (g_strcmp0(key, "SSID") == 0) {
		DBusMessageIter array;
		unsigned char *ssid;
		int ssid_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &ssid, &ssid_len);

		if (ssid_len > 0 && ssid_len < 33) {
			memcpy(interface->wps_cred.ssid, ssid, ssid_len);
			interface->wps_cred.ssid_len = ssid_len;
		} else {
			memset(interface->wps_cred.ssid, 0, 32);
			interface->wps_cred.ssid_len = 0;
		}
	}
}

static void signal_wps_credentials(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	supplicant_dbus_property_foreach(iter, wps_credentials, interface);
}

static void wps_event_args(const char *key, DBusMessageIter *iter,
			void *user_data)
{
	GSupplicantInterface *interface = user_data;

	if (!key || !interface)
		return;

	SUPPLICANT_DBG("Arg Key %s", key);
}

static void signal_wps_event(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	const char *name = NULL;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	dbus_message_iter_get_basic(iter, &name);

	SUPPLICANT_DBG("Name: %s", name);

	if (g_strcmp0(name, "success") == 0)
		interface->wps_state = G_SUPPLICANT_WPS_STATE_SUCCESS;
	else if (g_strcmp0(name, "fail") == 0)
		interface->wps_state = G_SUPPLICANT_WPS_STATE_FAIL;
	else
		interface->wps_state = G_SUPPLICANT_WPS_STATE_UNKNOWN;

	if (!dbus_message_iter_has_next(iter))
		return;

	dbus_message_iter_next(iter);

	supplicant_dbus_property_foreach(iter, wps_event_args, interface);
}

static void create_peer_identifier(GSupplicantPeer *peer)
{
	const unsigned char test[ETH_ALEN] = {};

	if (!peer)
		return;

	if (!memcmp(peer->device_address, test, ETH_ALEN)) {
		peer->identifier = g_strdup(peer->name);
		return;
	}

	peer->identifier = g_malloc0(19);
	snprintf(peer->identifier, 19, "%02x%02x%02x%02x%02x%02x",
						peer->device_address[0],
						peer->device_address[1],
						peer->device_address[2],
						peer->device_address[3],
						peer->device_address[4],
						peer->device_address[5]);
}

struct peer_property_data {
	GSupplicantPeer *peer;
	GSList *old_groups;
	bool groups_changed;
	bool services_changed;
};

static void peer_groups_relation(DBusMessageIter *iter, void *user_data)
{
	struct peer_property_data *data = user_data;
	GSupplicantPeer *peer = data->peer;
	GSupplicantGroup *group;
	const char *str = NULL;
	GSList *elem;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	group = g_hash_table_lookup(group_mapping, str);
	if (!group)
		return;

	elem = g_slist_find_custom(data->old_groups, str, g_str_equal);
	if (elem) {
		data->old_groups = g_slist_remove_link(data->old_groups, elem);
		peer->groups = g_slist_concat(elem, peer->groups);
	} else {
		peer->groups = g_slist_prepend(peer->groups, g_strdup(str));
		data->groups_changed = true;
	}
}

static void peer_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	GSupplicantPeer *pending_peer;
	struct peer_property_data *data = user_data;
	GSupplicantPeer *peer = data->peer;

	SUPPLICANT_DBG("key: %s", key);

	if (!peer->interface)
		return;

	if (!key) {
		if (peer->name) {
			create_peer_identifier(peer);
			callback_peer_found(peer);
			pending_peer = g_hash_table_lookup(
					pending_peer_connection, peer->path);

			if (pending_peer && pending_peer == peer) {
				callback_peer_request(peer);
				g_hash_table_remove(pending_peer_connection,
						peer->path);
			}

			dbus_free(data);
		}

		return;
	}

	if (g_strcmp0(key, "DeviceAddress") == 0) {
		unsigned char *dev_addr;
		DBusMessageIter array;
		int len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &dev_addr, &len);

		if (len == ETH_ALEN)
			memcpy(peer->device_address, dev_addr, len);
	} else if (g_strcmp0(key, "DeviceName") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str)
			peer->name = g_strdup(str);
	} else if (g_strcmp0(key, "config_method") == 0) {
		uint16_t wps_config;

		dbus_message_iter_get_basic(iter, &wps_config);

		if (wps_config & G_SUPPLICANT_WPS_CONFIG_PBC)
			peer->wps_capabilities |= G_SUPPLICANT_WPS_PBC;
		if (wps_config & ~G_SUPPLICANT_WPS_CONFIG_PBC)
			peer->wps_capabilities |= G_SUPPLICANT_WPS_PIN;
	} else if (g_strcmp0(key, "Groups") == 0) {
		data->old_groups = peer->groups;
		peer->groups = NULL;

		supplicant_dbus_array_foreach(iter,
						peer_groups_relation, data);
		if (g_slist_length(data->old_groups) > 0) {
			g_slist_free_full(data->old_groups, g_free);
			data->groups_changed = true;
		}
	} else if (g_strcmp0(key, "IEs") == 0) {
		DBusMessageIter array;
		unsigned char *ie;
		int ie_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &ie, &ie_len);

		if (!ie || ie_len < 2)
			return;

		if (peer->widi_ies) {
			if (memcmp(peer->widi_ies, ie, ie_len) == 0)
				return;

			g_free(peer->widi_ies);
			peer->widi_ies_length = 0;
		}

		peer->widi_ies = g_malloc0(ie_len * sizeof(unsigned char));

		memcpy(peer->widi_ies, ie, ie_len);
		peer->widi_ies_length = ie_len;
		data->services_changed = true;
	}
}

static void signal_peer_found(const char *path, DBusMessageIter *iter)
{
	struct peer_property_data *property_data;
	GSupplicantInterface *interface;
	const char *obj_path = NULL;
	GSupplicantPeer *peer;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	dbus_message_iter_get_basic(iter, &obj_path);
	if (!obj_path || g_strcmp0(obj_path, "/") == 0)
		return;

	peer = g_hash_table_lookup(interface->peer_table, obj_path);
	if (peer)
		return;

	peer = g_try_new0(GSupplicantPeer, 1);
	if (!peer)
		return;

	peer->interface = interface;
	peer->path = g_strdup(obj_path);
	g_hash_table_insert(interface->peer_table, peer->path, peer);
	g_hash_table_replace(peer_mapping, peer->path, interface);

	property_data = dbus_malloc0(sizeof(struct peer_property_data));
	property_data->peer = peer;

	dbus_message_iter_next(iter);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {
		supplicant_dbus_property_foreach(iter, peer_property,
							property_data);
		peer_property(NULL, NULL, property_data);
		return;
	}

	supplicant_dbus_property_get_all(obj_path,
					SUPPLICANT_INTERFACE ".Peer",
					peer_property, property_data, NULL);
}

static void signal_peer_lost(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	const char *obj_path = NULL;
	GSupplicantPeer *peer;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	dbus_message_iter_get_basic(iter, &obj_path);
	if (!obj_path || g_strcmp0(obj_path, "/") == 0)
		return;

	peer = g_hash_table_lookup(interface->peer_table, obj_path);
	if (!peer)
		return;

	g_hash_table_remove(interface->peer_table, obj_path);
}

static void signal_peer_changed(const char *path, DBusMessageIter *iter)
{
	struct peer_property_data *property_data;
	GSupplicantInterface *interface;
	GSupplicantPeer *peer;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(peer_mapping, path);
	if (!interface)
		return;

	peer = g_hash_table_lookup(interface->peer_table, path);
	if (!peer) {
		g_hash_table_remove(peer_mapping, path);
		return;
	}

	property_data = dbus_malloc0(sizeof(struct peer_property_data));
	property_data->peer = peer;

	supplicant_dbus_property_foreach(iter, peer_property, property_data);
	if (property_data->services_changed)
		callback_peer_changed(peer,
					G_SUPPLICANT_PEER_SERVICES_CHANGED);

	if (property_data->groups_changed)
		callback_peer_changed(peer, G_SUPPLICANT_PEER_GROUP_CHANGED);

	dbus_free(property_data);

	if (!g_supplicant_peer_is_in_a_group(peer))
		peer->connection_requested = false;
}

struct group_sig_data {
	const char *peer_obj_path;
	unsigned char iface_address[ETH_ALEN];
	const char *interface_obj_path;
	const char *group_obj_path;
	int role;
};

static void group_sig_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	struct group_sig_data *data = user_data;

	if (!key)
		return;

	if (g_strcmp0(key, "peer_interface_addr") == 0) {
		unsigned char *dev_addr;
		DBusMessageIter array;
		int len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &dev_addr, &len);

		if (len == ETH_ALEN)
			memcpy(data->iface_address, dev_addr, len);
	} else if (g_strcmp0(key, "role") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (g_strcmp0(str, "GO") == 0)
			data->role = G_SUPPLICANT_GROUP_ROLE_GO;
		else
			data->role = G_SUPPLICANT_GROUP_ROLE_CLIENT;
	} else if (g_strcmp0(key, "peer_object") == 0)
		dbus_message_iter_get_basic(iter, &data->peer_obj_path);
	else if (g_strcmp0(key, "interface_object") == 0)
		dbus_message_iter_get_basic(iter, &data->interface_obj_path);
	else if (g_strcmp0(key, "group_object") == 0)
		dbus_message_iter_get_basic(iter, &data->group_obj_path);

}

static void signal_group_success(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	struct group_sig_data data = {};
	GSupplicantPeer *peer;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	supplicant_dbus_property_foreach(iter, group_sig_property, &data);
	if (!data.peer_obj_path)
		return;

	peer = g_hash_table_lookup(interface->peer_table, data.peer_obj_path);
	if (!peer)
		return;

	memcpy(peer->iface_address, data.iface_address, ETH_ALEN);
	interface->pending_peer_path = peer->path;
}

static void signal_group_failure(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	struct group_sig_data data = {};
	GSupplicantPeer *peer;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	supplicant_dbus_property_foreach(iter, group_sig_property, &data);
	if (!data.peer_obj_path)
		return;

	peer = g_hash_table_lookup(interface->peer_table, data.peer_obj_path);
	if (!peer)
		return;

	callback_peer_changed(peer, G_SUPPLICANT_PEER_GROUP_FAILED);
	peer->connection_requested = false;
}

static void signal_group_started(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface, *g_interface;
	struct group_sig_data data = {};
	GSupplicantGroup *group;
	GSupplicantPeer *peer;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	supplicant_dbus_property_foreach(iter, group_sig_property, &data);
	if (!data.interface_obj_path || !data.group_obj_path)
		return;

	peer = g_hash_table_lookup(interface->peer_table,
						interface->pending_peer_path);
	interface->pending_peer_path = NULL;
	if (!peer)
		return;

	g_interface = g_hash_table_lookup(interface_table,
						data.interface_obj_path);
	if (!g_interface)
		return;

	group = g_hash_table_lookup(interface->group_table,
						data.group_obj_path);
	if (group)
		return;

	group = g_try_new0(GSupplicantGroup, 1);
	if (!group)
		return;

	group->interface = g_interface;
	group->orig_interface = interface;
	group->path = g_strdup(data.group_obj_path);
	group->role = data.role;

	g_hash_table_insert(interface->group_table, group->path, group);
	g_hash_table_replace(group_mapping, group->path, group);

	peer->current_group_iface = g_interface;
	callback_peer_changed(peer, G_SUPPLICANT_PEER_GROUP_STARTED);
}

static void remove_peer_group_interface(GHashTable *group_table,
				const char* path)
{
	GSupplicantGroup *group;
	GHashTableIter iter;
	gpointer value, key;

	if (!group_table)
		return;

	group = g_hash_table_lookup(group_table, path);

	if (!group || !group->orig_interface)
		return;

	g_hash_table_iter_init(&iter, group->orig_interface->peer_table);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		GSupplicantPeer *peer = value;

		if (peer->current_group_iface == group->interface)
			peer->current_group_iface = NULL;
	}
}

static void signal_group_finished(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	struct group_sig_data data = {};

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	supplicant_dbus_property_foreach(iter, group_sig_property, &data);
	if (!data.interface_obj_path || !data.group_obj_path)
		return;

	remove_peer_group_interface(interface->group_table, data.group_obj_path);

	g_hash_table_remove(group_mapping, data.group_obj_path);

	g_hash_table_remove(interface->group_table, data.group_obj_path);
}

static void signal_group_request(const char *path, DBusMessageIter *iter)
{
	GSupplicantInterface *interface;
	GSupplicantPeer *peer;
	const char *obj_path;

	SUPPLICANT_DBG("");

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	dbus_message_iter_get_basic(iter, &obj_path);
	if (!obj_path || !g_strcmp0(obj_path, "/"))
		return;

	peer = g_hash_table_lookup(interface->peer_table, obj_path);
	if (!peer)
		return;

	/*
	 * Peer has been previously found and property set,
	 * otherwise, defer connection to when peer property
	 * is set.
	 */
	if (peer->identifier)
		callback_peer_request(peer);
	else
		g_hash_table_replace(pending_peer_connection, peer->path, peer);
}

static void signal_group_peer_joined(const char *path, DBusMessageIter *iter)
{
	const char *peer_path = NULL;
	GSupplicantInterface *interface;
	GSupplicantGroup *group;
	GSupplicantPeer *peer;

	SUPPLICANT_DBG("");

	group = g_hash_table_lookup(group_mapping, path);
	if (!group)
		return;

	dbus_message_iter_get_basic(iter, &peer_path);
	if (!peer_path)
		return;

	interface = g_hash_table_lookup(peer_mapping, peer_path);
	if (!interface)
		return;

	peer = g_hash_table_lookup(interface->peer_table, peer_path);
	if (!peer)
		return;

	group->members = g_slist_prepend(group->members, g_strdup(peer_path));

	callback_peer_changed(peer, G_SUPPLICANT_PEER_GROUP_JOINED);
}

static void signal_group_peer_disconnected(const char *path, DBusMessageIter *iter)
{
	const char *peer_path = NULL;
	GSupplicantInterface *interface;
	GSupplicantGroup *group;
	GSupplicantPeer *peer;
	GSList *elem;

	SUPPLICANT_DBG("");

	group = g_hash_table_lookup(group_mapping, path);
	if (!group)
		return;

	dbus_message_iter_get_basic(iter, &peer_path);
	if (!peer_path)
		return;

	for (elem = group->members; elem; elem = elem->next) {
		if (!g_strcmp0(elem->data, peer_path))
			break;
	}

	if (!elem)
		return;

	g_free(elem->data);
	group->members = g_slist_delete_link(group->members, elem);

	interface = g_hash_table_lookup(peer_mapping, peer_path);
	if (!interface)
		return;

	peer = g_hash_table_lookup(interface->peer_table, peer_path);
	if (!peer)
		return;

	callback_peer_changed(peer, G_SUPPLICANT_PEER_GROUP_DISCONNECTED);
	peer->connection_requested = false;
}

static struct {
	const char *interface;
	const char *member;
	void (*function) (const char *path, DBusMessageIter *iter);
} signal_map[] = {
	{ DBUS_INTERFACE_DBUS,  "NameOwnerChanged",  signal_name_owner_changed },

	{ SUPPLICANT_INTERFACE, "PropertiesChanged", signal_properties_changed },
	{ SUPPLICANT_INTERFACE, "InterfaceAdded",    signal_interface_added    },
	{ SUPPLICANT_INTERFACE, "InterfaceCreated",  signal_interface_added    },
	{ SUPPLICANT_INTERFACE, "InterfaceRemoved",  signal_interface_removed  },

	{ SUPPLICANT_INTERFACE ".Interface", "PropertiesChanged", signal_interface_changed },
	{ SUPPLICANT_INTERFACE ".Interface", "ScanDone",          signal_scan_done         },
	{ SUPPLICANT_INTERFACE ".Interface", "BSSAdded",          signal_bss_added         },
	{ SUPPLICANT_INTERFACE ".Interface", "BSSRemoved",        signal_bss_removed       },
	{ SUPPLICANT_INTERFACE ".Interface", "NetworkAdded",      signal_network_added     },
	{ SUPPLICANT_INTERFACE ".Interface", "NetworkRemoved",    signal_network_removed   },

	{ SUPPLICANT_INTERFACE ".BSS", "PropertiesChanged", signal_bss_changed   },

	{ SUPPLICANT_INTERFACE ".Interface.WPS", "Credentials", signal_wps_credentials },
	{ SUPPLICANT_INTERFACE ".Interface.WPS", "Event",       signal_wps_event       },

	{ SUPPLICANT_INTERFACE ".Interface.P2PDevice", "DeviceFound", signal_peer_found },
	{ SUPPLICANT_INTERFACE ".Interface.P2PDevice", "DeviceLost",  signal_peer_lost  },

	{ SUPPLICANT_INTERFACE ".Peer", "PropertiesChanged", signal_peer_changed },

	{ SUPPLICANT_INTERFACE ".Interface.P2PDevice", "GONegotiationSuccess", signal_group_success },
	{ SUPPLICANT_INTERFACE ".Interface.P2PDevice", "GONegotiationFailure", signal_group_failure },
	{ SUPPLICANT_INTERFACE ".Interface.P2PDevice", "GroupStarted", signal_group_started },
	{ SUPPLICANT_INTERFACE ".Interface.P2PDevice", "GroupFinished", signal_group_finished },
	{ SUPPLICANT_INTERFACE ".Interface.P2PDevice", "GONegotiationRequest", signal_group_request },

	{ SUPPLICANT_INTERFACE ".Group", "PeerJoined", signal_group_peer_joined },
	{ SUPPLICANT_INTERFACE ".Group", "PeerDisconnected", signal_group_peer_disconnected },

	{ }
};

static DBusHandlerResult g_supplicant_filter(DBusConnection *conn,
					DBusMessage *message, void *data)
{
	DBusMessageIter iter;
	const char *path;
	int i;

	path = dbus_message_get_path(message);
	if (!path)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_iter_init(message, &iter))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	for (i = 0; signal_map[i].interface; i++) {
		if (!dbus_message_has_interface(message, signal_map[i].interface))
			continue;

		if (!dbus_message_has_member(message, signal_map[i].member))
			continue;

		signal_map[i].function(path, &iter);
		break;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void g_supplicant_interface_cancel(GSupplicantInterface *interface)
{
	SUPPLICANT_DBG("Cancelling any pending DBus calls");
	supplicant_dbus_method_call_cancel_all(interface);
	supplicant_dbus_property_call_cancel_all(interface);
}

struct supplicant_regdom {
	GSupplicantCountryCallback callback;
	const char *alpha2;
	const void *user_data;
};

static void country_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct supplicant_regdom *regdom = user_data;
	int result = 0;

	SUPPLICANT_DBG("Country setting result");

	if (!user_data)
		return;

	if (error) {
		SUPPLICANT_DBG("Country setting failure %s", error);
		result = -EINVAL;
	}

	if (regdom->callback)
		regdom->callback(result, regdom->alpha2,
					(void *) regdom->user_data);

	g_free(regdom);
}

static void country_params(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_regdom *regdom = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
							&regdom->alpha2);
}

int g_supplicant_set_country(const char *alpha2,
				GSupplicantCountryCallback callback,
					const void *user_data)
{
	struct supplicant_regdom *regdom;

	SUPPLICANT_DBG("Country setting %s", alpha2);

	if (!system_available)
		return -EFAULT;

	regdom = dbus_malloc0(sizeof(*regdom));
	if (!regdom)
		return -ENOMEM;

	regdom->callback = callback;
	regdom->alpha2 = alpha2;
	regdom->user_data = user_data;

	return supplicant_dbus_property_set(SUPPLICANT_PATH, SUPPLICANT_INTERFACE,
					"Country", DBUS_TYPE_STRING_AS_STRING,
					country_params, country_result,
					regdom, NULL);
}

int g_supplicant_interface_set_country(GSupplicantInterface *interface,
					GSupplicantCountryCallback callback,
							const char *alpha2,
							void *user_data)
{
	struct supplicant_regdom *regdom;

	regdom = dbus_malloc0(sizeof(*regdom));
	if (!regdom)
		return -ENOMEM;

	regdom->callback = callback;
	regdom->alpha2 = alpha2;
	regdom->user_data = user_data;

	return supplicant_dbus_property_set(interface->path,
				SUPPLICANT_INTERFACE ".Interface",
				"Country", DBUS_TYPE_STRING_AS_STRING,
				country_params, country_result,
					regdom, NULL);
}

bool g_supplicant_interface_has_p2p(GSupplicantInterface *interface)
{
	if (!interface)
		return false;

	return interface->p2p_support;
}

struct supplicant_p2p_dev_config {
	char *device_name;
	char *dev_type;
};

static void p2p_device_config_result(const char *error,
					DBusMessageIter *iter, void *user_data)
{
	struct supplicant_p2p_dev_config *config = user_data;

	if (error)
		SUPPLICANT_DBG("Unable to set P2P Device configuration: %s",
									error);

	g_free(config->device_name);
	g_free(config->dev_type);
	dbus_free(config);
}

static int dev_type_str2bin(const char *type, unsigned char dev_type[8])
{
	int length, pos, end;
	char b[3] = {};
	char *e = NULL;

	end = strlen(type);
	for (length = pos = 0; type[pos] != '\0' && length < 8; length++) {
		if (pos+2 > end)
			return 0;

		b[0] = type[pos];
		b[1] = type[pos+1];

		dev_type[length] = strtol(b, &e, 16);
		if (e && *e != '\0')
			return 0;

		pos += 2;
	}

	return 8;
}

static void p2p_device_config_params(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_p2p_dev_config *config = user_data;
	DBusMessageIter dict;

	supplicant_dbus_dict_open(iter, &dict);

	supplicant_dbus_dict_append_basic(&dict, "DeviceName",
				DBUS_TYPE_STRING, &config->device_name);

	if (config->dev_type) {
		unsigned char dev_type[8] = {}, *type;
		int len;

		len = dev_type_str2bin(config->dev_type, dev_type);
		if (len) {
			type = dev_type;
			supplicant_dbus_dict_append_fixed_array(&dict,
					"PrimaryDeviceType",
					DBUS_TYPE_BYTE, &type, len);
		}
	}

	supplicant_dbus_dict_close(iter, &dict);
}

int g_supplicant_interface_set_p2p_device_config(GSupplicantInterface *interface,
					const char *device_name,
					const char *primary_dev_type)
{
	struct supplicant_p2p_dev_config *config;
	int ret;

	SUPPLICANT_DBG("P2P Device settings %s/%s",
					device_name, primary_dev_type);

	config = dbus_malloc0(sizeof(*config));
	if (!config)
		return -ENOMEM;

	config->device_name = g_strdup(device_name);
	config->dev_type = g_strdup(primary_dev_type);

	ret = supplicant_dbus_property_set(interface->path,
				SUPPLICANT_INTERFACE ".Interface.P2PDevice",
				"P2PDeviceConfig",
				DBUS_TYPE_ARRAY_AS_STRING
				DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
				DBUS_TYPE_STRING_AS_STRING
				DBUS_TYPE_VARIANT_AS_STRING
				DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
				p2p_device_config_params,
				p2p_device_config_result, config, NULL);
	if (ret < 0) {
		g_free(config->device_name);
		g_free(config->dev_type);
		dbus_free(config);
		SUPPLICANT_DBG("Unable to set P2P Device configuration");
	}

	return ret;
}

static gboolean peer_lookup_by_identifier(gpointer key, gpointer value,
							gpointer user_data)
{
	const GSupplicantPeer *peer = value;
	const char *identifier = user_data;

	if (!g_strcmp0(identifier, peer->identifier))
		return TRUE;

	return FALSE;
}

GSupplicantPeer *g_supplicant_interface_peer_lookup(GSupplicantInterface *interface,
							const char *identifier)
{
	GSupplicantPeer *peer;

	peer = g_hash_table_find(interface->peer_table,
					peer_lookup_by_identifier,
					(void *) identifier);
	return peer;
}

struct interface_data {
	GSupplicantInterface *interface;
	char *path; /* Interface path cannot be taken from interface (above) as
		     * it might have been freed already.
		     */
	GSupplicantInterfaceCallback callback;
	void *user_data;
};

struct interface_create_data {
	char *ifname;
	char *driver;
	char *bridge;
	GSupplicantInterface *interface;
	GSupplicantInterfaceCallback callback;
	void *user_data;
};

struct interface_connect_data {
	GSupplicantInterface *interface;
	char *path;
	GSupplicantInterfaceCallback callback;
	union {
		GSupplicantSSID *ssid;
		GSupplicantPeerParams *peer;
	};
	void *user_data;
};

struct interface_scan_data {
	GSupplicantInterface *interface;
	char *path;
	GSupplicantInterfaceCallback callback;
	GSupplicantScanParams *scan_params;
	void *user_data;
};

static void interface_create_data_free(struct interface_create_data *data)
{
	g_free(data->ifname);
	g_free(data->driver);
	g_free(data->bridge);
	dbus_free(data);
}

static bool interface_exists(GSupplicantInterface *interface,
				const char *path)
{
	GSupplicantInterface *tmp;

	tmp = g_hash_table_lookup(interface_table, path);
	if (tmp && tmp == interface)
		return true;

	return false;
}

static void interface_create_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	struct interface_create_data *data = user_data;
	GSupplicantInterface *interface = data->interface;

	if (!key) {
		if (data->callback) {
			data->callback(0, data->interface, data->user_data);
			callback_p2p_support(interface);
		}

		interface_create_data_free(data);
	}

	interface_property(key, iter, interface);
}

static void interface_create_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;
	const char *path = NULL;
	int err;

	SUPPLICANT_DBG("");

	if (error) {
		g_warning("error %s", error);
		err = -EIO;
		goto done;
	}

	dbus_message_iter_get_basic(iter, &path);
	if (!path) {
		err = -EINVAL;
		goto done;
	}

	if (!system_available) {
		err = -EFAULT;
		goto done;
	}

	data->interface = g_hash_table_lookup(interface_table, path);
	if (!data->interface) {
		data->interface = interface_alloc(path);
		if (!data->interface) {
			err = -ENOMEM;
			goto done;
		}
	}

	err = supplicant_dbus_property_get_all(path,
					SUPPLICANT_INTERFACE ".Interface",
					interface_create_property, data,
					NULL);
	if (err == 0)
		return;

done:
	if (data->callback)
		data->callback(err, NULL, data->user_data);

	interface_create_data_free(data);
}

static void interface_create_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;
	DBusMessageIter dict;

	SUPPLICANT_DBG("");

	supplicant_dbus_dict_open(iter, &dict);

	supplicant_dbus_dict_append_basic(&dict, "Ifname",
					DBUS_TYPE_STRING, &data->ifname);

	if (data->driver)
		supplicant_dbus_dict_append_basic(&dict, "Driver",
					DBUS_TYPE_STRING, &data->driver);

	if (data->bridge)
		supplicant_dbus_dict_append_basic(&dict, "BridgeIfname",
					DBUS_TYPE_STRING, &data->bridge);

	supplicant_dbus_dict_close(iter, &dict);
}

static void interface_get_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;
	GSupplicantInterface *interface;
	const char *path = NULL;
	int err;

	SUPPLICANT_DBG("");

	if (error) {
		SUPPLICANT_DBG("Interface not created yet");
		goto create;
	}

	dbus_message_iter_get_basic(iter, &path);
	if (!path) {
		err = -EINVAL;
		goto done;
	}

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface) {
		err = -ENOENT;
		goto done;
	}

	if (data->callback) {
		data->callback(0, interface, data->user_data);
		callback_p2p_support(interface);
	}

	interface_create_data_free(data);

	return;

create:
	if (!system_available) {
		err = -EFAULT;
		goto done;
	}

	SUPPLICANT_DBG("Creating interface");

	err = supplicant_dbus_method_call(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						"CreateInterface",
						interface_create_params,
						interface_create_result, data,
						NULL);
	if (err == 0)
		return;

done:
	if (data->callback)
		data->callback(err, NULL, data->user_data);

	interface_create_data_free(data);
}

static void interface_get_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;

	SUPPLICANT_DBG("");

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &data->ifname);
}

int g_supplicant_interface_create(const char *ifname, const char *driver,
					const char *bridge,
					GSupplicantInterfaceCallback callback,
							void *user_data)
{
	struct interface_create_data *data;
	int ret;

	SUPPLICANT_DBG("ifname %s", ifname);

	if (!ifname)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->ifname = g_strdup(ifname);
	data->driver = g_strdup(driver);
	data->bridge = g_strdup(bridge);
	data->callback = callback;
	data->user_data = user_data;

	ret = supplicant_dbus_method_call(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						"GetInterface",
						interface_get_params,
						interface_get_result, data,
						NULL);
	if (ret < 0)
		interface_create_data_free(data);

	return ret;
}

static void interface_remove_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_data *data = user_data;
	int err;

	if (error) {
		err = -EIO;
		goto done;
	}

	if (!system_available) {
		err = -EFAULT;
		goto done;
	}

	/*
	 * The gsupplicant interface is already freed by the InterfaceRemoved
	 * signal callback. Simply invoke the interface_data callback.
	 */
	err = 0;

done:
	g_free(data->path);

	if (data->callback)
		data->callback(err, NULL, data->user_data);

	dbus_free(data);
}


static void interface_remove_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_data *data = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
							&data->interface->path);
}


int g_supplicant_interface_remove(GSupplicantInterface *interface,
			GSupplicantInterfaceCallback callback,
							void *user_data)
{
	struct interface_data *data;
	int ret;

	if (!interface)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	g_supplicant_interface_cancel(interface);

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->interface = interface;
	data->path = g_strdup(interface->path);
	data->callback = callback;
	data->user_data = user_data;

	ret = supplicant_dbus_method_call(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						"RemoveInterface",
						interface_remove_params,
						interface_remove_result, data,
						NULL);
	if (ret < 0) {
		g_free(data->path);
		dbus_free(data);
	}
	return ret;
}

static void interface_scan_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_scan_data *data = user_data;
	int err = 0;

	if (error) {
		SUPPLICANT_DBG("error %s", error);
		err = -EIO;
	}

	/* A non ready interface cannot send/receive anything */
	if (interface_exists(data->interface, data->path)) {
		if (!data->interface->ready)
			err = -ENOLINK;
	}

	g_free(data->path);

	if (err != 0) {
		if (data->callback)
			data->callback(err, data->interface, data->user_data);
	} else {
		data->interface->scan_callback = data->callback;
		data->interface->scan_data = data->user_data;
	}

	if (data->scan_params)
		g_supplicant_free_scan_params(data->scan_params);

	dbus_free(data);
}

static void add_scan_frequency(DBusMessageIter *iter, unsigned int freq)
{
	DBusMessageIter data;
	unsigned int width = 0; /* Not used by wpa_supplicant atm */

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &data);

	dbus_message_iter_append_basic(&data, DBUS_TYPE_UINT32, &freq);
	dbus_message_iter_append_basic(&data, DBUS_TYPE_UINT32, &width);

	dbus_message_iter_close_container(iter, &data);
}

static void add_scan_frequencies(DBusMessageIter *iter,
						void *user_data)
{
	GSupplicantScanParams *scan_data = user_data;
	unsigned int freq;
	int i;

	for (i = 0; i < scan_data->num_freqs; i++) {
		freq = scan_data->freqs[i];
		if (!freq)
			break;

		add_scan_frequency(iter, freq);
	}
}

static void append_ssid(DBusMessageIter *iter,
			const void *ssid, unsigned int len)
{
	DBusMessageIter array;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
	DBUS_TYPE_BYTE_AS_STRING, &array);

	dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
								&ssid, len);
	dbus_message_iter_close_container(iter, &array);
}

static void append_ssids(DBusMessageIter *iter, void *user_data)
{
	GSupplicantScanParams *scan_data = user_data;
	GSList *list;

	for (list = scan_data->ssids; list; list = list->next) {
		struct scan_ssid *scan_ssid = list->data;

		append_ssid(iter, scan_ssid->ssid, scan_ssid->ssid_len);
	}
}

static void supplicant_add_scan_frequency(DBusMessageIter *dict,
		supplicant_dbus_array_function function,
					void *user_data)
{
	GSupplicantScanParams *scan_params = user_data;
	DBusMessageIter entry, value, array;
	const char *key = "Channels";

	if (scan_params->freqs && scan_params->freqs[0] != 0) {
		dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
						NULL, &entry);

		dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
					DBUS_TYPE_ARRAY_AS_STRING
					DBUS_STRUCT_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_UINT32_AS_STRING
					DBUS_TYPE_UINT32_AS_STRING
					DBUS_STRUCT_END_CHAR_AS_STRING,
					&value);

		dbus_message_iter_open_container(&value, DBUS_TYPE_ARRAY,
					DBUS_STRUCT_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_UINT32_AS_STRING
					DBUS_TYPE_UINT32_AS_STRING
					DBUS_STRUCT_END_CHAR_AS_STRING,
					&array);

		if (function)
			function(&array, user_data);

		dbus_message_iter_close_container(&value, &array);
		dbus_message_iter_close_container(&entry, &value);
		dbus_message_iter_close_container(dict, &entry);
	}
}

static void interface_scan_params(DBusMessageIter *iter, void *user_data)
{
	DBusMessageIter dict;
	const char *type = "passive";
	struct interface_scan_data *data = user_data;

	supplicant_dbus_dict_open(iter, &dict);

	if (data && data->scan_params) {
		type = "active";

		supplicant_dbus_dict_append_basic(&dict, "Type",
					DBUS_TYPE_STRING, &type);

		supplicant_dbus_dict_append_array(&dict, "SSIDs",
						DBUS_TYPE_STRING,
						append_ssids,
						data->scan_params);

		supplicant_add_scan_frequency(&dict, add_scan_frequencies,
						data->scan_params);
	} else
		supplicant_dbus_dict_append_basic(&dict, "Type",
					DBUS_TYPE_STRING, &type);

	supplicant_dbus_dict_close(iter, &dict);
}

static int interface_ready_to_scan(GSupplicantInterface *interface)
{
	if (!interface)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	if (interface->scanning)
		return -EALREADY;

	switch (interface->state) {
	case G_SUPPLICANT_STATE_AUTHENTICATING:
	case G_SUPPLICANT_STATE_ASSOCIATING:
	case G_SUPPLICANT_STATE_ASSOCIATED:
	case G_SUPPLICANT_STATE_4WAY_HANDSHAKE:
	case G_SUPPLICANT_STATE_GROUP_HANDSHAKE:
		return -EBUSY;
	case G_SUPPLICANT_STATE_UNKNOWN:
	case G_SUPPLICANT_STATE_DISABLED:
	case G_SUPPLICANT_STATE_DISCONNECTED:
	case G_SUPPLICANT_STATE_INACTIVE:
	case G_SUPPLICANT_STATE_SCANNING:
	case G_SUPPLICANT_STATE_COMPLETED:
		break;
	}

	return 0;
}

int g_supplicant_interface_scan(GSupplicantInterface *interface,
				GSupplicantScanParams *scan_data,
				GSupplicantInterfaceCallback callback,
							void *user_data)
{
	struct interface_scan_data *data;
	int ret;

	ret = interface_ready_to_scan(interface);
	if (ret)
		return ret;

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->interface = interface;
	data->path = g_strdup(interface->path);
	data->callback = callback;
	data->user_data = user_data;
	data->scan_params = scan_data;

        interface->scan_callback = callback;
        interface->scan_data = user_data;

	ret = supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface", "Scan",
			interface_scan_params, interface_scan_result, data,
			interface);

	if (ret < 0) {
		g_free(data->path);
		dbus_free(data);
	}

	return ret;
}

static int parse_supplicant_error(DBusMessageIter *iter)
{
	int err = -ECANCELED;
	char *key;

	if (!iter)
		return err;

	/* If the given passphrase is malformed wpa_s returns
	 * "invalid message format" but this error should be interpreted as
	 * invalid-key.
	 */
	while (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING) {
		dbus_message_iter_get_basic(iter, &key);
		if (strncmp(key, "psk", 3) == 0 ||
				strncmp(key, "wep_key", 7) == 0 ||
				strcmp(key, "invalid message format") == 0) {
			err = -ENOKEY;
			break;
		}
		dbus_message_iter_next(iter);
	}

	return err;
}

static void interface_select_network_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_connect_data *data = user_data;
	int err;

	SUPPLICANT_DBG("");

	err = 0;
	if (error) {
		SUPPLICANT_DBG("SelectNetwork error %s", error);
		err = parse_supplicant_error(iter);
	}

	g_free(data->path);

	if (data->callback)
		data->callback(err, data->interface, data->user_data);

	g_free(data->ssid);
	dbus_free(data);
}

static void interface_select_network_params(DBusMessageIter *iter,
							void *user_data)
{
	struct interface_connect_data *data = user_data;
	GSupplicantInterface *interface = data->interface;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
					&interface->network_path);
}

static void interface_add_network_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_connect_data *data = user_data;
	GSupplicantInterface *interface = data->interface;
	const char *path;
	int err;

	if (error)
		goto error;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		goto error;

	SUPPLICANT_DBG("PATH: %s", path);

	g_free(interface->network_path);
	interface->network_path = g_strdup(path);

	supplicant_dbus_method_call(data->interface->path,
			SUPPLICANT_INTERFACE ".Interface", "SelectNetwork",
			interface_select_network_params,
			interface_select_network_result, data,
			interface);

	return;

error:
	SUPPLICANT_DBG("AddNetwork error %s", error);

	if (interface_exists(data->interface, data->interface->path)) {
		err = parse_supplicant_error(iter);
		if (data->callback)
			data->callback(err, data->interface, data->user_data);

		g_free(interface->network_path);
		interface->network_path = NULL;
	}

	g_free(data->path);
	g_free(data->ssid);
	g_free(data);
}

static void add_network_security_none(DBusMessageIter *dict)
{
	const char *auth_alg = "OPEN";

	supplicant_dbus_dict_append_basic(dict, "auth_alg",
					DBUS_TYPE_STRING, &auth_alg);
}

static void add_network_security_wep(DBusMessageIter *dict,
					GSupplicantSSID *ssid)
{
	const char *auth_alg = "OPEN SHARED";
	dbus_uint32_t key_index = 0;

	supplicant_dbus_dict_append_basic(dict, "auth_alg",
					DBUS_TYPE_STRING, &auth_alg);

	if (ssid->passphrase) {
		int size = strlen(ssid->passphrase);
		if (size == 10 || size == 26) {
			unsigned char *key = g_try_malloc(13);
			char tmp[3];
			int i;

			memset(tmp, 0, sizeof(tmp));
			if (!key)
				size = 0;

			for (i = 0; i < size / 2; i++) {
				memcpy(tmp, ssid->passphrase + (i * 2), 2);
				key[i] = (unsigned char) strtol(tmp, NULL, 16);
			}

			supplicant_dbus_dict_append_fixed_array(dict,
							"wep_key0",
							DBUS_TYPE_BYTE,
							&key, size / 2);
			g_free(key);
		} else if (size == 5 || size == 13) {
			unsigned char *key = g_try_malloc(13);
			int i;

			if (!key)
				size = 0;

			for (i = 0; i < size; i++)
				key[i] = (unsigned char) ssid->passphrase[i];

			supplicant_dbus_dict_append_fixed_array(dict,
								"wep_key0",
								DBUS_TYPE_BYTE,
								&key, size);
			g_free(key);
		} else
			supplicant_dbus_dict_append_basic(dict,
							"wep_key0",
							DBUS_TYPE_STRING,
							&ssid->passphrase);

		supplicant_dbus_dict_append_basic(dict, "wep_tx_keyidx",
					DBUS_TYPE_UINT32, &key_index);
	}
}

static dbus_bool_t is_psk_raw_key(const char *psk)
{
	int i;

	/* A raw key is always 64 bytes length... */
	if (strlen(psk) != 64)
		return FALSE;

	/* ... and its content is in hex representation */
	for (i = 0; i < 64; i++)
		if (!isxdigit((unsigned char) psk[i]))
			return FALSE;

	return TRUE;
}

static unsigned char hexchar2bin(char c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';
	else if ((c >= 'A') && (c <= 'F'))
		return c - 'A' + 10;
	else if ((c >= 'a') && (c <= 'f'))
		return c - 'a' + 10;
	else
		return c;
}

static void hexstring2bin(const char *string, unsigned char *data,
				size_t data_len)
{
	size_t i;

	for (i = 0; i < data_len; i++)
		data[i] = (hexchar2bin(string[i * 2 + 0]) << 4 |
			   hexchar2bin(string[i * 2 + 1]) << 0);
}

static void add_network_security_psk(DBusMessageIter *dict,
					GSupplicantSSID *ssid)
{
	if (ssid->passphrase && strlen(ssid->passphrase) > 0) {
		const char *key = "psk";

		if (is_psk_raw_key(ssid->passphrase)) {
			unsigned char data[32];
			unsigned char *datap = data;

			/* The above pointer alias is required by D-Bus because
			 * with D-Bus and GCC, non-heap-allocated arrays cannot
			 * be passed directly by their base pointer. */

			hexstring2bin(ssid->passphrase, datap, sizeof(data));

			supplicant_dbus_dict_append_fixed_array(dict,
							key, DBUS_TYPE_BYTE,
							&datap, sizeof(data));
		} else
			supplicant_dbus_dict_append_basic(dict,
							key, DBUS_TYPE_STRING,
							&ssid->passphrase);
	}
}

static void add_network_security_tls(DBusMessageIter *dict,
					GSupplicantSSID *ssid)
{
	/*
	 * For TLS, we at least need:
	 *              The client certificate
	 *              The client private key file
	 *              The client private key file password
	 *
	 * The Authority certificate is optional.
	 */
	if (!ssid->client_cert_path)
		return;

	if (!ssid->private_key_path)
		return;

	if (!ssid->private_key_passphrase)
		return;

	if (ssid->ca_cert_path)
		supplicant_dbus_dict_append_basic(dict, "ca_cert",
					DBUS_TYPE_STRING, &ssid->ca_cert_path);

	supplicant_dbus_dict_append_basic(dict, "private_key",
						DBUS_TYPE_STRING,
						&ssid->private_key_path);
	supplicant_dbus_dict_append_basic(dict, "private_key_passwd",
						DBUS_TYPE_STRING,
						&ssid->private_key_passphrase);
	supplicant_dbus_dict_append_basic(dict, "client_cert",
						DBUS_TYPE_STRING,
						&ssid->client_cert_path);
}

static void add_network_security_peap(DBusMessageIter *dict,
					GSupplicantSSID *ssid)
{
	char *phase2_auth;

	/*
	 * For PEAP/TTLS, we at least need
	 *              The authority certificate
	 *              The 2nd phase authentication method
	 *              The 2nd phase passphrase
	 *
	 * The Client certificate is optional although strongly recommended
	 * When setting it, we need in addition
	 *              The Client private key file
	 *              The Client private key file password
	 */
	if (!ssid->passphrase)
		return;

	if (!ssid->phase2_auth)
		return;

	if (ssid->client_cert_path) {
		if (!ssid->private_key_path)
			return;

		if (!ssid->private_key_passphrase)
			return;

		supplicant_dbus_dict_append_basic(dict, "client_cert",
						DBUS_TYPE_STRING,
						&ssid->client_cert_path);

		supplicant_dbus_dict_append_basic(dict, "private_key",
						DBUS_TYPE_STRING,
						&ssid->private_key_path);

		supplicant_dbus_dict_append_basic(dict, "private_key_passwd",
						DBUS_TYPE_STRING,
						&ssid->private_key_passphrase);

	}

	if (g_str_has_prefix(ssid->phase2_auth, "EAP-")) {
		phase2_auth = g_strdup_printf("autheap=%s",
					ssid->phase2_auth + strlen("EAP-"));
	} else
		phase2_auth = g_strdup_printf("auth=%s", ssid->phase2_auth);

	supplicant_dbus_dict_append_basic(dict, "password",
						DBUS_TYPE_STRING,
						&ssid->passphrase);

	if (ssid->ca_cert_path)
		supplicant_dbus_dict_append_basic(dict, "ca_cert",
						DBUS_TYPE_STRING,
						&ssid->ca_cert_path);

	supplicant_dbus_dict_append_basic(dict, "phase2",
						DBUS_TYPE_STRING,
						&phase2_auth);

	g_free(phase2_auth);
}

static void add_network_security_eap(DBusMessageIter *dict,
					GSupplicantSSID *ssid)
{
	char *eap_value;

	if (!ssid->eap || !ssid->identity)
		return;

	if (g_strcmp0(ssid->eap, "tls") == 0) {
		add_network_security_tls(dict, ssid);
	} else if (g_strcmp0(ssid->eap, "peap") == 0 ||
				g_strcmp0(ssid->eap, "ttls") == 0) {
		add_network_security_peap(dict, ssid);
	} else
		return;

	eap_value = g_ascii_strup(ssid->eap, -1);

	supplicant_dbus_dict_append_basic(dict, "eap",
						DBUS_TYPE_STRING,
						&eap_value);
	supplicant_dbus_dict_append_basic(dict, "identity",
						DBUS_TYPE_STRING,
						&ssid->identity);

	g_free(eap_value);
}

static void add_network_security_ciphers(DBusMessageIter *dict,
						GSupplicantSSID *ssid)
{
	unsigned int p_cipher, g_cipher, i;
	char *pairwise, *group;
	char *pair_ciphers[4];
	char *group_ciphers[5];

	p_cipher = ssid->pairwise_cipher;
	g_cipher = ssid->group_cipher;

	if (p_cipher == 0 && g_cipher == 0)
		return;

	i = 0;

	if (p_cipher & G_SUPPLICANT_PAIRWISE_CCMP)
		pair_ciphers[i++] = "CCMP";

	if (p_cipher & G_SUPPLICANT_PAIRWISE_TKIP)
		pair_ciphers[i++] = "TKIP";

	if (p_cipher & G_SUPPLICANT_PAIRWISE_NONE)
		pair_ciphers[i++] = "NONE";

	pair_ciphers[i] = NULL;

	i = 0;

	if (g_cipher & G_SUPPLICANT_GROUP_CCMP)
		group_ciphers[i++] = "CCMP";

	if (g_cipher & G_SUPPLICANT_GROUP_TKIP)
		group_ciphers[i++] = "TKIP";

	if (g_cipher & G_SUPPLICANT_GROUP_WEP104)
		group_ciphers[i++] = "WEP104";

	if (g_cipher & G_SUPPLICANT_GROUP_WEP40)
		group_ciphers[i++] = "WEP40";

	group_ciphers[i] = NULL;

	pairwise = g_strjoinv(" ", pair_ciphers);
	group = g_strjoinv(" ", group_ciphers);

	SUPPLICANT_DBG("cipher %s %s", pairwise, group);

	supplicant_dbus_dict_append_basic(dict, "pairwise",
						DBUS_TYPE_STRING,
						&pairwise);
	supplicant_dbus_dict_append_basic(dict, "group",
						DBUS_TYPE_STRING,
						&group);

	g_free(pairwise);
	g_free(group);
}

static void add_network_security_proto(DBusMessageIter *dict,
						GSupplicantSSID *ssid)
{
	unsigned int protocol, i;
	char *proto;
	char *protos[3];

	protocol = ssid->protocol;

	if (protocol == 0)
		return;

	i = 0;

	if (protocol & G_SUPPLICANT_PROTO_RSN)
		protos[i++] = "RSN";

	if (protocol & G_SUPPLICANT_PROTO_WPA)
		protos[i++] = "WPA";

	protos[i] = NULL;

	proto = g_strjoinv(" ", protos);

	SUPPLICANT_DBG("proto %s", proto);

	supplicant_dbus_dict_append_basic(dict, "proto",
						DBUS_TYPE_STRING,
						&proto);

	g_free(proto);
}

static void add_network_security(DBusMessageIter *dict, GSupplicantSSID *ssid)
{
	char *key_mgmt;

	switch (ssid->security) {
	case G_SUPPLICANT_SECURITY_NONE:
		key_mgmt = "NONE";
		add_network_security_none(dict);
		add_network_security_ciphers(dict, ssid);
		break;
	case G_SUPPLICANT_SECURITY_UNKNOWN:
	case G_SUPPLICANT_SECURITY_WEP:
		key_mgmt = "NONE";
		add_network_security_wep(dict, ssid);
		add_network_security_ciphers(dict, ssid);
		break;
	case G_SUPPLICANT_SECURITY_PSK:
		key_mgmt = "WPA-PSK";
		add_network_security_psk(dict, ssid);
		add_network_security_ciphers(dict, ssid);
		add_network_security_proto(dict, ssid);
		break;
	case G_SUPPLICANT_SECURITY_IEEE8021X:
		key_mgmt = "WPA-EAP";
		add_network_security_eap(dict, ssid);
		add_network_security_ciphers(dict, ssid);
		add_network_security_proto(dict, ssid);
		break;
	}

	supplicant_dbus_dict_append_basic(dict, "key_mgmt",
				DBUS_TYPE_STRING, &key_mgmt);
}

static void add_network_mode(DBusMessageIter *dict, GSupplicantSSID *ssid)
{
	dbus_uint32_t mode;

	switch (ssid->mode) {
	case G_SUPPLICANT_MODE_UNKNOWN:
	case G_SUPPLICANT_MODE_INFRA:
		mode = 0;
		break;
	case G_SUPPLICANT_MODE_IBSS:
		mode = 1;
		break;
	case G_SUPPLICANT_MODE_MASTER:
		mode = 2;
		break;
	}

	supplicant_dbus_dict_append_basic(dict, "mode",
				DBUS_TYPE_UINT32, &mode);
}

static void interface_add_network_params(DBusMessageIter *iter, void *user_data)
{
	DBusMessageIter dict;
	struct interface_connect_data *data = user_data;
	GSupplicantSSID *ssid = data->ssid;

	supplicant_dbus_dict_open(iter, &dict);

	if (ssid->scan_ssid)
		supplicant_dbus_dict_append_basic(&dict, "scan_ssid",
					 DBUS_TYPE_UINT32, &ssid->scan_ssid);

	if (ssid->freq)
		supplicant_dbus_dict_append_basic(&dict, "frequency",
					 DBUS_TYPE_UINT32, &ssid->freq);

	if (ssid->bgscan)
		supplicant_dbus_dict_append_basic(&dict, "bgscan",
					DBUS_TYPE_STRING, &ssid->bgscan);

	add_network_mode(&dict, ssid);

	add_network_security(&dict, ssid);

	supplicant_dbus_dict_append_fixed_array(&dict, "ssid",
					DBUS_TYPE_BYTE, &ssid->ssid,
						ssid->ssid_len);

	supplicant_dbus_dict_close(iter, &dict);
}

static void interface_wps_start_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_connect_data *data = user_data;

	SUPPLICANT_DBG("");
	if (error)
		SUPPLICANT_DBG("error: %s", error);

	g_free(data->path);
	g_free(data->ssid);
	dbus_free(data);
}

static void interface_add_wps_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_connect_data *data = user_data;
	GSupplicantSSID *ssid = data->ssid;
	const char *role = "enrollee", *type;
	DBusMessageIter dict;

	SUPPLICANT_DBG("");

	supplicant_dbus_dict_open(iter, &dict);

	supplicant_dbus_dict_append_basic(&dict, "Role",
						DBUS_TYPE_STRING, &role);

	type = "pbc";
	if (ssid->pin_wps) {
		type = "pin";
		supplicant_dbus_dict_append_basic(&dict, "Pin",
					DBUS_TYPE_STRING, &ssid->pin_wps);
	}

	supplicant_dbus_dict_append_basic(&dict, "Type",
					DBUS_TYPE_STRING, &type);

	supplicant_dbus_dict_close(iter, &dict);
}

static void wps_start(const char *error, DBusMessageIter *iter, void *user_data)
{
	struct interface_connect_data *data = user_data;

	SUPPLICANT_DBG("");

	if (error) {
		SUPPLICANT_DBG("error: %s", error);
		g_free(data->path);
		g_free(data->ssid);
		dbus_free(data);
		return;
	}

	supplicant_dbus_method_call(data->interface->path,
			SUPPLICANT_INTERFACE ".Interface.WPS", "Start",
			interface_add_wps_params,
			interface_wps_start_result, data, NULL);
}

static void wps_process_credentials(DBusMessageIter *iter, void *user_data)
{
	dbus_bool_t credentials = TRUE;

	SUPPLICANT_DBG("");

	dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &credentials);
}


int g_supplicant_interface_connect(GSupplicantInterface *interface,
				GSupplicantSSID *ssid,
				GSupplicantInterfaceCallback callback,
							void *user_data)
{
	struct interface_connect_data *data;
	int ret;

	if (!interface)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	/* TODO: Check if we're already connected and switch */

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->interface = interface;
	data->path = g_strdup(interface->path);
	data->callback = callback;
	data->ssid = ssid;
	data->user_data = user_data;

	if (ssid->use_wps) {
		g_free(interface->wps_cred.key);
		memset(&interface->wps_cred, 0,
				sizeof(struct _GSupplicantWpsCredentials));

		ret = supplicant_dbus_property_set(interface->path,
			SUPPLICANT_INTERFACE ".Interface.WPS",
			"ProcessCredentials", DBUS_TYPE_BOOLEAN_AS_STRING,
			wps_process_credentials, wps_start, data, interface);
	} else
		ret = supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface", "AddNetwork",
			interface_add_network_params,
			interface_add_network_result, data,
			interface);

	if (ret < 0) {
		g_free(data->path);
		dbus_free(data);
		return ret;
	}

	return -EINPROGRESS;
}

static void network_remove_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_data *data = user_data;
	int result = 0;

	SUPPLICANT_DBG("");

	if (error) {
		result = -EIO;
		if (g_strcmp0("org.freedesktop.DBus.Error.UnknownMethod",
						error) == 0)
			result = -ECONNABORTED;
	}

	g_free(data->path);

	if (data->callback)
		data->callback(result, data->interface, data->user_data);

	dbus_free(data);
}

static void network_remove_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_data *data = user_data;
	const char *path = data->interface->network_path;

	SUPPLICANT_DBG("path %s", path);

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);
}

static int network_remove(struct interface_data *data)
{
	GSupplicantInterface *interface = data->interface;

	SUPPLICANT_DBG("");

	return supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface", "RemoveNetwork",
			network_remove_params, network_remove_result, data,
			interface);
}

static void interface_disconnect_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_data *data = user_data;
	int result = 0;

	SUPPLICANT_DBG("");

	if (error) {
		result = -EIO;
		if (g_strcmp0("org.freedesktop.DBus.Error.UnknownMethod",
						error) == 0)
			result = -ECONNABORTED;
	}

	/* If we are disconnecting from previous WPS successful
	 * association. i.e.: it did not went through AddNetwork,
	 * and interface->network_path was never set. */
	if (!data->interface->network_path) {
		if (data->callback)
			data->callback(result, data->interface,
							data->user_data);

		g_free(data->path);
		dbus_free(data);
		return;
	}

	if (result < 0 && data->callback) {
		data->callback(result, data->interface, data->user_data);
		data->callback = NULL;
	}

	if (result != -ECONNABORTED) {
		if (network_remove(data) < 0) {
			g_free(data->path);
			dbus_free(data);
		}
	} else {
		g_free(data->path);
		dbus_free(data);
	}
}

int g_supplicant_interface_disconnect(GSupplicantInterface *interface,
					GSupplicantInterfaceCallback callback,
							void *user_data)
{
	struct interface_data *data;
	int ret;

	SUPPLICANT_DBG("");

	if (!interface)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->interface = interface;
	data->path = g_strdup(interface->path);
	data->callback = callback;
	data->user_data = user_data;

	ret = supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface", "Disconnect",
			NULL, interface_disconnect_result, data,
			interface);

	if (ret < 0) {
		g_free(data->path);
		dbus_free(data);
	}

	return ret;
}

static void interface_p2p_find_result(const char *error,
					DBusMessageIter *iter, void *user_data)
{
	struct interface_scan_data *data = user_data;
	int err = 0;

	SUPPLICANT_DBG("error %s", error);

	if (error)
		err = -EIO;

	if (interface_exists(data->interface, data->path)) {
		if (!data->interface->ready)
			err = -ENOLINK;
		if (!err)
			data->interface->p2p_finding = true;
	}

	if (data->callback)
		data->callback(err, data->interface, data->user_data);

	g_free(data->path);
	dbus_free(data);
}

static void interface_p2p_find_params(DBusMessageIter *iter, void *user_data)
{
	DBusMessageIter dict;

	supplicant_dbus_dict_open(iter, &dict);
	supplicant_dbus_dict_close(iter, &dict);
}

int g_supplicant_interface_p2p_find(GSupplicantInterface *interface,
					GSupplicantInterfaceCallback callback,
							void *user_data)
{
	struct interface_scan_data *data;
	int ret;

	if (!interface->p2p_support)
		return -ENOTSUP;

	ret = interface_ready_to_scan(interface);
	if (ret && ret != -EALREADY)
		return ret;

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->interface = interface;
	data->path = g_strdup(interface->path);
	data->callback = callback;
	data->user_data = user_data;

	ret = supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface.P2PDevice", "Find",
			interface_p2p_find_params, interface_p2p_find_result,
			data, interface);
	if (ret < 0) {
		g_free(data->path);
		dbus_free(data);
	}

	return ret;
}

bool g_supplicant_interface_is_p2p_finding(GSupplicantInterface *interface)
{
	if (!interface)
		return false;

	return interface->p2p_finding;
}

int g_supplicant_interface_p2p_stop_find(GSupplicantInterface *interface)
{
	if (!interface->p2p_finding)
		return 0;

	SUPPLICANT_DBG("");

	interface->p2p_finding = false;

	return supplicant_dbus_method_call(interface->path,
		SUPPLICANT_INTERFACE ".Interface.P2PDevice", "StopFind",
		NULL, NULL, NULL, NULL);
}

static void interface_p2p_connect_result(const char *error,
					DBusMessageIter *iter, void *user_data)
{
	struct interface_connect_data *data = user_data;
	int err = 0;

	SUPPLICANT_DBG("");

	if (error)
		err = parse_supplicant_error(iter);

	if (data->callback)
		data->callback(err, data->interface, data->user_data);

	g_free(data->path);
	g_free(data->peer->wps_pin);
	g_free(data->peer->path);
	g_free(data->peer);
	g_free(data);
}

static void interface_p2p_connect_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_connect_data *data = user_data;
	const char *wps = "pbc";
	DBusMessageIter dict;
	int go_intent = 7;

	SUPPLICANT_DBG("");

	supplicant_dbus_dict_open(iter, &dict);

	if (data->peer->master)
		go_intent = 15;

	if (data->peer->wps_pin)
		wps = "pin";

	supplicant_dbus_dict_append_basic(&dict, "peer",
				DBUS_TYPE_OBJECT_PATH, &data->peer->path);
	supplicant_dbus_dict_append_basic(&dict, "wps_method",
				DBUS_TYPE_STRING, &wps);
	if (data->peer->wps_pin) {
		supplicant_dbus_dict_append_basic(&dict, "pin",
				DBUS_TYPE_STRING, &data->peer->wps_pin);
	}

	supplicant_dbus_dict_append_basic(&dict, "go_intent",
					DBUS_TYPE_INT32, &go_intent);

	supplicant_dbus_dict_close(iter, &dict);
}

int g_supplicant_interface_p2p_connect(GSupplicantInterface *interface,
					GSupplicantPeerParams *peer_params,
					GSupplicantInterfaceCallback callback,
					void *user_data)
{
	struct interface_connect_data *data;
	int ret;

	SUPPLICANT_DBG("");

	if (!interface->p2p_support)
		return -ENOTSUP;

	data = dbus_malloc0(sizeof(*data));
	data->interface = interface;
	data->path = g_strdup(interface->path);
	data->peer = peer_params;
	data->callback = callback;
	data->user_data = user_data;

	ret = supplicant_dbus_method_call(interface->path,
		SUPPLICANT_INTERFACE ".Interface.P2PDevice", "Connect",
		interface_p2p_connect_params, interface_p2p_connect_result,
		data, interface);
	if (ret < 0) {
		g_free(data->path);
		dbus_free(data);
		return ret;
	}

	return -EINPROGRESS;
}

int g_supplicant_interface_p2p_disconnect(GSupplicantInterface *interface,
					GSupplicantPeerParams *peer_params)
{
	GSupplicantPeer *peer;
	int count = 0;
	GSList *list;

	SUPPLICANT_DBG("");

	if (!interface->p2p_support)
		return -ENOTSUP;

	peer = g_hash_table_lookup(interface->peer_table, peer_params->path);
	if (!peer)
		return -ENODEV;

	for (list = peer->groups; list; list = list->next, count++) {
		const char *group_obj_path = list->data;
		GSupplicantInterface *g_interface;
		GSupplicantGroup *group;

		group = g_hash_table_lookup(group_mapping, group_obj_path);
		if (!group || !group->interface)
			continue;

		g_interface = group->interface;
		supplicant_dbus_method_call(g_interface->path,
				SUPPLICANT_INTERFACE ".Interface.P2PDevice",
				"Disconnect", NULL, NULL, NULL, g_interface);
	}

	if (count == 0 && peer->current_group_iface) {
		supplicant_dbus_method_call(peer->current_group_iface->path,
				SUPPLICANT_INTERFACE ".Interface.P2PDevice",
				"Disconnect", NULL, NULL, NULL,
				peer->current_group_iface->path);
	}

	peer->current_group_iface = NULL;

	return -EINPROGRESS;
}

struct p2p_service_data {
	bool registration;
	GSupplicantInterface *interface;
	GSupplicantP2PServiceParams *service;
	GSupplicantInterfaceCallback callback;
	void *user_data;
};

static void interface_p2p_service_result(const char *error,
					DBusMessageIter *iter, void *user_data)
{
	struct p2p_service_data *data = user_data;
	int result = 0;

	SUPPLICANT_DBG("%s result - %s", data->registration ?
				"Registration" : "Deletion",
				error ? error : "Success");
	if (error)
		result = -EINVAL;

	if (data->callback)
		data->callback(result, data->interface, data->user_data);

	g_free(data->service->query);
	g_free(data->service->response);
	g_free(data->service->service);
	g_free(data->service->wfd_ies);
	g_free(data->service);
	dbus_free(data);
}

static void interface_p2p_service_params(DBusMessageIter *iter,
							void *user_data)
{
	struct p2p_service_data *data = user_data;
	GSupplicantP2PServiceParams *service;
	DBusMessageIter dict;
	const char *type;

	SUPPLICANT_DBG("");

	service = data->service;

	supplicant_dbus_dict_open(iter, &dict);

	if (service->query && service->response) {
		type = "bonjour";
		supplicant_dbus_dict_append_basic(&dict, "service_type",
						DBUS_TYPE_STRING, &type);
		supplicant_dbus_dict_append_fixed_array(&dict, "query",
					DBUS_TYPE_BYTE, &service->query,
					service->query_length);
		supplicant_dbus_dict_append_fixed_array(&dict, "response",
					DBUS_TYPE_BYTE, &service->response,
					service->response_length);
	} else if (service->version && service->service) {
		type = "upnp";
		supplicant_dbus_dict_append_basic(&dict, "service_type",
						DBUS_TYPE_STRING, &type);
		supplicant_dbus_dict_append_basic(&dict, "version",
					DBUS_TYPE_INT32, &service->version);
		supplicant_dbus_dict_append_basic(&dict, "service",
					DBUS_TYPE_STRING, &service->service);
	}

	supplicant_dbus_dict_close(iter, &dict);
}

int g_supplicant_interface_p2p_add_service(GSupplicantInterface *interface,
				GSupplicantInterfaceCallback callback,
				GSupplicantP2PServiceParams *p2p_service_params,
				void *user_data)
{
	struct p2p_service_data *data;
	int ret;

	SUPPLICANT_DBG("");

	if (!interface->p2p_support)
		return -ENOTSUP;

	data = dbus_malloc0(sizeof(*data));
	data->registration = true;
	data->interface = interface;
	data->service = p2p_service_params;
	data->callback = callback;
	data->user_data = user_data;

	ret = supplicant_dbus_method_call(interface->path,
		SUPPLICANT_INTERFACE ".Interface.P2PDevice", "AddService",
		interface_p2p_service_params, interface_p2p_service_result,
		data, interface);
	if (ret < 0) {
		dbus_free(data);
		return ret;
	}

	return -EINPROGRESS;
}

int g_supplicant_interface_p2p_del_service(GSupplicantInterface *interface,
				GSupplicantP2PServiceParams *p2p_service_params)
{
	struct p2p_service_data *data;
	int ret;

	SUPPLICANT_DBG("");

	if (!interface->p2p_support)
		return -ENOTSUP;

	data = dbus_malloc0(sizeof(*data));
	data->interface = interface;
	data->service = p2p_service_params;

	ret = supplicant_dbus_method_call(interface->path,
		SUPPLICANT_INTERFACE ".Interface.P2PDevice", "DeleteService",
		interface_p2p_service_params, interface_p2p_service_result,
		data, interface);
	if (ret < 0) {
		dbus_free(data);
		return ret;
	}

	return -EINPROGRESS;
}

struct p2p_listen_data {
	int period;
	int interval;
};

static void interface_p2p_listen_params(DBusMessageIter *iter, void *user_data)
{
	struct p2p_listen_data *params = user_data;
	DBusMessageIter dict;

	supplicant_dbus_dict_open(iter, &dict);

	supplicant_dbus_dict_append_basic(&dict, "period",
					DBUS_TYPE_INT32, &params->period);
	supplicant_dbus_dict_append_basic(&dict, "interval",
					DBUS_TYPE_INT32, &params->interval);
	supplicant_dbus_dict_close(iter, &dict);
}

int g_supplicant_interface_p2p_listen(GSupplicantInterface *interface,
						int period, int interval)
{
	struct p2p_listen_data params;

	SUPPLICANT_DBG("");

	if (!interface->p2p_support)
		return -ENOTSUP;

	params.period = period;
	params.interval = interval;

	return supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface.P2PDevice",
			"ExtendedListen", interface_p2p_listen_params,
			NULL, &params, NULL);
}

static void widi_ies_params(DBusMessageIter *iter, void *user_data)
{
	struct p2p_service_data *data = user_data;
	GSupplicantP2PServiceParams *service = data->service;
	DBusMessageIter array;

	SUPPLICANT_DBG("%p - %d", service->wfd_ies, service->wfd_ies_length);

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);

	if (service->wfd_ies && service->wfd_ies_length > 0) {
		dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
				&service->wfd_ies, service->wfd_ies_length);
	}

	dbus_message_iter_close_container(iter, &array);
}

int g_supplicant_set_widi_ies(GSupplicantP2PServiceParams *p2p_service_params,
					GSupplicantInterfaceCallback callback,
					void *user_data)
{
	struct p2p_service_data *data;
	int ret;

	SUPPLICANT_DBG("");

	if (!system_available)
		return -EFAULT;

	data = dbus_malloc0(sizeof(*data));
	data->service = p2p_service_params;
	data->callback = callback;
	data->user_data = user_data;

	if (p2p_service_params->wfd_ies)
		data->registration = true;

	ret = supplicant_dbus_property_set(SUPPLICANT_PATH,
					SUPPLICANT_INTERFACE, "WFDIEs",
					DBUS_TYPE_ARRAY_AS_STRING
					DBUS_TYPE_BYTE_AS_STRING,
					widi_ies_params,
					interface_p2p_service_result,
					data, NULL);
	if (ret < 0 && ret != -EINPROGRESS) {
		dbus_free(data);
		return ret;
	}

	return -EINPROGRESS;
}


static const char *g_supplicant_rule0 = "type=signal,"
					"path=" DBUS_PATH_DBUS ","
					"sender=" DBUS_SERVICE_DBUS ","
					"interface=" DBUS_INTERFACE_DBUS ","
					"member=NameOwnerChanged,"
					"arg0=" SUPPLICANT_SERVICE;
static const char *g_supplicant_rule1 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE;
static const char *g_supplicant_rule2 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Interface";
static const char *g_supplicant_rule3 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Interface.WPS";
static const char *g_supplicant_rule4 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".BSS";
static const char *g_supplicant_rule5 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Network";
static const char *g_supplicant_rule6 = "type=signal,"
		"interface=" SUPPLICANT_INTERFACE ".Interface.P2PDevice";
static const char *g_supplicant_rule7 = "type=signal,"
		"interface=" SUPPLICANT_INTERFACE ".Peer";
static const char *g_supplicant_rule8 = "type=signal,"
		"interface=" SUPPLICANT_INTERFACE ".Group";

static void invoke_introspect_method(void)
{
	DBusMessage *message;

	message = dbus_message_new_method_call(SUPPLICANT_SERVICE,
					SUPPLICANT_PATH,
					DBUS_INTERFACE_INTROSPECTABLE,
					"Introspect");

	if (!message)
		return;

	dbus_message_set_no_reply(message, TRUE);
	dbus_connection_send(connection, message, NULL);
	dbus_message_unref(message);
}

int g_supplicant_register(const GSupplicantCallbacks *callbacks)
{
	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!connection)
		return -EIO;

	if (!dbus_connection_add_filter(connection, g_supplicant_filter,
						NULL, NULL)) {
		dbus_connection_unref(connection);
		connection = NULL;
		return -EIO;
	}

	callbacks_pointer = callbacks;
	eap_methods = 0;

	interface_table = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, remove_interface);

	bss_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);
	peer_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);
	group_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);
	pending_peer_connection = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);

	supplicant_dbus_setup(connection);

	dbus_bus_add_match(connection, g_supplicant_rule0, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule1, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule2, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule3, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule4, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule5, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule6, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule7, NULL);
	dbus_bus_add_match(connection, g_supplicant_rule8, NULL);
	dbus_connection_flush(connection);

	if (dbus_bus_name_has_owner(connection,
					SUPPLICANT_SERVICE, NULL)) {
		system_available = TRUE;
		supplicant_dbus_property_get_all(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						service_property, NULL, NULL);
	} else
		invoke_introspect_method();

	return 0;
}

static void unregister_interface_remove_params(DBusMessageIter *iter,
						void *user_data)
{
	const char *path = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
							&path);
}


static void unregister_remove_interface(gpointer key, gpointer value,
						gpointer user_data)
{
	GSupplicantInterface *interface = value;

	supplicant_dbus_method_call(SUPPLICANT_PATH,
					SUPPLICANT_INTERFACE,
					"RemoveInterface",
					unregister_interface_remove_params,
					NULL, interface->path, NULL);
}

void g_supplicant_unregister(const GSupplicantCallbacks *callbacks)
{
	SUPPLICANT_DBG("");

	if (connection) {
		dbus_bus_remove_match(connection, g_supplicant_rule8, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule7, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule6, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule5, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule4, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule3, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule2, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule1, NULL);
		dbus_bus_remove_match(connection, g_supplicant_rule0, NULL);
		dbus_connection_flush(connection);

		dbus_connection_remove_filter(connection,
						g_supplicant_filter, NULL);
	}

	if (bss_mapping) {
		g_hash_table_destroy(bss_mapping);
		bss_mapping = NULL;
	}

	if (peer_mapping) {
		g_hash_table_destroy(peer_mapping);
		peer_mapping = NULL;
	}

	if (group_mapping) {
		g_hash_table_destroy(group_mapping);
		group_mapping = NULL;
	}

	if (interface_table) {
		g_hash_table_foreach(interface_table,
					unregister_remove_interface, NULL);
		g_hash_table_destroy(interface_table);
		interface_table = NULL;
	}

	if (system_available)
		callback_system_killed();

	if (connection) {
		dbus_connection_unref(connection);
		connection = NULL;
	}

	callbacks_pointer = NULL;
	eap_methods = 0;
}
