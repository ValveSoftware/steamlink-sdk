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
#include <errno.h>

#include <string.h>
#include <signal.h>

#include <dbus/dbus.h>

static volatile sig_atomic_t __io_terminate = 0;

static void sig_term(int sig)
{
	__io_terminate = 1;
}

static void add_dict_with_string_value(DBusMessageIter *iter,
					const char *key, const char *str)
{
	DBusMessageIter dict, entry, value;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);
	dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY,
								NULL, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
					DBUS_TYPE_STRING_AS_STRING, &value);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_STRING, &str);
	dbus_message_iter_close_container(&entry, &value);

	dbus_message_iter_close_container(&dict, &entry);
	dbus_message_iter_close_container(iter, &dict);
}

static void add_empty_string_dict(DBusMessageIter *iter)
{
	DBusMessageIter dict;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_STRING_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	dbus_message_iter_close_container(iter, &dict);
}

static void add_arguments(DBusConnection *conn, DBusMessageIter *iter)
{
	const char *busname = dbus_bus_get_unique_name(conn);
	const char *kind = "system-bus-name";
	const char *action = "org.freedesktop.policykit.exec";
	const char *cancel = "";
	dbus_uint32_t flags = 0x00000001;
	DBusMessageIter subject;

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT,
							NULL, &subject);
	dbus_message_iter_append_basic(&subject, DBUS_TYPE_STRING, &kind);
	add_dict_with_string_value(&subject, "name", busname);
	dbus_message_iter_close_container(iter, &subject);

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &action);
	add_empty_string_dict(iter);
	dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &flags);
	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &cancel);
}

static void print_arguments(DBusMessageIter *iter)
{
	DBusMessageIter result;
	dbus_bool_t authorized, challenge;

	dbus_message_iter_recurse(iter, &result);

	dbus_message_iter_get_basic(&result, &authorized);
	dbus_message_iter_get_basic(&result, &challenge);

	printf("Authorized %d (Challenge %d)\n", authorized, challenge);
}

#define AUTHORITY_DBUS	"org.freedesktop.PolicyKit1"
#define AUTHORITY_INTF	"org.freedesktop.PolicyKit1.Authority"
#define AUTHORITY_PATH	"/org/freedesktop/PolicyKit1/Authority"

static int check_authorization(DBusConnection *conn)
{
	DBusMessage *msg, *reply;
	DBusMessageIter iter;
	DBusError err;

	msg = dbus_message_new_method_call(AUTHORITY_DBUS, AUTHORITY_PATH,
				AUTHORITY_INTF, "CheckAuthorization");
	if (!msg) {
		fprintf(stderr, "Can't allocate new method call\n");
		return -ENOMEM;
	}

	dbus_message_iter_init_append(msg, &iter);
	add_arguments(conn, &iter);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

	dbus_message_unref(msg);

	if (!reply) {
		if (dbus_error_is_set(&err)) {
			fprintf(stderr, "%s\n", err.message);
			dbus_error_free(&err);
		} else
			fprintf(stderr, "Can't check authorization\n");
		return -EIO;
	}

	if (dbus_message_has_signature(reply, "(bba{ss})")) {
		dbus_message_iter_init(reply, &iter);
		print_arguments(&iter);
	}

	dbus_message_unref(reply);

	return 0;
}

int main(int argc, char *argv[])
{
	DBusConnection *conn;
	struct sigaction sa;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		fprintf(stderr, "Can't get on system bus");
		return 1;
	}

	check_authorization(conn);

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags   = SA_NOCLDSTOP;
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);

#if 0
	while (!__io_terminate) {
		if (dbus_connection_read_write_dispatch(conn, 500) == FALSE)
			break;
	}
#endif

	dbus_connection_unref(conn);

	return 0;
}
