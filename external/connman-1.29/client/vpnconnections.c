/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2013  Intel Corporation. All rights reserved.
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
#include <string.h>

#include <glib.h>

#include "vpnconnections.h"

static void print_connection(char *path, DBusMessageIter *iter)
{
	char *name = "";
	char state = ' ';
	char *str;
	char *property;
	DBusMessageIter entry, val;

	while (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {

		dbus_message_iter_recurse(iter, &entry);
		dbus_message_iter_get_basic(&entry, &property);
		if (strcmp(property, "Name") == 0) {
			dbus_message_iter_next(&entry);
			dbus_message_iter_recurse(&entry, &val);
			dbus_message_iter_get_basic(&val, &name);

		} else if (strcmp(property, "State") == 0) {
			dbus_message_iter_next(&entry);
			dbus_message_iter_recurse(&entry, &val);
			dbus_message_iter_get_basic(&val, &str);

			if (str) {
				if (strcmp(str, "ready") == 0)
					state = 'R';
				else if (strcmp(str, "configuration") == 0)
					state = 'C';
				else if (strcmp(str, "failure") == 0)
					state = 'F';
			}
		}

		dbus_message_iter_next(iter);
	}

	str = strrchr(path, '/');
	if (str)
		str++;
	else
		str = path;

	fprintf(stdout, "  %c %-20s %s", state, name, str);
}

void __connmanctl_vpnconnections_list(DBusMessageIter *iter)
{
	DBusMessageIter array, entry, dict;
	char *path = NULL;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &array);

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {

		dbus_message_iter_recurse(&array, &entry);
		if (dbus_message_iter_get_arg_type(&entry)
				!= DBUS_TYPE_OBJECT_PATH)
			return;

		dbus_message_iter_get_basic(&entry, &path);

		dbus_message_iter_next(&entry);
		if (dbus_message_iter_get_arg_type(&entry)
				== DBUS_TYPE_ARRAY) {
			dbus_message_iter_recurse(&entry, &dict);
			print_connection(path, &dict);
		}

		if (dbus_message_iter_has_next(&array))
			fprintf(stdout, "\n");

		dbus_message_iter_next(&array);
	}

	fprintf(stdout, "\n");
}
