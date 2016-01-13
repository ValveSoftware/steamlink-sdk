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

#include <dbus/dbus.h>

#define SUPPLICANT_SERVICE	"fi.w1.wpa_supplicant1"
#define SUPPLICANT_INTERFACE	"fi.w1.wpa_supplicant1"
#define SUPPLICANT_PATH		"/fi/w1/wpa_supplicant1"

typedef void (* supplicant_dbus_array_function) (DBusMessageIter *iter,
							void *user_data);

typedef void (* supplicant_dbus_property_function) (const char *key,
				DBusMessageIter *iter, void *user_data);

typedef void (* supplicant_dbus_setup_function) (DBusMessageIter *iter,
							void *user_data);

typedef void (* supplicant_dbus_result_function) (const char *error,
				DBusMessageIter *iter, void *user_data);

void supplicant_dbus_setup(DBusConnection *conn);

void supplicant_dbus_array_foreach(DBusMessageIter *iter,
				supplicant_dbus_array_function function,
							void *user_data);

void supplicant_dbus_property_foreach(DBusMessageIter *iter,
				supplicant_dbus_property_function function,
							void *user_data);

int supplicant_dbus_property_get_all(const char *path, const char *interface,
				supplicant_dbus_property_function function,
							void *user_data);

int supplicant_dbus_property_set(const char *path, const char *interface,
				const char *key, const char *signature,
				supplicant_dbus_setup_function setup,
				supplicant_dbus_result_function function,
							void *user_data);

int supplicant_dbus_method_call(const char *path,
				const char *interface, const char *method,
				supplicant_dbus_setup_function setup,
				supplicant_dbus_result_function function,
							void *user_data);

void supplicant_dbus_property_append_basic(DBusMessageIter *iter,
					const char *key, int type, void *val);

static inline void supplicant_dbus_dict_open(DBusMessageIter *iter,
							DBusMessageIter *dict)
{
	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, dict);
}

static inline void supplicant_dbus_dict_close(DBusMessageIter *iter,
							DBusMessageIter *dict)
{
	dbus_message_iter_close_container(iter, dict);
}

static inline void supplicant_dbus_dict_append_basic(DBusMessageIter *dict,
					const char *key, int type, void *val)
{
	DBusMessageIter entry;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
								NULL, &entry);
	supplicant_dbus_property_append_basic(&entry, key, type, val);
	dbus_message_iter_close_container(dict, &entry);
}
