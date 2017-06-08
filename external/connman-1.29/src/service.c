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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <gdbus.h>
#include <ctype.h>
#include <stdint.h>

#include <connman/storage.h>
#include <connman/setting.h>
#include <connman/agent.h>

#include "connman.h"

#define CONNECT_TIMEOUT		120

static DBusConnection *connection = NULL;

static GList *service_list = NULL;
static GHashTable *service_hash = NULL;
static GSList *counter_list = NULL;
static unsigned int autoconnect_timeout = 0;
static unsigned int vpn_autoconnect_timeout = 0;
static struct connman_service *current_default = NULL;
static bool services_dirty = false;
static char *last_explicitly_connected_service = NULL;

struct connman_stats {
	bool valid;
	bool enabled;
	struct connman_stats_data data_last;
	struct connman_stats_data data;
	GTimer *timer;
};

struct connman_stats_counter {
	bool append_all;
	struct connman_stats stats;
	struct connman_stats stats_roaming;
};

struct connman_service {
	int refcount;
	char *identifier;
	char *path;
	enum connman_service_type type;
	enum connman_service_security security;
	enum connman_service_state state;
	enum connman_service_state state_ipv4;
	enum connman_service_state state_ipv6;
	enum connman_service_error error;
	enum connman_service_connect_reason connect_reason;
	uint8_t strength;
	bool favorite;
	bool immutable;
	bool hidden;
	bool ignore;
	bool autoconnect;
	GTimeVal modified;
	unsigned int order;
	char *name;
	char *passphrase;
	bool roaming;
	struct connman_ipconfig *ipconfig_ipv4;
	struct connman_ipconfig *ipconfig_ipv6;
	struct connman_network *network;
	struct connman_provider *provider;
	char **nameservers;
	char **nameservers_config;
	char **nameservers_auto;
	char **domains;
	char *hostname;
	char *domainname;
	char **timeservers;
	char **timeservers_config;
	/* 802.1x settings from the config files */
	char *eap;
	char *identity;
	char *agent_identity;
	char *ca_cert_file;
	char *client_cert_file;
	char *private_key_file;
	char *private_key_passphrase;
	char *phase2;
	DBusMessage *pending;
	DBusMessage *provider_pending;
	guint timeout;
	struct connman_stats stats;
	struct connman_stats stats_roaming;
	GHashTable *counter_table;
	enum connman_service_proxy_method proxy;
	enum connman_service_proxy_method proxy_config;
	char **proxies;
	char **excludes;
	char *pac;
	bool wps;
	int online_check_count;
	bool do_split_routing;
	bool new_service;
	bool hidden_service;
	char *config_file;
	char *config_entry;
};

static bool allow_property_changed(struct connman_service *service);

static struct connman_ipconfig *create_ip4config(struct connman_service *service,
		int index, enum connman_ipconfig_method method);
static struct connman_ipconfig *create_ip6config(struct connman_service *service,
		int index);


struct find_data {
	const char *path;
	struct connman_service *service;
};

static void compare_path(gpointer value, gpointer user_data)
{
	struct connman_service *service = value;
	struct find_data *data = user_data;

	if (data->service)
		return;

	if (g_strcmp0(service->path, data->path) == 0)
		data->service = service;
}

static struct connman_service *find_service(const char *path)
{
	struct find_data data = { .path = path, .service = NULL };

	DBG("path %s", path);

	g_list_foreach(service_list, compare_path, &data);

	return data.service;
}

static const char *reason2string(enum connman_service_connect_reason reason)
{

	switch (reason) {
	case CONNMAN_SERVICE_CONNECT_REASON_NONE:
		return "none";
	case CONNMAN_SERVICE_CONNECT_REASON_USER:
		return "user";
	case CONNMAN_SERVICE_CONNECT_REASON_AUTO:
		return "auto";
	case CONNMAN_SERVICE_CONNECT_REASON_SESSION:
		return "session";
	}

	return "unknown";
}

const char *__connman_service_type2string(enum connman_service_type type)
{
	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
		break;
	case CONNMAN_SERVICE_TYPE_SYSTEM:
		return "system";
	case CONNMAN_SERVICE_TYPE_ETHERNET:
		return "ethernet";
	case CONNMAN_SERVICE_TYPE_WIFI:
		return "wifi";
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
		return "bluetooth";
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		return "cellular";
	case CONNMAN_SERVICE_TYPE_GPS:
		return "gps";
	case CONNMAN_SERVICE_TYPE_VPN:
		return "vpn";
	case CONNMAN_SERVICE_TYPE_GADGET:
		return "gadget";
	case CONNMAN_SERVICE_TYPE_P2P:
		return "p2p";
	}

	return NULL;
}

enum connman_service_type __connman_service_string2type(const char *str)
{
	if (!str)
		return CONNMAN_SERVICE_TYPE_UNKNOWN;

	if (strcmp(str, "ethernet") == 0)
		return CONNMAN_SERVICE_TYPE_ETHERNET;
	if (strcmp(str, "gadget") == 0)
		return CONNMAN_SERVICE_TYPE_GADGET;
	if (strcmp(str, "wifi") == 0)
		return CONNMAN_SERVICE_TYPE_WIFI;
	if (strcmp(str, "cellular") == 0)
		return CONNMAN_SERVICE_TYPE_CELLULAR;
	if (strcmp(str, "bluetooth") == 0)
		return CONNMAN_SERVICE_TYPE_BLUETOOTH;
	if (strcmp(str, "vpn") == 0)
		return CONNMAN_SERVICE_TYPE_VPN;
	if (strcmp(str, "gps") == 0)
		return CONNMAN_SERVICE_TYPE_GPS;
	if (strcmp(str, "system") == 0)
		return CONNMAN_SERVICE_TYPE_SYSTEM;
	if (strcmp(str, "p2p") == 0)
		return CONNMAN_SERVICE_TYPE_P2P;

	return CONNMAN_SERVICE_TYPE_UNKNOWN;
}

enum connman_service_security __connman_service_string2security(const char *str)
{
	if (!str)
		return CONNMAN_SERVICE_SECURITY_UNKNOWN;

	if (!strcmp(str, "psk"))
		return CONNMAN_SERVICE_SECURITY_PSK;
	if (!strcmp(str, "ieee8021x"))
		return CONNMAN_SERVICE_SECURITY_8021X;
	if (!strcmp(str, "none"))
		return CONNMAN_SERVICE_SECURITY_NONE;
	if (!strcmp(str, "wep"))
		return CONNMAN_SERVICE_SECURITY_WEP;

	return CONNMAN_SERVICE_SECURITY_UNKNOWN;
}

static const char *security2string(enum connman_service_security security)
{
	switch (security) {
	case CONNMAN_SERVICE_SECURITY_UNKNOWN:
		break;
	case CONNMAN_SERVICE_SECURITY_NONE:
		return "none";
	case CONNMAN_SERVICE_SECURITY_WEP:
		return "wep";
	case CONNMAN_SERVICE_SECURITY_PSK:
	case CONNMAN_SERVICE_SECURITY_WPA:
	case CONNMAN_SERVICE_SECURITY_RSN:
		return "psk";
	case CONNMAN_SERVICE_SECURITY_8021X:
		return "ieee8021x";
	}

	return NULL;
}

static const char *state2string(enum connman_service_state state)
{
	switch (state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
		break;
	case CONNMAN_SERVICE_STATE_IDLE:
		return "idle";
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
		return "association";
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
		return "configuration";
	case CONNMAN_SERVICE_STATE_READY:
		return "ready";
	case CONNMAN_SERVICE_STATE_ONLINE:
		return "online";
	case CONNMAN_SERVICE_STATE_DISCONNECT:
		return "disconnect";
	case CONNMAN_SERVICE_STATE_FAILURE:
		return "failure";
	}

	return NULL;
}

static const char *error2string(enum connman_service_error error)
{
	switch (error) {
	case CONNMAN_SERVICE_ERROR_UNKNOWN:
		break;
	case CONNMAN_SERVICE_ERROR_OUT_OF_RANGE:
		return "out-of-range";
	case CONNMAN_SERVICE_ERROR_PIN_MISSING:
		return "pin-missing";
	case CONNMAN_SERVICE_ERROR_DHCP_FAILED:
		return "dhcp-failed";
	case CONNMAN_SERVICE_ERROR_CONNECT_FAILED:
		return "connect-failed";
	case CONNMAN_SERVICE_ERROR_LOGIN_FAILED:
		return "login-failed";
	case CONNMAN_SERVICE_ERROR_AUTH_FAILED:
		return "auth-failed";
	case CONNMAN_SERVICE_ERROR_INVALID_KEY:
		return "invalid-key";
	}

	return NULL;
}

static const char *proxymethod2string(enum connman_service_proxy_method method)
{
	switch (method) {
	case CONNMAN_SERVICE_PROXY_METHOD_DIRECT:
		return "direct";
	case CONNMAN_SERVICE_PROXY_METHOD_MANUAL:
		return "manual";
	case CONNMAN_SERVICE_PROXY_METHOD_AUTO:
		return "auto";
	case CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN:
		break;
	}

	return NULL;
}

static enum connman_service_proxy_method string2proxymethod(const char *method)
{
	if (g_strcmp0(method, "direct") == 0)
		return CONNMAN_SERVICE_PROXY_METHOD_DIRECT;
	else if (g_strcmp0(method, "auto") == 0)
		return CONNMAN_SERVICE_PROXY_METHOD_AUTO;
	else if (g_strcmp0(method, "manual") == 0)
		return CONNMAN_SERVICE_PROXY_METHOD_MANUAL;
	else
		return CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN;
}

int __connman_service_load_modifiable(struct connman_service *service)
{
	GKeyFile *keyfile;
	GError *error = NULL;
	gchar *str;
	bool autoconnect;

	DBG("service %p", service);

	keyfile = connman_storage_load_service(service->identifier);
	if (!keyfile)
		return -EIO;

	switch (service->type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	case CONNMAN_SERVICE_TYPE_VPN:
		service->do_split_routing = g_key_file_get_boolean(keyfile,
				service->identifier, "SplitRouting", NULL);
		/* fall through */
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_ETHERNET:
		autoconnect = g_key_file_get_boolean(keyfile,
				service->identifier, "AutoConnect", &error);
		if (!error)
			service->autoconnect = autoconnect;
		g_clear_error(&error);
		break;
	}

	str = g_key_file_get_string(keyfile,
				service->identifier, "Modified", NULL);
	if (str) {
		g_time_val_from_iso8601(str, &service->modified);
		g_free(str);
	}

	g_key_file_free(keyfile);

	return 0;
}

static int service_load(struct connman_service *service)
{
	GKeyFile *keyfile;
	GError *error = NULL;
	gsize length;
	gchar *str;
	bool autoconnect;
	unsigned int ssid_len;
	int err = 0;

	DBG("service %p", service);

	keyfile = connman_storage_load_service(service->identifier);
	if (!keyfile) {
		service->new_service = true;
		return -EIO;
	} else
		service->new_service = false;

	switch (service->type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	case CONNMAN_SERVICE_TYPE_VPN:
		service->do_split_routing = g_key_file_get_boolean(keyfile,
				service->identifier, "SplitRouting", NULL);
		autoconnect = g_key_file_get_boolean(keyfile,
				service->identifier, "AutoConnect", &error);
		if (!error)
			service->autoconnect = autoconnect;
		g_clear_error(&error);
		break;
	case CONNMAN_SERVICE_TYPE_WIFI:
		if (!service->name) {
			gchar *name;

			name = g_key_file_get_string(keyfile,
					service->identifier, "Name", NULL);
			if (name) {
				g_free(service->name);
				service->name = name;
			}

			if (service->network)
				connman_network_set_name(service->network,
									name);
		}

		if (service->network &&
				!connman_network_get_blob(service->network,
						"WiFi.SSID", &ssid_len)) {
			gchar *hex_ssid;

			hex_ssid = g_key_file_get_string(keyfile,
							service->identifier,
								"SSID", NULL);

			if (hex_ssid) {
				gchar *ssid;
				unsigned int i, j = 0, hex;
				size_t hex_ssid_len = strlen(hex_ssid);

				ssid = g_try_malloc0(hex_ssid_len / 2);
				if (!ssid) {
					g_free(hex_ssid);
					err = -ENOMEM;
					goto done;
				}

				for (i = 0; i < hex_ssid_len; i += 2) {
					sscanf(hex_ssid + i, "%02x", &hex);
					ssid[j++] = hex;
				}

				connman_network_set_blob(service->network,
					"WiFi.SSID", ssid, hex_ssid_len / 2);
			}

			g_free(hex_ssid);
		}
		/* fall through */

	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		service->favorite = g_key_file_get_boolean(keyfile,
				service->identifier, "Favorite", NULL);

		/* fall through */

	case CONNMAN_SERVICE_TYPE_ETHERNET:
		autoconnect = g_key_file_get_boolean(keyfile,
				service->identifier, "AutoConnect", &error);
		if (!error)
			service->autoconnect = autoconnect;
		g_clear_error(&error);
		break;
	}

	str = g_key_file_get_string(keyfile,
				service->identifier, "Modified", NULL);
	if (str) {
		g_time_val_from_iso8601(str, &service->modified);
		g_free(str);
	}

	str = g_key_file_get_string(keyfile,
				service->identifier, "Passphrase", NULL);
	if (str) {
		g_free(service->passphrase);
		service->passphrase = str;
	}

	if (service->ipconfig_ipv4)
		__connman_ipconfig_load(service->ipconfig_ipv4, keyfile,
					service->identifier, "IPv4.");

	if (service->ipconfig_ipv6)
		__connman_ipconfig_load(service->ipconfig_ipv6, keyfile,
					service->identifier, "IPv6.");

	service->nameservers_config = g_key_file_get_string_list(keyfile,
			service->identifier, "Nameservers", &length, NULL);
	if (service->nameservers_config && length == 0) {
		g_strfreev(service->nameservers_config);
		service->nameservers_config = NULL;
	}

	service->timeservers_config = g_key_file_get_string_list(keyfile,
			service->identifier, "Timeservers", &length, NULL);
	if (service->timeservers_config && length == 0) {
		g_strfreev(service->timeservers_config);
		service->timeservers_config = NULL;
	}

	service->domains = g_key_file_get_string_list(keyfile,
			service->identifier, "Domains", &length, NULL);
	if (service->domains && length == 0) {
		g_strfreev(service->domains);
		service->domains = NULL;
	}

	str = g_key_file_get_string(keyfile,
				service->identifier, "Proxy.Method", NULL);
	if (str)
		service->proxy_config = string2proxymethod(str);

	g_free(str);

	service->proxies = g_key_file_get_string_list(keyfile,
			service->identifier, "Proxy.Servers", &length, NULL);
	if (service->proxies && length == 0) {
		g_strfreev(service->proxies);
		service->proxies = NULL;
	}

	service->excludes = g_key_file_get_string_list(keyfile,
			service->identifier, "Proxy.Excludes", &length, NULL);
	if (service->excludes && length == 0) {
		g_strfreev(service->excludes);
		service->excludes = NULL;
	}

	str = g_key_file_get_string(keyfile,
				service->identifier, "Proxy.URL", NULL);
	if (str) {
		g_free(service->pac);
		service->pac = str;
	}

	service->hidden_service = g_key_file_get_boolean(keyfile,
					service->identifier, "Hidden", NULL);

done:
	g_key_file_free(keyfile);

	return err;
}

static int service_save(struct connman_service *service)
{
	GKeyFile *keyfile;
	gchar *str;
	guint freq;
	const char *cst_str = NULL;
	int err = 0;

	DBG("service %p new %d", service, service->new_service);

	if (service->new_service)
		return -ESRCH;

	keyfile = __connman_storage_open_service(service->identifier);
	if (!keyfile)
		return -EIO;

	if (service->name)
		g_key_file_set_string(keyfile, service->identifier,
						"Name", service->name);

	switch (service->type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	case CONNMAN_SERVICE_TYPE_VPN:
		g_key_file_set_boolean(keyfile, service->identifier,
				"SplitRouting", service->do_split_routing);
		if (service->favorite)
			g_key_file_set_boolean(keyfile, service->identifier,
					"AutoConnect", service->autoconnect);
		break;
	case CONNMAN_SERVICE_TYPE_WIFI:
		if (service->network) {
			const unsigned char *ssid;
			unsigned int ssid_len = 0;

			ssid = connman_network_get_blob(service->network,
							"WiFi.SSID", &ssid_len);

			if (ssid && ssid_len > 0 && ssid[0] != '\0') {
				char *identifier = service->identifier;
				GString *ssid_str;
				unsigned int i;

				ssid_str = g_string_sized_new(ssid_len * 2);
				if (!ssid_str) {
					err = -ENOMEM;
					goto done;
				}

				for (i = 0; i < ssid_len; i++)
					g_string_append_printf(ssid_str,
							"%02x", ssid[i]);

				g_key_file_set_string(keyfile, identifier,
							"SSID", ssid_str->str);

				g_string_free(ssid_str, TRUE);
			}

			freq = connman_network_get_frequency(service->network);
			g_key_file_set_integer(keyfile, service->identifier,
						"Frequency", freq);
		}
		/* fall through */

	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		g_key_file_set_boolean(keyfile, service->identifier,
					"Favorite", service->favorite);

		g_key_file_remove_key(keyfile, service->identifier,
				"Failure", NULL);

		/* fall through */

	case CONNMAN_SERVICE_TYPE_ETHERNET:
		if (service->favorite)
			g_key_file_set_boolean(keyfile, service->identifier,
					"AutoConnect", service->autoconnect);
		break;
	}

	str = g_time_val_to_iso8601(&service->modified);
	if (str) {
		g_key_file_set_string(keyfile, service->identifier,
							"Modified", str);
		g_free(str);
	}

	if (service->passphrase && strlen(service->passphrase) > 0)
		g_key_file_set_string(keyfile, service->identifier,
					"Passphrase", service->passphrase);
	else
		g_key_file_remove_key(keyfile, service->identifier,
							"Passphrase", NULL);

	if (service->ipconfig_ipv4)
		__connman_ipconfig_save(service->ipconfig_ipv4, keyfile,
					service->identifier, "IPv4.");

	if (service->ipconfig_ipv6)
		__connman_ipconfig_save(service->ipconfig_ipv6, keyfile,
						service->identifier, "IPv6.");

	if (service->nameservers_config) {
		guint len = g_strv_length(service->nameservers_config);

		g_key_file_set_string_list(keyfile, service->identifier,
								"Nameservers",
				(const gchar **) service->nameservers_config, len);
	} else
	g_key_file_remove_key(keyfile, service->identifier,
							"Nameservers", NULL);

	if (service->timeservers_config) {
		guint len = g_strv_length(service->timeservers_config);

		g_key_file_set_string_list(keyfile, service->identifier,
								"Timeservers",
				(const gchar **) service->timeservers_config, len);
	} else
		g_key_file_remove_key(keyfile, service->identifier,
							"Timeservers", NULL);

	if (service->domains) {
		guint len = g_strv_length(service->domains);

		g_key_file_set_string_list(keyfile, service->identifier,
								"Domains",
				(const gchar **) service->domains, len);
	} else
		g_key_file_remove_key(keyfile, service->identifier,
							"Domains", NULL);

	cst_str = proxymethod2string(service->proxy_config);
	if (cst_str)
		g_key_file_set_string(keyfile, service->identifier,
				"Proxy.Method", cst_str);

	if (service->proxies) {
		guint len = g_strv_length(service->proxies);

		g_key_file_set_string_list(keyfile, service->identifier,
				"Proxy.Servers",
				(const gchar **) service->proxies, len);
	} else
		g_key_file_remove_key(keyfile, service->identifier,
						"Proxy.Servers", NULL);

	if (service->excludes) {
		guint len = g_strv_length(service->excludes);

		g_key_file_set_string_list(keyfile, service->identifier,
				"Proxy.Excludes",
				(const gchar **) service->excludes, len);
	} else
		g_key_file_remove_key(keyfile, service->identifier,
						"Proxy.Excludes", NULL);

	if (service->pac && strlen(service->pac) > 0)
		g_key_file_set_string(keyfile, service->identifier,
					"Proxy.URL", service->pac);
	else
		g_key_file_remove_key(keyfile, service->identifier,
							"Proxy.URL", NULL);

	if (service->hidden_service)
		g_key_file_set_boolean(keyfile, service->identifier, "Hidden",
									TRUE);

	if (service->config_file && strlen(service->config_file) > 0)
		g_key_file_set_string(keyfile, service->identifier,
				"Config.file", service->config_file);

	if (service->config_entry &&
					strlen(service->config_entry) > 0)
		g_key_file_set_string(keyfile, service->identifier,
				"Config.ident", service->config_entry);

done:
	__connman_storage_save_service(keyfile, service->identifier);

	g_key_file_free(keyfile);

	return err;
}

void __connman_service_save(struct connman_service *service)
{
	if (!service)
		return;

	service_save(service);
}

static enum connman_service_state combine_state(
					enum connman_service_state state_a,
					enum connman_service_state state_b)
{
	enum connman_service_state result;

	if (state_a == state_b) {
		result = state_a;
		goto done;
	}

	if (state_a == CONNMAN_SERVICE_STATE_UNKNOWN) {
		result = state_b;
		goto done;
	}

	if (state_b == CONNMAN_SERVICE_STATE_UNKNOWN) {
		result = state_a;
		goto done;
	}

	if (state_a == CONNMAN_SERVICE_STATE_IDLE) {
		result = state_b;
		goto done;
	}

	if (state_b == CONNMAN_SERVICE_STATE_IDLE) {
		result = state_a;
		goto done;
	}

	if (state_a == CONNMAN_SERVICE_STATE_ONLINE) {
		result = state_a;
		goto done;
	}

	if (state_b == CONNMAN_SERVICE_STATE_ONLINE) {
		result = state_b;
		goto done;
	}

	if (state_a == CONNMAN_SERVICE_STATE_READY) {
		result = state_a;
		goto done;
	}

	if (state_b == CONNMAN_SERVICE_STATE_READY) {
		result = state_b;
		goto done;
	}

	if (state_a == CONNMAN_SERVICE_STATE_CONFIGURATION) {
		result = state_a;
		goto done;
	}

	if (state_b == CONNMAN_SERVICE_STATE_CONFIGURATION) {
		result = state_b;
		goto done;
	}

	if (state_a == CONNMAN_SERVICE_STATE_ASSOCIATION) {
		result = state_a;
		goto done;
	}

	if (state_b == CONNMAN_SERVICE_STATE_ASSOCIATION) {
		result = state_b;
		goto done;
	}

	if (state_a == CONNMAN_SERVICE_STATE_DISCONNECT) {
		result = state_a;
		goto done;
	}

	if (state_b == CONNMAN_SERVICE_STATE_DISCONNECT) {
		result = state_b;
		goto done;
	}

	result = CONNMAN_SERVICE_STATE_FAILURE;

done:
	return result;
}

static bool is_connecting_state(struct connman_service *service,
					enum connman_service_state state)
{
	switch (state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_IDLE:
	case CONNMAN_SERVICE_STATE_FAILURE:
		if (service->network)
			return connman_network_get_connecting(service->network);
	case CONNMAN_SERVICE_STATE_DISCONNECT:
	case CONNMAN_SERVICE_STATE_READY:
	case CONNMAN_SERVICE_STATE_ONLINE:
		break;
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
		return true;
	}

	return false;
}

static bool is_connected_state(const struct connman_service *service,
					enum connman_service_state state)
{
	switch (state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_IDLE:
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
	case CONNMAN_SERVICE_STATE_DISCONNECT:
	case CONNMAN_SERVICE_STATE_FAILURE:
		break;
	case CONNMAN_SERVICE_STATE_READY:
	case CONNMAN_SERVICE_STATE_ONLINE:
		return true;
	}

	return false;
}

static bool is_idle_state(const struct connman_service *service,
				enum connman_service_state state)
{
	switch (state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
	case CONNMAN_SERVICE_STATE_READY:
	case CONNMAN_SERVICE_STATE_ONLINE:
	case CONNMAN_SERVICE_STATE_DISCONNECT:
	case CONNMAN_SERVICE_STATE_FAILURE:
		break;
	case CONNMAN_SERVICE_STATE_IDLE:
		return true;
	}

	return false;
}

static bool is_connecting(struct connman_service *service)
{
	return is_connecting_state(service, service->state);
}

static bool is_connected(struct connman_service *service)
{
	return is_connected_state(service, service->state);
}

static int nameserver_get_index(struct connman_service *service)
{
	switch (combine_state(service->state_ipv4, service->state_ipv6)) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_IDLE:
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
	case CONNMAN_SERVICE_STATE_FAILURE:
	case CONNMAN_SERVICE_STATE_DISCONNECT:
		return -1;
	case CONNMAN_SERVICE_STATE_READY:
	case CONNMAN_SERVICE_STATE_ONLINE:
		break;
	}

	return __connman_service_get_index(service);
}

static void remove_nameservers(struct connman_service *service,
		int index, char **ns)
{
	int i;

	if (!ns)
		return;

	if (index < 0)
		index = nameserver_get_index(service);

	if (index < 0)
			return;

	for (i = 0; ns[i]; i++)
		connman_resolver_remove(index, NULL, ns[i]);
}

static void remove_searchdomains(struct connman_service *service,
		int index, char **sd)
{
	int i;

	if (!sd)
		return;

	if (index < 0)
		index = nameserver_get_index(service);

	if (index < 0)
		return;

	for (i = 0; sd[i]; i++)
		connman_resolver_remove(index, sd[i], NULL);
}

static bool nameserver_available(struct connman_service *service, char *ns)
{
	int family;

	family = connman_inet_check_ipaddress(ns);

	if (family == AF_INET)
		return is_connected_state(service, service->state_ipv4);

	if (family == AF_INET6)
		return is_connected_state(service, service->state_ipv6);

	return false;
}

static void update_nameservers(struct connman_service *service)
{
	int index;
	char *ns;

	index = __connman_service_get_index(service);
	if (index < 0)
		return;

	switch (combine_state(service->state_ipv4, service->state_ipv6)) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_IDLE:
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
		return;
	case CONNMAN_SERVICE_STATE_FAILURE:
	case CONNMAN_SERVICE_STATE_DISCONNECT:
		connman_resolver_remove_all(index);
		return;
	case CONNMAN_SERVICE_STATE_READY:
	case CONNMAN_SERVICE_STATE_ONLINE:
		break;
	}

	if (service->nameservers_config) {
		int i;

		remove_nameservers(service, index, service->nameservers);

		i = g_strv_length(service->nameservers_config);
		while (i != 0) {
			i--;

			ns = service->nameservers_config[i];

			if (nameserver_available(service, ns))
				connman_resolver_append(index, NULL, ns);
		}
	} else if (service->nameservers) {
		int i;

		remove_nameservers(service, index, service->nameservers);

		i = g_strv_length(service->nameservers);
		while (i != 0) {
			i--;

			ns = service->nameservers[i];

			if (nameserver_available(service, ns))
				connman_resolver_append(index, NULL, ns);
		}
	}

	if (service->domains) {
		char *searchdomains[2] = {NULL, NULL};
		int i;

		searchdomains[0] = service->domainname;
		remove_searchdomains(service, index, searchdomains);

		i = g_strv_length(service->domains);
		while (i != 0) {
			i--;
			connman_resolver_append(index, service->domains[i],
						NULL);
		}
	} else if (service->domainname)
		connman_resolver_append(index, service->domainname, NULL);

	connman_resolver_flush();
}

/*
 * The is_auto variable is set to true when IPv6 autoconf nameservers are
 * inserted to resolver via netlink message (see rtnl.c:rtnl_newnduseropt()
 * for details) and not through service.c
 */
int __connman_service_nameserver_append(struct connman_service *service,
				const char *nameserver, bool is_auto)
{
	char **nameservers;
	int len, i;

	DBG("service %p nameserver %s auto %d",	service, nameserver, is_auto);

	if (!nameserver)
		return -EINVAL;

	if (is_auto)
		nameservers = service->nameservers_auto;
	else
		nameservers = service->nameservers;

	for (i = 0; nameservers && nameservers[i]; i++)
		if (g_strcmp0(nameservers[i], nameserver) == 0)
			return -EEXIST;

	if (nameservers) {
		len = g_strv_length(nameservers);
		nameservers = g_try_renew(char *, nameservers, len + 2);
	} else {
		len = 0;
		nameservers = g_try_new0(char *, len + 2);
	}

	if (!nameservers)
		return -ENOMEM;

	nameservers[len] = g_strdup(nameserver);
	if (!nameservers[len])
		return -ENOMEM;

	nameservers[len + 1] = NULL;

	if (is_auto) {
		service->nameservers_auto = nameservers;
	} else {
		service->nameservers = nameservers;
		update_nameservers(service);
	}

	return 0;
}

int __connman_service_nameserver_remove(struct connman_service *service,
				const char *nameserver, bool is_auto)
{
	char **servers, **nameservers;
	bool found = false;
	int len, i, j;

	DBG("service %p nameserver %s auto %d", service, nameserver, is_auto);

	if (!nameserver)
		return -EINVAL;

	if (is_auto)
		nameservers = service->nameservers_auto;
	else
		nameservers = service->nameservers;

	if (!nameservers)
		return 0;

	for (i = 0; nameservers && nameservers[i]; i++)
		if (g_strcmp0(nameservers[i], nameserver) == 0) {
			found = true;
			break;
		}

	if (!found)
		return 0;

	len = g_strv_length(nameservers);

	if (len == 1) {
		g_strfreev(nameservers);
		if (is_auto)
			service->nameservers_auto = NULL;
		else
			service->nameservers = NULL;

		return 0;
	}

	servers = g_try_new0(char *, len);
	if (!servers)
		return -ENOMEM;

	for (i = 0, j = 0; i < len; i++) {
		if (g_strcmp0(nameservers[i], nameserver) != 0) {
			servers[j] = g_strdup(nameservers[i]);
			if (!servers[j])
				return -ENOMEM;
			j++;
		}
	}
	servers[len - 1] = NULL;

	g_strfreev(nameservers);
	nameservers = servers;

	if (is_auto) {
		service->nameservers_auto = nameservers;
	} else {
		service->nameservers = nameservers;
		update_nameservers(service);
	}

	return 0;
}

void __connman_service_nameserver_clear(struct connman_service *service)
{
	g_strfreev(service->nameservers);
	service->nameservers = NULL;

	update_nameservers(service);
}

static void add_nameserver_route(int family, int index, char *nameserver,
				const char *gw)
{
	switch (family) {
	case AF_INET:
		if (connman_inet_compare_subnet(index, nameserver))
			break;

		if (connman_inet_add_host_route(index, nameserver, gw) < 0)
			/* For P-t-P link the above route add will fail */
			connman_inet_add_host_route(index, nameserver, NULL);
		break;

	case AF_INET6:
		if (connman_inet_add_ipv6_host_route(index, nameserver,
								gw) < 0)
			connman_inet_add_ipv6_host_route(index, nameserver,
							NULL);
		break;
	}
}

static void nameserver_add_routes(int index, char **nameservers,
					const char *gw)
{
	int i, ns_family, gw_family;

	gw_family = connman_inet_check_ipaddress(gw);
	if (gw_family < 0)
		return;

	for (i = 0; nameservers[i]; i++) {
		ns_family = connman_inet_check_ipaddress(nameservers[i]);
		if (ns_family < 0 || ns_family != gw_family)
			continue;

		add_nameserver_route(ns_family, index, nameservers[i], gw);
	}
}

static void nameserver_del_routes(int index, char **nameservers,
				enum connman_ipconfig_type type)
{
	int i, family;

	for (i = 0; nameservers[i]; i++) {
		family = connman_inet_check_ipaddress(nameservers[i]);
		if (family < 0)
			continue;

		switch (family) {
		case AF_INET:
			if (type != CONNMAN_IPCONFIG_TYPE_IPV6)
				connman_inet_del_host_route(index,
							nameservers[i]);
			break;
		case AF_INET6:
			if (type != CONNMAN_IPCONFIG_TYPE_IPV4)
				connman_inet_del_ipv6_host_route(index,
							nameservers[i]);
			break;
		}
	}
}

void __connman_service_nameserver_add_routes(struct connman_service *service,
						const char *gw)
{
	int index;

	if (!service)
		return;

	index = __connman_service_get_index(service);

	if (service->nameservers_config) {
		/*
		 * Configured nameserver takes preference over the
		 * discoverd nameserver gathered from DHCP, VPN, etc.
		 */
		nameserver_add_routes(index, service->nameservers_config, gw);
	} else if (service->nameservers) {
		/*
		 * We add nameservers host routes for nameservers that
		 * are not on our subnet. For those who are, the subnet
		 * route will be installed by the time the dns proxy code
		 * tries to reach them. The subnet route is installed
		 * when setting the interface IP address.
		 */
		nameserver_add_routes(index, service->nameservers, gw);
	}
}

void __connman_service_nameserver_del_routes(struct connman_service *service,
					enum connman_ipconfig_type type)
{
	int index;

	if (!service)
		return;

	index = __connman_service_get_index(service);

	if (service->nameservers_config)
		nameserver_del_routes(index, service->nameservers_config,
					type);
	else if (service->nameservers)
		nameserver_del_routes(index, service->nameservers, type);
}

static struct connman_stats *stats_get(struct connman_service *service)
{
	if (service->roaming)
		return &service->stats_roaming;
	else
		return &service->stats;
}

static bool stats_enabled(struct connman_service *service)
{
	struct connman_stats *stats = stats_get(service);

	return stats->enabled;
}

static void stats_start(struct connman_service *service)
{
	struct connman_stats *stats = stats_get(service);

	DBG("service %p", service);

	if (!stats->timer)
		return;

	stats->enabled = true;
	stats->data_last.time = stats->data.time;

	g_timer_start(stats->timer);
}

static void stats_stop(struct connman_service *service)
{
	struct connman_stats *stats = stats_get(service);
	unsigned int seconds;

	DBG("service %p", service);

	if (!stats->timer)
		return;

	if (!stats->enabled)
		return;

	g_timer_stop(stats->timer);

	seconds = g_timer_elapsed(stats->timer, NULL);
	stats->data.time = stats->data_last.time + seconds;

	stats->enabled = false;
}

static void reset_stats(struct connman_service *service)
{
	DBG("service %p", service);

	/* home */
	service->stats.valid = false;

	service->stats.data.rx_packets = 0;
	service->stats.data.tx_packets = 0;
	service->stats.data.rx_bytes = 0;
	service->stats.data.tx_bytes = 0;
	service->stats.data.rx_errors = 0;
	service->stats.data.tx_errors = 0;
	service->stats.data.rx_dropped = 0;
	service->stats.data.tx_dropped = 0;
	service->stats.data.time = 0;
	service->stats.data_last.time = 0;

	g_timer_reset(service->stats.timer);

	/* roaming */
	service->stats_roaming.valid = false;

	service->stats_roaming.data.rx_packets = 0;
	service->stats_roaming.data.tx_packets = 0;
	service->stats_roaming.data.rx_bytes = 0;
	service->stats_roaming.data.tx_bytes = 0;
	service->stats_roaming.data.rx_errors = 0;
	service->stats_roaming.data.tx_errors = 0;
	service->stats_roaming.data.rx_dropped = 0;
	service->stats_roaming.data.tx_dropped = 0;
	service->stats_roaming.data.time = 0;
	service->stats_roaming.data_last.time = 0;

	g_timer_reset(service->stats_roaming.timer);
}

struct connman_service *__connman_service_get_default(void)
{
	struct connman_service *service;

	if (!service_list)
		return NULL;

	service = service_list->data;

	if (!is_connected(service))
		return NULL;

	return service;
}

bool __connman_service_index_is_default(int index)
{
	struct connman_service *service;

	if (index < 0)
		return false;

	service = __connman_service_get_default();

	return __connman_service_get_index(service) == index;
}

static void default_changed(void)
{
	struct connman_service *service = __connman_service_get_default();

	if (service == current_default)
		return;

	DBG("current default %p %s", current_default,
		current_default ? current_default->identifier : "");
	DBG("new default %p %s", service, service ? service->identifier : "");

	__connman_service_timeserver_changed(current_default, NULL);

	current_default = service;

	if (service) {
		if (service->hostname &&
				connman_setting_get_bool("AllowHostnameUpdates"))
			__connman_utsname_set_hostname(service->hostname);

		if (service->domainname)
			__connman_utsname_set_domainname(service->domainname);
	}

	__connman_notifier_default_changed(service);
}

static void state_changed(struct connman_service *service)
{
	const char *str;

	__connman_notifier_service_state_changed(service, service->state);

	str = state2string(service->state);
	if (!str)
		return;

	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_basic(service->path,
				CONNMAN_SERVICE_INTERFACE, "State",
						DBUS_TYPE_STRING, &str);
}

static void strength_changed(struct connman_service *service)
{
	if (service->strength == 0)
		return;

	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_basic(service->path,
				CONNMAN_SERVICE_INTERFACE, "Strength",
					DBUS_TYPE_BYTE, &service->strength);
}

static void favorite_changed(struct connman_service *service)
{
	dbus_bool_t favorite;

	if (!service->path)
		return;

	if (!allow_property_changed(service))
		return;

	favorite = service->favorite;
	connman_dbus_property_changed_basic(service->path,
				CONNMAN_SERVICE_INTERFACE, "Favorite",
					DBUS_TYPE_BOOLEAN, &favorite);
}

static void immutable_changed(struct connman_service *service)
{
	dbus_bool_t immutable;

	if (!service->path)
		return;

	if (!allow_property_changed(service))
		return;

	immutable = service->immutable;
	connman_dbus_property_changed_basic(service->path,
				CONNMAN_SERVICE_INTERFACE, "Immutable",
					DBUS_TYPE_BOOLEAN, &immutable);
}

static void roaming_changed(struct connman_service *service)
{
	dbus_bool_t roaming;

	if (!service->path)
		return;

	if (!allow_property_changed(service))
		return;

	roaming = service->roaming;
	connman_dbus_property_changed_basic(service->path,
				CONNMAN_SERVICE_INTERFACE, "Roaming",
					DBUS_TYPE_BOOLEAN, &roaming);
}

static void autoconnect_changed(struct connman_service *service)
{
	dbus_bool_t autoconnect;

	if (!service->path)
		return;

	if (!allow_property_changed(service))
		return;

	autoconnect = service->autoconnect;
	connman_dbus_property_changed_basic(service->path,
				CONNMAN_SERVICE_INTERFACE, "AutoConnect",
				DBUS_TYPE_BOOLEAN, &autoconnect);
}

static void append_security(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;
	const char *str;

	str = security2string(service->security);
	if (str)
		dbus_message_iter_append_basic(iter,
				DBUS_TYPE_STRING, &str);

	/*
	 * Some access points incorrectly advertise WPS even when they
	 * are configured as open or no security, so filter
	 * appropriately.
	 */
	if (service->wps) {
		switch (service->security) {
		case CONNMAN_SERVICE_SECURITY_PSK:
		case CONNMAN_SERVICE_SECURITY_WPA:
		case CONNMAN_SERVICE_SECURITY_RSN:
			str = "wps";
			dbus_message_iter_append_basic(iter,
						DBUS_TYPE_STRING, &str);
			break;
		case CONNMAN_SERVICE_SECURITY_UNKNOWN:
		case CONNMAN_SERVICE_SECURITY_NONE:
		case CONNMAN_SERVICE_SECURITY_WEP:
		case CONNMAN_SERVICE_SECURITY_8021X:
			break;
		}
	}
}

static void append_ethernet(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (service->ipconfig_ipv4)
		__connman_ipconfig_append_ethernet(service->ipconfig_ipv4,
									iter);
	else if (service->ipconfig_ipv6)
		__connman_ipconfig_append_ethernet(service->ipconfig_ipv6,
									iter);
}

static void append_ipv4(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (!is_connected_state(service, service->state_ipv4))
		return;

	if (service->ipconfig_ipv4)
		__connman_ipconfig_append_ipv4(service->ipconfig_ipv4, iter);
}

static void append_ipv6(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (!is_connected_state(service, service->state_ipv6))
		return;

	if (service->ipconfig_ipv6)
		__connman_ipconfig_append_ipv6(service->ipconfig_ipv6, iter,
						service->ipconfig_ipv4);
}

static void append_ipv4config(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (service->ipconfig_ipv4)
		__connman_ipconfig_append_ipv4config(service->ipconfig_ipv4,
							iter);
}

static void append_ipv6config(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (service->ipconfig_ipv6)
		__connman_ipconfig_append_ipv6config(service->ipconfig_ipv6,
							iter);
}

static void append_nameservers(DBusMessageIter *iter,
		struct connman_service *service, char **servers)
{
	int i;
	bool available = true;

	for (i = 0; servers[i]; i++) {
		if (service)
			available = nameserver_available(service, servers[i]);

		DBG("servers[%d] %s available %d", i, servers[i], available);

		if (available)
			dbus_message_iter_append_basic(iter,
					DBUS_TYPE_STRING, &servers[i]);
	}
}

static void append_dns(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (!is_connected(service))
		return;

	if (service->nameservers_config) {
		append_nameservers(iter, service, service->nameservers_config);
		return;
	} else {
		if (service->nameservers)
			append_nameservers(iter, service,
					service->nameservers);

		if (service->nameservers_auto)
			append_nameservers(iter, service,
					service->nameservers_auto);

		if (!service->nameservers && !service->nameservers_auto) {
			char **ns;

			DBG("append fallback nameservers");

			ns = connman_setting_get_string_list("FallbackNameservers");
			if (ns)
				append_nameservers(iter, service, ns);
		}
	}
}

static void append_dnsconfig(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (!service->nameservers_config)
		return;

	append_nameservers(iter, NULL, service->nameservers_config);
}

static void append_ts(DBusMessageIter *iter, void *user_data)
{
	GSList *list = user_data;

	while (list) {
		char *timeserver = list->data;

		if (timeserver)
			dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
					&timeserver);

		list = g_slist_next(list);
	}
}

static void append_tsconfig(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;
	int i;

	if (!service->timeservers_config)
		return;

	for (i = 0; service->timeservers_config[i]; i++) {
		dbus_message_iter_append_basic(iter,
				DBUS_TYPE_STRING,
				&service->timeservers_config[i]);
	}
}

static void append_domainconfig(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;
	int i;

	if (!service->domains)
		return;

	for (i = 0; service->domains[i]; i++)
		dbus_message_iter_append_basic(iter,
				DBUS_TYPE_STRING, &service->domains[i]);
}

static void append_domain(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (!is_connected(service) &&
				!is_connecting(service))
		return;

	if (service->domains)
		append_domainconfig(iter, user_data);
	else if (service->domainname)
		dbus_message_iter_append_basic(iter,
				DBUS_TYPE_STRING, &service->domainname);
}

static void append_proxies(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;
	int i;

	if (!service->proxies)
		return;

	for (i = 0; service->proxies[i]; i++)
		dbus_message_iter_append_basic(iter,
				DBUS_TYPE_STRING, &service->proxies[i]);
}

static void append_excludes(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;
	int i;

	if (!service->excludes)
		return;

	for (i = 0; service->excludes[i]; i++)
		dbus_message_iter_append_basic(iter,
				DBUS_TYPE_STRING, &service->excludes[i]);
}

static void append_proxy(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;
	enum connman_service_proxy_method proxy;
	const char *pac = NULL;
	const char *method = proxymethod2string(
		CONNMAN_SERVICE_PROXY_METHOD_DIRECT);

	if (!is_connected(service))
		return;

	proxy = connman_service_get_proxy_method(service);

	switch (proxy) {
	case CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN:
		return;
	case CONNMAN_SERVICE_PROXY_METHOD_DIRECT:
		goto done;
	case CONNMAN_SERVICE_PROXY_METHOD_MANUAL:
		connman_dbus_dict_append_array(iter, "Servers",
					DBUS_TYPE_STRING, append_proxies,
					service);

		connman_dbus_dict_append_array(iter, "Excludes",
					DBUS_TYPE_STRING, append_excludes,
					service);
		break;
	case CONNMAN_SERVICE_PROXY_METHOD_AUTO:
		/* Maybe DHCP, or WPAD,  has provided an url for a pac file */
		if (service->ipconfig_ipv4)
			pac = __connman_ipconfig_get_proxy_autoconfig(
				service->ipconfig_ipv4);
		else if (service->ipconfig_ipv6)
			pac = __connman_ipconfig_get_proxy_autoconfig(
				service->ipconfig_ipv6);

		if (!service->pac && !pac)
			goto done;

		if (service->pac)
			pac = service->pac;

		connman_dbus_dict_append_basic(iter, "URL",
					DBUS_TYPE_STRING, &pac);
		break;
	}

	method = proxymethod2string(proxy);

done:
	connman_dbus_dict_append_basic(iter, "Method",
					DBUS_TYPE_STRING, &method);
}

static void append_proxyconfig(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;
	const char *method;

	if (service->proxy_config == CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN)
		return;

	switch (service->proxy_config) {
	case CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN:
		return;
	case CONNMAN_SERVICE_PROXY_METHOD_DIRECT:
		break;
	case CONNMAN_SERVICE_PROXY_METHOD_MANUAL:
		if (service->proxies)
			connman_dbus_dict_append_array(iter, "Servers",
						DBUS_TYPE_STRING,
						append_proxies, service);

		if (service->excludes)
			connman_dbus_dict_append_array(iter, "Excludes",
						DBUS_TYPE_STRING,
						append_excludes, service);
		break;
	case CONNMAN_SERVICE_PROXY_METHOD_AUTO:
		if (service->pac)
			connman_dbus_dict_append_basic(iter, "URL",
					DBUS_TYPE_STRING, &service->pac);
		break;
	}

	method = proxymethod2string(service->proxy_config);

	connman_dbus_dict_append_basic(iter, "Method",
				DBUS_TYPE_STRING, &method);
}

static void append_provider(DBusMessageIter *iter, void *user_data)
{
	struct connman_service *service = user_data;

	if (!is_connected(service))
		return;

	if (service->provider)
		__connman_provider_append_properties(service->provider, iter);
}


static void settings_changed(struct connman_service *service,
				struct connman_ipconfig *ipconfig)
{
	enum connman_ipconfig_type type;

	if (!allow_property_changed(service))
		return;

	type = __connman_ipconfig_get_config_type(ipconfig);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		connman_dbus_property_changed_dict(service->path,
					CONNMAN_SERVICE_INTERFACE, "IPv4",
					append_ipv4, service);
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		connman_dbus_property_changed_dict(service->path,
					CONNMAN_SERVICE_INTERFACE, "IPv6",
					append_ipv6, service);

	__connman_notifier_ipconfig_changed(service, ipconfig);
}

static void ipv4_configuration_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_dict(service->path,
					CONNMAN_SERVICE_INTERFACE,
							"IPv4.Configuration",
							append_ipv4config,
							service);
}

static void ipv6_configuration_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_dict(service->path,
					CONNMAN_SERVICE_INTERFACE,
							"IPv6.Configuration",
							append_ipv6config,
							service);
}

static void dns_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_array(service->path,
				CONNMAN_SERVICE_INTERFACE, "Nameservers",
					DBUS_TYPE_STRING, append_dns, service);
}

static void dns_configuration_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_array(service->path,
				CONNMAN_SERVICE_INTERFACE,
				"Nameservers.Configuration",
				DBUS_TYPE_STRING, append_dnsconfig, service);

	dns_changed(service);
}

static void domain_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_array(service->path,
				CONNMAN_SERVICE_INTERFACE, "Domains",
				DBUS_TYPE_STRING, append_domain, service);
}

static void domain_configuration_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_array(service->path,
				CONNMAN_SERVICE_INTERFACE,
				"Domains.Configuration",
				DBUS_TYPE_STRING, append_domainconfig, service);
}

static void proxy_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_dict(service->path,
					CONNMAN_SERVICE_INTERFACE, "Proxy",
							append_proxy, service);
}

static void proxy_configuration_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_dict(service->path,
			CONNMAN_SERVICE_INTERFACE, "Proxy.Configuration",
						append_proxyconfig, service);

	proxy_changed(service);
}

static void timeservers_configuration_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_array(service->path,
			CONNMAN_SERVICE_INTERFACE,
			"Timeservers.Configuration",
			DBUS_TYPE_STRING,
			append_tsconfig, service);
}

static void link_changed(struct connman_service *service)
{
	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_dict(service->path,
					CONNMAN_SERVICE_INTERFACE, "Ethernet",
						append_ethernet, service);
}

static void stats_append_counters(DBusMessageIter *dict,
			struct connman_stats_data *stats,
			struct connman_stats_data *counters,
			bool append_all)
{
	if (counters->rx_packets != stats->rx_packets || append_all) {
		counters->rx_packets = stats->rx_packets;
		connman_dbus_dict_append_basic(dict, "RX.Packets",
					DBUS_TYPE_UINT32, &stats->rx_packets);
	}

	if (counters->tx_packets != stats->tx_packets || append_all) {
		counters->tx_packets = stats->tx_packets;
		connman_dbus_dict_append_basic(dict, "TX.Packets",
					DBUS_TYPE_UINT32, &stats->tx_packets);
	}

	if (counters->rx_bytes != stats->rx_bytes || append_all) {
		counters->rx_bytes = stats->rx_bytes;
		connman_dbus_dict_append_basic(dict, "RX.Bytes",
					DBUS_TYPE_UINT32, &stats->rx_bytes);
	}

	if (counters->tx_bytes != stats->tx_bytes || append_all) {
		counters->tx_bytes = stats->tx_bytes;
		connman_dbus_dict_append_basic(dict, "TX.Bytes",
					DBUS_TYPE_UINT32, &stats->tx_bytes);
	}

	if (counters->rx_errors != stats->rx_errors || append_all) {
		counters->rx_errors = stats->rx_errors;
		connman_dbus_dict_append_basic(dict, "RX.Errors",
					DBUS_TYPE_UINT32, &stats->rx_errors);
	}

	if (counters->tx_errors != stats->tx_errors || append_all) {
		counters->tx_errors = stats->tx_errors;
		connman_dbus_dict_append_basic(dict, "TX.Errors",
					DBUS_TYPE_UINT32, &stats->tx_errors);
	}

	if (counters->rx_dropped != stats->rx_dropped || append_all) {
		counters->rx_dropped = stats->rx_dropped;
		connman_dbus_dict_append_basic(dict, "RX.Dropped",
					DBUS_TYPE_UINT32, &stats->rx_dropped);
	}

	if (counters->tx_dropped != stats->tx_dropped || append_all) {
		counters->tx_dropped = stats->tx_dropped;
		connman_dbus_dict_append_basic(dict, "TX.Dropped",
					DBUS_TYPE_UINT32, &stats->tx_dropped);
	}

	if (counters->time != stats->time || append_all) {
		counters->time = stats->time;
		connman_dbus_dict_append_basic(dict, "Time",
					DBUS_TYPE_UINT32, &stats->time);
	}
}

static void stats_append(struct connman_service *service,
				const char *counter,
				struct connman_stats_counter *counters,
				bool append_all)
{
	DBusMessageIter array, dict;
	DBusMessage *msg;

	DBG("service %p counter %s", service, counter);

	msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
	if (!msg)
		return;

	dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH,
				&service->path, DBUS_TYPE_INVALID);

	dbus_message_iter_init_append(msg, &array);

	/* home counter */
	connman_dbus_dict_open(&array, &dict);

	stats_append_counters(&dict, &service->stats.data,
				&counters->stats.data, append_all);

	connman_dbus_dict_close(&array, &dict);

	/* roaming counter */
	connman_dbus_dict_open(&array, &dict);

	stats_append_counters(&dict, &service->stats_roaming.data,
				&counters->stats_roaming.data, append_all);

	connman_dbus_dict_close(&array, &dict);

	__connman_counter_send_usage(counter, msg);
}

static void stats_update(struct connman_service *service,
				unsigned int rx_packets, unsigned int tx_packets,
				unsigned int rx_bytes, unsigned int tx_bytes,
				unsigned int rx_errors, unsigned int tx_errors,
				unsigned int rx_dropped, unsigned int tx_dropped)
{
	struct connman_stats *stats = stats_get(service);
	struct connman_stats_data *data_last = &stats->data_last;
	struct connman_stats_data *data = &stats->data;
	unsigned int seconds;

	DBG("service %p", service);

	if (stats->valid) {
		data->rx_packets +=
			rx_packets - data_last->rx_packets;
		data->tx_packets +=
			tx_packets - data_last->tx_packets;
		data->rx_bytes +=
			rx_bytes - data_last->rx_bytes;
		data->tx_bytes +=
			tx_bytes - data_last->tx_bytes;
		data->rx_errors +=
			rx_errors - data_last->rx_errors;
		data->tx_errors +=
			tx_errors - data_last->tx_errors;
		data->rx_dropped +=
			rx_dropped - data_last->rx_dropped;
		data->tx_dropped +=
			tx_dropped - data_last->tx_dropped;
	} else {
		stats->valid = true;
	}

	data_last->rx_packets = rx_packets;
	data_last->tx_packets = tx_packets;
	data_last->rx_bytes = rx_bytes;
	data_last->tx_bytes = tx_bytes;
	data_last->rx_errors = rx_errors;
	data_last->tx_errors = tx_errors;
	data_last->rx_dropped = rx_dropped;
	data_last->tx_dropped = tx_dropped;

	seconds = g_timer_elapsed(stats->timer, NULL);
	stats->data.time = stats->data_last.time + seconds;
}

void __connman_service_notify(struct connman_service *service,
			unsigned int rx_packets, unsigned int tx_packets,
			unsigned int rx_bytes, unsigned int tx_bytes,
			unsigned int rx_errors, unsigned int tx_errors,
			unsigned int rx_dropped, unsigned int tx_dropped)
{
	GHashTableIter iter;
	gpointer key, value;
	const char *counter;
	struct connman_stats_counter *counters;
	struct connman_stats_data *data;
	int err;

	if (!service)
		return;

	if (!is_connected(service))
		return;

	stats_update(service,
		rx_packets, tx_packets,
		rx_bytes, tx_bytes,
		rx_errors, tx_errors,
		rx_dropped, tx_dropped);

	data = &stats_get(service)->data;
	err = __connman_stats_update(service, service->roaming, data);
	if (err < 0)
		connman_error("Failed to store statistics for %s",
				service->identifier);

	g_hash_table_iter_init(&iter, service->counter_table);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		counter = key;
		counters = value;

		stats_append(service, counter, counters, counters->append_all);
		counters->append_all = false;
	}
}

int __connman_service_counter_register(const char *counter)
{
	struct connman_service *service;
	GList *list;
	struct connman_stats_counter *counters;

	DBG("counter %s", counter);

	counter_list = g_slist_prepend(counter_list, (gpointer)counter);

	for (list = service_list; list; list = list->next) {
		service = list->data;

		counters = g_try_new0(struct connman_stats_counter, 1);
		if (!counters)
			return -ENOMEM;

		counters->append_all = true;

		g_hash_table_replace(service->counter_table, (gpointer)counter,
					counters);
	}

	return 0;
}

void __connman_service_counter_unregister(const char *counter)
{
	struct connman_service *service;
	GList *list;

	DBG("counter %s", counter);

	for (list = service_list; list; list = list->next) {
		service = list->data;

		g_hash_table_remove(service->counter_table, counter);
	}

	counter_list = g_slist_remove(counter_list, counter);
}

int __connman_service_iterate_services(service_iterate_cb cb, void *user_data)
{
	GList *list;

	for (list = service_list; list; list = list->next) {
		struct connman_service *service = list->data;

		cb(service, user_data);
	}

	return 0;
}

static void append_properties(DBusMessageIter *dict, dbus_bool_t limited,
					struct connman_service *service)
{
	dbus_bool_t val;
	const char *str;
	GSList *list;

	str = __connman_service_type2string(service->type);
	if (str)
		connman_dbus_dict_append_basic(dict, "Type",
						DBUS_TYPE_STRING, &str);

	connman_dbus_dict_append_array(dict, "Security",
				DBUS_TYPE_STRING, append_security, service);

	str = state2string(service->state);
	if (str)
		connman_dbus_dict_append_basic(dict, "State",
						DBUS_TYPE_STRING, &str);

	str = error2string(service->error);
	if (str)
		connman_dbus_dict_append_basic(dict, "Error",
						DBUS_TYPE_STRING, &str);

	if (service->strength > 0)
		connman_dbus_dict_append_basic(dict, "Strength",
					DBUS_TYPE_BYTE, &service->strength);

	val = service->favorite;
	connman_dbus_dict_append_basic(dict, "Favorite",
					DBUS_TYPE_BOOLEAN, &val);

	val = service->immutable;
	connman_dbus_dict_append_basic(dict, "Immutable",
					DBUS_TYPE_BOOLEAN, &val);

	if (service->favorite)
		val = service->autoconnect;
	else
		val = service->favorite;

	connman_dbus_dict_append_basic(dict, "AutoConnect",
				DBUS_TYPE_BOOLEAN, &val);

	if (service->name)
		connman_dbus_dict_append_basic(dict, "Name",
					DBUS_TYPE_STRING, &service->name);

	switch (service->type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_VPN:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		val = service->roaming;
		connman_dbus_dict_append_basic(dict, "Roaming",
					DBUS_TYPE_BOOLEAN, &val);

		connman_dbus_dict_append_dict(dict, "Ethernet",
						append_ethernet, service);
		break;
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_GADGET:
		connman_dbus_dict_append_dict(dict, "Ethernet",
						append_ethernet, service);
		break;
	}

	connman_dbus_dict_append_dict(dict, "IPv4", append_ipv4, service);

	connman_dbus_dict_append_dict(dict, "IPv4.Configuration",
						append_ipv4config, service);

	connman_dbus_dict_append_dict(dict, "IPv6", append_ipv6, service);

	connman_dbus_dict_append_dict(dict, "IPv6.Configuration",
						append_ipv6config, service);

	connman_dbus_dict_append_array(dict, "Nameservers",
				DBUS_TYPE_STRING, append_dns, service);

	connman_dbus_dict_append_array(dict, "Nameservers.Configuration",
				DBUS_TYPE_STRING, append_dnsconfig, service);

	if (service->state == CONNMAN_SERVICE_STATE_READY ||
			service->state == CONNMAN_SERVICE_STATE_ONLINE)
		list = __connman_timeserver_get_all(service);
	else
		list = NULL;

	connman_dbus_dict_append_array(dict, "Timeservers",
				DBUS_TYPE_STRING, append_ts, list);

	g_slist_free_full(list, g_free);

	connman_dbus_dict_append_array(dict, "Timeservers.Configuration",
				DBUS_TYPE_STRING, append_tsconfig, service);

	connman_dbus_dict_append_array(dict, "Domains",
				DBUS_TYPE_STRING, append_domain, service);

	connman_dbus_dict_append_array(dict, "Domains.Configuration",
				DBUS_TYPE_STRING, append_domainconfig, service);

	connman_dbus_dict_append_dict(dict, "Proxy", append_proxy, service);

	connman_dbus_dict_append_dict(dict, "Proxy.Configuration",
						append_proxyconfig, service);

	connman_dbus_dict_append_dict(dict, "Provider",
						append_provider, service);
}

static void append_struct_service(DBusMessageIter *iter,
		connman_dbus_append_cb_t function,
		struct connman_service *service)
{
	DBusMessageIter entry, dict;

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_OBJECT_PATH,
							&service->path);

	connman_dbus_dict_open(&entry, &dict);
	if (function)
		function(&dict, service);
	connman_dbus_dict_close(&entry, &dict);

	dbus_message_iter_close_container(iter, &entry);
}

static void append_dict_properties(DBusMessageIter *dict, void *user_data)
{
	struct connman_service *service = user_data;

	append_properties(dict, TRUE, service);
}

static void append_struct(gpointer value, gpointer user_data)
{
	struct connman_service *service = value;
	DBusMessageIter *iter = user_data;

	if (!service->path)
		return;

	append_struct_service(iter, append_dict_properties, service);
}

void __connman_service_list_struct(DBusMessageIter *iter)
{
	g_list_foreach(service_list, append_struct, iter);
}

bool __connman_service_is_hidden(struct connman_service *service)
{
	return service->hidden;
}

bool
__connman_service_is_split_routing(struct connman_service *service)
{
	return service->do_split_routing;
}

bool __connman_service_index_is_split_routing(int index)
{
	struct connman_service *service;

	if (index < 0)
		return false;

	service = __connman_service_lookup_from_index(index);
	if (!service)
		return false;

	return __connman_service_is_split_routing(service);
}

int __connman_service_get_index(struct connman_service *service)
{
	if (!service)
		return -1;

	if (service->network)
		return connman_network_get_index(service->network);
	else if (service->provider)
		return connman_provider_get_index(service->provider);

	return -1;
}

void __connman_service_set_hidden(struct connman_service *service)
{
	if (!service || service->hidden)
		return;

	service->hidden_service = true;
}

void __connman_service_set_hostname(struct connman_service *service,
						const char *hostname)
{
	if (!service || service->hidden)
		return;

	g_free(service->hostname);
	service->hostname = g_strdup(hostname);
}

const char *__connman_service_get_hostname(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->hostname;
}

void __connman_service_set_domainname(struct connman_service *service,
						const char *domainname)
{
	if (!service || service->hidden)
		return;

	g_free(service->domainname);
	service->domainname = g_strdup(domainname);

	domain_changed(service);
}

const char *connman_service_get_domainname(struct connman_service *service)
{
	if (!service)
		return NULL;

	if (service->domains)
		return service->domains[0];
	else
		return service->domainname;
}

char **connman_service_get_nameservers(struct connman_service *service)
{
	if (!service)
		return NULL;

	if (service->nameservers_config)
		return g_strdupv(service->nameservers_config);
	else if (service->nameservers ||
					service->nameservers_auto) {
		int len = 0, len_auto = 0, i;
		char **nameservers;

		if (service->nameservers)
			len = g_strv_length(service->nameservers);
		if (service->nameservers_auto)
			len_auto = g_strv_length(service->nameservers_auto);

		nameservers = g_try_new0(char *, len + len_auto + 1);
		if (!nameservers)
			return NULL;

		for (i = 0; i < len; i++)
			nameservers[i] = g_strdup(service->nameservers[i]);

		for (i = 0; i < len_auto; i++)
			nameservers[i + len] =
				g_strdup(service->nameservers_auto[i]);

		return nameservers;
	}

	return g_strdupv(connman_setting_get_string_list("FallbackNameservers"));
}

char **connman_service_get_timeservers_config(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->timeservers_config;
}

char **connman_service_get_timeservers(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->timeservers;
}

void connman_service_set_proxy_method(struct connman_service *service,
					enum connman_service_proxy_method method)
{
	if (!service || service->hidden)
		return;

	service->proxy = method;

	proxy_changed(service);

	if (method != CONNMAN_SERVICE_PROXY_METHOD_AUTO)
		__connman_notifier_proxy_changed(service);
}

enum connman_service_proxy_method connman_service_get_proxy_method(
					struct connman_service *service)
{
	if (!service)
		return CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN;

	if (service->proxy_config != CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN) {
		if (service->proxy_config == CONNMAN_SERVICE_PROXY_METHOD_AUTO &&
				!service->pac)
			return service->proxy;

		return service->proxy_config;
	}

	return service->proxy;
}

char **connman_service_get_proxy_servers(struct connman_service *service)
{
	return g_strdupv(service->proxies);
}

char **connman_service_get_proxy_excludes(struct connman_service *service)
{
	return g_strdupv(service->excludes);
}

const char *connman_service_get_proxy_url(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->pac;
}

void __connman_service_set_proxy_autoconfig(struct connman_service *service,
							const char *url)
{
	if (!service || service->hidden)
		return;

	service->proxy = CONNMAN_SERVICE_PROXY_METHOD_AUTO;

	if (service->ipconfig_ipv4) {
		if (__connman_ipconfig_set_proxy_autoconfig(
			    service->ipconfig_ipv4, url) < 0)
			return;
	} else if (service->ipconfig_ipv6) {
		if (__connman_ipconfig_set_proxy_autoconfig(
			    service->ipconfig_ipv6, url) < 0)
			return;
	} else
		return;

	proxy_changed(service);

	__connman_notifier_proxy_changed(service);
}

const char *connman_service_get_proxy_autoconfig(struct connman_service *service)
{
	if (!service)
		return NULL;

	if (service->ipconfig_ipv4)
		return __connman_ipconfig_get_proxy_autoconfig(
						service->ipconfig_ipv4);
	else if (service->ipconfig_ipv6)
		return __connman_ipconfig_get_proxy_autoconfig(
						service->ipconfig_ipv6);
	return NULL;
}

void __connman_service_set_timeservers(struct connman_service *service,
				char **timeservers)
{
	int i;

	if (!service)
		return;

	g_strfreev(service->timeservers);
	service->timeservers = NULL;

	for (i = 0; timeservers && timeservers[i]; i++)
		__connman_service_timeserver_append(service, timeservers[i]);
}

int __connman_service_timeserver_append(struct connman_service *service,
						const char *timeserver)
{
	int len;

	DBG("service %p timeserver %s", service, timeserver);

	if (!timeserver)
		return -EINVAL;

	if (service->timeservers) {
		int i;

		for (i = 0; service->timeservers[i]; i++)
			if (g_strcmp0(service->timeservers[i], timeserver) == 0)
				return -EEXIST;

		len = g_strv_length(service->timeservers);
		service->timeservers = g_try_renew(char *, service->timeservers,
							len + 2);
	} else {
		len = 0;
		service->timeservers = g_try_new0(char *, len + 2);
	}

	if (!service->timeservers)
		return -ENOMEM;

	service->timeservers[len] = g_strdup(timeserver);
	service->timeservers[len + 1] = NULL;

	return 0;
}

int __connman_service_timeserver_remove(struct connman_service *service,
						const char *timeserver)
{
	char **servers;
	int len, i, j, found = 0;

	DBG("service %p timeserver %s", service, timeserver);

	if (!timeserver)
		return -EINVAL;

	if (!service->timeservers)
		return 0;

	for (i = 0; service->timeservers &&
					service->timeservers[i]; i++)
		if (g_strcmp0(service->timeservers[i], timeserver) == 0) {
			found = 1;
			break;
		}

	if (found == 0)
		return 0;

	len = g_strv_length(service->timeservers);

	if (len == 1) {
		g_strfreev(service->timeservers);
		service->timeservers = NULL;

		return 0;
	}

	servers = g_try_new0(char *, len);
	if (!servers)
		return -ENOMEM;

	for (i = 0, j = 0; i < len; i++) {
		if (g_strcmp0(service->timeservers[i], timeserver) != 0) {
			servers[j] = g_strdup(service->timeservers[i]);
			if (!servers[j])
				return -ENOMEM;
			j++;
		}
	}
	servers[len - 1] = NULL;

	g_strfreev(service->timeservers);
	service->timeservers = servers;

	return 0;
}

void __connman_service_timeserver_changed(struct connman_service *service,
		GSList *ts_list)
{
	if (!service)
		return;

	if (!allow_property_changed(service))
		return;

	connman_dbus_property_changed_array(service->path,
			CONNMAN_SERVICE_INTERFACE, "Timeservers",
			DBUS_TYPE_STRING, append_ts, ts_list);
}

void __connman_service_set_pac(struct connman_service *service,
					const char *pac)
{
	if (service->hidden)
		return;
	g_free(service->pac);
	service->pac = g_strdup(pac);

	proxy_changed(service);
}

void __connman_service_set_identity(struct connman_service *service,
					const char *identity)
{
	if (service->immutable || service->hidden)
		return;

	g_free(service->identity);
	service->identity = g_strdup(identity);

	if (service->network)
		connman_network_set_string(service->network,
					"WiFi.Identity",
					service->identity);
}

void __connman_service_set_agent_identity(struct connman_service *service,
						const char *agent_identity)
{
	if (service->hidden)
		return;
	g_free(service->agent_identity);
	service->agent_identity = g_strdup(agent_identity);

	if (service->network)
		connman_network_set_string(service->network,
					"WiFi.AgentIdentity",
					service->agent_identity);
}

static int check_passphrase(enum connman_service_security security,
		const char *passphrase)
{
	guint i;
	gsize length;

	if (!passphrase)
		return 0;

	length = strlen(passphrase);

	switch (security) {
	case CONNMAN_SERVICE_SECURITY_UNKNOWN:
	case CONNMAN_SERVICE_SECURITY_NONE:
	case CONNMAN_SERVICE_SECURITY_WPA:
	case CONNMAN_SERVICE_SECURITY_RSN:

		DBG("service security '%s' (%d) not handled",
				security2string(security), security);

		return -EOPNOTSUPP;

	case CONNMAN_SERVICE_SECURITY_PSK:
		/* A raw key is always 64 bytes length,
		 * its content is in hex representation.
		 * A PSK key must be between [8..63].
		 */
		if (length == 64) {
			for (i = 0; i < 64; i++)
				if (!isxdigit((unsigned char)
					      passphrase[i]))
					return -ENOKEY;
		} else if (length < 8 || length > 63)
			return -ENOKEY;
		break;
	case CONNMAN_SERVICE_SECURITY_WEP:
		/* length of WEP key is 10 or 26
		 * length of WEP passphrase is 5 or 13
		 */
		if (length == 10 || length == 26) {
			for (i = 0; i < length; i++)
				if (!isxdigit((unsigned char)
					      passphrase[i]))
					return -ENOKEY;
		} else if (length != 5 && length != 13)
			return -ENOKEY;
		break;

	case CONNMAN_SERVICE_SECURITY_8021X:
		break;
	}

	return 0;
}

int __connman_service_set_passphrase(struct connman_service *service,
					const char *passphrase)
{
	int err;

	if (service->hidden)
		return -EINVAL;

	if (service->immutable &&
			service->security != CONNMAN_SERVICE_SECURITY_8021X)
		return -EINVAL;

	err = check_passphrase(service->security, passphrase);

	if (err < 0)
		return err;

	g_free(service->passphrase);
	service->passphrase = g_strdup(passphrase);

	if (service->network)
		connman_network_set_string(service->network, "WiFi.Passphrase",
				service->passphrase);

	return 0;
}

const char *__connman_service_get_passphrase(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->passphrase;
}

static DBusMessage *get_properties(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_service *service = user_data;
	DBusMessage *reply;
	DBusMessageIter array, dict;

	DBG("service %p", service);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &array);

	connman_dbus_dict_open(&array, &dict);
	append_properties(&dict, FALSE, service);
	connman_dbus_dict_close(&array, &dict);

	return reply;
}

static int update_proxy_configuration(struct connman_service *service,
				DBusMessageIter *array)
{
	DBusMessageIter dict;
	enum connman_service_proxy_method method;
	GString *servers_str = NULL;
	GString *excludes_str = NULL;
	const char *url = NULL;

	method = CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN;

	dbus_message_iter_recurse(array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, variant;
		const char *key;
		int type;

		dbus_message_iter_recurse(&dict, &entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
			goto error;

		dbus_message_iter_get_basic(&entry, &key);
		dbus_message_iter_next(&entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
			goto error;

		dbus_message_iter_recurse(&entry, &variant);

		type = dbus_message_iter_get_arg_type(&variant);

		if (g_str_equal(key, "Method")) {
			const char *val;

			if (type != DBUS_TYPE_STRING)
				goto error;

			dbus_message_iter_get_basic(&variant, &val);
			method = string2proxymethod(val);
		} else if (g_str_equal(key, "URL")) {
			if (type != DBUS_TYPE_STRING)
				goto error;

			dbus_message_iter_get_basic(&variant, &url);
		} else if (g_str_equal(key, "Servers")) {
			DBusMessageIter str_array;

			if (type != DBUS_TYPE_ARRAY)
				goto error;

			servers_str = g_string_new(NULL);
			if (!servers_str)
				goto error;

			dbus_message_iter_recurse(&variant, &str_array);

			while (dbus_message_iter_get_arg_type(&str_array) ==
							DBUS_TYPE_STRING) {
				char *val = NULL;

				dbus_message_iter_get_basic(&str_array, &val);

				if (servers_str->len > 0)
					g_string_append_printf(servers_str,
							" %s", val);
				else
					g_string_append(servers_str, val);

				dbus_message_iter_next(&str_array);
			}
		} else if (g_str_equal(key, "Excludes")) {
			DBusMessageIter str_array;

			if (type != DBUS_TYPE_ARRAY)
				goto error;

			excludes_str = g_string_new(NULL);
			if (!excludes_str)
				goto error;

			dbus_message_iter_recurse(&variant, &str_array);

			while (dbus_message_iter_get_arg_type(&str_array) ==
							DBUS_TYPE_STRING) {
				char *val = NULL;

				dbus_message_iter_get_basic(&str_array, &val);

				if (excludes_str->len > 0)
					g_string_append_printf(excludes_str,
							" %s", val);
				else
					g_string_append(excludes_str, val);

				dbus_message_iter_next(&str_array);
			}
		}

		dbus_message_iter_next(&dict);
	}

	switch (method) {
	case CONNMAN_SERVICE_PROXY_METHOD_DIRECT:
		break;
	case CONNMAN_SERVICE_PROXY_METHOD_MANUAL:
		if (!servers_str && !service->proxies)
			goto error;

		if (servers_str) {
			g_strfreev(service->proxies);

			if (servers_str->len > 0)
				service->proxies = g_strsplit_set(
					servers_str->str, " ", 0);
			else
				service->proxies = NULL;
		}

		if (excludes_str) {
			g_strfreev(service->excludes);

			if (excludes_str->len > 0)
				service->excludes = g_strsplit_set(
					excludes_str->str, " ", 0);
			else
				service->excludes = NULL;
		}

		if (!service->proxies)
			method = CONNMAN_SERVICE_PROXY_METHOD_DIRECT;

		break;
	case CONNMAN_SERVICE_PROXY_METHOD_AUTO:
		g_free(service->pac);

		if (url && strlen(url) > 0)
			service->pac = g_strdup(url);
		else
			service->pac = NULL;

		/* if we are connected:
		   - if service->pac == NULL
		   - if __connman_ipconfig_get_proxy_autoconfig(
		   service->ipconfig) == NULL
		   --> We should start WPAD */

		break;
	case CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN:
		goto error;
	}

	if (servers_str)
		g_string_free(servers_str, TRUE);

	if (excludes_str)
		g_string_free(excludes_str, TRUE);

	service->proxy_config = method;

	return 0;

error:
	if (servers_str)
		g_string_free(servers_str, TRUE);

	if (excludes_str)
		g_string_free(excludes_str, TRUE);

	return -EINVAL;
}

int __connman_service_reset_ipconfig(struct connman_service *service,
		enum connman_ipconfig_type type, DBusMessageIter *array,
		enum connman_service_state *new_state)
{
	struct connman_ipconfig *ipconfig, *new_ipconfig;
	enum connman_ipconfig_method old_method, new_method;
	enum connman_service_state state;
	int err = 0, index;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4) {
		ipconfig = service->ipconfig_ipv4;
		state = service->state_ipv4;
		new_method = CONNMAN_IPCONFIG_METHOD_DHCP;
	} else if (type == CONNMAN_IPCONFIG_TYPE_IPV6) {
		ipconfig = service->ipconfig_ipv6;
		state = service->state_ipv6;
		new_method = CONNMAN_IPCONFIG_METHOD_AUTO;
	} else
		return -EINVAL;

	if (!ipconfig)
		return -ENXIO;

	old_method = __connman_ipconfig_get_method(ipconfig);
	index = __connman_ipconfig_get_index(ipconfig);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		new_ipconfig = create_ip4config(service, index,
				CONNMAN_IPCONFIG_METHOD_UNKNOWN);
	else
		new_ipconfig = create_ip6config(service, index);

	if (array) {
		err = __connman_ipconfig_set_config(new_ipconfig, array);
		if (err < 0) {
			__connman_ipconfig_unref(new_ipconfig);
			return err;
		}

		new_method = __connman_ipconfig_get_method(new_ipconfig);
	}

	if (is_connecting_state(service, state) ||
					is_connected_state(service, state))
		__connman_network_clear_ipconfig(service->network, ipconfig);

	__connman_ipconfig_unref(ipconfig);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		service->ipconfig_ipv4 = new_ipconfig;
	else if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		service->ipconfig_ipv6 = new_ipconfig;

	if (is_connecting_state(service, state) ||
					is_connected_state(service, state))
		__connman_ipconfig_enable(new_ipconfig);

	if (new_state && new_method != old_method) {
		if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
			*new_state = service->state_ipv4;
		else
			*new_state = service->state_ipv6;

		__connman_service_auto_connect(CONNMAN_SERVICE_CONNECT_REASON_AUTO);
	}

	DBG("err %d ipconfig %p type %d method %d state %s", err,
		new_ipconfig, type, new_method,
		!new_state  ? "-" : state2string(*new_state));

	return err;
}

static DBusMessage *set_property(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_service *service = user_data;
	DBusMessageIter iter, value;
	const char *name;
	int type;

	DBG("service %p", service);

	if (!dbus_message_iter_init(msg, &iter))
		return __connman_error_invalid_arguments(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return __connman_error_invalid_arguments(msg);

	dbus_message_iter_get_basic(&iter, &name);
	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
		return __connman_error_invalid_arguments(msg);

	dbus_message_iter_recurse(&iter, &value);

	type = dbus_message_iter_get_arg_type(&value);

	if (g_str_equal(name, "AutoConnect")) {
		dbus_bool_t autoconnect;

		if (type != DBUS_TYPE_BOOLEAN)
			return __connman_error_invalid_arguments(msg);

		if (!service->favorite)
			return __connman_error_invalid_service(msg);

		dbus_message_iter_get_basic(&value, &autoconnect);

		if (service->autoconnect == autoconnect)
			return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);

		service->autoconnect = autoconnect;

		autoconnect_changed(service);

		if (autoconnect)
			__connman_service_auto_connect(CONNMAN_SERVICE_CONNECT_REASON_AUTO);

		service_save(service);
	} else if (g_str_equal(name, "Nameservers.Configuration")) {
		DBusMessageIter entry;
		GString *str;
		int index;
		const char *gw;

		if (__connman_provider_is_immutable(service->provider) ||
				service->immutable)
			return __connman_error_not_supported(msg);

		if (type != DBUS_TYPE_ARRAY)
			return __connman_error_invalid_arguments(msg);

		str = g_string_new(NULL);
		if (!str)
			return __connman_error_invalid_arguments(msg);

		index = __connman_service_get_index(service);
		gw = __connman_ipconfig_get_gateway_from_index(index,
			CONNMAN_IPCONFIG_TYPE_ALL);

		if (gw && strlen(gw))
			__connman_service_nameserver_del_routes(service,
						CONNMAN_IPCONFIG_TYPE_ALL);

		dbus_message_iter_recurse(&value, &entry);

		while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
			const char *val;
			dbus_message_iter_get_basic(&entry, &val);
			dbus_message_iter_next(&entry);
			if (connman_inet_check_ipaddress(val) > 0) {
				if (str->len > 0)
					g_string_append_printf(str, " %s", val);
				else
					g_string_append(str, val);
			}
		}

		remove_nameservers(service, -1, service->nameservers_config);
		g_strfreev(service->nameservers_config);

		if (str->len > 0) {
			service->nameservers_config =
				g_strsplit_set(str->str, " ", 0);
		} else {
			service->nameservers_config = NULL;
		}

		g_string_free(str, TRUE);

		if (gw && strlen(gw))
			__connman_service_nameserver_add_routes(service, gw);

		update_nameservers(service);
		dns_configuration_changed(service);

		if (__connman_service_is_connected_state(service,
						CONNMAN_IPCONFIG_TYPE_IPV4))
			__connman_wispr_start(service,
						CONNMAN_IPCONFIG_TYPE_IPV4);

		if (__connman_service_is_connected_state(service,
						CONNMAN_IPCONFIG_TYPE_IPV6))
			__connman_wispr_start(service,
						CONNMAN_IPCONFIG_TYPE_IPV6);

		service_save(service);
	} else if (g_str_equal(name, "Timeservers.Configuration")) {
		DBusMessageIter entry;
		GSList *list = NULL;
		int count = 0;

		if (service->immutable)
			return __connman_error_not_supported(msg);

		if (type != DBUS_TYPE_ARRAY)
			return __connman_error_invalid_arguments(msg);

		dbus_message_iter_recurse(&value, &entry);

		while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
			const char *val;
			GSList *new_head;

			dbus_message_iter_get_basic(&entry, &val);

			new_head = __connman_timeserver_add_list(list, val);
			if (list != new_head) {
				count++;
				list = new_head;
			}

			dbus_message_iter_next(&entry);
		}

		g_strfreev(service->timeservers_config);
		service->timeservers_config = NULL;

		if (list) {
			service->timeservers_config = g_new0(char *, count+1);

			while (list) {
				count--;
				service->timeservers_config[count] = list->data;
				list = g_slist_delete_link(list, list);
			};
		}

		service_save(service);
		timeservers_configuration_changed(service);

		if (service == __connman_service_get_default())
			__connman_timeserver_sync(service);

	} else if (g_str_equal(name, "Domains.Configuration")) {
		DBusMessageIter entry;
		GString *str;

		if (service->immutable)
			return __connman_error_not_supported(msg);

		if (type != DBUS_TYPE_ARRAY)
			return __connman_error_invalid_arguments(msg);

		str = g_string_new(NULL);
		if (!str)
			return __connman_error_invalid_arguments(msg);

		dbus_message_iter_recurse(&value, &entry);

		while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
			const char *val;
			dbus_message_iter_get_basic(&entry, &val);
			dbus_message_iter_next(&entry);
			if (str->len > 0)
				g_string_append_printf(str, " %s", val);
			else
				g_string_append(str, val);
		}

		remove_searchdomains(service, -1, service->domains);
		g_strfreev(service->domains);

		if (str->len > 0)
			service->domains = g_strsplit_set(str->str, " ", 0);
		else
			service->domains = NULL;

		g_string_free(str, TRUE);

		update_nameservers(service);
		domain_configuration_changed(service);
		domain_changed(service);

		service_save(service);
	} else if (g_str_equal(name, "Proxy.Configuration")) {
		int err;

		if (service->immutable)
			return __connman_error_not_supported(msg);

		if (type != DBUS_TYPE_ARRAY)
			return __connman_error_invalid_arguments(msg);

		err = update_proxy_configuration(service, &value);

		if (err < 0)
			return __connman_error_failed(msg, -err);

		proxy_configuration_changed(service);

		__connman_notifier_proxy_changed(service);

		service_save(service);
	} else if (g_str_equal(name, "IPv4.Configuration") ||
			g_str_equal(name, "IPv6.Configuration")) {

		enum connman_service_state state =
						CONNMAN_SERVICE_STATE_UNKNOWN;
		enum connman_ipconfig_type type =
			CONNMAN_IPCONFIG_TYPE_UNKNOWN;
		int err = 0;

		if (service->type == CONNMAN_SERVICE_TYPE_VPN ||
				service->immutable)
			return __connman_error_not_supported(msg);

		DBG("%s", name);

		if (!service->ipconfig_ipv4 &&
					!service->ipconfig_ipv6)
			return __connman_error_invalid_property(msg);

		if (g_str_equal(name, "IPv4.Configuration"))
			type = CONNMAN_IPCONFIG_TYPE_IPV4;
		else
			type = CONNMAN_IPCONFIG_TYPE_IPV6;

		err = __connman_service_reset_ipconfig(service, type, &value,
								&state);

		if (err < 0) {
			if (is_connected_state(service, state) ||
					is_connecting_state(service, state)) {
				__connman_network_enable_ipconfig(service->network,
							service->ipconfig_ipv4);
				__connman_network_enable_ipconfig(service->network,
							service->ipconfig_ipv6);
			}

			return __connman_error_failed(msg, -err);
		}

		if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
			ipv4_configuration_changed(service);
		else
			ipv6_configuration_changed(service);

		if (is_connecting(service) || is_connected(service)) {
			__connman_network_enable_ipconfig(service->network,
							service->ipconfig_ipv4);
			__connman_network_enable_ipconfig(service->network,
							service->ipconfig_ipv6);
		}

		service_save(service);
	} else
		return __connman_error_invalid_property(msg);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static void set_error(struct connman_service *service,
					enum connman_service_error error)
{
	const char *str;

	if (service->error == error)
		return;

	service->error = error;

	if (!service->path)
		return;

	if (!allow_property_changed(service))
		return;

	str = error2string(service->error);

	if (!str)
		str = "";

	connman_dbus_property_changed_basic(service->path,
				CONNMAN_SERVICE_INTERFACE, "Error",
				DBUS_TYPE_STRING, &str);
}

static DBusMessage *clear_property(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_service *service = user_data;
	const char *name;

	DBG("service %p", service);

	dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &name,
							DBUS_TYPE_INVALID);

	if (g_str_equal(name, "Error")) {
		set_error(service, CONNMAN_SERVICE_ERROR_UNKNOWN);

		g_get_current_time(&service->modified);
		service_save(service);
	} else
		return __connman_error_invalid_property(msg);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static bool is_ipconfig_usable(struct connman_service *service)
{
	if (!__connman_ipconfig_is_usable(service->ipconfig_ipv4) &&
			!__connman_ipconfig_is_usable(service->ipconfig_ipv6))
		return false;

	return true;
}

static bool is_ignore(struct connman_service *service)
{
	if (!service->autoconnect)
		return true;

	if (service->roaming)
		return true;

	if (service->ignore)
		return true;

	if (service->state == CONNMAN_SERVICE_STATE_FAILURE)
		return true;

	if (!is_ipconfig_usable(service))
		return true;

	return false;
}

static void disconnect_on_last_session(enum connman_service_type type)
{
	GList *list;

	for (list = service_list; list; list = list->next) {
		struct connman_service *service = list->data;

		if (service->type != type)
			continue;

		if (service->connect_reason != CONNMAN_SERVICE_CONNECT_REASON_SESSION)
			 continue;

		__connman_service_disconnect(service);
		return;
	}
}

static int active_sessions[MAX_CONNMAN_SERVICE_TYPES] = {};
static int active_count = 0;

void __connman_service_set_active_session(bool enable, GSList *list)
{
	if (!list)
		return;

	if (enable)
		active_count++;
	else
		active_count--;

	while (list != NULL) {
		enum connman_service_type type = GPOINTER_TO_INT(list->data);

		switch (type) {
		case CONNMAN_SERVICE_TYPE_ETHERNET:
		case CONNMAN_SERVICE_TYPE_WIFI:
		case CONNMAN_SERVICE_TYPE_BLUETOOTH:
		case CONNMAN_SERVICE_TYPE_CELLULAR:
		case CONNMAN_SERVICE_TYPE_GADGET:
			if (enable)
				active_sessions[type]++;
			else
				active_sessions[type]--;
			break;

		case CONNMAN_SERVICE_TYPE_UNKNOWN:
		case CONNMAN_SERVICE_TYPE_SYSTEM:
		case CONNMAN_SERVICE_TYPE_GPS:
		case CONNMAN_SERVICE_TYPE_VPN:
		case CONNMAN_SERVICE_TYPE_P2P:
			break;
		}

		if (active_sessions[type] == 0)
			disconnect_on_last_session(type);

		list = g_slist_next(list);
	}

	DBG("eth %d wifi %d bt %d cellular %d gadget %d sessions %d",
			active_sessions[CONNMAN_SERVICE_TYPE_ETHERNET],
			active_sessions[CONNMAN_SERVICE_TYPE_WIFI],
			active_sessions[CONNMAN_SERVICE_TYPE_BLUETOOTH],
			active_sessions[CONNMAN_SERVICE_TYPE_CELLULAR],
			active_sessions[CONNMAN_SERVICE_TYPE_GADGET],
			active_count);
}

struct preferred_tech_data {
	GList *preferred_list;
	enum connman_service_type type;
};

static void preferred_tech_add_by_type(gpointer data, gpointer user_data)
{
	struct connman_service *service = data;
	struct preferred_tech_data *tech_data = user_data;

	if (service->type == tech_data->type) {
		tech_data->preferred_list =
			g_list_append(tech_data->preferred_list, service);

		DBG("type %d service %p %s", tech_data->type, service,
				service->name);
	}
}

static GList *preferred_tech_list_get(void)
{
	unsigned int *tech_array;
	struct preferred_tech_data tech_data = { 0, };
	int i;

	tech_array = connman_setting_get_uint_list("PreferredTechnologies");
	if (!tech_array)
		return NULL;

	if (connman_setting_get_bool("SingleConnectedTechnology")) {
		GList *list;
		for (list = service_list; list; list = list->next) {
			struct connman_service *service = list->data;

			if (!is_connected(service))
				break;

			if (service->connect_reason ==
					CONNMAN_SERVICE_CONNECT_REASON_USER) {
				DBG("service %p name %s is user connected",
						service, service->name);
				return NULL;
			}
		}
	}

	for (i = 0; tech_array[i] != 0; i += 1) {
		tech_data.type = tech_array[i];
		g_list_foreach(service_list, preferred_tech_add_by_type,
				&tech_data);
	}

	return tech_data.preferred_list;
}

static bool auto_connect_service(GList *services,
				enum connman_service_connect_reason reason,
				bool preferred)
{
	struct connman_service *service = NULL;
	bool ignore[MAX_CONNMAN_SERVICE_TYPES] = { };
	bool autoconnecting = false;
	GList *list;

	DBG("preferred %d sessions %d reason %s", preferred, active_count,
		reason2string(reason));

	ignore[CONNMAN_SERVICE_TYPE_VPN] = true;

	for (list = services; list; list = list->next) {
		service = list->data;

		if (ignore[service->type]) {
			DBG("service %p type %s ignore", service,
				__connman_service_type2string(service->type));
			continue;
		}

		if (service->pending ||
				is_connecting(service) ||
				is_connected(service)) {
			if (!active_count)
				return true;

			ignore[service->type] = true;
			autoconnecting = true;

			DBG("service %p type %s busy", service,
				__connman_service_type2string(service->type));

			continue;
		}

		if (!service->favorite) {
			if (preferred)
			       continue;

			return autoconnecting;
		}

		if (is_ignore(service) || service->state !=
				CONNMAN_SERVICE_STATE_IDLE)
			continue;

		if (autoconnecting && !active_sessions[service->type]) {
			DBG("service %p type %s has no users", service,
				__connman_service_type2string(service->type));
			continue;
		}

		DBG("service %p %s %s", service, service->name,
			(preferred) ? "preferred" : reason2string(reason));

		__connman_service_connect(service, reason);

		if (!active_count)
			return true;

		ignore[service->type] = true;
	}

	return autoconnecting;
}

static gboolean run_auto_connect(gpointer data)
{
	enum connman_service_connect_reason reason = GPOINTER_TO_UINT(data);
	bool autoconnecting = false;
	GList *preferred_tech;

	autoconnect_timeout = 0;

	DBG("");

	preferred_tech = preferred_tech_list_get();
	if (preferred_tech) {
		autoconnecting = auto_connect_service(preferred_tech, reason,
							true);
		g_list_free(preferred_tech);
	}

	if (!autoconnecting || active_count)
		auto_connect_service(service_list, reason, false);

	return FALSE;
}

void __connman_service_auto_connect(enum connman_service_connect_reason reason)
{
	DBG("");

	if (autoconnect_timeout != 0)
		return;

	if (!__connman_session_policy_autoconnect(reason))
		return;

	autoconnect_timeout = g_timeout_add_seconds(0, run_auto_connect,
						GUINT_TO_POINTER(reason));
}

static gboolean run_vpn_auto_connect(gpointer data) {
	GList *list;
	bool need_split = false;

	vpn_autoconnect_timeout = 0;

	for (list = service_list; list; list = list->next) {
		struct connman_service *service = list->data;
		int res;

		if (service->type != CONNMAN_SERVICE_TYPE_VPN)
			continue;

		if (is_connected(service) || is_connecting(service)) {
			if (!service->do_split_routing)
				need_split = true;
			continue;
		}

		if (is_ignore(service) || !service->favorite)
			continue;

		if (need_split && !service->do_split_routing) {
			DBG("service %p no split routing", service);
			continue;
		}

		DBG("service %p %s %s", service, service->name,
				service->do_split_routing ?
				"split routing" : "");

		res = __connman_service_connect(service,
				CONNMAN_SERVICE_CONNECT_REASON_AUTO);
		if (res < 0 && res != -EINPROGRESS)
			continue;

		if (!service->do_split_routing)
			need_split = true;
	}

	return FALSE;
}

static void vpn_auto_connect(void)
{
	if (vpn_autoconnect_timeout)
		return;

	vpn_autoconnect_timeout =
		g_timeout_add_seconds(0, run_vpn_auto_connect, NULL);
}

static void remove_timeout(struct connman_service *service)
{
	if (service->timeout > 0) {
		g_source_remove(service->timeout);
		service->timeout = 0;
	}
}

static void reply_pending(struct connman_service *service, int error)
{
	remove_timeout(service);

	if (service->pending) {
		connman_dbus_reply_pending(service->pending, error, NULL);
		service->pending = NULL;
	}

	if (service->provider_pending) {
		connman_dbus_reply_pending(service->provider_pending,
						error, service->path);
		service->provider_pending = NULL;
	}
}

bool
__connman_service_is_provider_pending(struct connman_service *service)
{
	if (!service)
		return false;

	if (service->provider_pending)
		return true;

	return false;
}

void __connman_service_set_provider_pending(struct connman_service *service,
							DBusMessage *msg)
{
	if (service->provider_pending) {
		DBG("service %p provider pending msg %p already exists",
			service, service->provider_pending);
		return;
	}

	service->provider_pending = msg;
	return;
}

static void check_pending_msg(struct connman_service *service)
{
	if (!service->pending)
		return;

	DBG("service %p pending msg %p already exists", service,
						service->pending);
	dbus_message_unref(service->pending);
}

void __connman_service_set_hidden_data(struct connman_service *service,
							gpointer user_data)
{
	DBusMessage *pending = user_data;

	DBG("service %p pending %p", service, pending);

	if (!pending)
		return;

	check_pending_msg(service);

	service->pending = pending;
}

void __connman_service_return_error(struct connman_service *service,
				int error, gpointer user_data)
{
	DBG("service %p error %d user_data %p", service, error, user_data);

	__connman_service_set_hidden_data(service, user_data);

	reply_pending(service, error);
}

static gboolean connect_timeout(gpointer user_data)
{
	struct connman_service *service = user_data;
	bool autoconnect = false;

	DBG("service %p", service);

	service->timeout = 0;

	if (service->network)
		__connman_network_disconnect(service->network);
	else if (service->provider)
		connman_provider_disconnect(service->provider);

	__connman_ipconfig_disable(service->ipconfig_ipv4);
	__connman_ipconfig_disable(service->ipconfig_ipv6);

	__connman_stats_service_unregister(service);

	if (service->pending) {
		DBusMessage *reply;

		reply = __connman_error_operation_timeout(service->pending);
		if (reply)
			g_dbus_send_message(connection, reply);

		dbus_message_unref(service->pending);
		service->pending = NULL;
	} else
		autoconnect = true;

	__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_FAILURE,
					CONNMAN_IPCONFIG_TYPE_IPV4);
	__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_FAILURE,
					CONNMAN_IPCONFIG_TYPE_IPV6);

	if (autoconnect &&
			service->connect_reason !=
				CONNMAN_SERVICE_CONNECT_REASON_USER)
		__connman_service_auto_connect(CONNMAN_SERVICE_CONNECT_REASON_AUTO);

	return FALSE;
}

static DBusMessage *connect_service(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_service *service = user_data;
	int index, err = 0;
	GList *list;

	DBG("service %p", service);

	if (service->pending)
		return __connman_error_in_progress(msg);

	index = __connman_service_get_index(service);

	for (list = service_list; list; list = list->next) {
		struct connman_service *temp = list->data;

		if (!is_connecting(temp) && !is_connected(temp))
			break;

		if (service == temp)
			continue;

		if (service->type != temp->type)
			continue;

		if (__connman_service_get_index(temp) == index &&
				__connman_service_disconnect(temp) == -EINPROGRESS)
			err = -EINPROGRESS;

	}
	if (err == -EINPROGRESS)
		return __connman_error_operation_timeout(msg);

	service->ignore = false;

	service->pending = dbus_message_ref(msg);

	err = __connman_service_connect(service,
			CONNMAN_SERVICE_CONNECT_REASON_USER);

	if (err == -EINPROGRESS)
		return NULL;

	if (service->pending) {
		dbus_message_unref(service->pending);
		service->pending = NULL;
	}

	if (err < 0)
		return __connman_error_failed(msg, -err);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static DBusMessage *disconnect_service(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_service *service = user_data;
	int err;

	DBG("service %p", service);

	service->ignore = true;

	err = __connman_service_disconnect(service);
	if (err < 0 && err != -EINPROGRESS)
		return __connman_error_failed(msg, -err);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

bool __connman_service_remove(struct connman_service *service)
{
	if (service->type == CONNMAN_SERVICE_TYPE_ETHERNET ||
			service->type == CONNMAN_SERVICE_TYPE_GADGET)
		return false;

	if (service->immutable || service->hidden ||
			__connman_provider_is_immutable(service->provider))
		return false;

	if (!service->favorite && service->state !=
						CONNMAN_SERVICE_STATE_FAILURE)
		return false;

	__connman_service_disconnect(service);

	g_free(service->passphrase);
	service->passphrase = NULL;

	g_free(service->identity);
	service->identity = NULL;

	g_free(service->agent_identity);
	service->agent_identity = NULL;

	g_free(service->eap);
	service->eap = NULL;

	service->error = CONNMAN_SERVICE_ERROR_UNKNOWN;

	__connman_service_set_favorite(service, false);

	__connman_ipconfig_ipv6_reset_privacy(service->ipconfig_ipv6);

	service_save(service);

	return true;
}

static DBusMessage *remove_service(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_service *service = user_data;

	DBG("service %p", service);

	if (!__connman_service_remove(service))
		return __connman_error_not_supported(msg);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static bool check_suitable_state(enum connman_service_state a,
					enum connman_service_state b)
{
	/*
	 * Special check so that "ready" service can be moved before
	 * "online" one.
	 */
	if ((a == CONNMAN_SERVICE_STATE_ONLINE &&
			b == CONNMAN_SERVICE_STATE_READY) ||
		(b == CONNMAN_SERVICE_STATE_ONLINE &&
			a == CONNMAN_SERVICE_STATE_READY))
		return true;

	return a == b;
}

static void downgrade_state(struct connman_service *service)
{
	if (!service)
		return;

	DBG("service %p state4 %d state6 %d", service, service->state_ipv4,
						service->state_ipv6);

	if (service->state_ipv4 == CONNMAN_SERVICE_STATE_ONLINE)
		__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_READY,
						CONNMAN_IPCONFIG_TYPE_IPV4);

	if (service->state_ipv6 == CONNMAN_SERVICE_STATE_ONLINE)
		__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_READY,
						CONNMAN_IPCONFIG_TYPE_IPV6);
}

static void apply_relevant_default_downgrade(struct connman_service *service)
{
	struct connman_service *def_service;

	def_service = __connman_service_get_default();
	if (!def_service)
		return;

	if (def_service == service &&
			def_service->state == CONNMAN_SERVICE_STATE_ONLINE) {
		def_service->state = CONNMAN_SERVICE_STATE_READY;
		__connman_notifier_leave_online(def_service->type);
		state_changed(def_service);
	}
}

static void switch_default_service(struct connman_service *default_service,
		struct connman_service *downgrade_service)
{
	struct connman_service *service;
	GList *src, *dst;

	apply_relevant_default_downgrade(default_service);
	src = g_list_find(service_list, downgrade_service);
	dst = g_list_find(service_list, default_service);

	/* Nothing to do */
	if (src == dst || src->next == dst)
		return;

	service = src->data;
	service_list = g_list_delete_link(service_list, src);
	service_list = g_list_insert_before(service_list, dst, service);

	downgrade_state(downgrade_service);
}

static DBusMessage *move_service(DBusConnection *conn,
					DBusMessage *msg, void *user_data,
								bool before)
{
	struct connman_service *service = user_data;
	struct connman_service *target;
	const char *path;
	enum connman_ipconfig_method target4, target6;
	enum connman_ipconfig_method service4, service6;

	DBG("service %p", service);

	dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
							DBUS_TYPE_INVALID);

	if (!service->favorite)
		return __connman_error_not_supported(msg);

	target = find_service(path);
	if (!target || !target->favorite || target == service)
		return __connman_error_invalid_service(msg);

	if (target->type == CONNMAN_SERVICE_TYPE_VPN) {
		/*
		 * We only allow VPN route splitting if there are
		 * routes defined for a given VPN.
		 */
		if (!__connman_provider_check_routes(target->provider)) {
			connman_info("Cannot move service. "
				"No routes defined for provider %s",
				__connman_provider_get_ident(target->provider));
			return __connman_error_invalid_service(msg);
		}

		target->do_split_routing = true;
	} else
		target->do_split_routing = false;

	service->do_split_routing = false;

	target4 = __connman_ipconfig_get_method(target->ipconfig_ipv4);
	target6 = __connman_ipconfig_get_method(target->ipconfig_ipv6);
	service4 = __connman_ipconfig_get_method(service->ipconfig_ipv4);
	service6 = __connman_ipconfig_get_method(service->ipconfig_ipv6);

	DBG("target %s method %d/%d state %d/%d split %d", target->identifier,
		target4, target6, target->state_ipv4, target->state_ipv6,
		target->do_split_routing);

	DBG("service %s method %d/%d state %d/%d", service->identifier,
				service4, service6,
				service->state_ipv4, service->state_ipv6);

	/*
	 * If method is OFF, then we do not need to check the corresponding
	 * ipconfig state.
	 */
	if (target4 == CONNMAN_IPCONFIG_METHOD_OFF) {
		if (service6 != CONNMAN_IPCONFIG_METHOD_OFF) {
			if (!check_suitable_state(target->state_ipv6,
							service->state_ipv6))
				return __connman_error_invalid_service(msg);
		}
	}

	if (target6 == CONNMAN_IPCONFIG_METHOD_OFF) {
		if (service4 != CONNMAN_IPCONFIG_METHOD_OFF) {
			if (!check_suitable_state(target->state_ipv4,
							service->state_ipv4))
				return __connman_error_invalid_service(msg);
		}
	}

	if (service4 == CONNMAN_IPCONFIG_METHOD_OFF) {
		if (target6 != CONNMAN_IPCONFIG_METHOD_OFF) {
			if (!check_suitable_state(target->state_ipv6,
							service->state_ipv6))
				return __connman_error_invalid_service(msg);
		}
	}

	if (service6 == CONNMAN_IPCONFIG_METHOD_OFF) {
		if (target4 != CONNMAN_IPCONFIG_METHOD_OFF) {
			if (!check_suitable_state(target->state_ipv4,
							service->state_ipv4))
				return __connman_error_invalid_service(msg);
		}
	}

	g_get_current_time(&service->modified);
	service_save(service);
	service_save(target);

	/*
	 * If the service which goes down is the default service and is
	 * online, we downgrade directly its state to ready so:
	 * the service which goes up, needs to recompute its state which
	 * is triggered via downgrading it - if relevant - to state ready.
	 */
	if (before)
		switch_default_service(target, service);
	else
		switch_default_service(service, target);

	__connman_connection_update_gateway();

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static DBusMessage *move_before(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	return move_service(conn, msg, user_data, true);
}

static DBusMessage *move_after(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	return move_service(conn, msg, user_data, false);
}

static DBusMessage *reset_counters(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct connman_service *service = user_data;

	reset_stats(service);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static struct _services_notify {
	int id;
	GHashTable *add;
	GHashTable *remove;
} *services_notify;

static void service_append_added_foreach(gpointer data, gpointer user_data)
{
	struct connman_service *service = data;
	DBusMessageIter *iter = user_data;

	if (!service || !service->path) {
		DBG("service %p or path is NULL", service);
		return;
	}

	if (g_hash_table_lookup(services_notify->add, service->path)) {
		DBG("new %s", service->path);

		append_struct(service, iter);
		g_hash_table_remove(services_notify->add, service->path);
	} else {
		DBG("changed %s", service->path);

		append_struct_service(iter, NULL, service);
	}
}

static void service_append_ordered(DBusMessageIter *iter, void *user_data)
{
	g_list_foreach(service_list, service_append_added_foreach, iter);
}

static void append_removed(gpointer key, gpointer value, gpointer user_data)
{
	char *objpath = key;
	DBusMessageIter *iter = user_data;

	DBG("removed %s", objpath);
	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &objpath);
}

static void service_append_removed(DBusMessageIter *iter, void *user_data)
{
	g_hash_table_foreach(services_notify->remove, append_removed, iter);
}

static gboolean service_send_changed(gpointer data)
{
	DBusMessage *signal;

	DBG("");

	services_notify->id = 0;

	signal = dbus_message_new_signal(CONNMAN_MANAGER_PATH,
			CONNMAN_MANAGER_INTERFACE, "ServicesChanged");
	if (!signal)
		return FALSE;

	__connman_dbus_append_objpath_dict_array(signal,
					service_append_ordered, NULL);
	__connman_dbus_append_objpath_array(signal,
					service_append_removed, NULL);

	dbus_connection_send(connection, signal, NULL);
	dbus_message_unref(signal);

	g_hash_table_remove_all(services_notify->remove);
	g_hash_table_remove_all(services_notify->add);

	return FALSE;
}

static void service_schedule_changed(void)
{
	if (services_notify->id != 0)
		return;

	services_notify->id = g_timeout_add(100, service_send_changed, NULL);
}

static void service_schedule_added(struct connman_service *service)
{
	DBG("service %p", service);

	g_hash_table_remove(services_notify->remove, service->path);
	g_hash_table_replace(services_notify->add, service->path, service);

	service_schedule_changed();
}

static void service_schedule_removed(struct connman_service *service)
{
	if (!service || !service->path) {
		DBG("service %p or path is NULL", service);
		return;
	}

	DBG("service %p %s", service, service->path);

	g_hash_table_remove(services_notify->add, service->path);
	g_hash_table_replace(services_notify->remove, g_strdup(service->path),
			NULL);

	service_schedule_changed();
}

static bool allow_property_changed(struct connman_service *service)
{
	if (g_hash_table_lookup_extended(services_notify->add, service->path,
					NULL, NULL)) {
		DBG("no property updates for service %p", service);
		return false;
	}

	return true;
}

static const GDBusMethodTable service_methods[] = {
	{ GDBUS_DEPRECATED_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			get_properties) },
	{ GDBUS_METHOD("SetProperty",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" }),
			NULL, set_property) },
	{ GDBUS_METHOD("ClearProperty",
			GDBUS_ARGS({ "name", "s" }), NULL,
			clear_property) },
	{ GDBUS_ASYNC_METHOD("Connect", NULL, NULL,
			      connect_service) },
	{ GDBUS_METHOD("Disconnect", NULL, NULL,
			disconnect_service) },
	{ GDBUS_METHOD("Remove", NULL, NULL, remove_service) },
	{ GDBUS_METHOD("MoveBefore",
			GDBUS_ARGS({ "service", "o" }), NULL,
			move_before) },
	{ GDBUS_METHOD("MoveAfter",
			GDBUS_ARGS({ "service", "o" }), NULL,
			move_after) },
	{ GDBUS_METHOD("ResetCounters", NULL, NULL, reset_counters) },
	{ },
};

static const GDBusSignalTable service_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" })) },
	{ },
};

static void service_free(gpointer user_data)
{
	struct connman_service *service = user_data;
	char *path = service->path;

	DBG("service %p", service);

	reply_pending(service, ENOENT);

	__connman_notifier_service_remove(service);
	service_schedule_removed(service);

	__connman_wispr_stop(service);
	stats_stop(service);

	service->path = NULL;

	if (path) {
		__connman_connection_update_gateway();

		g_dbus_unregister_interface(connection, path,
						CONNMAN_SERVICE_INTERFACE);
		g_free(path);
	}

	g_hash_table_destroy(service->counter_table);

	if (service->network) {
		__connman_network_disconnect(service->network);
		connman_network_unref(service->network);
		service->network = NULL;
	}

	if (service->provider)
		connman_provider_unref(service->provider);

	if (service->ipconfig_ipv4) {
		__connman_ipconfig_set_ops(service->ipconfig_ipv4, NULL);
		__connman_ipconfig_set_data(service->ipconfig_ipv4, NULL);
		__connman_ipconfig_unref(service->ipconfig_ipv4);
		service->ipconfig_ipv4 = NULL;
	}

	if (service->ipconfig_ipv6) {
		__connman_ipconfig_set_ops(service->ipconfig_ipv6, NULL);
		__connman_ipconfig_set_data(service->ipconfig_ipv6, NULL);
		__connman_ipconfig_unref(service->ipconfig_ipv6);
		service->ipconfig_ipv6 = NULL;
	}

	g_strfreev(service->timeservers);
	g_strfreev(service->timeservers_config);
	g_strfreev(service->nameservers);
	g_strfreev(service->nameservers_config);
	g_strfreev(service->nameservers_auto);
	g_strfreev(service->domains);
	g_strfreev(service->proxies);
	g_strfreev(service->excludes);

	g_free(service->hostname);
	g_free(service->domainname);
	g_free(service->pac);
	g_free(service->name);
	g_free(service->passphrase);
	g_free(service->identifier);
	g_free(service->eap);
	g_free(service->identity);
	g_free(service->agent_identity);
	g_free(service->ca_cert_file);
	g_free(service->client_cert_file);
	g_free(service->private_key_file);
	g_free(service->private_key_passphrase);
	g_free(service->phase2);
	g_free(service->config_file);
	g_free(service->config_entry);

	if (service->stats.timer)
		g_timer_destroy(service->stats.timer);
	if (service->stats_roaming.timer)
		g_timer_destroy(service->stats_roaming.timer);

	if (current_default == service)
		current_default = NULL;

	g_free(service);
}

static void stats_init(struct connman_service *service)
{
	/* home */
	service->stats.valid = false;
	service->stats.enabled = false;
	service->stats.timer = g_timer_new();

	/* roaming */
	service->stats_roaming.valid = false;
	service->stats_roaming.enabled = false;
	service->stats_roaming.timer = g_timer_new();
}

static void service_initialize(struct connman_service *service)
{
	DBG("service %p", service);

	service->refcount = 1;

	service->error = CONNMAN_SERVICE_ERROR_UNKNOWN;

	service->type     = CONNMAN_SERVICE_TYPE_UNKNOWN;
	service->security = CONNMAN_SERVICE_SECURITY_UNKNOWN;

	service->state = CONNMAN_SERVICE_STATE_UNKNOWN;
	service->state_ipv4 = CONNMAN_SERVICE_STATE_UNKNOWN;
	service->state_ipv6 = CONNMAN_SERVICE_STATE_UNKNOWN;

	service->favorite  = false;
	service->immutable = false;
	service->hidden = false;

	service->ignore = false;

	service->connect_reason = CONNMAN_SERVICE_CONNECT_REASON_NONE;

	service->order = 0;

	stats_init(service);

	service->provider = NULL;

	service->wps = false;
}

/**
 * connman_service_create:
 *
 * Allocate a new service.
 *
 * Returns: a newly-allocated #connman_service structure
 */
struct connman_service *connman_service_create(void)
{
	GSList *list;
	struct connman_stats_counter *counters;
	const char *counter;

	struct connman_service *service;

	service = g_try_new0(struct connman_service, 1);
	if (!service)
		return NULL;

	DBG("service %p", service);

	service->counter_table = g_hash_table_new_full(g_str_hash,
						g_str_equal, NULL, g_free);

	for (list = counter_list; list; list = list->next) {
		counter = list->data;

		counters = g_try_new0(struct connman_stats_counter, 1);
		if (!counters) {
			g_hash_table_destroy(service->counter_table);
			g_free(service);
			return NULL;
		}

		counters->append_all = true;

		g_hash_table_replace(service->counter_table, (gpointer)counter,
				counters);
	}

	service_initialize(service);

	return service;
}

/**
 * connman_service_ref:
 * @service: service structure
 *
 * Increase reference counter of service
 */
struct connman_service *
connman_service_ref_debug(struct connman_service *service,
			const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", service, service->refcount + 1,
		file, line, caller);

	__sync_fetch_and_add(&service->refcount, 1);

	return service;
}

/**
 * connman_service_unref:
 * @service: service structure
 *
 * Decrease reference counter of service and release service if no
 * longer needed.
 */
void connman_service_unref_debug(struct connman_service *service,
			const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", service, service->refcount - 1,
		file, line, caller);

	if (__sync_fetch_and_sub(&service->refcount, 1) != 1)
		return;

	service_list = g_list_remove(service_list, service);

	__connman_service_disconnect(service);

	g_hash_table_remove(service_hash, service->identifier);
}

static gint service_compare(gconstpointer a, gconstpointer b)
{
	struct connman_service *service_a = (void *) a;
	struct connman_service *service_b = (void *) b;
	enum connman_service_state state_a, state_b;
	bool a_connected, b_connected;
	gint strength;

	state_a = service_a->state;
	state_b = service_b->state;
	a_connected = is_connected(service_a);
	b_connected = is_connected(service_b);

	if (a_connected && b_connected) {
		if (service_a->order > service_b->order)
			return -1;

		if (service_a->order < service_b->order)
			return 1;
	}

	if (state_a != state_b) {
		if (a_connected && b_connected) {
			/* We prefer online over ready state */
			if (state_a == CONNMAN_SERVICE_STATE_ONLINE)
				return -1;

			if (state_b == CONNMAN_SERVICE_STATE_ONLINE)
				return 1;
		}

		if (a_connected)
			return -1;
		if (b_connected)
			return 1;

		if (is_connecting(service_a))
			return -1;
		if (is_connecting(service_b))
			return 1;
	}

	if (service_a->favorite && !service_b->favorite)
		return -1;

	if (!service_a->favorite && service_b->favorite)
		return 1;

	if (service_a->type != service_b->type) {

		if (service_a->type == CONNMAN_SERVICE_TYPE_ETHERNET)
			return -1;
		if (service_b->type == CONNMAN_SERVICE_TYPE_ETHERNET)
			return 1;

		if (service_a->type == CONNMAN_SERVICE_TYPE_WIFI)
			return -1;
		if (service_b->type == CONNMAN_SERVICE_TYPE_WIFI)
			return 1;

		if (service_a->type == CONNMAN_SERVICE_TYPE_CELLULAR)
			return -1;
		if (service_b->type == CONNMAN_SERVICE_TYPE_CELLULAR)
			return 1;

		if (service_a->type == CONNMAN_SERVICE_TYPE_BLUETOOTH)
			return -1;
		if (service_b->type == CONNMAN_SERVICE_TYPE_BLUETOOTH)
			return 1;

		if (service_a->type == CONNMAN_SERVICE_TYPE_VPN)
			return -1;
		if (service_b->type == CONNMAN_SERVICE_TYPE_VPN)
			return 1;

		if (service_a->type == CONNMAN_SERVICE_TYPE_GADGET)
			return -1;
		if (service_b->type == CONNMAN_SERVICE_TYPE_GADGET)
			return 1;
	}

	// Prefer previously Explicitly connected service
	if (g_strcmp0(service_a->path, last_explicitly_connected_service) == 0)
		return -1;
	if (g_strcmp0(service_b->path, last_explicitly_connected_service) == 0)
		return 1;

	strength = (gint) service_b->strength - (gint) service_a->strength;
	if (strength)
		return strength;

	return g_strcmp0(service_a->name, service_b->name);
}

static void service_list_sort(void)
{
	if (service_list && service_list->next) {
		service_list = g_list_sort(service_list, service_compare);
		service_schedule_changed();
	}
}

/**
 * connman_service_get_type:
 * @service: service structure
 *
 * Get the type of service
 */
enum connman_service_type connman_service_get_type(struct connman_service *service)
{
	if (!service)
		return CONNMAN_SERVICE_TYPE_UNKNOWN;

	return service->type;
}

/**
 * connman_service_get_interface:
 * @service: service structure
 *
 * Get network interface of service
 */
char *connman_service_get_interface(struct connman_service *service)
{
	int index;

	if (!service)
		return NULL;

	index = __connman_service_get_index(service);

	return connman_inet_ifname(index);
}

/**
 * connman_service_get_network:
 * @service: service structure
 *
 * Get the service network
 */
struct connman_network *
__connman_service_get_network(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->network;
}

struct connman_ipconfig *
__connman_service_get_ip4config(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->ipconfig_ipv4;
}

struct connman_ipconfig *
__connman_service_get_ip6config(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->ipconfig_ipv6;
}

struct connman_ipconfig *
__connman_service_get_ipconfig(struct connman_service *service, int family)
{
	if (family == AF_INET)
		return __connman_service_get_ip4config(service);
	else if (family == AF_INET6)
		return __connman_service_get_ip6config(service);
	else
		return NULL;

}

bool __connman_service_is_connected_state(struct connman_service *service,
					enum connman_ipconfig_type type)
{
	if (!service)
		return false;

	switch (type) {
	case CONNMAN_IPCONFIG_TYPE_UNKNOWN:
		break;
	case CONNMAN_IPCONFIG_TYPE_IPV4:
		return is_connected_state(service, service->state_ipv4);
	case CONNMAN_IPCONFIG_TYPE_IPV6:
		return is_connected_state(service, service->state_ipv6);
	case CONNMAN_IPCONFIG_TYPE_ALL:
		return is_connected_state(service,
					CONNMAN_IPCONFIG_TYPE_IPV4) &&
			is_connected_state(service,
					CONNMAN_IPCONFIG_TYPE_IPV6);
	}

	return false;
}
enum connman_service_security __connman_service_get_security(
				struct connman_service *service)
{
	if (!service)
		return CONNMAN_SERVICE_SECURITY_UNKNOWN;

	return service->security;
}

const char *__connman_service_get_phase2(struct connman_service *service)
{
	if (!service)
		return NULL;

	return service->phase2;
}

bool __connman_service_wps_enabled(struct connman_service *service)
{
	if (!service)
		return false;

	return service->wps;
}

void __connman_service_mark_dirty(void)
{
	services_dirty = true;
}

/**
 * __connman_service_set_favorite_delayed:
 * @service: service structure
 * @favorite: favorite value
 * @delay_ordering: do not order service sequence
 *
 * Change the favorite setting of service
 */
int __connman_service_set_favorite_delayed(struct connman_service *service,
					bool favorite,
					bool delay_ordering)
{
	if (service->hidden)
		return -EOPNOTSUPP;

	if (service->favorite == favorite)
		return -EALREADY;

	service->favorite = favorite;

	if (!delay_ordering)
		__connman_service_get_order(service);

	favorite_changed(service);

	if (!delay_ordering) {

		service_list_sort();

		__connman_connection_update_gateway();
	}

	return 0;
}

/**
 * __connman_service_set_favorite:
 * @service: service structure
 * @favorite: favorite value
 *
 * Change the favorite setting of service
 */
int __connman_service_set_favorite(struct connman_service *service,
						bool favorite)
{
	return __connman_service_set_favorite_delayed(service, favorite,
							false);
}

bool connman_service_get_favorite(struct connman_service *service)
{
	return service->favorite;
}

bool connman_service_get_autoconnect(struct connman_service *service)
{
	return service->autoconnect;
}

int __connman_service_set_immutable(struct connman_service *service,
						bool immutable)
{
	if (service->hidden)
		return -EOPNOTSUPP;

	if (service->immutable == immutable)
		return 0;

	service->immutable = immutable;

	immutable_changed(service);

	return 0;
}

int __connman_service_set_ignore(struct connman_service *service,
						bool ignore)
{
	if (!service)
		return -EINVAL;

	service->ignore = ignore;

	return 0;
}

void __connman_service_set_string(struct connman_service *service,
				  const char *key, const char *value)
{
	if (service->hidden)
		return;
	if (g_str_equal(key, "EAP")) {
		g_free(service->eap);
		service->eap = g_strdup(value);
	} else if (g_str_equal(key, "Identity")) {
		g_free(service->identity);
		service->identity = g_strdup(value);
	} else if (g_str_equal(key, "CACertFile")) {
		g_free(service->ca_cert_file);
		service->ca_cert_file = g_strdup(value);
	} else if (g_str_equal(key, "ClientCertFile")) {
		g_free(service->client_cert_file);
		service->client_cert_file = g_strdup(value);
	} else if (g_str_equal(key, "PrivateKeyFile")) {
		g_free(service->private_key_file);
		service->private_key_file = g_strdup(value);
	} else if (g_str_equal(key, "PrivateKeyPassphrase")) {
		g_free(service->private_key_passphrase);
		service->private_key_passphrase = g_strdup(value);
	} else if (g_str_equal(key, "Phase2")) {
		g_free(service->phase2);
		service->phase2 = g_strdup(value);
	} else if (g_str_equal(key, "Passphrase"))
		__connman_service_set_passphrase(service, value);
}

void __connman_service_set_search_domains(struct connman_service *service,
					char **domains)
{
	int index;

	index = __connman_service_get_index(service);
	if (index < 0)
		return;

	if (service->domains) {
		remove_searchdomains(service, index, service->domains);
		g_strfreev(service->domains);

		service->domains = g_strdupv(domains);

		update_nameservers(service);
	}
}

/*
 * This variant is used when domain search list is updated via
 * dhcp and in that case the service is not yet fully connected so
 * we cannot do same things as what the set() variant is doing.
 */
void __connman_service_update_search_domains(struct connman_service *service,
					char **domains)
{
	g_strfreev(service->domains);
	service->domains = g_strdupv(domains);
}

static void service_complete(struct connman_service *service)
{
	reply_pending(service, EIO);

	if (service->connect_reason != CONNMAN_SERVICE_CONNECT_REASON_USER)
		__connman_service_auto_connect(service->connect_reason);

	g_get_current_time(&service->modified);
	service_save(service);
}

static void report_error_cb(void *user_context, bool retry,
							void *user_data)
{
	struct connman_service *service = user_context;

	if (retry)
		__connman_service_connect(service,
					CONNMAN_SERVICE_CONNECT_REASON_USER);
	else {
		/* It is not relevant to stay on Failure state
		 * when failing is due to wrong user input */
		__connman_service_clear_error(service);

		service_complete(service);
		__connman_connection_update_gateway();
	}
}

static int check_wpspin(struct connman_service *service, const char *wpspin)
{
	int length;
	guint i;

	if (!wpspin)
		return 0;

	length = strlen(wpspin);

	/* If 0, it will mean user wants to use PBC method */
	if (length == 0) {
		connman_network_set_string(service->network,
							"WiFi.PinWPS", NULL);
		return 0;
	}

	/* A WPS PIN is always 8 chars length,
	 * its content is in digit representation.
	 */
	if (length != 8)
		return -ENOKEY;

	for (i = 0; i < 8; i++)
		if (!isdigit((unsigned char) wpspin[i]))
			return -ENOKEY;

	connman_network_set_string(service->network, "WiFi.PinWPS", wpspin);

	return 0;
}

static void request_input_cb(struct connman_service *service,
			bool values_received,
			const char *name, int name_len,
			const char *identity, const char *passphrase,
			bool wps, const char *wpspin,
			const char *error, void *user_data)
{
	struct connman_device *device;
	const char *security;
	int err = 0;

	DBG("RequestInput return, %p", service);

	if (error) {
		DBG("error: %s", error);

		if (g_strcmp0(error,
				"net.connman.Agent.Error.Canceled") == 0) {
			err = -EINVAL;

			if (service->hidden)
				__connman_service_return_error(service,
							ECANCELED, user_data);
			goto done;
		} else {
			if (service->hidden)
				__connman_service_return_error(service,
							ETIMEDOUT, user_data);
		}
	}

	if (service->hidden && name_len > 0 && name_len <= 32) {
		device = connman_network_get_device(service->network);
		security = connman_network_get_string(service->network,
							"WiFi.Security");
		err = __connman_device_request_hidden_scan(device,
						name, name_len,
						identity, passphrase,
						security, user_data);
		if (err < 0)
			__connman_service_return_error(service,	-err,
							user_data);
	}

	if (!values_received || service->hidden) {
		err = -EINVAL;
		goto done;
	}

	if (wps && service->network) {
		err = check_wpspin(service, wpspin);
		if (err < 0)
			goto done;

		connman_network_set_bool(service->network, "WiFi.UseWPS", wps);
	}

	if (identity)
		__connman_service_set_agent_identity(service, identity);

	if (passphrase)
		err = __connman_service_set_passphrase(service, passphrase);

 done:
	if (err >= 0) {
		/* We forget any previous error. */
		set_error(service, CONNMAN_SERVICE_ERROR_UNKNOWN);

		__connman_service_connect(service,
					CONNMAN_SERVICE_CONNECT_REASON_USER);

	} else if (err == -ENOKEY) {
		__connman_service_indicate_error(service,
					CONNMAN_SERVICE_ERROR_INVALID_KEY);
	} else {
		/* It is not relevant to stay on Failure state
		 * when failing is due to wrong user input */
		service->state = CONNMAN_SERVICE_STATE_IDLE;

		if (!service->hidden) {
			/*
			 * If there was a real error when requesting
			 * hidden scan, then that error is returned already
			 * to the user somewhere above so do not try to
			 * do this again.
			 */
			__connman_service_return_error(service,	-err,
							user_data);
		}

		service_complete(service);
		__connman_connection_update_gateway();
	}
}

static void downgrade_connected_services(void)
{
	struct connman_service *up_service;
	GList *list;

	for (list = service_list; list; list = list->next) {
		up_service = list->data;

		if (!is_connected(up_service))
			continue;

		if (up_service->state == CONNMAN_SERVICE_STATE_ONLINE)
			return;

		downgrade_state(up_service);
	}
}

static int service_update_preferred_order(struct connman_service *default_service,
		struct connman_service *new_service,
		enum connman_service_state new_state)
{
	unsigned int *tech_array;
	int i;

	if (!default_service || default_service == new_service ||
			default_service->state != new_state)
		return 0;

	tech_array = connman_setting_get_uint_list("PreferredTechnologies");
	if (tech_array) {

		for (i = 0; tech_array[i] != 0; i += 1) {
			if (default_service->type == tech_array[i])
				return -EALREADY;

			if (new_service->type == tech_array[i]) {
				switch_default_service(default_service,
						new_service);
				__connman_connection_update_gateway();
				return 0;
			}
		}
	}

	return -EALREADY;
}

static void single_connected_tech(struct connman_service *allowed)
{
	struct connman_service *service;
	GSList *services = NULL, *list;
	GList *iter;

	DBG("keeping %p %s", allowed, allowed->path);

	for (iter = service_list; iter; iter = iter->next) {
		service = iter->data;

		if (!is_connected(service))
			break;

		if (service == allowed)
			continue;

		services = g_slist_prepend(services, service);
	}

	for (list = services; list; list = list->next) {
		service = list->data;

		DBG("disconnecting %p %s", service, service->path);
		__connman_service_disconnect(service);
	}

	g_slist_free(services);
}

static const char *get_dbus_sender(struct connman_service *service)
{
	if (!service->pending)
		return NULL;

	return dbus_message_get_sender(service->pending);
}

static void connman_service_save_last_explicit_service()
{
	GKeyFile *keyfile;

	keyfile = __connman_storage_load_global();
	if (!keyfile)
		keyfile = g_key_file_new();

	g_key_file_set_string(keyfile, "global",
				"LastExplicitService", last_explicitly_connected_service);

	__connman_storage_save_global(keyfile);

	g_key_file_free(keyfile);

	return;
}

static void connman_service_load_last_explicit_service()
{
	GKeyFile *keyfile;
	GError *error = NULL;

	keyfile = __connman_storage_load_global();
	if (!keyfile)
		return;

	last_explicitly_connected_service = g_key_file_get_string(keyfile, "global",
								   "LastExplicitService", &error);
	if (error) {
		g_clear_error(&error);
	}

	g_key_file_free(keyfile);

	return;
}

static void reset_autoconnect_states(gpointer value, gpointer user_data)
{
	struct connman_service *service = value;
	char *identifier = user_data;

	if (service->type == CONNMAN_SERVICE_TYPE_ETHERNET)
		return;

	if (g_strcmp0(service->identifier, identifier)) {
		if (service->autoconnect) {
			service->autoconnect = false;
			autoconnect_changed(service);
			service_save(service);
		}
	}
	else {
		if (!service->autoconnect) {
			service->autoconnect = true;
			autoconnect_changed(service);
			service_save(service);
		}
	}
}

/* Set AutoConnect to true for given service, false for all others except ethernet, always keep it on */
static void service_reset_autoconnect_states(struct connman_service *service)
{
	DBG("identifier %s", service->identifier);

	g_list_foreach(service_list, reset_autoconnect_states, service->identifier);
}

static int service_indicate_state(struct connman_service *service)
{
	enum connman_service_state old_state, new_state;
	struct connman_service *def_service;
	enum connman_ipconfig_method method;
	int result;

	if (!service)
		return -EINVAL;

	old_state = service->state;
	new_state = combine_state(service->state_ipv4, service->state_ipv6);

	DBG("service %p old %s - new %s/%s => %s",
					service,
					state2string(old_state),
					state2string(service->state_ipv4),
					state2string(service->state_ipv6),
					state2string(new_state));

	if (old_state == new_state)
		return -EALREADY;

	def_service = __connman_service_get_default();

	if (new_state == CONNMAN_SERVICE_STATE_ONLINE) {
		result = service_update_preferred_order(def_service,
				service, new_state);
		if (result == -EALREADY)
			return result;
	}

	/* Save the last explicitly connected to service in the global
	 * settings file as well as locally in a variable.
	 * It is used to sort the service list to prefer this service
	 */
	if (service->connect_reason == CONNMAN_SERVICE_CONNECT_REASON_USER) {
		if (new_state == CONNMAN_SERVICE_STATE_ONLINE ||
		    new_state == CONNMAN_SERVICE_STATE_READY) {
			if (g_strcmp0(service->path, last_explicitly_connected_service)) {
				g_free(last_explicitly_connected_service);
				last_explicitly_connected_service = NULL;
				last_explicitly_connected_service = g_strdup(service->path);
				connman_service_save_last_explicit_service();

				service_reset_autoconnect_states(service);
			}
		}
	}
	else if(service->type == CONNMAN_SERVICE_TYPE_ETHERNET) {
	/*
	 * Steam link:
	 * Always keep Ethernet AutoConnect as true and if we (automatically or explicitly)
	 * complete a connection to ethernet service, disable autoconnect on WiFi services to
	 * stop "silent switch-offs" if ethernet is non-functional or disconnected.
	 * Instead an explicit action from user is required
	 */
		if (new_state == CONNMAN_SERVICE_STATE_ONLINE ||
		    new_state == CONNMAN_SERVICE_STATE_READY) {
			service_reset_autoconnect_states(service);
		}
	}


	if (old_state == CONNMAN_SERVICE_STATE_ONLINE)
		__connman_notifier_leave_online(service->type);

	service->state = new_state;
	state_changed(service);

	switch(new_state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:

		break;

	case CONNMAN_SERVICE_STATE_IDLE:
		if (old_state != CONNMAN_SERVICE_STATE_DISCONNECT)
			__connman_service_disconnect(service);

		break;

	case CONNMAN_SERVICE_STATE_ASSOCIATION:

		break;

	case CONNMAN_SERVICE_STATE_CONFIGURATION:
		if (!service->new_service &&
				__connman_stats_service_register(service) == 0) {
			/*
			 * For new services the statistics are updated after
			 * we have successfully connected.
			 */
			__connman_stats_get(service, false,
						&service->stats.data);
			__connman_stats_get(service, true,
						&service->stats_roaming.data);
		}

		break;

	case CONNMAN_SERVICE_STATE_READY:
		set_error(service, CONNMAN_SERVICE_ERROR_UNKNOWN);

		if (service->new_service &&
				__connman_stats_service_register(service) == 0) {
			/*
			 * This is normally done after configuring state
			 * but for new service do this after we have connected
			 * successfully.
			 */
			__connman_stats_get(service, false,
						&service->stats.data);
			__connman_stats_get(service, true,
						&service->stats_roaming.data);
		}

		service->new_service = false;

		default_changed();

		def_service = __connman_service_get_default();

		service_update_preferred_order(def_service, service, new_state);

		__connman_service_set_favorite(service, true);

		reply_pending(service, 0);

		if (service->type == CONNMAN_SERVICE_TYPE_WIFI &&
			connman_network_get_bool(service->network,
						"WiFi.UseWPS")) {
			const char *pass;

			pass = connman_network_get_string(service->network,
							"WiFi.Passphrase");

			__connman_service_set_passphrase(service, pass);

			connman_network_set_bool(service->network,
							"WiFi.UseWPS", false);
		}

		g_get_current_time(&service->modified);
		service_save(service);

		dns_changed(service);
		domain_changed(service);
		proxy_changed(service);

		if (old_state != CONNMAN_SERVICE_STATE_ONLINE)
			__connman_notifier_connect(service->type);

		method = __connman_ipconfig_get_method(service->ipconfig_ipv6);
		if (method == CONNMAN_IPCONFIG_METHOD_OFF)
			__connman_ipconfig_disable_ipv6(
						service->ipconfig_ipv6);

		if (connman_setting_get_bool("SingleConnectedTechnology"))
			single_connected_tech(service);
		else if (service->type != CONNMAN_SERVICE_TYPE_VPN)
			vpn_auto_connect();

		break;

	case CONNMAN_SERVICE_STATE_ONLINE:

		break;

	case CONNMAN_SERVICE_STATE_DISCONNECT:
		set_error(service, CONNMAN_SERVICE_ERROR_UNKNOWN);

		reply_pending(service, ECONNABORTED);

		def_service = __connman_service_get_default();

		if (!__connman_notifier_is_connected() &&
			def_service &&
				def_service->provider)
			connman_provider_disconnect(def_service->provider);

		default_changed();

		__connman_wispr_stop(service);

		__connman_wpad_stop(service);

		dns_changed(service);
		domain_changed(service);
		proxy_changed(service);

		/*
		 * Previous services which are connected and which states
		 * are set to online should reset relevantly ipconfig_state
		 * to ready so wispr/portal will be rerun on those
		 */
		downgrade_connected_services();

		__connman_service_auto_connect(CONNMAN_SERVICE_CONNECT_REASON_AUTO);
		break;

	case CONNMAN_SERVICE_STATE_FAILURE:

		/* Clear WPS usage so on retry new method will be asked from the user */
		if (service->type == CONNMAN_SERVICE_TYPE_WIFI)
			connman_network_set_bool(service->network,
							"WiFi.UseWPS", false);

		if (service->connect_reason == CONNMAN_SERVICE_CONNECT_REASON_USER &&
			connman_agent_report_error(service, service->path,
					error2string(service->error),
					report_error_cb,
					get_dbus_sender(service),
					NULL) == -EINPROGRESS)
			return 0;
		service_complete(service);

		break;
	}

	service_list_sort();

	__connman_connection_update_gateway();

	if ((old_state == CONNMAN_SERVICE_STATE_ONLINE &&
			new_state != CONNMAN_SERVICE_STATE_READY) ||
		(old_state == CONNMAN_SERVICE_STATE_READY &&
			new_state != CONNMAN_SERVICE_STATE_ONLINE)) {
		__connman_notifier_disconnect(service->type);
	}

	if (new_state == CONNMAN_SERVICE_STATE_ONLINE) {
		__connman_notifier_enter_online(service->type);
		default_changed();
	}

	return 0;
}

int __connman_service_indicate_error(struct connman_service *service,
					enum connman_service_error error)
{
	DBG("service %p error %d", service, error);

	if (!service)
		return -EINVAL;

	if (service->state == CONNMAN_SERVICE_STATE_FAILURE)
		return -EALREADY;

	set_error(service, error);

	__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_FAILURE,
						CONNMAN_IPCONFIG_TYPE_IPV4);
	__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_FAILURE,
						CONNMAN_IPCONFIG_TYPE_IPV6);
	return 0;
}

int __connman_service_clear_error(struct connman_service *service)
{
	DBusMessage *pending, *provider_pending;

	DBG("service %p", service);

	if (!service)
		return -EINVAL;

	if (service->state != CONNMAN_SERVICE_STATE_FAILURE)
		return -EINVAL;

	pending = service->pending;
	service->pending = NULL;
	provider_pending = service->provider_pending;
	service->provider_pending = NULL;

	__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_IDLE,
						CONNMAN_IPCONFIG_TYPE_IPV6);

	__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_IDLE,
						CONNMAN_IPCONFIG_TYPE_IPV4);

	service->pending = pending;
	service->provider_pending = provider_pending;

	return 0;
}

int __connman_service_indicate_default(struct connman_service *service)
{
	DBG("service %p state %s", service, state2string(service->state));

	if (!is_connected(service)) {
		/*
		 * If service is not yet fully connected, then we must not
		 * change the default yet. The default gw will be changed
		 * after the service state is in ready.
		 */
		return -EINPROGRESS;
	}

	default_changed();

	return 0;
}

enum connman_service_state __connman_service_ipconfig_get_state(
					struct connman_service *service,
					enum connman_ipconfig_type type)
{
	if (!service)
		return CONNMAN_SERVICE_STATE_UNKNOWN;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		return service->state_ipv4;

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6)
		return service->state_ipv6;

	return CONNMAN_SERVICE_STATE_UNKNOWN;
}

static void check_proxy_setup(struct connman_service *service)
{
	/*
	 * We start WPAD if we haven't got a PAC URL from DHCP and
	 * if our proxy manual configuration is either empty or set
	 * to AUTO with an empty URL.
	 */

	if (service->proxy != CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN)
		goto done;

	if (service->proxy_config != CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN &&
		(service->proxy_config != CONNMAN_SERVICE_PROXY_METHOD_AUTO ||
			service->pac))
		goto done;

	if (__connman_wpad_start(service) < 0) {
		service->proxy = CONNMAN_SERVICE_PROXY_METHOD_DIRECT;
		__connman_notifier_proxy_changed(service);
		goto done;
	}

	return;

done:
	__connman_wispr_start(service, CONNMAN_IPCONFIG_TYPE_IPV4);
}

/*
 * How many networks are connected at the same time. If more than 1,
 * then set the rp_filter setting properly (loose mode routing) so that network
 * connectivity works ok. This is only done for IPv4 networks as IPv6
 * does not have rp_filter knob.
 */
static int connected_networks_count;
static int original_rp_filter;

static void service_rp_filter(struct connman_service *service,
				bool connected)
{
	enum connman_ipconfig_method method;

	method = __connman_ipconfig_get_method(service->ipconfig_ipv4);

	switch (method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		return;
	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
		break;
	}

	if (connected) {
		if (connected_networks_count == 1) {
			int filter_value;
			filter_value = __connman_ipconfig_set_rp_filter();
			if (filter_value < 0)
				return;

			original_rp_filter = filter_value;
		}
		connected_networks_count++;

	} else {
		if (connected_networks_count == 2)
			__connman_ipconfig_unset_rp_filter(original_rp_filter);

		connected_networks_count--;
		if (connected_networks_count < 0)
			connected_networks_count = 0;
	}

	DBG("%s %s ipconfig %p method %d count %d filter %d",
		connected ? "connected" : "disconnected", service->identifier,
		service->ipconfig_ipv4, method,
		connected_networks_count, original_rp_filter);
}

static gboolean redo_wispr(gpointer user_data)
{
	struct connman_service *service = user_data;
	int refcount = service->refcount - 1;

	connman_service_unref(service);
	if (refcount == 0) {
		DBG("Service %p already removed", service);
		return FALSE;
	}

	__connman_wispr_start(service, CONNMAN_IPCONFIG_TYPE_IPV6);

	return FALSE;
}

int __connman_service_online_check_failed(struct connman_service *service,
					enum connman_ipconfig_type type)
{
	DBG("service %p type %d count %d", service, type,
						service->online_check_count);

	/* currently we only retry IPv6 stuff */
	if (type == CONNMAN_IPCONFIG_TYPE_IPV4 ||
			service->online_check_count != 1) {
		connman_warn("Online check failed for %p %s", service,
			service->name);
		return 0;
	}

	service->online_check_count = 0;

	/*
	 * We set the timeout to 1 sec so that we have a chance to get
	 * necessary IPv6 router advertisement messages that might have
	 * DNS data etc.
	 */
	g_timeout_add_seconds(1, redo_wispr, connman_service_ref(service));

	return EAGAIN;
}

int __connman_service_ipconfig_indicate_state(struct connman_service *service,
					enum connman_service_state new_state,
					enum connman_ipconfig_type type)
{
	struct connman_ipconfig *ipconfig = NULL;
	enum connman_service_state old_state;
	enum connman_ipconfig_method method;

	if (!service)
		return -EINVAL;

	switch (type) {
	case CONNMAN_IPCONFIG_TYPE_UNKNOWN:
	case CONNMAN_IPCONFIG_TYPE_ALL:
		return -EINVAL;

	case CONNMAN_IPCONFIG_TYPE_IPV4:
		old_state = service->state_ipv4;
		ipconfig = service->ipconfig_ipv4;

		break;

	case CONNMAN_IPCONFIG_TYPE_IPV6:
		old_state = service->state_ipv6;
		ipconfig = service->ipconfig_ipv6;

		break;
	}

	if (!ipconfig)
		return -EINVAL;

	/* Any change? */
	if (old_state == new_state)
		return -EALREADY;

	DBG("service %p (%s) old state %d (%s) new state %d (%s) type %d (%s)",
		service, service ? service->identifier : NULL,
		old_state, state2string(old_state),
		new_state, state2string(new_state),
		type, __connman_ipconfig_type2string(type));

	switch (new_state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_IDLE:
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
		break;
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
		__connman_ipconfig_enable(ipconfig);
		break;
	case CONNMAN_SERVICE_STATE_READY:
		if (type == CONNMAN_IPCONFIG_TYPE_IPV4) {
			check_proxy_setup(service);
			service_rp_filter(service, true);
		} else {
			service->online_check_count = 1;
			__connman_wispr_start(service, type);
		}
		break;
	case CONNMAN_SERVICE_STATE_ONLINE:
		break;
	case CONNMAN_SERVICE_STATE_DISCONNECT:
		if (service->state == CONNMAN_SERVICE_STATE_IDLE)
			return -EINVAL;

		if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
			service_rp_filter(service, false);

		break;
	case CONNMAN_SERVICE_STATE_FAILURE:
		break;
	}

	/* Keep that state, but if the ipconfig method is OFF, then we set
	   the state to IDLE so that it will not affect the combined state
	   in the future.
	 */
	method = __connman_ipconfig_get_method(ipconfig);
	switch (method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		new_state = CONNMAN_SERVICE_STATE_IDLE;
		break;

	case CONNMAN_IPCONFIG_METHOD_FIXED:
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		break;

	}

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4)
		service->state_ipv4 = new_state;
	else
		service->state_ipv6 = new_state;

	update_nameservers(service);

	return service_indicate_state(service);
}

static bool prepare_network(struct connman_service *service)
{
	enum connman_network_type type;
	unsigned int ssid_len;

	type = connman_network_get_type(service->network);

	switch (type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
		return false;
	case CONNMAN_NETWORK_TYPE_WIFI:
		if (!connman_network_get_blob(service->network, "WiFi.SSID",
						&ssid_len))
			return false;

		if (service->passphrase)
			connman_network_set_string(service->network,
				"WiFi.Passphrase", service->passphrase);
		break;
	case CONNMAN_NETWORK_TYPE_ETHERNET:
	case CONNMAN_NETWORK_TYPE_GADGET:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
	case CONNMAN_NETWORK_TYPE_CELLULAR:
		break;
	}

	return true;
}

static void prepare_8021x(struct connman_service *service)
{
	if (service->eap)
		connman_network_set_string(service->network, "WiFi.EAP",
								service->eap);

	if (service->identity)
		connman_network_set_string(service->network, "WiFi.Identity",
							service->identity);

	if (service->ca_cert_file)
		connman_network_set_string(service->network, "WiFi.CACertFile",
							service->ca_cert_file);

	if (service->client_cert_file)
		connman_network_set_string(service->network,
						"WiFi.ClientCertFile",
						service->client_cert_file);

	if (service->private_key_file)
		connman_network_set_string(service->network,
						"WiFi.PrivateKeyFile",
						service->private_key_file);

	if (service->private_key_passphrase)
		connman_network_set_string(service->network,
					"WiFi.PrivateKeyPassphrase",
					service->private_key_passphrase);

	if (service->phase2)
		connman_network_set_string(service->network, "WiFi.Phase2",
							service->phase2);
}

static int service_connect(struct connman_service *service)
{
	int err;

	if (service->hidden)
		return -EPERM;

	switch (service->type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_P2P:
		return -EINVAL;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_VPN:
		break;
	case CONNMAN_SERVICE_TYPE_WIFI:
		switch (service->security) {
		case CONNMAN_SERVICE_SECURITY_UNKNOWN:
		case CONNMAN_SERVICE_SECURITY_NONE:
			break;
		case CONNMAN_SERVICE_SECURITY_WEP:
		case CONNMAN_SERVICE_SECURITY_PSK:
		case CONNMAN_SERVICE_SECURITY_WPA:
		case CONNMAN_SERVICE_SECURITY_RSN:
			if (service->error == CONNMAN_SERVICE_ERROR_INVALID_KEY)
				return -ENOKEY;

			if (!service->passphrase) {
				if (!service->network)
					return -EOPNOTSUPP;

				if (!service->wps ||
					!connman_network_get_bool(service->network, "WiFi.UseWPS"))
					return -ENOKEY;
			}
			break;

		case CONNMAN_SERVICE_SECURITY_8021X:
			if (!service->eap)
				return -EINVAL;

			/*
			 * never request credentials if using EAP-TLS
			 * (EAP-TLS networks need to be fully provisioned)
			 */
			if (g_str_equal(service->eap, "tls"))
				break;

			/*
			 * Return -ENOKEY if either identity or passphrase is
			 * missing. Agent provided credentials can be used as
			 * fallback if needed.
			 */
			if (((!service->identity &&
					!service->agent_identity) ||
					!service->passphrase) ||
					service->error == CONNMAN_SERVICE_ERROR_INVALID_KEY)
				return -ENOKEY;

			break;
		}
		break;
	}

	if (service->network) {
		if (!prepare_network(service))
			return -EINVAL;

		switch (service->security) {
		case CONNMAN_SERVICE_SECURITY_UNKNOWN:
		case CONNMAN_SERVICE_SECURITY_NONE:
		case CONNMAN_SERVICE_SECURITY_WEP:
		case CONNMAN_SERVICE_SECURITY_PSK:
		case CONNMAN_SERVICE_SECURITY_WPA:
		case CONNMAN_SERVICE_SECURITY_RSN:
			break;
		case CONNMAN_SERVICE_SECURITY_8021X:
			prepare_8021x(service);
			break;
		}

		if (__connman_stats_service_register(service) == 0) {
			__connman_stats_get(service, false,
						&service->stats.data);
			__connman_stats_get(service, true,
						&service->stats_roaming.data);
		}

		if (service->ipconfig_ipv4)
			__connman_ipconfig_enable(service->ipconfig_ipv4);
		if (service->ipconfig_ipv6)
			__connman_ipconfig_enable(service->ipconfig_ipv6);

		err = __connman_network_connect(service->network);
	} else if (service->type == CONNMAN_SERVICE_TYPE_VPN &&
					service->provider)
		err = __connman_provider_connect(service->provider);
	else
		return -EOPNOTSUPP;

	if (err < 0) {
		if (err != -EINPROGRESS) {
			__connman_ipconfig_disable(service->ipconfig_ipv4);
			__connman_ipconfig_disable(service->ipconfig_ipv6);
			__connman_stats_service_unregister(service);
		}
	}

	return err;
}

int __connman_service_connect(struct connman_service *service,
			enum connman_service_connect_reason reason)
{
	int err;

	DBG("service %p state %s connect reason %s -> %s",
		service, state2string(service->state),
		reason2string(service->connect_reason),
		reason2string(reason));

	if (is_connected(service))
		return -EISCONN;

	if (is_connecting(service))
		return -EALREADY;

	switch (service->type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_P2P:
		return -EINVAL;

	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_VPN:
	case CONNMAN_SERVICE_TYPE_WIFI:
		break;
	}

	if (!is_ipconfig_usable(service))
		return -ENOLINK;

	__connman_service_clear_error(service);

	err = service_connect(service);

	service->connect_reason = reason;

	if (err >= 0)
		return 0;

	if (err == -EINPROGRESS) {
		if (service->timeout == 0)
			service->timeout = g_timeout_add_seconds(
				CONNECT_TIMEOUT, connect_timeout, service);

		return -EINPROGRESS;
	}

	if (service->network)
		__connman_network_disconnect(service->network);
	else if (service->type == CONNMAN_SERVICE_TYPE_VPN &&
				service->provider)
			connman_provider_disconnect(service->provider);

	if (service->connect_reason == CONNMAN_SERVICE_CONNECT_REASON_USER) {
		if (err == -ENOKEY || err == -EPERM) {
			DBusMessage *pending = NULL;

			/*
			 * We steal the reply here. The idea is that the
			 * connecting client will see the connection status
			 * after the real hidden network is connected or
			 * connection failed.
			 */
			if (service->hidden) {
				pending = service->pending;
				service->pending = NULL;
			}

			err = __connman_agent_request_passphrase_input(service,
					request_input_cb,
					get_dbus_sender(service),
					pending);
			if (service->hidden && err != -EINPROGRESS)
				service->pending = pending;

			return err;
		}
		reply_pending(service, -err);
	}

	return err;
}

int __connman_service_disconnect(struct connman_service *service)
{
	int err;

	DBG("service %p", service);

	service->connect_reason = CONNMAN_SERVICE_CONNECT_REASON_NONE;
	service->proxy = CONNMAN_SERVICE_PROXY_METHOD_UNKNOWN;

	connman_agent_cancel(service);

	reply_pending(service, ECONNABORTED);

	if (service->network) {
		err = __connman_network_disconnect(service->network);
	} else if (service->type == CONNMAN_SERVICE_TYPE_VPN &&
					service->provider)
		err = connman_provider_disconnect(service->provider);
	else
		return -EOPNOTSUPP;

	if (err < 0 && err != -EINPROGRESS)
		return err;

	__connman_6to4_remove(service->ipconfig_ipv4);

	if (service->ipconfig_ipv4)
		__connman_ipconfig_set_proxy_autoconfig(service->ipconfig_ipv4,
							NULL);
	else
		__connman_ipconfig_set_proxy_autoconfig(service->ipconfig_ipv6,
							NULL);

	__connman_ipconfig_address_remove(service->ipconfig_ipv4);
	settings_changed(service, service->ipconfig_ipv4);

	__connman_ipconfig_address_remove(service->ipconfig_ipv6);
	settings_changed(service, service->ipconfig_ipv6);

	__connman_ipconfig_disable(service->ipconfig_ipv4);
	__connman_ipconfig_disable(service->ipconfig_ipv6);

	__connman_stats_service_unregister(service);

	return err;
}

int __connman_service_disconnect_all(void)
{
	struct connman_service *service;
	GSList *services = NULL, *list;
	GList *iter;

	DBG("");

	for (iter = service_list; iter; iter = iter->next) {
		service = iter->data;

		if (!is_connected(service))
			break;

		services = g_slist_prepend(services, service);
	}

	for (list = services; list; list = list->next) {
		struct connman_service *service = list->data;

		service->ignore = true;

		__connman_service_disconnect(service);
	}

	g_slist_free(services);

	return 0;
}

/**
 * lookup_by_identifier:
 * @identifier: service identifier
 *
 * Look up a service by identifier (reference count will not be increased)
 */
static struct connman_service *lookup_by_identifier(const char *identifier)
{
	return g_hash_table_lookup(service_hash, identifier);
}

struct provision_user_data {
	const char *ident;
	int ret;
};

static void provision_changed(gpointer value, gpointer user_data)
{
	struct connman_service *service = value;
	struct provision_user_data *data = user_data;
	const char *path = data->ident;
	int ret;

	ret = __connman_config_provision_service_ident(service, path,
			service->config_file, service->config_entry);
	if (ret > 0)
		data->ret = ret;
}

int __connman_service_provision_changed(const char *ident)
{
	struct provision_user_data data = {
		.ident = ident,
		.ret = 0
	};

	g_list_foreach(service_list, provision_changed, (void *)&data);

	/*
	 * Because the provision_changed() might have set some services
	 * as favorite, we must sort the sequence now.
	 */
	if (services_dirty) {
		services_dirty = false;

		service_list_sort();

		__connman_connection_update_gateway();
	}

	return data.ret;
}

void __connman_service_set_config(struct connman_service *service,
				const char *file_id, const char *entry)
{
	if (!service)
		return;

	g_free(service->config_file);
	service->config_file = g_strdup(file_id);

	g_free(service->config_entry);
	service->config_entry = g_strdup(entry);
}

/**
 * __connman_service_get:
 * @identifier: service identifier
 *
 * Look up a service by identifier or create a new one if not found
 */
static struct connman_service *service_get(const char *identifier)
{
	struct connman_service *service;

	service = g_hash_table_lookup(service_hash, identifier);
	if (service) {
		connman_service_ref(service);
		return service;
	}

	service = connman_service_create();
	if (!service)
		return NULL;

	DBG("service %p", service);

	service->identifier = g_strdup(identifier);

	service_list = g_list_insert_sorted(service_list, service,
						service_compare);

	g_hash_table_insert(service_hash, service->identifier, service);

	return service;
}

static int service_register(struct connman_service *service)
{
	DBG("service %p", service);

	if (service->path)
		return -EALREADY;

	service->path = g_strdup_printf("%s/service/%s", CONNMAN_PATH,
						service->identifier);

	DBG("path %s", service->path);

	if (__connman_config_provision_service(service) < 0)
		service_load(service);

	g_dbus_register_interface(connection, service->path,
					CONNMAN_SERVICE_INTERFACE,
					service_methods, service_signals,
							NULL, service, NULL);

	service_list_sort();

	__connman_connection_update_gateway();

	return 0;
}

static void service_up(struct connman_ipconfig *ipconfig,
		const char *ifname)
{
	struct connman_service *service = __connman_ipconfig_get_data(ipconfig);

	DBG("%s up", ifname);

	link_changed(service);

	service->stats.valid = false;
	service->stats_roaming.valid = false;
}

static void service_down(struct connman_ipconfig *ipconfig,
			const char *ifname)
{
	DBG("%s down", ifname);
}

static void service_lower_up(struct connman_ipconfig *ipconfig,
			const char *ifname)
{
	struct connman_service *service = __connman_ipconfig_get_data(ipconfig);

	DBG("%s lower up", ifname);

	stats_start(service);
}

static void service_lower_down(struct connman_ipconfig *ipconfig,
			const char *ifname)
{
	struct connman_service *service = __connman_ipconfig_get_data(ipconfig);

	DBG("%s lower down", ifname);

	if (!is_idle_state(service, service->state_ipv4))
		__connman_ipconfig_disable(service->ipconfig_ipv4);

	if (!is_idle_state(service, service->state_ipv6))
		__connman_ipconfig_disable(service->ipconfig_ipv6);

	stats_stop(service);
	service_save(service);
}

static void service_ip_bound(struct connman_ipconfig *ipconfig,
			const char *ifname)
{
	struct connman_service *service = __connman_ipconfig_get_data(ipconfig);
	enum connman_ipconfig_method method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;
	enum connman_ipconfig_type type = CONNMAN_IPCONFIG_TYPE_UNKNOWN;

	DBG("%s ip bound", ifname);

	type = __connman_ipconfig_get_config_type(ipconfig);
	method = __connman_ipconfig_get_method(ipconfig);

	DBG("service %p ipconfig %p type %d method %d", service, ipconfig,
							type, method);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6 &&
			method == CONNMAN_IPCONFIG_METHOD_AUTO)
		__connman_service_ipconfig_indicate_state(service,
						CONNMAN_SERVICE_STATE_READY,
						CONNMAN_IPCONFIG_TYPE_IPV6);

	settings_changed(service, ipconfig);
}

static void service_ip_release(struct connman_ipconfig *ipconfig,
			const char *ifname)
{
	struct connman_service *service = __connman_ipconfig_get_data(ipconfig);
	enum connman_ipconfig_method method = CONNMAN_IPCONFIG_METHOD_UNKNOWN;
	enum connman_ipconfig_type type = CONNMAN_IPCONFIG_TYPE_UNKNOWN;

	DBG("%s ip release", ifname);

	type = __connman_ipconfig_get_config_type(ipconfig);
	method = __connman_ipconfig_get_method(ipconfig);

	DBG("service %p ipconfig %p type %d method %d", service, ipconfig,
							type, method);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV6 &&
			method == CONNMAN_IPCONFIG_METHOD_OFF)
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_DISCONNECT,
					CONNMAN_IPCONFIG_TYPE_IPV6);

	if (type == CONNMAN_IPCONFIG_TYPE_IPV4 &&
			method == CONNMAN_IPCONFIG_METHOD_OFF)
		__connman_service_ipconfig_indicate_state(service,
					CONNMAN_SERVICE_STATE_DISCONNECT,
					CONNMAN_IPCONFIG_TYPE_IPV4);

	settings_changed(service, ipconfig);
}

static void service_route_changed(struct connman_ipconfig *ipconfig,
				const char *ifname)
{
	struct connman_service *service = __connman_ipconfig_get_data(ipconfig);

	DBG("%s route changed", ifname);

	settings_changed(service, ipconfig);
}

static const struct connman_ipconfig_ops service_ops = {
	.up		= service_up,
	.down		= service_down,
	.lower_up	= service_lower_up,
	.lower_down	= service_lower_down,
	.ip_bound	= service_ip_bound,
	.ip_release	= service_ip_release,
	.route_set	= service_route_changed,
	.route_unset	= service_route_changed,
};

static struct connman_ipconfig *create_ip4config(struct connman_service *service,
		int index, enum connman_ipconfig_method method)
{
	struct connman_ipconfig *ipconfig_ipv4;

	ipconfig_ipv4 = __connman_ipconfig_create(index,
						CONNMAN_IPCONFIG_TYPE_IPV4);
	if (!ipconfig_ipv4)
		return NULL;

	__connman_ipconfig_set_method(ipconfig_ipv4, method);

	__connman_ipconfig_set_data(ipconfig_ipv4, service);

	__connman_ipconfig_set_ops(ipconfig_ipv4, &service_ops);

	return ipconfig_ipv4;
}

static struct connman_ipconfig *create_ip6config(struct connman_service *service,
		int index)
{
	struct connman_ipconfig *ipconfig_ipv6;

	ipconfig_ipv6 = __connman_ipconfig_create(index,
						CONNMAN_IPCONFIG_TYPE_IPV6);
	if (!ipconfig_ipv6)
		return NULL;

	__connman_ipconfig_set_data(ipconfig_ipv6, service);

	__connman_ipconfig_set_ops(ipconfig_ipv6, &service_ops);

	return ipconfig_ipv6;
}

void __connman_service_read_ip4config(struct connman_service *service)
{
	GKeyFile *keyfile;

	if (!service->ipconfig_ipv4)
		return;

	keyfile = connman_storage_load_service(service->identifier);
	if (!keyfile)
		return;

	__connman_ipconfig_load(service->ipconfig_ipv4, keyfile,
				service->identifier, "IPv4.");

	g_key_file_free(keyfile);
}

void connman_service_create_ip4config(struct connman_service *service,
					int index)
{
	DBG("ipv4 %p", service->ipconfig_ipv4);

	if (service->ipconfig_ipv4)
		return;

	service->ipconfig_ipv4 = create_ip4config(service, index,
			CONNMAN_IPCONFIG_METHOD_DHCP);
	__connman_service_read_ip4config(service);
}

void __connman_service_read_ip6config(struct connman_service *service)
{
	GKeyFile *keyfile;

	if (!service->ipconfig_ipv6)
		return;

	keyfile = connman_storage_load_service(service->identifier);
	if (!keyfile)
		return;

	__connman_ipconfig_load(service->ipconfig_ipv6, keyfile,
				service->identifier, "IPv6.");

	g_key_file_free(keyfile);
}

void connman_service_create_ip6config(struct connman_service *service,
								int index)
{
	DBG("ipv6 %p", service->ipconfig_ipv6);

	if (service->ipconfig_ipv6)
		return;

	service->ipconfig_ipv6 = create_ip6config(service, index);

	__connman_service_read_ip6config(service);
}

/**
 * connman_service_lookup_from_network:
 * @network: network structure
 *
 * Look up a service by network (reference count will not be increased)
 */
struct connman_service *connman_service_lookup_from_network(struct connman_network *network)
{
	struct connman_service *service;
	const char *ident, *group;
	char *name;

	if (!network)
		return NULL;

	ident = __connman_network_get_ident(network);
	if (!ident)
		return NULL;

	group = connman_network_get_group(network);
	if (!group)
		return NULL;

	name = g_strdup_printf("%s_%s_%s",
			__connman_network_get_type(network), ident, group);
	service = lookup_by_identifier(name);
	g_free(name);

	return service;
}

struct connman_service *__connman_service_lookup_from_index(int index)
{
	struct connman_service *service;
	GList *list;

	for (list = service_list; list; list = list->next) {
		service = list->data;

		if (__connman_ipconfig_get_index(service->ipconfig_ipv4)
							== index)
			return service;

		if (__connman_ipconfig_get_index(service->ipconfig_ipv6)
							== index)
			return service;
	}

	return NULL;
}

struct connman_service *__connman_service_lookup_from_ident(const char *identifier)
{
	return lookup_by_identifier(identifier);
}

const char *__connman_service_get_ident(struct connman_service *service)
{
	return service->identifier;
}

const char *__connman_service_get_path(struct connman_service *service)
{
	return service->path;
}

const char *__connman_service_get_name(struct connman_service *service)
{
	return service->name;
}

enum connman_service_state __connman_service_get_state(struct connman_service *service)
{
	return service->state;
}

unsigned int __connman_service_get_order(struct connman_service *service)
{
	unsigned int order = 0;

	if (!service)
		return 0;

	service->order = 0;

	if (!service->favorite)
		return 0;

	if (service == service_list->data)
		order = 1;

	if (service->type == CONNMAN_SERVICE_TYPE_VPN &&
			!service->do_split_routing) {
		service->order = 10;
		order = 10;
	}

	DBG("service %p name %s order %d split %d", service, service->name,
		order, service->do_split_routing);

	return order;
}

void __connman_service_update_ordering(void)
{
	if (service_list && service_list->next)
		service_list = g_list_sort(service_list, service_compare);
}

static enum connman_service_type convert_network_type(struct connman_network *network)
{
	enum connman_network_type type = connman_network_get_type(network);

	switch (type) {
	case CONNMAN_NETWORK_TYPE_UNKNOWN:
	case CONNMAN_NETWORK_TYPE_VENDOR:
		break;
	case CONNMAN_NETWORK_TYPE_ETHERNET:
		return CONNMAN_SERVICE_TYPE_ETHERNET;
	case CONNMAN_NETWORK_TYPE_WIFI:
		return CONNMAN_SERVICE_TYPE_WIFI;
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_PAN:
	case CONNMAN_NETWORK_TYPE_BLUETOOTH_DUN:
		return CONNMAN_SERVICE_TYPE_BLUETOOTH;
	case CONNMAN_NETWORK_TYPE_CELLULAR:
		return CONNMAN_SERVICE_TYPE_CELLULAR;
	case CONNMAN_NETWORK_TYPE_GADGET:
		return CONNMAN_SERVICE_TYPE_GADGET;
	}

	return CONNMAN_SERVICE_TYPE_UNKNOWN;
}

static enum connman_service_security convert_wifi_security(const char *security)
{
	if (!security)
		return CONNMAN_SERVICE_SECURITY_UNKNOWN;
	else if (g_str_equal(security, "none"))
		return CONNMAN_SERVICE_SECURITY_NONE;
	else if (g_str_equal(security, "wep"))
		return CONNMAN_SERVICE_SECURITY_WEP;
	else if (g_str_equal(security, "psk"))
		return CONNMAN_SERVICE_SECURITY_PSK;
	else if (g_str_equal(security, "ieee8021x"))
		return CONNMAN_SERVICE_SECURITY_8021X;
	else if (g_str_equal(security, "wpa"))
		return CONNMAN_SERVICE_SECURITY_WPA;
	else if (g_str_equal(security, "rsn"))
		return CONNMAN_SERVICE_SECURITY_RSN;
	else
		return CONNMAN_SERVICE_SECURITY_UNKNOWN;
}

static void update_from_network(struct connman_service *service,
					struct connman_network *network)
{
	uint8_t strength = service->strength;
	const char *str;

	DBG("service %p network %p", service, network);

	if (is_connected(service))
		return;

	if (is_connecting(service))
		return;

	str = connman_network_get_string(network, "Name");
	if (str) {
		g_free(service->name);
		service->name = g_strdup(str);
		service->hidden = false;
	} else {
		g_free(service->name);
		service->name = NULL;
		service->hidden = true;
	}

	service->strength = connman_network_get_strength(network);
	service->roaming = connman_network_get_bool(network, "Roaming");

	if (service->strength == 0) {
		/*
		 * Filter out 0-values; it's unclear what they mean
		 * and they cause anomalous sorting of the priority list.
		 */
		service->strength = strength;
	}

	str = connman_network_get_string(network, "WiFi.Security");
	service->security = convert_wifi_security(str);

	if (service->type == CONNMAN_SERVICE_TYPE_WIFI)
		service->wps = connman_network_get_bool(network, "WiFi.WPS");

	if (service->strength > strength && service->network) {
		connman_network_unref(service->network);
		service->network = connman_network_ref(network);

		strength_changed(service);
	}

	if (!service->network)
		service->network = connman_network_ref(network);

	service_list_sort();
}

/**
 * __connman_service_create_from_network:
 * @network: network structure
 *
 * Look up service by network and if not found, create one
 */
struct connman_service * __connman_service_create_from_network(struct connman_network *network)
{
	struct connman_service *service;
	struct connman_device *device;
	const char *ident, *group;
	char *name;
	unsigned int *auto_connect_types;
	int i, index;

	DBG("network %p", network);

	if (!network)
		return NULL;

	ident = __connman_network_get_ident(network);
	if (!ident)
		return NULL;

	group = connman_network_get_group(network);
	if (!group)
		return NULL;

	name = g_strdup_printf("%s_%s_%s",
			__connman_network_get_type(network), ident, group);
	service = service_get(name);
	g_free(name);

	if (!service)
		return NULL;

	if (__connman_network_get_weakness(network))
		return service;

	if (service->path) {
		update_from_network(service, network);
		__connman_connection_update_gateway();
		return service;
	}

	service->type = convert_network_type(network);

	auto_connect_types = connman_setting_get_uint_list("DefaultAutoConnectTechnologies");
	service->autoconnect = false;
	for (i = 0; auto_connect_types &&
		     auto_connect_types[i] != 0; i++) {
		if (service->type == auto_connect_types[i]) {
			service->autoconnect = true;
			break;
		}
	}

	switch (service->type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_VPN:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
		service->favorite = true;
		break;
	}

	service->state_ipv4 = service->state_ipv6 = CONNMAN_SERVICE_STATE_IDLE;
	service->state = combine_state(service->state_ipv4, service->state_ipv6);

	update_from_network(service, network);

	index = connman_network_get_index(network);

	if (!service->ipconfig_ipv4)
		service->ipconfig_ipv4 = create_ip4config(service, index,
				CONNMAN_IPCONFIG_METHOD_DHCP);

	if (!service->ipconfig_ipv6)
		service->ipconfig_ipv6 = create_ip6config(service, index);

	service_register(service);

	if (service->favorite) {
		device = connman_network_get_device(service->network);
		if (device && !connman_device_get_scanning(device)) {

			switch (service->type) {
			case CONNMAN_SERVICE_TYPE_UNKNOWN:
			case CONNMAN_SERVICE_TYPE_SYSTEM:
			case CONNMAN_SERVICE_TYPE_P2P:
				break;

			case CONNMAN_SERVICE_TYPE_GADGET:
			case CONNMAN_SERVICE_TYPE_ETHERNET:
				if (service->autoconnect) {
					__connman_service_connect(service,
						CONNMAN_SERVICE_CONNECT_REASON_AUTO);
					break;
				}

				/* fall through */
			case CONNMAN_SERVICE_TYPE_BLUETOOTH:
			case CONNMAN_SERVICE_TYPE_GPS:
			case CONNMAN_SERVICE_TYPE_VPN:
			case CONNMAN_SERVICE_TYPE_WIFI:
			case CONNMAN_SERVICE_TYPE_CELLULAR:
				__connman_service_auto_connect(CONNMAN_SERVICE_CONNECT_REASON_AUTO);
				break;
			}
		}
	}

	__connman_notifier_service_add(service, service->name);
	service_schedule_added(service);

	return service;
}

void __connman_service_update_from_network(struct connman_network *network)
{
	bool need_sort = false;
	struct connman_service *service;
	uint8_t strength;
	bool roaming;
	const char *name;
	bool stats_enable;

	service = connman_service_lookup_from_network(network);
	if (!service)
		return;

	if (!service->network)
		return;

	name = connman_network_get_string(service->network, "Name");
	if (g_strcmp0(service->name, name) != 0) {
		g_free(service->name);
		service->name = g_strdup(name);

		if (allow_property_changed(service))
			connman_dbus_property_changed_basic(service->path,
					CONNMAN_SERVICE_INTERFACE, "Name",
					DBUS_TYPE_STRING, &service->name);
	}

	if (service->type == CONNMAN_SERVICE_TYPE_WIFI)
		service->wps = connman_network_get_bool(network, "WiFi.WPS");

	strength = connman_network_get_strength(service->network);
	if (strength == service->strength)
		goto roaming;

	service->strength = strength;
	need_sort = true;

	strength_changed(service);

roaming:
	roaming = connman_network_get_bool(service->network, "Roaming");
	if (roaming == service->roaming)
		goto sorting;

	stats_enable = stats_enabled(service);
	if (stats_enable)
		stats_stop(service);

	service->roaming = roaming;
	need_sort = true;

	if (stats_enable)
		stats_start(service);

	roaming_changed(service);

sorting:
	if (need_sort) {
		service_list_sort();
	}
}

void __connman_service_remove_from_network(struct connman_network *network)
{
	struct connman_service *service;

	service = connman_service_lookup_from_network(network);

	DBG("network %p service %p", network, service);

	if (!service)
		return;

	service->ignore = true;

	__connman_connection_gateway_remove(service,
					CONNMAN_IPCONFIG_TYPE_ALL);

	connman_service_unref(service);
}

/**
 * __connman_service_create_from_provider:
 * @provider: provider structure
 *
 * Look up service by provider and if not found, create one
 */
struct connman_service *
__connman_service_create_from_provider(struct connman_provider *provider)
{
	struct connman_service *service;
	const char *ident, *str;
	char *name;
	int index = connman_provider_get_index(provider);

	DBG("provider %p", provider);

	ident = __connman_provider_get_ident(provider);
	if (!ident)
		return NULL;

	name = g_strdup_printf("vpn_%s", ident);
	service = service_get(name);
	g_free(name);

	if (!service)
		return NULL;

	service->type = CONNMAN_SERVICE_TYPE_VPN;
	service->provider = connman_provider_ref(provider);
	service->autoconnect = false;
	service->favorite = true;

	service->state_ipv4 = service->state_ipv6 = CONNMAN_SERVICE_STATE_IDLE;
	service->state = combine_state(service->state_ipv4, service->state_ipv6);

	str = connman_provider_get_string(provider, "Name");
	if (str) {
		g_free(service->name);
		service->name = g_strdup(str);
		service->hidden = false;
	} else {
		g_free(service->name);
		service->name = NULL;
		service->hidden = true;
	}

	service->strength = 0;

	if (!service->ipconfig_ipv4)
		service->ipconfig_ipv4 = create_ip4config(service, index,
				CONNMAN_IPCONFIG_METHOD_MANUAL);

	if (!service->ipconfig_ipv6)
		service->ipconfig_ipv6 = create_ip6config(service, index);

	service_register(service);

	__connman_notifier_service_add(service, service->name);
	service_schedule_added(service);

	return service;
}

static void remove_unprovisioned_services(void)
{
	gchar **services;
	GKeyFile *keyfile, *configkeyfile;
	char *file, *section;
	int i = 0;

	services = connman_storage_get_services();
	if (!services)
		return;

	for (; services[i]; i++) {
		file = section = NULL;
		keyfile = configkeyfile = NULL;

		keyfile = connman_storage_load_service(services[i]);
		if (!keyfile)
			continue;

		file = g_key_file_get_string(keyfile, services[i],
					"Config.file", NULL);
		if (!file)
			goto next;

		section = g_key_file_get_string(keyfile, services[i],
					"Config.ident", NULL);
		if (!section)
			goto next;

		configkeyfile = __connman_storage_load_config(file);
		if (!configkeyfile) {
			/*
			 * Config file is missing, remove the provisioned
			 * service.
			 */
			__connman_storage_remove_service(services[i]);
			goto next;
		}

		if (!g_key_file_has_group(configkeyfile, section))
			/*
			 * Config section is missing, remove the provisioned
			 * service.
			 */
			__connman_storage_remove_service(services[i]);

	next:
		if (keyfile)
			g_key_file_free(keyfile);

		if (configkeyfile)
			g_key_file_free(configkeyfile);

		g_free(section);
		g_free(file);
	}

	g_strfreev(services);
}

static int agent_probe(struct connman_agent *agent)
{
	DBG("agent %p", agent);
	return 0;
}

static void agent_remove(struct connman_agent *agent)
{
	DBG("agent %p", agent);
}

static void *agent_context_ref(void *context)
{
	struct connman_service *service = context;

	return (void *)connman_service_ref(service);
}

static void agent_context_unref(void *context)
{
	struct connman_service *service = context;

	connman_service_unref(service);
}

static struct connman_agent_driver agent_driver = {
	.name		= "service",
	.interface      = CONNMAN_AGENT_INTERFACE,
	.probe		= agent_probe,
	.remove		= agent_remove,
	.context_ref	= agent_context_ref,
	.context_unref	= agent_context_unref,
};

int __connman_service_init(void)
{
	int err;

	DBG("");

	err = connman_agent_driver_register(&agent_driver);
	if (err < 0) {
		connman_error("Cannot register agent driver for %s",
						agent_driver.name);
		return err;
	}

	connection = connman_dbus_get_connection();

	service_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
							NULL, service_free);

	services_notify = g_new0(struct _services_notify, 1);
	services_notify->remove = g_hash_table_new_full(g_str_hash,
			g_str_equal, g_free, NULL);
	services_notify->add = g_hash_table_new(g_str_hash, g_str_equal);

	remove_unprovisioned_services();

	connman_service_load_last_explicit_service();

	return 0;
}

void __connman_service_cleanup(void)
{
	DBG("");

	if (vpn_autoconnect_timeout) {
		g_source_remove(vpn_autoconnect_timeout);
		vpn_autoconnect_timeout = 0;
	}

	if (autoconnect_timeout != 0) {
		g_source_remove(autoconnect_timeout);
		autoconnect_timeout = 0;
	}

	connman_agent_driver_unregister(&agent_driver);

	g_free(last_explicitly_connected_service);
	last_explicitly_connected_service = NULL;

	g_list_free(service_list);
	service_list = NULL;

	g_hash_table_destroy(service_hash);
	service_hash = NULL;

	g_slist_free(counter_list);
	counter_list = NULL;

	if (services_notify->id != 0) {
		g_source_remove(services_notify->id);
		service_send_changed(NULL);
		g_hash_table_destroy(services_notify->remove);
		g_hash_table_destroy(services_notify->add);
	}
	g_free(services_notify);

	dbus_connection_unref(connection);
}
