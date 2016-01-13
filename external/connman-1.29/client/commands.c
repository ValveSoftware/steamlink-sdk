/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2014  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include <glib.h>
#include <gdbus.h>

#include "dbus_helpers.h"
#include "input.h"
#include "services.h"
#include "peers.h"
#include "commands.h"
#include "agent.h"
#include "vpnconnections.h"

static DBusConnection *connection;
static GHashTable *service_hash;
static GHashTable *peer_hash;
static GHashTable *technology_hash;
static char *session_notify_path;
static char *session_path;
static bool session_connected;

struct connman_option {
	const char *name;
	const char val;
	const char *desc;
};

static char *ipv4[] = {
	"Method",
	"Address",
	"Netmask",
	"Gateway",
	NULL
};

static char *ipv6[] = {
	"Method",
	"Address",
	"PrefixLength",
	"Gateway",
	NULL
};

static int cmd_help(char *args[], int num, struct connman_option *options);

static bool check_dbus_name(const char *name)
{
	/*
	 * Valid dbus chars should be [A-Z][a-z][0-9]_
	 * and should not start with number.
	 */
	unsigned int i;

	if (!name || name[0] == '\0')
		return false;

	for (i = 0; name[i] != '\0'; i++)
		if (!((name[i] >= 'A' && name[i] <= 'Z') ||
				(name[i] >= 'a' && name[i] <= 'z') ||
				(name[i] >= '0' && name[i] <= '9') ||
				name[i] == '_'))
			return false;

	return true;
}

static int parse_boolean(char *arg)
{
	if (!arg)
		return -1;

	if (strcasecmp(arg, "no") == 0 ||
			strcasecmp(arg, "false") == 0 ||
			strcasecmp(arg, "off" ) == 0 ||
			strcasecmp(arg, "disable" ) == 0 ||
			strcasecmp(arg, "n") == 0 ||
			strcasecmp(arg, "f") == 0 ||
			strcasecmp(arg, "0") == 0)
		return 0;

	if (strcasecmp(arg, "yes") == 0 ||
			strcasecmp(arg, "true") == 0 ||
			strcasecmp(arg, "on") == 0 ||
			strcasecmp(arg, "enable" ) == 0 ||
			strcasecmp(arg, "y") == 0 ||
			strcasecmp(arg, "t") == 0 ||
			strcasecmp(arg, "1") == 0)
		return 1;

	return -1;
}

static int parse_args(char *arg, struct connman_option *options)
{
	int i;

	if (!arg)
		return -1;

	for (i = 0; options[i].name; i++) {
		if (strcmp(options[i].name, arg) == 0 ||
				(strncmp(arg, "--", 2) == 0 &&
					strcmp(&arg[2], options[i].name) == 0))
			return options[i].val;
	}

	return '?';
}

static int enable_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *tech = user_data;
	char *str;

	str = strrchr(tech, '/');
	if (str)
		str++;
	else
		str = tech;

	if (!error)
		fprintf(stdout, "Enabled %s\n", str);
	else
		fprintf(stderr, "Error %s: %s\n", str, error);

	g_free(user_data);

	return 0;
}

static int cmd_enable(char *args[], int num, struct connman_option *options)
{
	char *tech;
	dbus_bool_t b = TRUE;

	if (num > 2)
		return -E2BIG;

	if (num < 2)
		return -EINVAL;

	if (check_dbus_name(args[1]) == false)
		return -EINVAL;

	if (strcmp(args[1], "offline") == 0) {
		tech = g_strdup(args[1]);
		return __connmanctl_dbus_set_property(connection, "/",
				"net.connman.Manager", enable_return, tech,
				"OfflineMode", DBUS_TYPE_BOOLEAN, &b);
	}

	tech = g_strdup_printf("/net/connman/technology/%s", args[1]);
	return __connmanctl_dbus_set_property(connection, tech,
				"net.connman.Technology", enable_return, tech,
				"Powered", DBUS_TYPE_BOOLEAN, &b);
}

static int disable_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *tech = user_data;
	char *str;

	str = strrchr(tech, '/');
	if (str)
		str++;
	else
		str = tech;

	if (!error)
		fprintf(stdout, "Disabled %s\n", str);
	else
		fprintf(stderr, "Error %s: %s\n", str, error);

	g_free(user_data);

	return 0;
}

static int cmd_disable(char *args[], int num, struct connman_option *options)
{
	char *tech;
	dbus_bool_t b = FALSE;

	if (num > 2)
		return -E2BIG;

	if (num < 2)
		return -EINVAL;

	if (check_dbus_name(args[1]) == false)
		return -EINVAL;

	if (strcmp(args[1], "offline") == 0) {
		tech = g_strdup(args[1]);
		return __connmanctl_dbus_set_property(connection, "/",
				"net.connman.Manager", disable_return, tech,
				"OfflineMode", DBUS_TYPE_BOOLEAN, &b);
	}

	tech = g_strdup_printf("/net/connman/technology/%s", args[1]);
	return __connmanctl_dbus_set_property(connection, tech,
				"net.connman.Technology", disable_return, tech,
				"Powered", DBUS_TYPE_BOOLEAN, &b);
}

static int state_print(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	DBusMessageIter entry;

	if (error) {
		fprintf(stderr, "Error: %s", error);
		return 0;
	}

	dbus_message_iter_recurse(iter, &entry);
	__connmanctl_dbus_print(&entry, "  ", " = ", "\n");
	fprintf(stdout, "\n");

	return 0;
}

static int cmd_state(char *args[], int num, struct connman_option *options)
{
	if (num > 1)
		return -E2BIG;

	return __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE,
			CONNMAN_PATH, "net.connman.Manager", "GetProperties",
			state_print, NULL, NULL, NULL);
}

static int services_list(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	if (!error) {
		__connmanctl_services_list(iter);
		fprintf(stdout, "\n");
	} else {
		fprintf(stderr, "Error: %s\n", error);
	}

	return 0;
}

static int peers_list(DBusMessageIter *iter,
					const char *error, void *user_data)
{
	if (!error) {
		__connmanctl_peers_list(iter);
		fprintf(stdout, "\n");
	} else
		fprintf(stderr, "Error: %s\n", error);

	return 0;
}

static int object_properties(DBusMessageIter *iter,
					const char *error, void *user_data)
{
	char *path = user_data;
	char *str;
	DBusMessageIter dict;

	if (!error) {
		fprintf(stdout, "%s\n", path);

		dbus_message_iter_recurse(iter, &dict);
		__connmanctl_dbus_print(&dict, "  ", " = ", "\n");

		fprintf(stdout, "\n");

	} else {
		str = strrchr(path, '/');
		if (str)
			str++;
		else
			str = path;

		fprintf(stderr, "Error %s: %s\n", str, error);
	}

	g_free(user_data);

	return 0;
}

static int cmd_services(char *args[], int num, struct connman_option *options)
{
	char *service_name = NULL;
	char *path;
	int c;

	if (num > 3)
		return -E2BIG;

	c = parse_args(args[1], options);
	switch (c) {
	case -1:
		break;
	case 'p':
		if (num < 3)
			return -EINVAL;
		service_name = args[2];
		break;
	default:
		if (num > 2)
			return -E2BIG;
		service_name = args[1];
		break;
	}

	if (!service_name) {
		return __connmanctl_dbus_method_call(connection,
				CONNMAN_SERVICE, CONNMAN_PATH,
				"net.connman.Manager", "GetServices",
				services_list, NULL, NULL, NULL);
	}

	if (check_dbus_name(service_name) == false)
		return -EINVAL;

	path = g_strdup_printf("/net/connman/service/%s", service_name);
	return __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE, path,
			"net.connman.Service", "GetProperties",
			object_properties, path, NULL, NULL);
}

static int cmd_peers(char *args[], int num, struct connman_option *options)
{
	char *peer_name = NULL;
	char *path;

	if (num > 2)
		return -E2BIG;

	if (num == 2)
		peer_name = args[1];

	if (!peer_name) {
		return __connmanctl_dbus_method_call(connection,
					CONNMAN_SERVICE, CONNMAN_PATH,
					"net.connman.Manager", "GetPeers",
					peers_list, NULL, NULL, NULL);
	}

	if (check_dbus_name(peer_name) == false)
		return -EINVAL;

	path = g_strdup_printf("/net/connman/peer/%s", peer_name);
	return __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE,
				path, "net.connman.Peer", "GetProperties",
				object_properties, path, NULL, NULL);
}

static int technology_print(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	DBusMessageIter array;

	if (error) {
		fprintf(stderr, "Error: %s\n", error);
		return 0;
	}

	dbus_message_iter_recurse(iter, &array);
	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
		DBusMessageIter entry, dict;
		const char *path;

		dbus_message_iter_recurse(&array, &entry);
		dbus_message_iter_get_basic(&entry, &path);
		fprintf(stdout, "%s\n", path);

		dbus_message_iter_next(&entry);

		dbus_message_iter_recurse(&entry, &dict);
		__connmanctl_dbus_print(&dict, "  ", " = ", "\n");
		fprintf(stdout, "\n");

		dbus_message_iter_next(&array);
	}

	return 0;
}

static int cmd_technologies(char *args[], int num,
		struct connman_option *options)
{
	if (num > 1)
		return -E2BIG;

	return __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE,
			CONNMAN_PATH, "net.connman.Manager", "GetTechnologies",
			technology_print, NULL,	NULL, NULL);
}

struct tether_enable {
	char *path;
	dbus_bool_t enable;
};

static int tether_set_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	struct tether_enable *tether = user_data;
	char *str;

	str = strrchr(tether->path, '/');
	if (str)
		str++;
	else
		str = tether->path;

	if (!error) {
		fprintf(stdout, "%s tethering for %s\n",
				tether->enable ? "Enabled" : "Disabled",
				str);
	} else
		fprintf(stderr, "Error %s %s tethering: %s\n",
				tether->enable ?
				"enabling" : "disabling", str, error);

	g_free(tether->path);
	g_free(user_data);

	return 0;
}

static int tether_set(char *technology, int set_tethering)
{
	struct tether_enable *tether = g_new(struct tether_enable, 1);

	switch(set_tethering) {
	case 1:
		tether->enable = TRUE;
		break;
	case 0:
		tether->enable = FALSE;
		break;
	default:
		g_free(tether);
		return 0;
	}

	tether->path = g_strdup_printf("/net/connman/technology/%s",
			technology);

	return __connmanctl_dbus_set_property(connection, tether->path,
			"net.connman.Technology", tether_set_return,
			tether, "Tethering", DBUS_TYPE_BOOLEAN,
			&tether->enable);
}

struct tether_properties {
	int ssid_result;
	int passphrase_result;
	int set_tethering;
};

static int tether_update(struct tether_properties *tether)
{
	if (tether->ssid_result == 0 && tether->passphrase_result == 0)
		return tether_set("wifi", tether->set_tethering);

	if (tether->ssid_result != -EINPROGRESS &&
			tether->passphrase_result != -EINPROGRESS) {
		g_free(tether);
		return 0;
	}

	return -EINPROGRESS;
}

static int tether_set_ssid_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	struct tether_properties *tether = user_data;

	if (!error) {
		fprintf(stdout, "Wifi SSID set\n");
		tether->ssid_result = 0;
	} else {
		fprintf(stderr, "Error setting wifi SSID: %s\n", error);
		tether->ssid_result = -EINVAL;
	}

	return tether_update(tether);
}

static int tether_set_passphrase_return(DBusMessageIter *iter,
		const char *error, void *user_data)
{
	struct tether_properties *tether = user_data;

	if (!error) {
		fprintf(stdout, "Wifi passphrase set\n");
		tether->passphrase_result = 0;
	} else {
		fprintf(stderr, "Error setting wifi passphrase: %s\n", error);
		tether->passphrase_result = -EINVAL;
	}

	return tether_update(tether);
}

static int tether_set_ssid(char *ssid, char *passphrase, int set_tethering)
{
	struct tether_properties *tether = g_new(struct tether_properties, 1);

	tether->set_tethering = set_tethering;

	tether->ssid_result = __connmanctl_dbus_set_property(connection,
			"/net/connman/technology/wifi",
			"net.connman.Technology",
			tether_set_ssid_return, tether,
			"TetheringIdentifier", DBUS_TYPE_STRING, &ssid);

	tether->passphrase_result =__connmanctl_dbus_set_property(connection,
			"/net/connman/technology/wifi",
			"net.connman.Technology",
			tether_set_passphrase_return, tether,
			"TetheringPassphrase", DBUS_TYPE_STRING, &passphrase);

	if (tether->ssid_result != -EINPROGRESS &&
			tether->passphrase_result != -EINPROGRESS) {
		g_free(tether);
		return -ENXIO;
	}

	return -EINPROGRESS;
}

static int cmd_tether(char *args[], int num, struct connman_option *options)
{
	char *ssid, *passphrase;
	int set_tethering;

	if (num < 3)
		return -EINVAL;

	passphrase = args[num - 1];
	ssid = args[num - 2];

	set_tethering = parse_boolean(args[2]);

	if (strcmp(args[1], "wifi") == 0) {

		if (num > 5)
			return -E2BIG;

		if (num == 5 && set_tethering == -1)
			return -EINVAL;

		if (num == 4)
			set_tethering = -1;

		if (num > 3)
			return tether_set_ssid(ssid, passphrase, set_tethering);
	}

	if (num > 3)
		return -E2BIG;

	if (set_tethering == -1)
		return -EINVAL;

	if (check_dbus_name(args[1]) == false)
		return -EINVAL;

	return tether_set(args[1], set_tethering);
}

static int scan_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *path = user_data;

	if (!error) {
		char *str = strrchr(path, '/');
		str++;
		fprintf(stdout, "Scan completed for %s\n", str);
	} else
		fprintf(stderr, "Error %s: %s\n", path, error);

	g_free(user_data);

	return 0;
}

static int cmd_scan(char *args[], int num, struct connman_option *options)
{
	char *path;

	if (num > 2)
		return -E2BIG;

	if (num < 2)
		return -EINVAL;

	if (check_dbus_name(args[1]) == false)
		return -EINVAL;

	path = g_strdup_printf("/net/connman/technology/%s", args[1]);
	return __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE, path,
			"net.connman.Technology", "Scan",
			scan_return, path, NULL, NULL);
}

static int connect_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *path = user_data;

	if (!error) {
		char *str = strrchr(path, '/');
		str++;
		fprintf(stdout, "Connected %s\n", str);
	} else
		fprintf(stderr, "Error %s: %s\n", path, error);

	g_free(user_data);

	return 0;
}

static int cmd_connect(char *args[], int num, struct connman_option *options)
{
	const char *iface = "net.connman.Service";
	char *path;

	if (num > 2)
		return -E2BIG;

	if (num < 2)
		return -EINVAL;

	if (check_dbus_name(args[1]) == false)
		return -EINVAL;

	if (g_strstr_len(args[1], 5, "peer_") == args[1]) {
		iface = "net.connman.Peer";
		path = g_strdup_printf("/net/connman/peer/%s", args[1]);
	} else
		path = g_strdup_printf("/net/connman/service/%s", args[1]);

	return __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE, path,
			iface, "Connect", connect_return, path, NULL, NULL);
}

static int disconnect_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *path = user_data;

	if (!error) {
		char *str = strrchr(path, '/');
		str++;
		fprintf(stdout, "Disconnected %s\n", str);
	} else
		fprintf(stderr, "Error %s: %s\n", path, error);

	g_free(user_data);

	return 0;
}

static int cmd_disconnect(char *args[], int num, struct connman_option *options)
{
	const char *iface = "net.connman.Service";
	char *path;

	if (num > 2)
		return -E2BIG;

	if (num < 2)
		return -EINVAL;

	if (check_dbus_name(args[1]) == false)
		return -EINVAL;

	if (g_strstr_len(args[1], 5, "peer_") == args[1]) {
		iface = "net.connman.Peer";
		path = g_strdup_printf("/net/connman/peer/%s", args[1]);
	} else
		path = g_strdup_printf("/net/connman/service/%s", args[1]);

	return __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE,
					path, iface, "Disconnect",
					disconnect_return, path, NULL, NULL);
}

static int config_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *service_name = user_data;

	if (error)
		fprintf(stderr, "Error %s: %s\n", service_name, error);

	g_free(user_data);

	return 0;
}

struct config_append {
	char **opts;
	int values;
};

static void config_append_ipv4(DBusMessageIter *iter,
		void *user_data)
{
	struct config_append *append = user_data;
	char **opts = append->opts;
	int i = 0;

	if (!opts)
		return;

	while (opts[i] && ipv4[i]) {
		__connmanctl_dbus_append_dict_entry(iter, ipv4[i],
				DBUS_TYPE_STRING, &opts[i]);
		i++;
	}

	append->values = i;
}

static void config_append_ipv6(DBusMessageIter *iter, void *user_data)
{
	struct config_append *append = user_data;
	char **opts = append->opts;

	if (!opts)
		return;

	append->values = 1;

	if (g_strcmp0(opts[0], "auto") == 0) {
		char *str;

		switch (parse_boolean(opts[1])) {
		case 0:
			append->values = 2;

			str = "disabled";
			__connmanctl_dbus_append_dict_entry(iter, "Privacy",
					DBUS_TYPE_STRING, &str);
			break;

		case 1:
			append->values = 2;

			str = "enabled";
			__connmanctl_dbus_append_dict_entry(iter, "Privacy",
					DBUS_TYPE_STRING, &str);
			break;

		default:
			if (opts[1]) {
				append->values = 2;

				if (g_strcmp0(opts[1], "prefered") != 0 &&
						g_strcmp0(opts[1],
							"preferred") != 0) {
					fprintf(stderr, "Error %s: %s\n",
							opts[1],
							strerror(EINVAL));
					return;
				}

				str = "prefered";
				__connmanctl_dbus_append_dict_entry(iter,
						"Privacy", DBUS_TYPE_STRING,
						&str);
			}
			break;
		}
	} else if (g_strcmp0(opts[0], "manual") == 0) {
		int i = 1;

		while (opts[i] && ipv6[i]) {
			if (i == 2) {
				int value = atoi(opts[i]);
				__connmanctl_dbus_append_dict_entry(iter,
						ipv6[i], DBUS_TYPE_BYTE,
						&value);
			} else {
				__connmanctl_dbus_append_dict_entry(iter,
						ipv6[i], DBUS_TYPE_STRING,
						&opts[i]);
			}
			i++;
		}

		append->values = i;

	} else if (g_strcmp0(opts[0], "off") != 0) {
		fprintf(stderr, "Error %s: %s\n", opts[0], strerror(EINVAL));

		return;
	}

	__connmanctl_dbus_append_dict_entry(iter, "Method", DBUS_TYPE_STRING,
				&opts[0]);
}

static void config_append_str(DBusMessageIter *iter, void *user_data)
{
	struct config_append *append = user_data;
	char **opts = append->opts;
	int i = 0;

	if (!opts)
		return;

	while (opts[i]) {
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
				&opts[i]);
		i++;
	}

	append->values = i;
}

static void append_servers(DBusMessageIter *iter, void *user_data)
{
	struct config_append *append = user_data;
	char **opts = append->opts;
	int i = 1;

	if (!opts)
		return;

	while (opts[i] && g_strcmp0(opts[i], "--excludes") != 0) {
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
				&opts[i]);
		i++;
	}

	append->values = i;
}

static void append_excludes(DBusMessageIter *iter, void *user_data)
{
	struct config_append *append = user_data;
	char **opts = append->opts;
	int i = append->values;

	if (!opts || !opts[i] ||
			g_strcmp0(opts[i], "--excludes") != 0)
		return;

	i++;
	while (opts[i]) {
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
				&opts[i]);
		i++;
	}

	append->values = i;
}

static void config_append_proxy(DBusMessageIter *iter, void *user_data)
{
	struct config_append *append = user_data;
	char **opts = append->opts;

	if (!opts)
		return;

	if (g_strcmp0(opts[0], "manual") == 0) {
		__connmanctl_dbus_append_dict_string_array(iter, "Servers",
				append_servers, append);

		__connmanctl_dbus_append_dict_string_array(iter, "Excludes",
				append_excludes, append);

	} else if (g_strcmp0(opts[0], "auto") == 0) {
		if (opts[1]) {
			__connmanctl_dbus_append_dict_entry(iter, "URL",
					DBUS_TYPE_STRING, &opts[1]);
			append->values++;
		}

	} else if (g_strcmp0(opts[0], "direct") != 0)
		return;

	__connmanctl_dbus_append_dict_entry(iter, "Method",DBUS_TYPE_STRING,
			&opts[0]);

	append->values++;
}

static int cmd_config(char *args[], int num, struct connman_option *options)
{
	int result = 0, res = 0, index = 2, oldindex = 0;
	int c;
	char *service_name, *path;
	char **opt_start;
	dbus_bool_t val;
	struct config_append append;

	service_name = args[1];
	if (!service_name)
		return -EINVAL;

	if (check_dbus_name(service_name) == false)
		return -EINVAL;

	while (index < num && args[index]) {
		c = parse_args(args[index], options);
		opt_start = &args[index + 1];
		append.opts = opt_start;
		append.values = 0;

		res = 0;

		oldindex = index;
		path = g_strdup_printf("/net/connman/service/%s", service_name);

		switch (c) {
		case 'a':
			switch (parse_boolean(*opt_start)) {
			case 1:
				val = TRUE;
				break;
			case 0:
				val = FALSE;
				break;
			default:
				res = -EINVAL;
				break;
			}

			index++;

			if (res == 0) {
				res = __connmanctl_dbus_set_property(connection,
						path, "net.connman.Service",
						config_return,
						g_strdup(service_name),
						"AutoConnect",
						DBUS_TYPE_BOOLEAN, &val);
			}
			break;
		case 'i':
			res = __connmanctl_dbus_set_property_dict(connection,
					path, "net.connman.Service",
					config_return, g_strdup(service_name),
					"IPv4.Configuration", DBUS_TYPE_STRING,
					config_append_ipv4, &append);
			index += append.values;
			break;

		case 'v':
			res = __connmanctl_dbus_set_property_dict(connection,
					path, "net.connman.Service",
					config_return, g_strdup(service_name),
					"IPv6.Configuration", DBUS_TYPE_STRING,
					config_append_ipv6, &append);
			index += append.values;
			break;

		case 'n':
			res = __connmanctl_dbus_set_property_array(connection,
					path, "net.connman.Service",
					config_return, g_strdup(service_name),
					"Nameservers.Configuration",
					DBUS_TYPE_STRING, config_append_str,
					&append);
			index += append.values;
			break;

		case 't':
			res = __connmanctl_dbus_set_property_array(connection,
					path, "net.connman.Service",
					config_return, g_strdup(service_name),
					"Timeservers.Configuration",
					DBUS_TYPE_STRING, config_append_str,
					&append);
			index += append.values;
			break;

		case 'd':
			res = __connmanctl_dbus_set_property_array(connection,
					path, "net.connman.Service",
					config_return, g_strdup(service_name),
					"Domains.Configuration",
					DBUS_TYPE_STRING, config_append_str,
					&append);
			index += append.values;
			break;

		case 'x':
			res = __connmanctl_dbus_set_property_dict(connection,
					path, "net.connman.Service",
					config_return, g_strdup(service_name),
					"Proxy.Configuration",
					DBUS_TYPE_STRING, config_append_proxy,
					&append);
			index += append.values;
			break;
		case 'r':
			res = __connmanctl_dbus_method_call(connection,
					CONNMAN_SERVICE, path,
					"net.connman.Service", "Remove",
					config_return, g_strdup(service_name),
					NULL, NULL);
			break;
		default:
			res = -EINVAL;
			break;
		}

		g_free(path);

		if (res < 0) {
			if (res == -EINPROGRESS)
				result = -EINPROGRESS;
			else
				printf("Error '%s': %s\n", args[oldindex],
						strerror(-res));
		} else
			index += res;

		index++;
	}

	return result;
}

static DBusHandlerResult monitor_changed(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;
	const char *interface, *path;

	interface = dbus_message_get_interface(message);
	if (!interface)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strncmp(interface, "net.connman.", 12) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!strcmp(interface, "net.connman.Agent") ||
			!strcmp(interface, "net.connman.vpn.Agent") ||
			!strcmp(interface, "net.connman.Session") ||
			!strcmp(interface, "net.connman.Notification"))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	interface = strrchr(interface, '.');
	if (interface && *interface != '\0')
		interface++;

	path = strrchr(dbus_message_get_path(message), '/');
	if (path && *path != '\0')
		path++;

	__connmanctl_save_rl();

	if (dbus_message_is_signal(message, "net.connman.Manager",
					"ServicesChanged")) {

		fprintf(stdout, "%-12s %-20s = {\n", interface,
				"ServicesChanged");
		dbus_message_iter_init(message, &iter);
		__connmanctl_services_list(&iter);
		fprintf(stdout, "\n}\n");

		__connmanctl_redraw_rl();

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (dbus_message_is_signal(message, "net.connman.Manager",
							"PeersChanged")) {
		fprintf(stdout, "%-12s %-20s = {\n", interface,
							"PeersChanged");
		dbus_message_iter_init(message, &iter);
		__connmanctl_peers_list(&iter);
		fprintf(stdout, "\n}\n");

		__connmanctl_redraw_rl();

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (dbus_message_is_signal(message, "net.connman.vpn.Manager",
					"ConnectionAdded") ||
			dbus_message_is_signal(message,
					"net.connman.vpn.Manager",
					"ConnectionRemoved")) {
		interface = "vpn.Manager";
		path = dbus_message_get_member(message);

	} else if (dbus_message_is_signal(message, "net.connman.Manager",
					"TechnologyAdded") ||
			dbus_message_is_signal(message, "net.connman.Manager",
					"TechnologyRemoved"))
		path = dbus_message_get_member(message);

	fprintf(stdout, "%-12s %-20s ", interface, path);
	dbus_message_iter_init(message, &iter);

	__connmanctl_dbus_print(&iter, "", " = ", " = ");
	fprintf(stdout, "\n");

	__connmanctl_redraw_rl();

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static struct {
	char *interface;
	bool enabled;
} monitor[] = {
	{ "Service", false },
	{ "Technology", false },
	{ "Manager", false },
	{ "vpn.Manager", false },
	{ "vpn.Connection", false },
	{ NULL, },
};

static void monitor_add(char *interface)
{
	bool add_filter = true, found = false;
	int i;
	char *rule;
	DBusError err;

	for (i = 0; monitor[i].interface; i++) {
		if (monitor[i].enabled == true)
			add_filter = false;

		if (g_strcmp0(interface, monitor[i].interface) == 0) {
			if (monitor[i].enabled == true)
				return;

			monitor[i].enabled = true;
			found = true;
		}
	}

	if (found == false)
		return;

	if (add_filter == true)
		dbus_connection_add_filter(connection, monitor_changed,
				NULL, NULL);

	dbus_error_init(&err);
	rule  = g_strdup_printf("type='signal',interface='net.connman.%s'",
			interface);
	dbus_bus_add_match(connection, rule, &err);
	g_free(rule);

	if (dbus_error_is_set(&err))
		fprintf(stderr, "Error: %s\n", err.message);
}

static void monitor_del(char *interface)
{
	bool del_filter = true, found = false;
	int i;
	char *rule;


	for (i = 0; monitor[i].interface; i++) {
		if (g_strcmp0(interface, monitor[i].interface) == 0) {
			if (monitor[i].enabled == false)
				return;

			monitor[i].enabled = false;
			found = true;
		}

		if (monitor[i].enabled == true)
			del_filter = false;
	}

	if (found == false)
		return;

	rule  = g_strdup_printf("type='signal',interface='net.connman.%s'",
			interface);
	dbus_bus_remove_match(connection, rule, NULL);
	g_free(rule);

	if (del_filter == true)
		dbus_connection_remove_filter(connection, monitor_changed,
				NULL);
}

static int cmd_monitor(char *args[], int num, struct connman_option *options)
{
	bool add = true;
	int c;

	if (num > 3)
		return -E2BIG;

	if (num == 3) {
		switch (parse_boolean(args[2])) {
		case 0:
			add = false;
			break;

		default:
			break;
		}
	}

	c = parse_args(args[1], options);
	switch (c) {
	case -1:
		monitor_add("Service");
		monitor_add("Technology");
		monitor_add("Manager");
		monitor_add("vpn.Manager");
		monitor_add("vpn.Connection");
		break;

	case 's':
		if (add == true)
			monitor_add("Service");
		else
			monitor_del("Service");
		break;

	case 'c':
		if (add == true)
			monitor_add("Technology");
		else
			monitor_del("Technology");
		break;

	case 'm':
		if (add == true)
			monitor_add("Manager");
		else
			monitor_del("Manager");
		break;

	case 'M':
		if (add == true)
			monitor_add("vpn.Manager");
		else
			monitor_del("vpn.Manager");
		break;

	case 'C':
		if (add == true)
			monitor_add("vpn.Connection");
		else
			monitor_del("vpn.Connection");
		break;

	default:
		switch(parse_boolean(args[1])) {
		case 0:
			monitor_del("Service");
			monitor_del("Technology");
			monitor_del("Manager");
			monitor_del("vpn.Manager");
			monitor_del("vpn.Connection");
			break;

		case 1:
			monitor_add("Service");
			monitor_add("Technology");
			monitor_add("Manager");
			monitor_add("vpn.Manager");
			monitor_add("vpn.Connection");
			break;

		default:
			return -EINVAL;
		}
	}

	if (add == true)
		return -EINPROGRESS;

	return 0;
}

static int cmd_agent(char *args[], int num, struct connman_option *options)
{
	if (!__connmanctl_is_interactive()) {
		fprintf(stderr, "Error: Not supported in non-interactive "
				"mode\n");
		return 0;
	}

	if (num > 2)
		return -E2BIG;

	if (num < 2)
		return -EINVAL;

	switch(parse_boolean(args[1])) {
	case 0:
		__connmanctl_agent_unregister(connection);
		break;

	case 1:
		if (__connmanctl_agent_register(connection) == -EINPROGRESS)
			return -EINPROGRESS;

		break;

	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static int vpnconnections_properties(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *path = user_data;
	char *str;
	DBusMessageIter dict;

	if (!error) {
		fprintf(stdout, "%s\n", path);

		dbus_message_iter_recurse(iter, &dict);
		__connmanctl_dbus_print(&dict, "  ", " = ", "\n");

		fprintf(stdout, "\n");

	} else {
		str = strrchr(path, '/');
		if (str)
			str++;
		else
			str = path;

		fprintf(stderr, "Error %s: %s\n", str, error);
	}

	g_free(user_data);

	return 0;
}

static int vpnconnections_list(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	if (!error)
		__connmanctl_vpnconnections_list(iter);
        else
		fprintf(stderr, "Error: %s\n", error);

	return 0;
}

static int cmd_vpnconnections(char *args[], int num,
		struct connman_option *options)
{
	char *vpnconnection_name, *path;

	if (num > 2)
		return -E2BIG;

	vpnconnection_name = args[1];

	if (!vpnconnection_name)
		return __connmanctl_dbus_method_call(connection,
				VPN_SERVICE, VPN_PATH,
				"net.connman.vpn.Manager", "GetConnections",
				vpnconnections_list, NULL,
				NULL, NULL);

	if (check_dbus_name(vpnconnection_name) == false)
		return -EINVAL;

	path = g_strdup_printf("/net/connman/vpn/connection/%s",
			vpnconnection_name);
	return __connmanctl_dbus_method_call(connection, VPN_SERVICE, path,
			"net.connman.vpn.Connection", "GetProperties",
			vpnconnections_properties, path, NULL, NULL);

}

static int cmd_vpnagent(char *args[], int num, struct connman_option *options)
{
	if (!__connmanctl_is_interactive()) {
		fprintf(stderr, "Error: Not supported in non-interactive "
				"mode\n");
		return 0;
	}

	if (num > 2)
		return -E2BIG;

	if (num < 2)
		return -EINVAL;

	switch(parse_boolean(args[1])) {
	case 0:
		__connmanctl_vpn_agent_unregister(connection);
		break;

	case 1:
		if (__connmanctl_vpn_agent_register(connection) ==
				-EINPROGRESS)
			return -EINPROGRESS;

		break;

	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static DBusMessage *session_release(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	__connmanctl_save_rl();

	fprintf(stdout, "Session %s released\n", session_path);

	__connmanctl_redraw_rl();

	g_free(session_path);
	session_path = NULL;
	session_connected = false;

	return g_dbus_create_reply(message, DBUS_TYPE_INVALID);
}

static DBusMessage *session_update(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	DBusMessageIter iter, dict;

	__connmanctl_save_rl();

	fprintf(stdout, "Session      Update               = {\n");

	dbus_message_iter_init(message, &iter);
	dbus_message_iter_recurse(&iter, &dict);

	__connmanctl_dbus_print(&dict, "", " = ", "\n");
	fprintf(stdout, "\n}\n");

	dbus_message_iter_recurse(&iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, variant;
		char *field, *state;

		dbus_message_iter_recurse(&dict, &entry);

		dbus_message_iter_get_basic(&entry, &field);

		if (dbus_message_iter_get_arg_type(&entry)
				== DBUS_TYPE_STRING
				&& !strcmp(field, "State")) {

			dbus_message_iter_next(&entry);
			dbus_message_iter_recurse(&entry, &variant);
			if (dbus_message_iter_get_arg_type(&variant)
					!= DBUS_TYPE_STRING)
				break;

			dbus_message_iter_get_basic(&variant, &state);

			if (!session_connected && (!strcmp(state, "connected")
					|| !strcmp(state, "online"))) {

				fprintf(stdout, "Session %s connected\n",
					session_path);
				session_connected = true;

				break;
			}

			if (!strcmp(state, "disconnected") &&
					session_connected) {

				fprintf(stdout, "Session %s disconnected\n",
					session_path);
				session_connected = false;
			}
			break;
		}

		dbus_message_iter_next(&dict);
	}

	__connmanctl_redraw_rl();

	return g_dbus_create_reply(message, DBUS_TYPE_INVALID);
}

static const GDBusMethodTable notification_methods[] = {
	{ GDBUS_METHOD("Release", NULL, NULL, session_release) },
	{ GDBUS_METHOD("Update", GDBUS_ARGS({"settings", "a{sv}"}),
				NULL, session_update) },
	{ },
};

static int session_notify_add(const char *path)
{
	if (session_notify_path)
		return 0;

	if (!g_dbus_register_interface(connection, path,
					"net.connman.Notification",
					notification_methods, NULL, NULL,
					NULL, NULL)) {
		fprintf(stderr, "Error: Failed to register VPN Agent "
				"callbacks\n");
		return -EIO;
	}

	session_notify_path = g_strdup(path);

	return 0;
}

static void session_notify_remove(void)
{
	if (!session_notify_path)
		return;

	g_dbus_unregister_interface(connection, session_notify_path,
			"net.connman.Notification");

	g_free(session_notify_path);
	session_notify_path = NULL;
}

static int session_connect_cb(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	if (error) {
		fprintf(stderr, "Error: %s", error);
		return 0;
	}

	return -EINPROGRESS;
}


static int session_connect(void)
{
	return __connmanctl_dbus_method_call(connection, "net.connman",
			session_path, "net.connman.Session", "Connect",
			session_connect_cb, NULL, NULL, NULL);
}

static int session_disconnect_cb(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	if (error)
		fprintf(stderr, "Error: %s", error);

	return 0;
}

static int session_disconnect(void)
{
	return __connmanctl_dbus_method_call(connection, "net.connman",
			session_path, "net.connman.Session", "Disconnect",
			session_disconnect_cb, NULL, NULL, NULL);
}

static int session_create_cb(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	gboolean connect = GPOINTER_TO_INT(user_data);
	char *str;

	if (error) {
		fprintf(stderr, "Error creating session: %s", error);
		session_notify_remove();
		return 0;
	}

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_OBJECT_PATH) {
		fprintf(stderr, "Error creating session: No session path\n");
		return -EINVAL;
	}

	g_free(session_path);

	dbus_message_iter_get_basic(iter, &str);
	session_path = g_strdup(str);

	fprintf(stdout, "Session %s created\n", session_path);

	if (connect)
		return session_connect();

	return -EINPROGRESS;
}

static void session_create_append(DBusMessageIter *iter, void *user_data)
{
	const char *notify_path = user_data;

	__connmanctl_dbus_append_dict(iter, NULL, NULL);

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
			&notify_path);
}

static int session_create(gboolean connect)
{
	int res;
	char *notify_path;

	notify_path = g_strdup_printf("/net/connman/connmanctl%d", getpid());
	session_notify_add(notify_path);

	res = __connmanctl_dbus_method_call(connection, "net.connman", "/",
			"net.connman.Manager", "CreateSession",
			session_create_cb, GINT_TO_POINTER(connect),
			session_create_append, notify_path);

	g_free(notify_path);

	if (res < 0 && res != -EINPROGRESS)
		session_notify_remove();

	return res;
}

static int session_destroy_cb(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	if (error) {
		fprintf(stderr, "Error destroying session: %s", error);
		return 0;
	}

	fprintf(stdout, "Session %s ended\n", session_path);

	g_free(session_path);
	session_path = NULL;
	session_connected = false;

	return 0;
}

static void session_destroy_append(DBusMessageIter *iter, void *user_data)
{
	const char *path = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);
}

static int session_destroy(void)
{
	return __connmanctl_dbus_method_call(connection, "net.connman", "/",
			"net.connman.Manager", "DestroySession",
			session_destroy_cb, NULL,
			session_destroy_append, session_path);
}

static int session_config_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	char *property_name = user_data;

	if (error)
		fprintf(stderr, "Error setting session %s: %s\n",
				property_name, error);

	return 0;
}

static void session_config_append_array(DBusMessageIter *iter,
		void *user_data)
{
	struct config_append *append = user_data;
	char **opts = append->opts;
	int i = 1;

	if (!opts)
		return;

	while (opts[i] && strncmp(opts[i], "--", 2) != 0) {
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
				&opts[i]);
		i++;
	}

	append->values = i;
}

static int session_config(char *args[], int num,
		struct connman_option *options)
{
	int index = 0, res = 0;
	struct config_append append;
	char c;

	while (index < num && args[index]) {
		append.opts = &args[index];
		append.values = 0;

		c = parse_args(args[index], options);

		switch (c) {
		case 'b':
			res = __connmanctl_dbus_session_change_array(connection,
					session_path, session_config_return,
					"AllowedBearers", "AllowedBearers",
					session_config_append_array, &append);
			break;
		case 't':
			if (!args[index + 1]) {
				res = -EINVAL;
				break;
			}

			res = __connmanctl_dbus_session_change(connection,
					session_path, session_config_return,
					"ConnectionType", "ConnectionType",
					DBUS_TYPE_STRING, &args[index + 1]);
			append.values = 2;
			break;

		default:
			res = -EINVAL;
		}

		if (res < 0 && res != -EINPROGRESS) {
			printf("Error '%s': %s\n", args[index],
					strerror(-res));
			return 0;
		}

		index += append.values;
	}

	return 0;
}

static int cmd_session(char *args[], int num, struct connman_option *options)
{
	char *command;

	if (num < 2)
		return -EINVAL;

	command = args[1];

	switch(parse_boolean(command)) {
	case 0:
		if (!session_path)
			return -EALREADY;
		return session_destroy();

	case 1:
		if (session_path)
			return -EALREADY;
		return session_create(FALSE);

	default:
		if (!strcmp(command, "connect")) {
			if (!session_path)
				return session_create(TRUE);

			return session_connect();

		} else if (!strcmp(command, "disconnect")) {

			if (!session_path) {
				fprintf(stdout, "Session does not exist\n");
				return 0;
			}

			return session_disconnect();
		} else if (!strcmp(command, "config")) {
			if (!session_path) {
				fprintf(stdout, "Session does not exist\n");
				return 0;
			}

			if (num == 2)
				return -EINVAL;

			return session_config(&args[2], num - 2, options);
		}

	}

	return -EINVAL;
}

static int cmd_exit(char *args[], int num, struct connman_option *options)
{
	return 1;
}

static char *lookup_service(const char *text, int state)
{
	static int len = 0;
	static GHashTableIter iter;
	gpointer key, value;

	if (state == 0) {
		g_hash_table_iter_init(&iter, service_hash);
		len = strlen(text);
	}

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		const char *service = key;
		if (strncmp(text, service, len) == 0)
			return strdup(service);
	}

	return NULL;
}

static char *lookup_service_arg(const char *text, int state)
{
	if (__connmanctl_input_calc_level() > 1) {
		__connmanctl_input_lookup_end();
		return NULL;
	}

	return lookup_service(text, state);
}

static char *lookup_peer(const char *text, int state)
{
	static GHashTableIter iter;
	gpointer key, value;
	static int len = 0;

	if (state == 0) {
		g_hash_table_iter_init(&iter, peer_hash);
		len = strlen(text);
	}

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		const char *peer = key;
		if (strncmp(text, peer, len) == 0)
			return strdup(peer);
	}

	return NULL;
}

static char *lookup_peer_arg(const char *text, int state)
{
	if (__connmanctl_input_calc_level() > 1) {
		__connmanctl_input_lookup_end();
		return NULL;
	}

	return lookup_peer(text, state);
}

static char *lookup_technology(const char *text, int state)
{
	static int len = 0;
	static GHashTableIter iter;
	gpointer key, value;

	if (state == 0) {
		g_hash_table_iter_init(&iter, technology_hash);
		len = strlen(text);
	}

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		const char *technology = key;
		if (strncmp(text, technology, len) == 0)
			return strdup(technology);
	}

	return NULL;
}

static char *lookup_technology_arg(const char *text, int state)
{
	if (__connmanctl_input_calc_level() > 1) {
		__connmanctl_input_lookup_end();
		return NULL;
	}

	return lookup_technology(text, state);
}

static char *lookup_technology_offline(const char *text, int state)
{
	static int len = 0;
	static bool end = false;
	char *str;

	if (__connmanctl_input_calc_level() > 1) {
		__connmanctl_input_lookup_end();
		return NULL;
	}

	if (state == 0) {
		len = strlen(text);
		end = false;
	}

	if (end)
		return NULL;

	str = lookup_technology(text, state);
	if (str)
		return str;

	end = true;

	if (strncmp(text, "offline", len) == 0)
		return strdup("offline");

	return NULL;
}

static char *lookup_on_off(const char *text, int state)
{
	char *onoff[] = { "on", "off", NULL };
	static int idx = 0;
	static int len = 0;

	char *str;

	if (!state) {
		idx = 0;
		len = strlen(text);
	}

	while (onoff[idx]) {
		str = onoff[idx];
		idx++;

		if (!strncmp(text, str, len))
			return strdup(str);
	}

	return NULL;
}

static char *lookup_tether(const char *text, int state)
{
	int level;

	level = __connmanctl_input_calc_level();
	if (level < 2)
		return lookup_technology(text, state);

	if (level == 2)
		return lookup_on_off(text, state);

	__connmanctl_input_lookup_end();

	return NULL;
}

static char *lookup_agent(const char *text, int state)
{
	if (__connmanctl_input_calc_level() > 1) {
		__connmanctl_input_lookup_end();
		return NULL;
	}

	return lookup_on_off(text, state);
}

static struct connman_option service_options[] = {
	{"properties", 'p', "[<service>]      (obsolete)"},
	{ NULL, }
};

static struct connman_option config_options[] = {
	{"nameservers", 'n', "<dns1> [<dns2>] [<dns3>]"},
	{"timeservers", 't', "<ntp1> [<ntp2>] [...]"},
	{"domains", 'd', "<domain1> [<domain2>] [...]"},
	{"ipv6", 'v', "off|auto [enable|disable|preferred]|\n"
	              "\t\t\tmanual <address> <prefixlength> <gateway>"},
	{"proxy", 'x', "direct|auto <URL>|manual <URL1> [<URL2>] [...]\n"
	               "\t\t\t[exclude <exclude1> [<exclude2>] [...]]"},
	{"autoconnect", 'a', "yes|no"},
	{"ipv4", 'i', "off|dhcp|manual <address> <netmask> <gateway>"},
	{"remove", 'r', "                 Remove service"},
	{ NULL, }
};

static struct connman_option monitor_options[] = {
	{"services", 's', "[off]            Monitor only services"},
	{"tech", 'c', "[off]            Monitor only technologies"},
	{"manager", 'm', "[off]            Monitor only manager interface"},
	{"vpnmanager", 'M', "[off]            Monitor only VPN manager "
	 "interface"},
	{"vpnconnection", 'C', "[off]            Monitor only VPN "
	 "connections" },
	{ NULL, }
};

static struct connman_option session_options[] = {
	{"bearers", 'b', "<technology1> [<technology2> [...]]"},
	{"type", 't', "local|internet|any"},
	{ NULL, }
};

static char *lookup_options(struct connman_option *options, const char *text,
		int state)
{
	static int idx = 0;
	static int len = 0;
	const char *str;

	if (state == 0) {
		idx = 0;
		len = strlen(text);
	}

	while (options[idx].name) {
		str = options[idx].name;
		idx++;

		if (str && strncmp(text, str, len) == 0)
			return strdup(str);
	}

	return NULL;
}

static char *lookup_monitor(const char *text, int state)
{
	int level;

	level = __connmanctl_input_calc_level();

	if (level < 2)
		return lookup_options(monitor_options, text, state);

	if (level == 2)
		return lookup_on_off(text, state);

	__connmanctl_input_lookup_end();
	return NULL;
}

static char *lookup_config(const char *text, int state)
{
	if (__connmanctl_input_calc_level() < 2)
		return lookup_service(text, state);

	return lookup_options(config_options, text, state);
}

static char *lookup_session(const char *text, int state)
{
	return lookup_options(session_options, text, state);
}

static int peer_service_cb(DBusMessageIter *iter, const char *error,
							void *user_data)
{
	bool registration = GPOINTER_TO_INT(user_data);

	if (error)
		fprintf(stderr, "Error %s peer service: %s\n",
			registration ? "registering" : "unregistering", error);
	else
		fprintf(stdout, "Peer service %s\n",
			registration ? "registered" : "unregistered");

	return 0;
}

struct _peer_service {
	unsigned char *bjr_query;
	int bjr_query_len;
	unsigned char *bjr_response;
	int bjr_response_len;
	unsigned char *wfd_ies;
	int wfd_ies_len;
	char *upnp_service;
	int version;
	int master;
};

static void append_dict_entry_fixed_array(DBusMessageIter *iter,
			const char *property, void *value, int length)
{
	DBusMessageIter dict_entry, variant, array;

	dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY,
							NULL, &dict_entry);
	dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING,
								&property);
	dbus_message_iter_open_container(&dict_entry, DBUS_TYPE_VARIANT,
			DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING,
			&variant);
	dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);
	dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
							value, length);
	dbus_message_iter_close_container(&variant, &array);
	dbus_message_iter_close_container(&dict_entry, &variant);
	dbus_message_iter_close_container(iter, &dict_entry);
}

static void append_peer_service_dict(DBusMessageIter *iter, void *user_data)
{
	struct _peer_service *service = user_data;

	if (service->bjr_query && service->bjr_response) {
		append_dict_entry_fixed_array(iter, "BonjourQuery",
			&service->bjr_query, service->bjr_query_len);
		append_dict_entry_fixed_array(iter, "BonjourResponse",
			&service->bjr_response, service->bjr_response_len);
	} else if (service->upnp_service && service->version) {
		__connmanctl_dbus_append_dict_entry(iter, "UpnpVersion",
					DBUS_TYPE_INT32, &service->version);
		__connmanctl_dbus_append_dict_entry(iter, "UpnpService",
				DBUS_TYPE_STRING, &service->upnp_service);
	} else if (service->wfd_ies) {
		append_dict_entry_fixed_array(iter, "WiFiDisplayIEs",
				&service->wfd_ies, service->wfd_ies_len);
	}
}

static void peer_service_append(DBusMessageIter *iter, void *user_data)
{
	struct _peer_service *service = user_data;
	dbus_bool_t master;

	__connmanctl_dbus_append_dict(iter, append_peer_service_dict, service);

	if (service->master < 0)
		return;

	master = service->master == 1 ? TRUE : FALSE;
	dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &master);
}

static struct _peer_service *fill_in_peer_service(unsigned char *bjr_query,
				int bjr_query_len, unsigned char *bjr_response,
				int bjr_response_len, char *upnp_service,
				int version, unsigned char *wfd_ies,
				int wfd_ies_len)
{
	struct _peer_service *service;

	service = dbus_malloc0(sizeof(*service));

	if (bjr_query_len && bjr_response_len) {
		service->bjr_query = dbus_malloc0(bjr_query_len);
		memcpy(service->bjr_query, bjr_query, bjr_query_len);
		service->bjr_query_len = bjr_query_len;

		service->bjr_response = dbus_malloc0(bjr_response_len);
		memcpy(service->bjr_response, bjr_response, bjr_response_len);
		service->bjr_response_len = bjr_response_len;
	} else if (upnp_service && version) {
		service->upnp_service = strdup(upnp_service);
		service->version = version;
	} else if (wfd_ies && wfd_ies_len) {
		service->wfd_ies = dbus_malloc0(wfd_ies_len);
		memcpy(service->wfd_ies, wfd_ies, wfd_ies_len);
		service->wfd_ies_len = wfd_ies_len;
	} else {
		dbus_free(service);
		service = NULL;
	}

	return service;
}

static void free_peer_service(struct _peer_service *service)
{
	dbus_free(service->bjr_query);
	dbus_free(service->bjr_response);
	dbus_free(service->wfd_ies);
	free(service->upnp_service);
	dbus_free(service);
}

static int peer_service_register(unsigned char *bjr_query, int bjr_query_len,
			unsigned char *bjr_response, int bjr_response_len,
			char *upnp_service, int version,
			unsigned char *wfd_ies, int wfd_ies_len, int master)
{
	struct _peer_service *service;
	bool registration = true;
	int ret;

	service = fill_in_peer_service(bjr_query, bjr_query_len, bjr_response,
				bjr_response_len, upnp_service, version,
				wfd_ies, wfd_ies_len);
	if (!service)
		return -EINVAL;

	service->master = master;

	ret = __connmanctl_dbus_method_call(connection, "net.connman", "/",
			"net.connman.Manager", "RegisterPeerService",
			peer_service_cb, GINT_TO_POINTER(registration),
			peer_service_append, service);

	free_peer_service(service);

	return ret;
}

static int peer_service_unregister(unsigned char *bjr_query, int bjr_query_len,
			unsigned char *bjr_response, int bjr_response_len,
			char *upnp_service, int version,
			unsigned char *wfd_ies, int wfd_ies_len)
{
	struct _peer_service *service;
	bool registration = false;
	int ret;

	service = fill_in_peer_service(bjr_query, bjr_query_len, bjr_response,
				bjr_response_len, upnp_service, version,
				wfd_ies, wfd_ies_len);
	if (!service)
		return -EINVAL;

	service->master = -1;

	ret = __connmanctl_dbus_method_call(connection, "net.connman", "/",
			"net.connman.Manager", "UnregisterPeerService",
			peer_service_cb, GINT_TO_POINTER(registration),
			peer_service_append, service);

	free_peer_service(service);

	return ret;
}

static int parse_spec_array(char *command, unsigned char spec[1024])
{
	int length, pos, end;
	char b[3] = {};
	char *e;

	end = strlen(command);
	for (e = NULL, length = pos = 0; command[pos] != '\0'; length++) {
		if (pos+2 > end)
			return -EINVAL;

		b[0] = command[pos];
		b[1] = command[pos+1];

		spec[length] = strtol(b, &e, 16);
		if (e && *e != '\0')
			return -EINVAL;

		pos += 2;
	}

	return length;
}

static int cmd_peer_service(char *args[], int num,
				struct connman_option *options)
{
	unsigned char bjr_query[1024] = {};
	unsigned char bjr_response[1024] = {};
	unsigned char wfd_ies[1024] = {};
	char *upnp_service = NULL;
	int bjr_query_len = 0, bjr_response_len = 0;
	int version = 0, master = 0, wfd_ies_len = 0;
	int limit;

	if (num < 4)
		return -EINVAL;

	if (!strcmp(args[2], "wfd_ies")) {
		wfd_ies_len = parse_spec_array(args[3], wfd_ies);
		if (wfd_ies_len == -EINVAL)
			return -EINVAL;
		limit = 5;
		goto master;
	}

	if (num < 6)
		return -EINVAL;

	limit = 7;
	if (!strcmp(args[2], "bjr_query")) {
		if (strcmp(args[4], "bjr_response"))
			return -EINVAL;
		bjr_query_len = parse_spec_array(args[3], bjr_query);
		bjr_response_len = parse_spec_array(args[5], bjr_response);

		if (bjr_query_len == -EINVAL || bjr_response_len == -EINVAL)
			return -EINVAL;
	} else if (!strcmp(args[2], "upnp_service")) {
		char *e = NULL;

		if (strcmp(args[4], "upnp_version"))
			return -EINVAL;
		upnp_service = args[3];
		version = strtol(args[5], &e, 10);
		if (*e != '\0')
			return -EINVAL;
	}

master:
	if (num == limit) {
		master = parse_boolean(args[6]);
		if (master < 0)
			return -EINVAL;
	}

	if (!strcmp(args[1], "register")) {
		return peer_service_register(bjr_query, bjr_query_len,
				bjr_response, bjr_response_len, upnp_service,
				version, wfd_ies, wfd_ies_len, master);
	} else if (!strcmp(args[1], "unregister")) {
		return peer_service_unregister(bjr_query, bjr_query_len,
				bjr_response, bjr_response_len, upnp_service,
				version, wfd_ies, wfd_ies_len);
	}

	return -EINVAL;
}

static const struct {
        const char *cmd;
	const char *argument;
        struct connman_option *options;
        int (*func) (char *args[], int num, struct connman_option *options);
        const char *desc;
	__connmanctl_lookup_cb cb;
} cmd_table[] = {
	{ "state",        NULL,           NULL,            cmd_state,
	  "Shows if the system is online or offline", NULL },
	{ "technologies", NULL,           NULL,            cmd_technologies,
	  "Display technologies", NULL },
	{ "enable",       "<technology>|offline", NULL,    cmd_enable,
	  "Enables given technology or offline mode",
	  lookup_technology_offline },
	{ "disable",      "<technology>|offline", NULL,    cmd_disable,
	  "Disables given technology or offline mode",
	  lookup_technology_offline },
	{ "tether", "<technology> on|off\n"
	            "            wifi [on|off] <ssid> <passphrase> ",
	                                  NULL,            cmd_tether,
	  "Enable, disable tethering, set SSID and passphrase for wifi",
	  lookup_tether },
	{ "services",     "[<service>]",  service_options, cmd_services,
	  "Display services", lookup_service_arg },
	{ "peers",        "[peer]",       NULL,            cmd_peers,
	  "Display peers", lookup_peer_arg },
	{ "scan",         "<technology>", NULL,            cmd_scan,
	  "Scans for new services for given technology",
	  lookup_technology_arg },
	{ "connect",      "<service/peer>", NULL,          cmd_connect,
	  "Connect a given service or peer", lookup_service_arg },
	{ "disconnect",   "<service/peer>", NULL,          cmd_disconnect,
	  "Disconnect a given service or peer", lookup_service_arg },
	{ "config",       "<service>",    config_options,  cmd_config,
	  "Set service configuration options", lookup_config },
	{ "monitor",      "[off]",        monitor_options, cmd_monitor,
	  "Monitor signals from interfaces", lookup_monitor },
	{ "agent", "on|off",              NULL,            cmd_agent,
	  "Agent mode", lookup_agent },
	{"vpnconnections", "[<connection>]", NULL,         cmd_vpnconnections,
	 "Display VPN connections", NULL },
	{ "vpnagent",     "on|off",     NULL,            cmd_vpnagent,
	  "VPN Agent mode", lookup_agent },
	{ "session",      "on|off|connect|disconnect|config", session_options,
	  cmd_session, "Enable or disable a session", lookup_session },
	{ "peer_service", "register|unregister <specs> <master>\n"
			  "Where specs are:\n"
			  "\tbjr_query <query> bjr_response <response>\n"
			  "\tupnp_service <service> upnp_version <version>\n"
			  "\twfd_ies <ies>\n", NULL,
	  cmd_peer_service, "(Un)Register a Peer Service", NULL },
	{ "help",         NULL,           NULL,            cmd_help,
	  "Show help", NULL },
	{ "exit",         NULL,           NULL,            cmd_exit,
	  "Exit", NULL },
	{ "quit",         NULL,           NULL,            cmd_exit,
	  "Quit", NULL },
	{  NULL, },
};

static int cmd_help(char *args[], int num, struct connman_option *options)
{
	bool interactive = __connmanctl_is_interactive();
	int i, j;

	if (interactive == false)
		fprintf(stdout, "Usage: connmanctl [[command] [args]]\n");

	for (i = 0; cmd_table[i].cmd; i++) {
		const char *cmd = cmd_table[i].cmd;
		const char *argument = cmd_table[i].argument;
		const char *desc = cmd_table[i].desc;

		printf("%-16s%-22s%s\n", cmd? cmd: "",
				argument? argument: "",
				desc? desc: "");

		if (cmd_table[i].options) {
			for (j = 0; cmd_table[i].options[j].name;
			     j++) {
				const char *options_desc =
					cmd_table[i].options[j].desc ?
					cmd_table[i].options[j].desc: "";

				printf("   --%-16s%s\n",
						cmd_table[i].options[j].name,
						options_desc);
			}
		}
	}

	if (interactive == false)
		fprintf(stdout, "\nNote: arguments and output are considered "
				"EXPERIMENTAL for now.\n");

	return 0;
}

__connmanctl_lookup_cb __connmanctl_get_lookup_func(const char *text)
{
	int i, cmdlen, textlen;

	if (!text)
		return NULL;

	textlen = strlen(text);

	for (i = 0; cmd_table[i].cmd; i++) {
		cmdlen = strlen(cmd_table[i].cmd);

		if (textlen > cmdlen && text[cmdlen] != ' ')
			continue;

		if (strncmp(cmd_table[i].cmd, text, cmdlen) == 0)
			return cmd_table[i].cb;
	}

	return NULL;
}

int __connmanctl_commands(DBusConnection *dbus_conn, char *argv[], int argc)
{
	int i, result;

	connection = dbus_conn;

	for (i = 0; cmd_table[i].cmd; i++) {
		if (g_strcmp0(cmd_table[i].cmd, argv[0]) == 0 &&
				cmd_table[i].func) {
			result = cmd_table[i].func(argv, argc,
					cmd_table[i].options);
			if (result < 0 && result != -EINPROGRESS)
				fprintf(stderr, "Error '%s': %s\n", argv[0],
						strerror(-result));
			return result;
		}
	}

	fprintf(stderr, "Error '%s': Unknown command\n", argv[0]);
	return -EINVAL;
}

char *__connmanctl_lookup_command(const char *text, int state)
{
	static int i = 0;
	static int len = 0;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while (cmd_table[i].cmd) {
		const char *command = cmd_table[i].cmd;

		i++;

		if (strncmp(text, command, len) == 0)
			return strdup(command);
	}

	return NULL;
}

static char *get_path(char *full_path)
{
	char *path;

	path = strrchr(full_path, '/');
	if (path && *path != '\0')
		path++;
	else
		path = full_path;

	return path;
}

static void add_service_id(const char *path)
{
	g_hash_table_replace(service_hash, g_strdup(path),
			GINT_TO_POINTER(TRUE));
}

static void remove_service_id(const char *path)
{
	g_hash_table_remove(service_hash, path);
}

static void services_added(DBusMessageIter *iter)
{
	DBusMessageIter array;
	char *path = NULL;

	while (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRUCT) {

		dbus_message_iter_recurse(iter, &array);
		if (dbus_message_iter_get_arg_type(&array) !=
						DBUS_TYPE_OBJECT_PATH)
			return;

		dbus_message_iter_get_basic(&array, &path);
		add_service_id(get_path(path));

		dbus_message_iter_next(iter);
	}
}

static void update_services(DBusMessageIter *iter)
{
	DBusMessageIter array;
	char *path;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &array);
	services_added(&array);

	dbus_message_iter_next(iter);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &array);
	while (dbus_message_iter_get_arg_type(&array) ==
						DBUS_TYPE_OBJECT_PATH) {
		dbus_message_iter_get_basic(&array, &path);
		remove_service_id(get_path(path));

		dbus_message_iter_next(&array);
	}
}

static int populate_service_hash(DBusMessageIter *iter, const char *error,
				void *user_data)
{
	if (error) {
		fprintf(stderr, "Error getting services: %s", error);
		return 0;
	}

	update_services(iter);
	return 0;
}

static void add_peer_id(const char *path)
{
	g_hash_table_replace(peer_hash, g_strdup(path),	GINT_TO_POINTER(TRUE));
}

static void remove_peer_id(const char *path)
{
	g_hash_table_remove(peer_hash, path);
}

static void peers_added(DBusMessageIter *iter)
{
	DBusMessageIter array;
	char *path = NULL;

	while (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRUCT) {

		dbus_message_iter_recurse(iter, &array);
		if (dbus_message_iter_get_arg_type(&array) !=
						DBUS_TYPE_OBJECT_PATH)
			return;

		dbus_message_iter_get_basic(&array, &path);
		add_peer_id(get_path(path));

		dbus_message_iter_next(iter);
	}
}

static void update_peers(DBusMessageIter *iter)
{
	DBusMessageIter array;
	char *path;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &array);
	peers_added(&array);

	dbus_message_iter_next(iter);
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &array);
	while (dbus_message_iter_get_arg_type(&array) ==
						DBUS_TYPE_OBJECT_PATH) {
		dbus_message_iter_get_basic(&array, &path);
		remove_peer_id(get_path(path));

		dbus_message_iter_next(&array);
	}
}

static int populate_peer_hash(DBusMessageIter *iter,
					const char *error, void *user_data)
{
	if (error) {
		fprintf(stderr, "Error getting peers: %s", error);
		return 0;
	}

	update_peers(iter);
	return 0;
}

static void add_technology_id(const char *path)
{
	g_hash_table_replace(technology_hash, g_strdup(path),
			GINT_TO_POINTER(TRUE));
}

static void remove_technology_id(const char *path)
{
	g_hash_table_remove(technology_hash, path);
}

static void remove_technology(DBusMessageIter *iter)
{
	char *path = NULL;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_OBJECT_PATH)
		return;

	dbus_message_iter_get_basic(iter, &path);
	remove_technology_id(get_path(path));
}

static void add_technology(DBusMessageIter *iter)
{
	char *path = NULL;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_OBJECT_PATH)
		return;

	dbus_message_iter_get_basic(iter, &path);
	add_technology_id(get_path(path));
}

static void update_technologies(DBusMessageIter *iter)
{
	DBusMessageIter array;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &array);

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
		DBusMessageIter object_path;

		dbus_message_iter_recurse(&array, &object_path);

		add_technology(&object_path);

		dbus_message_iter_next(&array);
	}
}

static int populate_technology_hash(DBusMessageIter *iter, const char *error,
				void *user_data)
{
	if (error) {
		fprintf(stderr, "Error getting technologies: %s", error);
		return 0;
	}

	update_technologies(iter);

	return 0;
}

static DBusHandlerResult monitor_completions_changed(
		DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	bool *enabled = user_data;
	DBusMessageIter iter;
	DBusHandlerResult handled;

	if (*enabled)
		handled = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	else
		handled = DBUS_HANDLER_RESULT_HANDLED;

	if (dbus_message_is_signal(message, "net.connman.Manager",
					"ServicesChanged")) {
		dbus_message_iter_init(message, &iter);
		update_services(&iter);
		return handled;
	}

	if (dbus_message_is_signal(message, "net.connman.Manager",
						"PeersChanged")) {
		dbus_message_iter_init(message, &iter);
		update_peers(&iter);
		return handled;
	}

	if (dbus_message_is_signal(message, "net.connman.Manager",
					"TechnologyAdded")) {
		dbus_message_iter_init(message, &iter);
		add_technology(&iter);
		return handled;
	}

	if (dbus_message_is_signal(message, "net.connman.Manager",
					"TechnologyRemoved")) {
		dbus_message_iter_init(message, &iter);
		remove_technology(&iter);
		return handled;
	}

	if (!g_strcmp0(dbus_message_get_interface(message),
					"net.connman.Manager"))
		return handled;

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void __connmanctl_monitor_completions(DBusConnection *dbus_conn)
{
	bool *manager_enabled = NULL;
	DBusError err;
	int i;

	for (i = 0; monitor[i].interface; i++) {
		if (!strcmp(monitor[i].interface, "Manager")) {
			manager_enabled = &monitor[i].enabled;
			break;
		}
	}

	if (!dbus_conn) {
		g_hash_table_destroy(service_hash);
		g_hash_table_destroy(technology_hash);

		dbus_bus_remove_match(connection,
			"type='signal',interface='net.connman.Manager'", NULL);
		dbus_connection_remove_filter(connection,
					monitor_completions_changed,
					manager_enabled);
		return;
	}

	connection = dbus_conn;

	service_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
								g_free, NULL);

	peer_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
								g_free, NULL);

	technology_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
								g_free, NULL);

	__connmanctl_dbus_method_call(connection,
				CONNMAN_SERVICE, CONNMAN_PATH,
				"net.connman.Manager", "GetServices",
				populate_service_hash, NULL, NULL, NULL);

	__connmanctl_dbus_method_call(connection,
				CONNMAN_SERVICE, CONNMAN_PATH,
				"net.connman.Manager", "GetPeers",
				populate_peer_hash, NULL, NULL, NULL);

	__connmanctl_dbus_method_call(connection,
				CONNMAN_SERVICE, CONNMAN_PATH,
				"net.connman.Manager", "GetTechnologies",
				populate_technology_hash, NULL, NULL, NULL);

	dbus_connection_add_filter(connection,
				monitor_completions_changed, manager_enabled,
			NULL);

	dbus_error_init(&err);
	dbus_bus_add_match(connection,
			"type='signal',interface='net.connman.Manager'", &err);

	if (dbus_error_is_set(&err))
		fprintf(stderr, "Error: %s\n", err.message);
}
