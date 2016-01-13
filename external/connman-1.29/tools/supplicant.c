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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <syslog.h>

#include <glib.h>
#include <gdbus.h>

#include "supplicant-dbus.h"
#include "supplicant.h"

#define DBG(fmt, arg...) do { \
	syslog(LOG_DEBUG, "%s() " fmt, __FUNCTION__ , ## arg); \
} while (0)

#define TIMEOUT 5000

#define IEEE80211_CAP_ESS	0x0001
#define IEEE80211_CAP_IBSS	0x0002
#define IEEE80211_CAP_PRIVACY	0x0010

static DBusConnection *connection;

static const struct supplicant_callbacks *callbacks_pointer;

static dbus_bool_t system_available = FALSE;
static dbus_bool_t system_ready = FALSE;

static dbus_int32_t debug_level = 0;
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
	{ "MD5",	SUPPLICANT_EAP_METHOD_MD5	},
	{ "TLS",	SUPPLICANT_EAP_METHOD_TLS	},
	{ "MSCHAPV2",	SUPPLICANT_EAP_METHOD_MSCHAPV2	},
	{ "PEAP",	SUPPLICANT_EAP_METHOD_PEAP	},
	{ "TTLS",	SUPPLICANT_EAP_METHOD_TTLS	},
	{ "GTC",	SUPPLICANT_EAP_METHOD_GTC	},
	{ "OTP",	SUPPLICANT_EAP_METHOD_OTP	},
	{ "LEAP",	SUPPLICANT_EAP_METHOD_LEAP	},
	{ "WSC",	SUPPLICANT_EAP_METHOD_WSC	},
	{ }
};

static struct strvalmap keymgmt_capa_map[] = {
	{ "none",	SUPPLICANT_CAPABILITY_KEYMGMT_NONE	},
	{ "ieee8021x",	SUPPLICANT_CAPABILITY_KEYMGMT_IEEE8021X	},
	{ "wpa-none",	SUPPLICANT_CAPABILITY_KEYMGMT_WPA_NONE	},
	{ "wpa-psk",	SUPPLICANT_CAPABILITY_KEYMGMT_WPA_PSK	},
	{ "wpa-eap",	SUPPLICANT_CAPABILITY_KEYMGMT_WPA_EAP	},
	{ "wps",	SUPPLICANT_CAPABILITY_KEYMGMT_WPS	},
	{ }
};

static struct strvalmap authalg_capa_map[] = {
	{ "open",	SUPPLICANT_CAPABILITY_AUTHALG_OPEN	},
	{ "shared",	SUPPLICANT_CAPABILITY_AUTHALG_SHARED	},
	{ "leap",	SUPPLICANT_CAPABILITY_AUTHALG_LEAP	},
	{ }
};

static struct strvalmap proto_capa_map[] = {
	{ "wpa",	SUPPLICANT_CAPABILITY_PROTO_WPA		},
	{ "rsn",	SUPPLICANT_CAPABILITY_PROTO_RSN		},
	{ }
};

static struct strvalmap group_capa_map[] = {
	{ "wep40",	SUPPLICANT_CAPABILITY_GROUP_WEP40	},
	{ "wep104",	SUPPLICANT_CAPABILITY_GROUP_WEP104	},
	{ "tkip",	SUPPLICANT_CAPABILITY_GROUP_TKIP	},
	{ "ccmp",	SUPPLICANT_CAPABILITY_GROUP_CCMP	},
	{ }
};

static struct strvalmap pairwise_capa_map[] = {
	{ "none",	SUPPLICANT_CAPABILITY_PAIRWISE_NONE	},
	{ "tkip",	SUPPLICANT_CAPABILITY_PAIRWISE_TKIP	},
	{ "ccmp",	SUPPLICANT_CAPABILITY_PAIRWISE_CCMP	},
	{ }
};

static struct strvalmap scan_capa_map[] = {
	{ "active",	SUPPLICANT_CAPABILITY_SCAN_ACTIVE	},
	{ "passive",	SUPPLICANT_CAPABILITY_SCAN_PASSIVE	},
	{ "ssid",	SUPPLICANT_CAPABILITY_SCAN_SSID		},
	{ }
};

static struct strvalmap mode_capa_map[] = {
	{ "infrastructure",	SUPPLICANT_CAPABILITY_MODE_INFRA	},
	{ "ad-hoc",		SUPPLICANT_CAPABILITY_MODE_IBSS		},
	{ "ap",			SUPPLICANT_CAPABILITY_MODE_AP		},
	{ }
};

static GHashTable *interface_table;
static GHashTable *bss_mapping;

struct supplicant_interface {
	char *path;
	unsigned int keymgmt_capa;
	unsigned int authalg_capa;
	unsigned int proto_capa;
	unsigned int group_capa;
	unsigned int pairwise_capa;
	unsigned int scan_capa;
	unsigned int mode_capa;
	dbus_bool_t ready;
	enum supplicant_state state;
	dbus_bool_t scanning;
	supplicant_interface_scan_callback scan_callback;
	void *scan_data;
	int apscan;
	char *ifname;
	char *driver;
	char *bridge;
	GHashTable *network_table;
	GHashTable *net_mapping;
	GHashTable *bss_mapping;
};

struct supplicant_network {
	struct supplicant_interface *interface;
	char *path;
	char *group;
	char *name;
	enum supplicant_mode mode;
	GHashTable *bss_table;
	GHashTable *config_table;
};

struct supplicant_bss {
	struct supplicant_interface *interface;
	char *path;
	unsigned char bssid[6];
	unsigned char ssid[32];
	unsigned int ssid_len;
	dbus_uint16_t frequency;
	dbus_uint32_t maxrate;
	enum supplicant_mode mode;
	enum supplicant_security security;
	dbus_bool_t privacy;
	dbus_bool_t psk;
	dbus_bool_t ieee8021x;
};

static enum supplicant_mode string2mode(const char *mode)
{
	if (!mode)
		return SUPPLICANT_MODE_UNKNOWN;

	if (g_str_equal(mode, "infrastructure"))
		return SUPPLICANT_MODE_INFRA;
	else if (g_str_equal(mode, "ad-hoc"))
		return SUPPLICANT_MODE_IBSS;

	return SUPPLICANT_MODE_UNKNOWN;
}

static const char *mode2string(enum supplicant_mode mode)
{
	switch (mode) {
	case SUPPLICANT_MODE_UNKNOWN:
		break;
	case SUPPLICANT_MODE_INFRA:
		return "infra";
	case SUPPLICANT_MODE_IBSS:
		return "adhoc";
	}

	return NULL;
}

static const char *security2string(enum supplicant_security security)
{
	switch (security) {
	case SUPPLICANT_SECURITY_UNKNOWN:
		break;
	case SUPPLICANT_SECURITY_NONE:
		return "none";
	case SUPPLICANT_SECURITY_WEP:
		return "wep";
	case SUPPLICANT_SECURITY_PSK:
		return "psk";
	case SUPPLICANT_SECURITY_IEEE8021X:
		return "ieee8021x";
	}

	return NULL;
}

static enum supplicant_state string2state(const char *state)
{
	if (!state)
		return SUPPLICANT_STATE_UNKNOWN;

	if (g_str_equal(state, "unknown"))
		return SUPPLICANT_STATE_UNKNOWN;
	else if (g_str_equal(state, "disconnected"))
		return SUPPLICANT_STATE_DISCONNECTED;
	else if (g_str_equal(state, "inactive"))
		return SUPPLICANT_STATE_INACTIVE;
	else if (g_str_equal(state, "scanning"))
		return SUPPLICANT_STATE_SCANNING;
	else if (g_str_equal(state, "authenticating"))
		return SUPPLICANT_STATE_AUTHENTICATING;
	else if (g_str_equal(state, "associating"))
		return SUPPLICANT_STATE_ASSOCIATING;
	else if (g_str_equal(state, "associated"))
		return SUPPLICANT_STATE_ASSOCIATED;
	else if (g_str_equal(state, "group_handshake"))
		return SUPPLICANT_STATE_GROUP_HANDSHAKE;
	else if (g_str_equal(state, "4way_handshake"))
		return SUPPLICANT_STATE_4WAY_HANDSHAKE;
	else if (g_str_equal(state, "completed"))
		return SUPPLICANT_STATE_COMPLETED;

	return SUPPLICANT_STATE_UNKNOWN;
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

static void callback_interface_added(struct supplicant_interface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->interface_added)
		return;

	callbacks_pointer->interface_added(interface);
}

static void callback_interface_removed(struct supplicant_interface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->interface_removed)
		return;

	callbacks_pointer->interface_removed(interface);
}

static void callback_scan_started(struct supplicant_interface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->scan_started)
		return;

	callbacks_pointer->scan_started(interface);
}

static void callback_scan_finished(struct supplicant_interface *interface)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->scan_finished)
		return;

	callbacks_pointer->scan_finished(interface);
}

static void callback_network_added(struct supplicant_network *network)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->network_added)
		return;

	callbacks_pointer->network_added(network);
}

static void callback_network_removed(struct supplicant_network *network)
{
	if (!callbacks_pointer)
		return;

	if (!callbacks_pointer->network_removed)
		return;

	callbacks_pointer->network_removed(network);
}

static void remove_interface(gpointer data)
{
	struct supplicant_interface *interface = data;

	g_hash_table_destroy(interface->bss_mapping);
	g_hash_table_destroy(interface->net_mapping);
	g_hash_table_destroy(interface->network_table);

	callback_interface_removed(interface);

	g_free(interface->path);
	g_free(interface->ifname);
	g_free(interface->driver);
	g_free(interface->bridge);
	g_free(interface);
}

static void remove_network(gpointer data)
{
	struct supplicant_network *network = data;

	g_hash_table_destroy(network->bss_table);

	callback_network_removed(network);

	g_hash_table_destroy(network->config_table);

	g_free(network->group);
	g_free(network->name);
	g_free(network);
}

static void remove_bss(gpointer data)
{
	struct supplicant_bss *bss = data;

	g_free(bss->path);
	g_free(bss);
}

static void debug_strvalmap(const char *label, struct strvalmap *map,
							unsigned int val)
{
	int i;

	for (i = 0; map[i].str; i++) {
		if (val & map[i].val)
			DBG("%s: %s", label, map[i].str);
	}
}

static void interface_capability_keymgmt(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; keymgmt_capa_map[i].str; i++)
		if (strcmp(str, keymgmt_capa_map[i].str) == 0) {
			interface->keymgmt_capa |= keymgmt_capa_map[i].val;
			break;
		}
}

static void interface_capability_authalg(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
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
	struct supplicant_interface *interface = user_data;
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

static void interface_capability_pairwise(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; pairwise_capa_map[i].str; i++)
		if (strcmp(str, pairwise_capa_map[i].str) == 0) {
			interface->pairwise_capa |= pairwise_capa_map[i].val;
			break;
		}
}

static void interface_capability_group(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
	const char *str = NULL;
	int i;

	dbus_message_iter_get_basic(iter, &str);
	if (!str)
		return;

	for (i = 0; group_capa_map[i].str; i++)
		if (strcmp(str, group_capa_map[i].str) == 0) {
			interface->group_capa |= group_capa_map[i].val;
			break;
		}
}

static void interface_capability_scan(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
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
	struct supplicant_interface *interface = user_data;
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
	struct supplicant_interface *interface = user_data;

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
	else
		DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

const char *supplicant_interface_get_ifname(struct supplicant_interface *interface)
{
	if (!interface)
		return NULL;

	return interface->ifname;
}

const char *supplicant_interface_get_driver(struct supplicant_interface *interface)
{
	if (!interface)
		return NULL;

	return interface->driver;
}

struct supplicant_interface *supplicant_network_get_interface(struct supplicant_network *network)
{
	if (!network)
		return NULL;

	return network->interface;
}

const char *supplicant_network_get_name(struct supplicant_network *network)
{
	if (!network || !network->name)
		return "";

	return network->name;
}

const char *supplicant_network_get_identifier(struct supplicant_network *network)
{
	if (!network || !network->group)
		return "";

	return network->group;
}

enum supplicant_mode supplicant_network_get_mode(struct supplicant_network *network)
{
	if (!network)
		return SUPPLICANT_MODE_UNKNOWN;

	return network->mode;
}

static void merge_network(struct supplicant_network *network)
{
	GString *str;
	const char *ssid, *mode, *key_mgmt;
	unsigned int i, ssid_len;
	char *group;

	ssid = g_hash_table_lookup(network->config_table, "ssid");
	mode = g_hash_table_lookup(network->config_table, "mode");
	key_mgmt = g_hash_table_lookup(network->config_table, "key_mgmt");

	DBG("ssid %s mode %s", ssid, mode);

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
		g_string_append_printf(str, "_infra");
	else if (g_strcmp0(mode, "1") == 0)
		g_string_append_printf(str, "_adhoc");

	if (g_strcmp0(key_mgmt, "WPA-PSK") == 0)
		g_string_append_printf(str, "_psk");

	group = g_string_free(str, FALSE);

	DBG("%s", group);

	g_free(group);

	g_hash_table_destroy(network->config_table);

	g_free(network->path);
	g_free(network);
}

static void network_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	struct supplicant_network *network = user_data;

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
		if (str)
			g_hash_table_replace(network->config_table,
						g_strdup(key), g_strdup(str));
	} else
		DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static void interface_network_added(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
	struct supplicant_network *network;
	const char *path = NULL;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	if (g_strcmp0(path, "/") == 0)
		return;

	network = g_hash_table_lookup(interface->net_mapping, path);
	if (network)
		return;

	network = g_try_new0(struct supplicant_network, 1);
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
				SUPPLICANT_INTERFACE ".Interface.Network",
						network_property, network);
}

static void interface_network_removed(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
	struct supplicant_network *network;
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
	char *name;
	int i;

	if (ssid_len < 1 || ssid[0] == '\0')
		name = NULL;
	else
		name = g_try_malloc0(ssid_len + 1);

	if (!name)
		return g_strdup("");

	for (i = 0; i < ssid_len; i++) {
		if (g_ascii_isprint(ssid[i]))
			name[i] = ssid[i];
		else
			name[i] = ' ';
	}

	return name;
}

static char *create_group(struct supplicant_bss *bss)
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

static void add_bss_to_network(struct supplicant_bss *bss)
{
	struct supplicant_interface *interface = bss->interface;
	struct supplicant_network *network;
	char *group;

	group = create_group(bss);
	if (!group)
		return;

	network = g_hash_table_lookup(interface->network_table, group);
	if (network) {
		g_free(group);
		goto done;
	}

	network = g_try_new0(struct supplicant_network, 1);
	if (!network) {
		g_free(group);
		return;
	}

	network->group = group;
	network->name = create_name(bss->ssid, bss->ssid_len);
	network->mode = bss->mode;

	network->bss_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							NULL, remove_bss);

	network->config_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							g_free, g_free);

	g_hash_table_replace(interface->network_table,
						network->group, network);

	callback_network_added(network);

done:
	g_hash_table_replace(interface->bss_mapping, bss->path, network);
	g_hash_table_replace(network->bss_table, bss->path, bss);

	g_hash_table_replace(bss_mapping, bss->path, interface);
}

static unsigned char wifi_oui[3]      = { 0x00, 0x50, 0xf2 };
static unsigned char ieee80211_oui[3] = { 0x00, 0x0f, 0xac };

static void extract_rsn(struct supplicant_bss *bss,
					const unsigned char *buf, int len)
{
	uint16_t count;
	int i;

	/* Version */
	if (len < 2)
		return;

	buf += 2;
	len -= 2;

	/* Group cipher */
	if (len < 4)
		return;

	buf += 4;
	len -= 4;

	/* Pairwise cipher */
	if (len < 2)
		return;

	count = buf[0] | (buf[1] << 8);
	if (2 + (count * 4) > len)
		return;

	buf += 2 + (count * 4);
	len -= 2 + (count * 4);

	/* Authentication */
	if (len < 2)
		return;

	count = buf[0] | (buf[1] << 8);
	if (2 + (count * 4) > len)
		return;

	for (i = 0; i < count; i++) {
		const unsigned char *ptr = buf + 2 + (i * 4);

		if (memcmp(ptr, wifi_oui, 3) == 0) {
			switch (ptr[3]) {
			case 1:
				bss->ieee8021x = TRUE;
				break;
			case 2:
				bss->psk = TRUE;
				break;
			}
		} else if (memcmp(ptr, ieee80211_oui, 3) == 0) {
			switch (ptr[3]) {
			case 1:
				bss->ieee8021x = TRUE;
				break;
			case 2:
				bss->psk = TRUE;
				break;
			}
		}
	}

}

static void bss_rates(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_bss *bss = user_data;
	dbus_uint32_t rate = 0;

	dbus_message_iter_get_basic(iter, &rate);
	if (rate == 0)
		return;

	if (rate > bss->maxrate)
		bss->maxrate = rate;
}

static void bss_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	struct supplicant_bss *bss = user_data;

	if (!bss->interface)
		return;

	if (!key) {
		if (bss->ieee8021x)
			bss->security = SUPPLICANT_SECURITY_IEEE8021X;
		else if (bss->psk)
			bss->security = SUPPLICANT_SECURITY_PSK;
		else if (bss->privacy)
			bss->security = SUPPLICANT_SECURITY_WEP;
		else
			bss->security = SUPPLICANT_SECURITY_NONE;

		add_bss_to_network(bss);
		return;
	}

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
			bss->mode = SUPPLICANT_MODE_INFRA;
		else if (capabilities & IEEE80211_CAP_IBSS)
			bss->mode = SUPPLICANT_MODE_IBSS;

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
	} else if (g_strcmp0(key, "Level") == 0) {
		dbus_int32_t level = 0;

		dbus_message_iter_get_basic(iter, &level);
	} else if (g_strcmp0(key, "Rates") == 0) {
		supplicant_dbus_array_foreach(iter, bss_rates, bss);
	} else if (g_strcmp0(key, "MaxRate") == 0) {
		dbus_uint32_t maxrate = 0;

		dbus_message_iter_get_basic(iter, &maxrate);
		if (maxrate != 0)
			bss->maxrate =maxrate;
	} else if (g_strcmp0(key, "Privacy") == 0) {
		dbus_bool_t privacy = FALSE;

		dbus_message_iter_get_basic(iter, &privacy);
		bss->privacy = privacy;
	} else if (g_strcmp0(key, "RSNIE") == 0) {
		DBusMessageIter array;
		unsigned char *ie;
		int ie_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &ie, &ie_len);

		if (ie_len > 2)
			extract_rsn(bss, ie + 2, ie_len - 2);
	} else if (g_strcmp0(key, "WPAIE") == 0) {
		DBusMessageIter array;
		unsigned char *ie;
		int ie_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &ie, &ie_len);

		if (ie_len > 6)
			extract_rsn(bss, ie + 6, ie_len - 6);
	} else if (g_strcmp0(key, "WPSIE") == 0) {
		DBusMessageIter array;
		unsigned char *ie;
		int ie_len;

		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &ie, &ie_len);
	} else
		DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static void interface_bss_added(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
	struct supplicant_network *network;
	struct supplicant_bss *bss;
	const char *path = NULL;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	if (g_strcmp0(path, "/") == 0)
		return;

	network = g_hash_table_lookup(interface->bss_mapping, path);
	if (network) {
		bss = g_hash_table_lookup(network->bss_table, path);
		if (bss)
			return;
	}

	bss = g_try_new0(struct supplicant_bss, 1);
	if (!bss)
		return;

	bss->interface = interface;
	bss->path = g_strdup(path);

	dbus_message_iter_next(iter);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {
		supplicant_dbus_property_foreach(iter, bss_property, bss);
		bss_property(NULL, NULL, bss);
		return;
	}

	supplicant_dbus_property_get_all(path,
					SUPPLICANT_INTERFACE ".Interface.BSS",
							bss_property, bss);
}

static void interface_bss_removed(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface = user_data;
	struct supplicant_network *network;
	const char *path = NULL;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

	network = g_hash_table_lookup(interface->bss_mapping, path);
	if (!network)
		return;

	g_hash_table_remove(bss_mapping, path);

	g_hash_table_remove(interface->bss_mapping, path);
	g_hash_table_remove(network->bss_table, path);

	if (g_hash_table_size(network->bss_table) == 0)
		g_hash_table_remove(interface->network_table, network->group);
}

static void interface_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	struct supplicant_interface *interface = user_data;

	if (!interface)
		return;

	if (!key) {
		debug_strvalmap("KeyMgmt capability", keymgmt_capa_map,
						interface->keymgmt_capa);
		debug_strvalmap("AuthAlg capability", authalg_capa_map,
						interface->authalg_capa);
		debug_strvalmap("Protocol capability", proto_capa_map,
						interface->proto_capa);
		debug_strvalmap("Pairwise capability", pairwise_capa_map,
						interface->pairwise_capa);
		debug_strvalmap("Group capability", group_capa_map,
						interface->group_capa);
		debug_strvalmap("Scan capability", scan_capa_map,
						interface->scan_capa);
		debug_strvalmap("Mode capability", mode_capa_map,
						interface->mode_capa);

		interface->ready = TRUE;
		callback_interface_added(interface);
		return;
	}

	if (g_strcmp0(key, "Capabilities") == 0) {
		supplicant_dbus_property_foreach(iter, interface_capability,
								interface);
	} else if (g_strcmp0(key, "State") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str)
			interface->state = string2state(str);

		DBG("state %s (%d)", str, interface->state);
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
		if (str)
			interface->ifname = g_strdup(str);
	} else if (g_strcmp0(key, "Driver") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str)
			interface->driver = g_strdup(str);
	} else if (g_strcmp0(key, "BridgeIfname") == 0) {
		const char *str = NULL;

		dbus_message_iter_get_basic(iter, &str);
		if (str)
			interface->bridge = g_strdup(str);
	} else if (g_strcmp0(key, "CurrentBSS") == 0) {
		interface_bss_added(iter, interface);
	} else if (g_strcmp0(key, "CurrentNetwork") == 0) {
		interface_network_added(iter, interface);
	} else if (g_strcmp0(key, "BSSs") == 0) {
		supplicant_dbus_array_foreach(iter, interface_bss_added,
								interface);
	} else if (g_strcmp0(key, "Blobs") == 0) {
	} else if (g_strcmp0(key, "Networks") == 0) {
		supplicant_dbus_array_foreach(iter, interface_network_added,
								interface);
	} else
		DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static struct supplicant_interface *interface_alloc(const char *path)
{
	struct supplicant_interface *interface;

	interface = g_try_new0(struct supplicant_interface, 1);
	if (!interface)
		return NULL;

	interface->path = g_strdup(path);

	interface->network_table = g_hash_table_new_full(g_str_hash, g_str_equal,
							NULL, remove_network);

	interface->net_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);
	interface->bss_mapping = g_hash_table_new_full(g_str_hash, g_str_equal,
								NULL, NULL);

	g_hash_table_replace(interface_table, interface->path, interface);

	return interface;
}

static void interface_added(DBusMessageIter *iter, void *user_data)
{
	struct supplicant_interface *interface;
	const char *path = NULL;

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
						interface_property, interface);
}

static void interface_removed(DBusMessageIter *iter, void *user_data)
{
	const char *path = NULL;

	dbus_message_iter_get_basic(iter, &path);
	if (!path)
		return;

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
		DBG("Debug level %d", debug_level);
	} else if (g_strcmp0(key, "DebugTimestamp") == 0) {
		dbus_message_iter_get_basic(iter, &debug_timestamp);
		DBG("Debug timestamp %u", debug_timestamp);
	} else if (g_strcmp0(key, "DebugShowKeys") == 0) {
		dbus_message_iter_get_basic(iter, &debug_showkeys);
		DBG("Debug show keys %u", debug_showkeys);
	} else if (g_strcmp0(key, "Interfaces") == 0) {
		supplicant_dbus_array_foreach(iter, interface_added, NULL);
	} else if (g_strcmp0(key, "EapMethods") == 0) {
		supplicant_dbus_array_foreach(iter, eap_method, NULL);
		debug_strvalmap("EAP method", eap_method_map, eap_methods);
	} else
		DBG("key %s type %c",
				key, dbus_message_iter_get_arg_type(iter));
}

static void supplicant_bootstrap(void)
{
	supplicant_dbus_property_get_all(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						service_property, NULL);
}

static void signal_name_owner_changed(const char *path, DBusMessageIter *iter)
{
	const char *name = NULL, *old = NULL, *new = NULL;

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
		g_hash_table_remove_all(interface_table);
		callback_system_killed();
	}

	if (strlen(new) > 0 && strlen(old) == 0) {
		system_available = TRUE;
		supplicant_bootstrap();
	}
}

static void signal_properties_changed(const char *path, DBusMessageIter *iter)
{
	if (g_strcmp0(path, SUPPLICANT_PATH) != 0)
		return;

	supplicant_dbus_property_foreach(iter, service_property, NULL);
}

static void signal_interface_added(const char *path, DBusMessageIter *iter)
{
	if (g_strcmp0(path, SUPPLICANT_PATH) == 0)
		interface_added(iter, NULL);
}

static void signal_interface_removed(const char *path, DBusMessageIter *iter)
{
	if (g_strcmp0(path, SUPPLICANT_PATH) == 0)
		interface_removed(iter, NULL);
}

static void signal_interface_changed(const char *path, DBusMessageIter *iter)
{
	struct supplicant_interface *interface;

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	supplicant_dbus_property_foreach(iter, interface_property, interface);
}

static void signal_scan_done(const char *path, DBusMessageIter *iter)
{
	struct supplicant_interface *interface;
	dbus_bool_t success = FALSE;

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	dbus_message_iter_get_basic(iter, &success);

	if (interface->scan_callback) {
		int result = 0;

		if (!success)
			result = -EIO;

		interface->scan_callback(result, interface->scan_data);
	}

	interface->scan_callback = NULL;
	interface->scan_data = NULL;
}

static void signal_bss_added(const char *path, DBusMessageIter *iter)
{
	struct supplicant_interface *interface;

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_bss_added(iter, interface);
}

static void signal_bss_removed(const char *path, DBusMessageIter *iter)
{
	struct supplicant_interface *interface;

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_bss_removed(iter, interface);
}

static void signal_network_added(const char *path, DBusMessageIter *iter)
{
	struct supplicant_interface *interface;

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_network_added(iter, interface);
}

static void signal_network_removed(const char *path, DBusMessageIter *iter)
{
	struct supplicant_interface *interface;

	interface = g_hash_table_lookup(interface_table, path);
	if (!interface)
		return;

	interface_network_removed(iter, interface);
}

static void signal_bss_changed(const char *path, DBusMessageIter *iter)
{
	struct supplicant_interface *interface;
	struct supplicant_network *network;
	struct supplicant_bss *bss;

	interface = g_hash_table_lookup(bss_mapping, path);
	if (!interface)
		return;

	network = g_hash_table_lookup(interface->bss_mapping, path);
	if (!network)
		return;

	bss = g_hash_table_lookup(network->bss_table, path);
	if (!bss)
		return;

	supplicant_dbus_property_foreach(iter, bss_property, bss);
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

	{ SUPPLICANT_INTERFACE ".Interface.BSS", "PropertiesChanged", signal_bss_changed   },

	{ }
};

static DBusHandlerResult supplicant_filter(DBusConnection *conn,
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

static const char *supplicant_rule0 = "type=signal,"
					"path=" DBUS_PATH_DBUS ","
					"sender=" DBUS_SERVICE_DBUS ","
					"interface=" DBUS_INTERFACE_DBUS ","
					"member=NameOwnerChanged,"
					"arg0=" SUPPLICANT_SERVICE;
static const char *supplicant_rule1 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE;
static const char *supplicant_rule2 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Interface";
static const char *supplicant_rule3 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Interface.WPS";
static const char *supplicant_rule4 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Interface.BSS";
static const char *supplicant_rule5 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Interface.Network";
static const char *supplicant_rule6 = "type=signal,"
			"interface=" SUPPLICANT_INTERFACE ".Interface.Blob";

int supplicant_register(const struct supplicant_callbacks *callbacks)
{
	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!connection)
		return -EIO;

	if (!dbus_connection_add_filter(connection, supplicant_filter, NULL, NULL)) {
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

	supplicant_dbus_setup(connection);

	dbus_bus_add_match(connection, supplicant_rule0, NULL);
	dbus_bus_add_match(connection, supplicant_rule1, NULL);
	dbus_bus_add_match(connection, supplicant_rule2, NULL);
	dbus_bus_add_match(connection, supplicant_rule3, NULL);
	dbus_bus_add_match(connection, supplicant_rule4, NULL);
	dbus_bus_add_match(connection, supplicant_rule5, NULL);
	dbus_bus_add_match(connection, supplicant_rule6, NULL);
	dbus_connection_flush(connection);

	if (dbus_bus_name_has_owner(connection,
					SUPPLICANT_SERVICE, NULL)) {
		system_available = TRUE;
		supplicant_bootstrap();
	}

	return 0;
}

void supplicant_unregister(const struct supplicant_callbacks *callbacks)
{
	if (connection) {
		dbus_bus_remove_match(connection, supplicant_rule6, NULL);
		dbus_bus_remove_match(connection, supplicant_rule5, NULL);
		dbus_bus_remove_match(connection, supplicant_rule4, NULL);
		dbus_bus_remove_match(connection, supplicant_rule3, NULL);
		dbus_bus_remove_match(connection, supplicant_rule2, NULL);
		dbus_bus_remove_match(connection, supplicant_rule1, NULL);
		dbus_bus_remove_match(connection, supplicant_rule0, NULL);
		dbus_connection_flush(connection);

		dbus_connection_remove_filter(connection,
						supplicant_filter, NULL);
	}

	if (bss_mapping) {
		g_hash_table_destroy(bss_mapping);
		bss_mapping = NULL;
	}

	if (interface_table) {
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

static void debug_level_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	if (error)
		DBG("debug level failure: %s", error);
}

static void debug_level_params(DBusMessageIter *iter, void *user_data)
{
	guint level = GPOINTER_TO_UINT(user_data);
	const char *str;

	if (level > 4)
		level = 4;

	str = debug_strings[level];

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &str);
}

void supplicant_set_debug_level(unsigned int level)
{
	if (!system_available)
		return;

	supplicant_dbus_property_set(SUPPLICANT_PATH, SUPPLICANT_INTERFACE,
				"DebugLevel", DBUS_TYPE_STRING_AS_STRING,
				debug_level_params, debug_level_result,
						GUINT_TO_POINTER(level));
}

struct interface_create_data {
	const char *ifname;
	const char *driver;
	struct supplicant_interface *interface;
	supplicant_interface_create_callback callback;
	void *user_data;
};

static void interface_create_property(const char *key, DBusMessageIter *iter,
							void *user_data)
{
	struct interface_create_data *data = user_data;
	struct supplicant_interface *interface = data->interface;

	if (!key) {
		if (data->callback)
			data->callback(0, data->interface, data->user_data);

		dbus_free(data);
	}

	interface_property(key, iter, interface);
}

static void interface_create_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;
	const char *path = NULL;
	int err;

	if (error) {
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
					interface_create_property, data);
	if (err == 0)
		return;

done:
	if (data->callback)
		data->callback(err, NULL, data->user_data);

	dbus_free(data);
}

static void interface_create_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;
	DBusMessageIter dict;

	supplicant_dbus_dict_open(iter, &dict);

	supplicant_dbus_dict_append_basic(&dict, "Ifname",
					DBUS_TYPE_STRING, &data->ifname);

	if (data->driver)
		supplicant_dbus_dict_append_basic(&dict, "Driver",
					DBUS_TYPE_STRING, &data->driver);

	supplicant_dbus_dict_close(iter, &dict);
}

static void interface_get_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;
	struct supplicant_interface *interface;
	const char *path = NULL;
	int err;

	if (error)
		goto create;

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

	if (data->callback)
		data->callback(0, interface, data->user_data);

	dbus_free(data);

	return;

create:
	if (!system_available) {
		err = -EFAULT;
		goto done;
	}

	err = supplicant_dbus_method_call(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						"CreateInterface",
						interface_create_params,
						interface_create_result, data);
	if (err == 0)
		return;

done:
	if (data->callback)
		data->callback(err, NULL, data->user_data);

	dbus_free(data);
}

static void interface_get_params(DBusMessageIter *iter, void *user_data)
{
	struct interface_create_data *data = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &data->ifname);
}

int supplicant_interface_create(const char *ifname, const char *driver,
			supplicant_interface_create_callback callback,
							void *user_data)
{
	struct interface_create_data *data;

	if (!ifname)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->ifname = ifname;
	data->driver = driver;
	data->callback = callback;
	data->user_data = user_data;

	return supplicant_dbus_method_call(SUPPLICANT_PATH,
						SUPPLICANT_INTERFACE,
						"GetInterface",
						interface_get_params,
						interface_get_result, data);
}

int supplicant_interface_remove(struct supplicant_interface *interface,
			supplicant_interface_remove_callback callback,
							void *user_data)
{
	if (!interface)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	return 0;
}

struct interface_scan_data {
	struct supplicant_interface *interface;
	supplicant_interface_scan_callback callback;
	void *user_data;
};

static void interface_scan_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_scan_data *data = user_data;

	if (error) {
		if (data->callback)
			data->callback(-EIO, data->user_data);
	} else {
		data->interface->scan_callback = data->callback;
		data->interface->scan_data = data->user_data;
	}

	dbus_free(data);
}

static void interface_scan_params(DBusMessageIter *iter, void *user_data)
{
	DBusMessageIter dict;
	const char *type = "passive";

	supplicant_dbus_dict_open(iter, &dict);

	supplicant_dbus_dict_append_basic(&dict, "Type",
						DBUS_TYPE_STRING, &type);

	supplicant_dbus_dict_close(iter, &dict);
}

int supplicant_interface_scan(struct supplicant_interface *interface,
			supplicant_interface_scan_callback callback,
							void *user_data)
{
	struct interface_scan_data *data;

	if (!interface)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	if (interface->scanning)
		return -EALREADY;

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->interface = interface;
	data->callback = callback;
	data->user_data = user_data;

	return supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface", "Scan",
			interface_scan_params, interface_scan_result, data);
}

struct interface_disconnect_data {
	supplicant_interface_disconnect_callback callback;
	void *user_data;
};

static void interface_disconnect_result(const char *error,
				DBusMessageIter *iter, void *user_data)
{
	struct interface_disconnect_data *data = user_data;
	int result = 0;

	if (error)
		result = -EIO;

	if (data->callback)
		data->callback(result, data->user_data);

	dbus_free(data);
}

int supplicant_interface_disconnect(struct supplicant_interface *interface,
			supplicant_interface_disconnect_callback callback,
							void *user_data)
{
	struct interface_disconnect_data *data;

	if (!interface)
		return -EINVAL;

	if (!system_available)
		return -EFAULT;

	data = dbus_malloc0(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->callback = callback;
	data->user_data = user_data;

	return supplicant_dbus_method_call(interface->path,
			SUPPLICANT_INTERFACE ".Interface", "Disconnect",
				NULL, interface_disconnect_result, data);
}
