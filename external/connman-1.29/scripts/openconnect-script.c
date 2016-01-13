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
#include <syslog.h>
#include <libgen.h>

#include <dbus/dbus.h>

extern char **environ;

static void print(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsyslog(LOG_INFO, format, ap);
	va_end(ap);
}

static void append(DBusMessageIter *dict, const char *pattern)
{
	DBusMessageIter entry;
	const char *key, *value;
	char *delim;

	delim = strchr(pattern, '=');
	*delim = '\0';

	key = pattern;
	value = delim + 1;

	/* We clean the environment before invoking openconnect, but
	   might as well still filter out the few things that get
	   added that we're not interested in */
	if (!strcmp(key, "PWD") || !strcmp(key, "_") ||
	    !strcmp(key, "SHLVL") || !strcmp(key, "connman_busname") ||
	    !strcmp(key, "connman_network"))
		return;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
							NULL, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &value);

	dbus_message_iter_close_container(dict, &entry);
}

int main(int argc, char *argv[])
{
	DBusConnection *conn;
	DBusError error;
	DBusMessage *msg;
	DBusMessageIter iter, dict;
	char **envp, *busname, *reason, *interface, *path;
	int ret = 0;

	openlog(basename(argv[0]), LOG_NDELAY | LOG_PID, LOG_DAEMON);

	busname = getenv("CONNMAN_BUSNAME");
	interface = getenv("CONNMAN_INTERFACE");
	path = getenv("CONNMAN_PATH");

	reason = getenv("reason");

	if (!busname || !interface || !path || !reason) {
		print("Required environment variables not set");
		ret = 1;
		goto out;
	}

	if (strcmp(reason, "pre-init") == 0)
		goto out;

	dbus_error_init(&error);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!conn) {
		if (dbus_error_is_set(&error)) {
			print("%s", error.message);
			dbus_error_free(&error);
		} else
			print("Failed to get on system bus");

		goto out;
	}

	msg = dbus_message_new_method_call(busname, path,
						interface, "notify");
	if (!msg) {
		dbus_connection_unref(conn);
		print("Failed to allocate method call");
		goto out;
	}

	dbus_message_set_no_reply(msg, TRUE);

	dbus_message_append_args(msg,
				 DBUS_TYPE_STRING, &reason,
				 DBUS_TYPE_INVALID);

	dbus_message_iter_init_append(msg, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_STRING_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	for (envp = environ; envp && *envp; envp++)
		append(&dict, *envp);

	dbus_message_iter_close_container(&iter, &dict);

	if (!dbus_connection_send(conn, msg, NULL)) {
		print("Failed to send message");
		goto out;
	}

	dbus_connection_flush(conn);

	dbus_message_unref(msg);

	dbus_connection_unref(conn);

out:
	closelog();

	return ret;
}
