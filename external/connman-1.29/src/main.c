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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <net/if.h>
#include <netdb.h>

#include <gdbus.h>

#include "connman.h"

#define DEFAULT_INPUT_REQUEST_TIMEOUT (120 * 1000)
#define DEFAULT_BROWSER_LAUNCH_TIMEOUT (300 * 1000)

#define MAINFILE "main.conf"
#define CONFIGMAINFILE CONFIGDIR "/" MAINFILE

static char *default_auto_connect[] = {
	"wifi",
	"ethernet",
	"cellular",
	NULL
};

static char *default_blacklist[] = {
	"vmnet",
	"vboxnet",
	"virbr",
	"ifb",
	NULL
};

static struct {
	bool bg_scan;
	char **pref_timeservers;
	unsigned int *auto_connect;
	unsigned int *preferred_techs;
	char **fallback_nameservers;
	unsigned int timeout_inputreq;
	unsigned int timeout_browserlaunch;
	char **blacklisted_interfaces;
	bool allow_hostname_updates;
	bool single_tech;
	char **tethering_technologies;
	bool persistent_tethering_mode;
} connman_settings  = {
	.bg_scan = true,
	.pref_timeservers = NULL,
	.auto_connect = NULL,
	.preferred_techs = NULL,
	.fallback_nameservers = NULL,
	.timeout_inputreq = DEFAULT_INPUT_REQUEST_TIMEOUT,
	.timeout_browserlaunch = DEFAULT_BROWSER_LAUNCH_TIMEOUT,
	.blacklisted_interfaces = NULL,
	.allow_hostname_updates = true,
	.single_tech = false,
	.tethering_technologies = NULL,
	.persistent_tethering_mode = false,
};

#define CONF_BG_SCAN                    "BackgroundScanning"
#define CONF_PREF_TIMESERVERS           "FallbackTimeservers"
#define CONF_AUTO_CONNECT               "DefaultAutoConnectTechnologies"
#define CONF_PREFERRED_TECHS            "PreferredTechnologies"
#define CONF_FALLBACK_NAMESERVERS       "FallbackNameservers"
#define CONF_TIMEOUT_INPUTREQ           "InputRequestTimeout"
#define CONF_TIMEOUT_BROWSERLAUNCH      "BrowserLaunchTimeout"
#define CONF_BLACKLISTED_INTERFACES     "NetworkInterfaceBlacklist"
#define CONF_ALLOW_HOSTNAME_UPDATES     "AllowHostnameUpdates"
#define CONF_SINGLE_TECH                "SingleConnectedTechnology"
#define CONF_TETHERING_TECHNOLOGIES      "TetheringTechnologies"
#define CONF_PERSISTENT_TETHERING_MODE  "PersistentTetheringMode"

static const char *supported_options[] = {
	CONF_BG_SCAN,
	CONF_PREF_TIMESERVERS,
	CONF_AUTO_CONNECT,
	CONF_PREFERRED_TECHS,
	CONF_FALLBACK_NAMESERVERS,
	CONF_TIMEOUT_INPUTREQ,
	CONF_TIMEOUT_BROWSERLAUNCH,
	CONF_BLACKLISTED_INTERFACES,
	CONF_ALLOW_HOSTNAME_UPDATES,
	CONF_SINGLE_TECH,
	CONF_TETHERING_TECHNOLOGIES,
	CONF_PERSISTENT_TETHERING_MODE,
	NULL
};

static GKeyFile *load_config(const char *file)
{
	GError *err = NULL;
	GKeyFile *keyfile;

	keyfile = g_key_file_new();

	g_key_file_set_list_separator(keyfile, ',');

	if (!g_key_file_load_from_file(keyfile, file, 0, &err)) {
		if (err->code != G_FILE_ERROR_NOENT) {
			connman_error("Parsing %s failed: %s", file,
								err->message);
		}

		g_error_free(err);
		g_key_file_free(keyfile);
		return NULL;
	}

	return keyfile;
}

static uint *parse_service_types(char **str_list, gsize len)
{
	unsigned int *type_list;
	int i, j;
	enum connman_service_type type;

	type_list = g_try_new0(unsigned int, len + 1);
	if (!type_list)
		return NULL;

	i = 0;
	j = 0;
	while (str_list[i]) {
		type = __connman_service_string2type(str_list[i]);

		if (type != CONNMAN_SERVICE_TYPE_UNKNOWN) {
			type_list[j] = type;
			j += 1;
		}
		i += 1;
	}

	type_list[j] = CONNMAN_SERVICE_TYPE_UNKNOWN;

	return type_list;
}

static char **parse_fallback_nameservers(char **nameservers, gsize len)
{
	char **servers;
	int i, j;

	servers = g_try_new0(char *, len + 1);
	if (!servers)
		return NULL;

	i = 0;
	j = 0;
	while (nameservers[i]) {
		if (connman_inet_check_ipaddress(nameservers[i]) > 0) {
			servers[j] = g_strdup(nameservers[i]);
			j += 1;
		}
		i += 1;
	}

	return servers;
}

static void check_config(GKeyFile *config)
{
	char **keys;
	int j;

	if (!config)
		return;

	keys = g_key_file_get_groups(config, NULL);

	for (j = 0; keys && keys[j]; j++) {
		if (g_strcmp0(keys[j], "General") != 0)
			connman_warn("Unknown group %s in %s",
						keys[j], MAINFILE);
	}

	g_strfreev(keys);

	keys = g_key_file_get_keys(config, "General", NULL, NULL);

	for (j = 0; keys && keys[j]; j++) {
		bool found;
		int i;

		found = false;
		for (i = 0; supported_options[i]; i++) {
			if (g_strcmp0(keys[j], supported_options[i]) == 0) {
				found = true;
				break;
			}
		}
		if (!found && !supported_options[i])
			connman_warn("Unknown option %s in %s",
						keys[j], MAINFILE);
	}

	g_strfreev(keys);
}

static void parse_config(GKeyFile *config)
{
	GError *error = NULL;
	bool boolean;
	char **timeservers;
	char **interfaces;
	char **str_list;
	char **tethering;
	gsize len;
	int timeout;

	if (!config) {
		connman_settings.auto_connect =
			parse_service_types(default_auto_connect, 3);
		connman_settings.blacklisted_interfaces =
			g_strdupv(default_blacklist);
		return;
	}

	DBG("parsing %s", MAINFILE);

	boolean = g_key_file_get_boolean(config, "General",
						CONF_BG_SCAN, &error);
	if (!error)
		connman_settings.bg_scan = boolean;

	g_clear_error(&error);

	timeservers = __connman_config_get_string_list(config, "General",
					CONF_PREF_TIMESERVERS, NULL, &error);
	if (!error)
		connman_settings.pref_timeservers = timeservers;

	g_clear_error(&error);

	str_list = __connman_config_get_string_list(config, "General",
			CONF_AUTO_CONNECT, &len, &error);

	if (!error)
		connman_settings.auto_connect =
			parse_service_types(str_list, len);
	else
		connman_settings.auto_connect =
			parse_service_types(default_auto_connect, 3);

	g_strfreev(str_list);

	g_clear_error(&error);

	str_list = __connman_config_get_string_list(config, "General",
			CONF_PREFERRED_TECHS, &len, &error);

	if (!error)
		connman_settings.preferred_techs =
			parse_service_types(str_list, len);

	g_strfreev(str_list);

	g_clear_error(&error);

	str_list = __connman_config_get_string_list(config, "General",
			CONF_FALLBACK_NAMESERVERS, &len, &error);

	if (!error)
		connman_settings.fallback_nameservers =
			parse_fallback_nameservers(str_list, len);

	g_strfreev(str_list);

	g_clear_error(&error);

	timeout = g_key_file_get_integer(config, "General",
			CONF_TIMEOUT_INPUTREQ, &error);
	if (!error && timeout >= 0)
		connman_settings.timeout_inputreq = timeout * 1000;

	g_clear_error(&error);

	timeout = g_key_file_get_integer(config, "General",
			CONF_TIMEOUT_BROWSERLAUNCH, &error);
	if (!error && timeout >= 0)
		connman_settings.timeout_browserlaunch = timeout * 1000;

	g_clear_error(&error);

	interfaces = __connman_config_get_string_list(config, "General",
			CONF_BLACKLISTED_INTERFACES, &len, &error);

	if (!error)
		connman_settings.blacklisted_interfaces = interfaces;
	else
		connman_settings.blacklisted_interfaces =
			g_strdupv(default_blacklist);

	g_clear_error(&error);

	boolean = __connman_config_get_bool(config, "General",
					CONF_ALLOW_HOSTNAME_UPDATES,
					&error);
	if (!error)
		connman_settings.allow_hostname_updates = boolean;

	g_clear_error(&error);

	boolean = __connman_config_get_bool(config, "General",
			CONF_SINGLE_TECH, &error);
	if (!error)
		connman_settings.single_tech = boolean;

	g_clear_error(&error);

	tethering = __connman_config_get_string_list(config, "General",
			CONF_TETHERING_TECHNOLOGIES, &len, &error);

	if (!error)
		connman_settings.tethering_technologies = tethering;

	g_clear_error(&error);

	boolean = __connman_config_get_bool(config, "General",
					CONF_PERSISTENT_TETHERING_MODE,
					&error);
	if (!error)
		connman_settings.persistent_tethering_mode = boolean;

	g_clear_error(&error);
}

static int config_init(const char *file)
{
	GKeyFile *config;

	config = load_config(file);
	check_config(config);
	parse_config(config);
	if (config)
		g_key_file_free(config);

	return 0;
}

static GMainLoop *main_loop = NULL;

static unsigned int __terminated = 0;

static gboolean signal_handler(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	struct signalfd_siginfo si;
	ssize_t result;
	int fd;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP))
		return FALSE;

	fd = g_io_channel_unix_get_fd(channel);

	result = read(fd, &si, sizeof(si));
	if (result != sizeof(si))
		return FALSE;

	switch (si.ssi_signo) {
	case SIGINT:
	case SIGTERM:
		if (__terminated == 0) {
			connman_info("Terminating");
			g_main_loop_quit(main_loop);
		}

		__terminated = 1;
		break;
	}

	return TRUE;
}

static guint setup_signalfd(void)
{
	GIOChannel *channel;
	guint source;
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
		perror("Failed to set signal mask");
		return 0;
	}

	fd = signalfd(-1, &mask, 0);
	if (fd < 0) {
		perror("Failed to create signal descriptor");
		return 0;
	}

	channel = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(channel, TRUE);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	source = g_io_add_watch(channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				signal_handler, NULL);

	g_io_channel_unref(channel);

	return source;
}

static void disconnect_callback(DBusConnection *conn, void *user_data)
{
	connman_error("D-Bus disconnect");

	g_main_loop_quit(main_loop);
}

static gchar *option_config = NULL;
static gchar *option_debug = NULL;
static gchar *option_device = NULL;
static gchar *option_plugin = NULL;
static gchar *option_nodevice = NULL;
static gchar *option_noplugin = NULL;
static gchar *option_wifi = NULL;
static gboolean option_detach = TRUE;
static gboolean option_dnsproxy = TRUE;
static gboolean option_backtrace = TRUE;
static gboolean option_version = FALSE;

static bool parse_debug(const char *key, const char *value,
					gpointer user_data, GError **error)
{
	if (value)
		option_debug = g_strdup(value);
	else
		option_debug = g_strdup("*");

	return true;
}

static GOptionEntry options[] = {
	{ "config", 'c', 0, G_OPTION_ARG_STRING, &option_config,
				"Load the specified configuration file "
				"instead of " CONFIGMAINFILE, "FILE" },
	{ "debug", 'd', G_OPTION_FLAG_OPTIONAL_ARG,
				G_OPTION_ARG_CALLBACK, parse_debug,
				"Specify debug options to enable", "DEBUG" },
	{ "device", 'i', 0, G_OPTION_ARG_STRING, &option_device,
			"Specify networking device or interface", "DEV" },
	{ "nodevice", 'I', 0, G_OPTION_ARG_STRING, &option_nodevice,
			"Specify networking interface to ignore", "DEV" },
	{ "plugin", 'p', 0, G_OPTION_ARG_STRING, &option_plugin,
				"Specify plugins to load", "NAME,..." },
	{ "noplugin", 'P', 0, G_OPTION_ARG_STRING, &option_noplugin,
				"Specify plugins not to load", "NAME,..." },
	{ "wifi", 'W', 0, G_OPTION_ARG_STRING, &option_wifi,
				"Specify driver for WiFi/Supplicant", "NAME" },
	{ "nodaemon", 'n', G_OPTION_FLAG_REVERSE,
				G_OPTION_ARG_NONE, &option_detach,
				"Don't fork daemon to background" },
	{ "nodnsproxy", 'r', G_OPTION_FLAG_REVERSE,
				G_OPTION_ARG_NONE, &option_dnsproxy,
				"Don't enable DNS Proxy" },
	{ "nobacktrace", 0, G_OPTION_FLAG_REVERSE,
				G_OPTION_ARG_NONE, &option_backtrace,
				"Don't print out backtrace information" },
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &option_version,
				"Show version information and exit" },
	{ NULL },
};

const char *connman_option_get_string(const char *key)
{
	if (g_strcmp0(key, "wifi") == 0) {
		if (!option_wifi)
			return "nl80211,wext";
		else
			return option_wifi;
	}

	return NULL;
}

bool connman_setting_get_bool(const char *key)
{
	if (g_str_equal(key, CONF_BG_SCAN))
		return connman_settings.bg_scan;

	if (g_str_equal(key, CONF_ALLOW_HOSTNAME_UPDATES))
		return connman_settings.allow_hostname_updates;

	if (g_str_equal(key, CONF_SINGLE_TECH))
		return connman_settings.single_tech;

	if (g_str_equal(key, CONF_PERSISTENT_TETHERING_MODE))
		return connman_settings.persistent_tethering_mode;

	return false;
}

char **connman_setting_get_string_list(const char *key)
{
	if (g_str_equal(key, CONF_PREF_TIMESERVERS))
		return connman_settings.pref_timeservers;

	if (g_str_equal(key, CONF_FALLBACK_NAMESERVERS))
		return connman_settings.fallback_nameservers;

	if (g_str_equal(key, CONF_BLACKLISTED_INTERFACES))
		return connman_settings.blacklisted_interfaces;

	if (g_str_equal(key, CONF_TETHERING_TECHNOLOGIES))
		return connman_settings.tethering_technologies;

	return NULL;
}

unsigned int *connman_setting_get_uint_list(const char *key)
{
	if (g_str_equal(key, CONF_AUTO_CONNECT))
		return connman_settings.auto_connect;

	if (g_str_equal(key, CONF_PREFERRED_TECHS))
		return connman_settings.preferred_techs;

	return NULL;
}

unsigned int connman_timeout_input_request(void)
{
	return connman_settings.timeout_inputreq;
}

unsigned int connman_timeout_browser_launch(void)
{
	return connman_settings.timeout_browserlaunch;
}

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	DBusConnection *conn;
	DBusError err;
	guint signal;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");
		exit(1);
	}

	g_option_context_free(context);

	if (option_version) {
		printf("%s\n", VERSION);
		exit(0);
	}

	if (option_detach) {
		if (daemon(0, 0)) {
			perror("Can't start daemon");
			exit(1);
		}
	}

	if (mkdir(STORAGEDIR, S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0) {
		if (errno != EEXIST)
			perror("Failed to create storage directory");
	}

	umask(0077);

	main_loop = g_main_loop_new(NULL, FALSE);

	signal = setup_signalfd();

	dbus_error_init(&err);

	conn = g_dbus_setup_bus(DBUS_BUS_SYSTEM, CONNMAN_SERVICE, &err);
	if (!conn) {
		if (dbus_error_is_set(&err)) {
			fprintf(stderr, "%s\n", err.message);
			dbus_error_free(&err);
		} else
			fprintf(stderr, "Can't register with system bus\n");
		exit(1);
	}

	g_dbus_set_disconnect_function(conn, disconnect_callback, NULL, NULL);

	__connman_log_init(argv[0], option_debug, option_detach,
			option_backtrace, "Connection Manager", VERSION);

	__connman_dbus_init(conn);

	if (!option_config)
		config_init(CONFIGMAINFILE);
	else
		config_init(option_config);

	__connman_util_init();
	__connman_inotify_init();
	__connman_technology_init();
	__connman_notifier_init();
	__connman_agent_init();
	__connman_service_init();
	__connman_peer_service_init();
	__connman_peer_init();
	__connman_provider_init();
	__connman_network_init();
	__connman_config_init();
	__connman_device_init(option_device, option_nodevice);

	__connman_ippool_init();
	__connman_iptables_init();
	__connman_firewall_init();
	__connman_nat_init();
	__connman_tethering_init();
	__connman_counter_init();
	__connman_manager_init();
	__connman_stats_init();
	__connman_clock_init();

	__connman_resolver_init(option_dnsproxy);
	__connman_ipconfig_init();
	__connman_rtnl_init();
	__connman_task_init();
	__connman_proxy_init();
	__connman_detect_init();
	__connman_session_init();
	__connman_timeserver_init();
	__connman_connection_init();

	__connman_plugin_init(option_plugin, option_noplugin);

	__connman_rtnl_start();
	__connman_dhcp_init();
	__connman_dhcpv6_init();
	__connman_wpad_init();
	__connman_wispr_init();
	__connman_rfkill_init();
	__connman_machine_init();

	g_free(option_config);
	g_free(option_device);
	g_free(option_plugin);
	g_free(option_nodevice);
	g_free(option_noplugin);

	g_main_loop_run(main_loop);

	g_source_remove(signal);

	__connman_machine_cleanup();
	__connman_rfkill_cleanup();
	__connman_wispr_cleanup();
	__connman_wpad_cleanup();
	__connman_dhcpv6_cleanup();
	__connman_session_cleanup();
	__connman_plugin_cleanup();
	__connman_provider_cleanup();
	__connman_connection_cleanup();
	__connman_timeserver_cleanup();
	__connman_detect_cleanup();
	__connman_proxy_cleanup();
	__connman_task_cleanup();
	__connman_rtnl_cleanup();
	__connman_resolver_cleanup();

	__connman_clock_cleanup();
	__connman_stats_cleanup();
	__connman_config_cleanup();
	__connman_manager_cleanup();
	__connman_counter_cleanup();
	__connman_tethering_cleanup();
	__connman_nat_cleanup();
	__connman_firewall_cleanup();
	__connman_iptables_cleanup();
	__connman_peer_service_cleanup();
	__connman_peer_cleanup();
	__connman_ippool_cleanup();
	__connman_device_cleanup();
	__connman_network_cleanup();
	__connman_dhcp_cleanup();
	__connman_service_cleanup();
	__connman_agent_cleanup();
	__connman_ipconfig_cleanup();
	__connman_notifier_cleanup();
	__connman_technology_cleanup();
	__connman_inotify_cleanup();

	__connman_util_cleanup();
	__connman_dbus_cleanup();

	__connman_log_cleanup(option_backtrace);

	dbus_connection_unref(conn);

	g_main_loop_unref(main_loop);

	if (connman_settings.pref_timeservers)
		g_strfreev(connman_settings.pref_timeservers);

	g_free(connman_settings.auto_connect);
	g_free(connman_settings.preferred_techs);
	g_strfreev(connman_settings.fallback_nameservers);
	g_strfreev(connman_settings.blacklisted_interfaces);
	g_strfreev(connman_settings.tethering_technologies);

	g_free(option_debug);
	g_free(option_wifi);

	return 0;
}
