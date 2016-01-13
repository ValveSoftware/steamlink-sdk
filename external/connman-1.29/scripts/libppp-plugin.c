/*
 *
 *  Connection Manager
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pppd/pppd.h>
#include <pppd/fsm.h>
#include <pppd/ipcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dbus/dbus.h>

#define INET_ADDRES_LEN (INET_ADDRSTRLEN + 5)
#define INET_DNS_LEN	(2*INET_ADDRSTRLEN + 9)

static char *busname;
static char *interface;
static char *path;

static DBusConnection *connection;
static int prev_phase;

char pppd_version[] = VERSION;

int plugin_init(void);

static void append(DBusMessageIter *dict, const char *key, const char *value)
{
	DBusMessageIter entry;

	/* We clean the environment before invoking pppd, but
	 * might as well still filter out the few things that get
	 * added that we're not interested in
	 */
	if (!strcmp(key, "PWD") || !strcmp(key, "_") ||
			!strcmp(key, "SHLVL") ||
			!strcmp(key, "connman_busname") ||
			!strcmp(key, "connman_network"))
		return;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
							NULL, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &value);

	dbus_message_iter_close_container(dict, &entry);
}


static int ppp_have_secret(void)
{
	return 1;
}

static int ppp_get_secret(char *username, char *password)
{
	DBusMessage *msg, *reply;
	const char *user, *pass;
	DBusError err;

	if (!username && !password)
		return -1;

	if (!password)
		return 1;

	if (!connection)
		return -1;

	dbus_error_init(&err);

	msg = dbus_message_new_method_call(busname, path, interface, "getsec");
	if (!msg)
		return -1;

	dbus_message_append_args(msg, DBUS_TYPE_INVALID, DBUS_TYPE_INVALID);

	reply = dbus_connection_send_with_reply_and_block(connection,
								msg, -1, &err);
	if (!reply) {
		if (dbus_error_is_set(&err))
			dbus_error_free(&err);

		dbus_message_unref(msg);
		return -1;
	}

	dbus_message_unref(msg);

	dbus_error_init(&err);

	if (!dbus_message_get_args(reply, &err, DBUS_TYPE_STRING, &user,
					DBUS_TYPE_STRING, &pass,
					DBUS_TYPE_INVALID)) {
		if (dbus_error_is_set(&err))
			dbus_error_free(&err);

		dbus_message_unref(reply);
		return -1;
	}

	if (username)
		strcpy(username, user);

	strcpy(password, pass);

	dbus_message_unref(reply);

	return 1;
}

static void ppp_up(void *data, int arg)
{
	char buf[INET_ADDRES_LEN];
	char dns[INET_DNS_LEN];
	const char *reason = "connect";
	bool add_blank = FALSE;
	DBusMessageIter iter, dict;
	DBusMessage *msg;

	if (!connection)
		return;

	if (ipcp_gotoptions[0].ouraddr == 0)
		return;

	msg = dbus_message_new_method_call(busname, path,
						interface, "notify");
	if (!msg)
		return;

	dbus_message_set_no_reply(msg, TRUE);

	dbus_message_append_args(msg,
			DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID);

	dbus_message_iter_init_append(msg, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_STRING_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	append(&dict, "INTERNAL_IFNAME", ifname);

	inet_ntop(AF_INET, &ipcp_gotoptions[0].ouraddr, buf, INET_ADDRSTRLEN);
	append(&dict, "INTERNAL_IP4_ADDRESS", buf);

	strcpy(buf, "255.255.255.255");
	append(&dict, "INTERNAL_IP4_NETMASK", buf);

	if (ipcp_gotoptions[0].dnsaddr[0] || ipcp_gotoptions[0].dnsaddr[1]) {
		memset(dns, 0, sizeof(dns));
		dns[0] = '\0';

		if (ipcp_gotoptions[0].dnsaddr[0]) {
			inet_ntop(AF_INET, &ipcp_gotoptions[0].dnsaddr[0],
							buf, INET_ADDRSTRLEN);
			strcat(dns, buf);

			add_blank = TRUE;
		}

		if (ipcp_gotoptions[0].dnsaddr[1]) {
			inet_ntop(AF_INET, &ipcp_gotoptions[0].dnsaddr[1],
							buf, INET_ADDRSTRLEN);
			if (add_blank)
				strcat(dns, " ");

			strcat(dns, buf);
		}
		append(&dict, "INTERNAL_IP4_DNS", dns);
	}

	append(&dict, "MTU", "1400");

	dbus_message_iter_close_container(&iter, &dict);

	dbus_connection_send(connection, msg, NULL);

	dbus_connection_flush(connection);

	dbus_message_unref(msg);
}

static void ppp_exit(void *data, int arg)
{
	if (connection) {
		dbus_connection_unref(connection);
		connection = NULL;
	}

	if (busname) {
		free(busname);
		busname = NULL;
	}

	if (interface) {
		free(interface);
		interface = NULL;
	}

	if (path) {
		free(path);
		path = NULL;
	}
}

static void ppp_phase_change(void *data, int arg)
{
	const char *reason = "disconnect";
	DBusMessage *msg;
	int send_msg = 0;

	if (!connection)
		return;

	if (prev_phase == PHASE_AUTHENTICATE &&
				arg == PHASE_TERMINATE) {
		reason = "auth failed";
		send_msg = 1;
	}

	if (send_msg > 0 || arg == PHASE_DEAD || arg == PHASE_DISCONNECT) {
		msg = dbus_message_new_method_call(busname, path,
						interface, "notify");
		if (!msg)
			return;

		dbus_message_set_no_reply(msg, TRUE);

		dbus_message_append_args(msg,
			DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID);

		dbus_connection_send(connection, msg, NULL);

		dbus_connection_flush(connection);

		dbus_message_unref(msg);
	}

	prev_phase = arg;
}

int plugin_init(void)
{
	DBusError error;
	static const char *bus, *inter, *p;

	dbus_error_init(&error);

	bus = getenv("CONNMAN_BUSNAME");
	inter = getenv("CONNMAN_INTERFACE");
	p = getenv("CONNMAN_PATH");

	if (!bus || !inter || !p)
		return -1;

	busname = strdup(bus);
	interface = strdup(inter);
	path = strdup(p);

	if (!busname || !interface || !path) {
		ppp_exit(NULL, 0);
		return -1;
	}

	connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!connection) {
		if (dbus_error_is_set(&error))
			dbus_error_free(&error);

		ppp_exit(NULL, 0);
		return -1;
	}

	pap_passwd_hook = ppp_get_secret;
	chap_passwd_hook = ppp_get_secret;

	chap_check_hook = ppp_have_secret;
	pap_check_hook = ppp_have_secret;

	add_notifier(&ip_up_notifier, ppp_up, NULL);
	add_notifier(&phasechange, ppp_phase_change, NULL);
	add_notifier(&exitnotify, ppp_exit, connection);

	return 0;
}
