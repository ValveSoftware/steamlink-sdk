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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>

#include <gdbus.h>

#include "supplicant.h"

#define DBG(fmt, arg...) do { \
	syslog(LOG_DEBUG, "%s() " fmt, __FUNCTION__ , ## arg); \
} while (0)

static void create_callback(int result, struct supplicant_interface *interface,
							void *user_data)
{
	DBG("* result %d ifname %s", result,
				supplicant_interface_get_ifname(interface));

	if (result < 0)
		return;

	//supplicant_set_debug_level(1);
}

static void system_ready(void)
{
	DBG("*");

	supplicant_interface_create("wlan0", "nl80211,wext",
						create_callback, NULL);
}

static void system_killed(void)
{
	DBG("*");
}

static void scan_callback(int result, void *user_data)
{
	DBG("* result %d", result);

	if (result < 0)
		return;
}

static void interface_added(struct supplicant_interface *interface)
{
	const char *ifname = supplicant_interface_get_ifname(interface);
	const char *driver = supplicant_interface_get_driver(interface);

	DBG("* ifname %s driver %s", ifname, driver);

	if (supplicant_interface_scan(interface, scan_callback, NULL) < 0)
		DBG("scan failed");
}

static void interface_removed(struct supplicant_interface *interface)
{
	const char *ifname = supplicant_interface_get_ifname(interface);

	DBG("* ifname %s", ifname);
}

static void scan_started(struct supplicant_interface *interface)
{
	const char *ifname = supplicant_interface_get_ifname(interface);

	DBG("* ifname %s", ifname);
}

static void scan_finished(struct supplicant_interface *interface)
{
	const char *ifname = supplicant_interface_get_ifname(interface);

	DBG("* ifname %s", ifname);
}

static void network_added(struct supplicant_network *network)
{
	const char *name = supplicant_network_get_name(network);

	DBG("* name %s", name);

	DBG("* %s", supplicant_network_get_identifier(network));
}

static void network_removed(struct supplicant_network *network)
{
	const char *name = supplicant_network_get_name(network);

	DBG("* name %s", name);
}

static const struct supplicant_callbacks callbacks = {
	.system_ready		= system_ready,
	.system_killed		= system_killed,
	.interface_added	= interface_added,
	.interface_removed	= interface_removed,
	.scan_started		= scan_started,
	.scan_finished		= scan_finished,
	.network_added		= network_added,
	.network_removed	= network_removed,
};

static GMainLoop *main_loop = NULL;

static void sig_term(int sig)
{
	syslog(LOG_INFO, "Terminating");

	g_main_loop_quit(main_loop);
}

static void disconnect_callback(DBusConnection *conn, void *user_data)
{
	syslog(LOG_ERR, "D-Bus disconnect");

	g_main_loop_quit(main_loop);
}

int main(int argc, char *argv[])
{
	DBusConnection *conn;
	DBusError err;
	struct sigaction sa;

	main_loop = g_main_loop_new(NULL, FALSE);

	dbus_error_init(&err);

	conn = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, &err);
	if (!conn) {
		if (dbus_error_is_set(&err)) {
			fprintf(stderr, "%s\n", err.message);
			dbus_error_free(&err);
		} else
			fprintf(stderr, "Can't register with system bus\n");
		exit(1);
	}

	openlog("supplicant", LOG_NDELAY | LOG_PERROR, LOG_USER);

	g_dbus_set_disconnect_function(conn, disconnect_callback, NULL, NULL);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	syslog(LOG_INFO, "Startup");

	if (supplicant_register(&callbacks) < 0) {
		syslog(LOG_ERR, "Failed to init supplicant");
		goto done;
	}

	g_main_loop_run(main_loop);

	supplicant_unregister(&callbacks);

done:
	syslog(LOG_INFO, "Exit");

	dbus_connection_unref(conn);

	g_main_loop_unref(main_loop);

	closelog();

	return 0;
}
