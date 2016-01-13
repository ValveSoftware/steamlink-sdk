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

#include <sys/time.h>

#include <gdbus.h>

#include "connman.h"

enum timezone_updates {
	TIMEZONE_UPDATES_UNKNOWN = 0,
	TIMEZONE_UPDATES_MANUAL  = 1,
	TIMEZONE_UPDATES_AUTO    = 2,
};

static enum time_updates time_updates_config = TIME_UPDATES_AUTO;
static enum timezone_updates timezone_updates_config = TIMEZONE_UPDATES_AUTO;

static char *timezone_config = NULL;

static const char *time_updates2string(enum time_updates value)
{
	switch (value) {
	case TIME_UPDATES_UNKNOWN:
		break;
	case TIME_UPDATES_MANUAL:
		return "manual";
	case TIME_UPDATES_AUTO:
		return "auto";
	}

	return NULL;
}

static enum time_updates string2time_updates(const char *value)
{
	if (g_strcmp0(value, "manual") == 0)
		return TIME_UPDATES_MANUAL;
	else if (g_strcmp0(value, "auto") == 0)
		return TIME_UPDATES_AUTO;

	return TIME_UPDATES_UNKNOWN;
}

static const char *timezone_updates2string(enum timezone_updates value)
{
	switch (value) {
	case TIMEZONE_UPDATES_UNKNOWN:
		break;
	case TIMEZONE_UPDATES_MANUAL:
		return "manual";
	case TIMEZONE_UPDATES_AUTO:
		return "auto";
	}

	return NULL;
}

static enum timezone_updates string2timezone_updates(const char *value)
{
	if (g_strcmp0(value, "manual") == 0)
		return TIMEZONE_UPDATES_MANUAL;
	else if (g_strcmp0(value, "auto") == 0)
		return TIMEZONE_UPDATES_AUTO;

        return TIMEZONE_UPDATES_UNKNOWN;
}

static void clock_properties_load(void)
{
	GKeyFile *keyfile;
	char *str;
	enum time_updates time_value;
	enum timezone_updates timezone_value;

	keyfile = __connman_storage_load_global();
	if (!keyfile)
		return;

	str = g_key_file_get_string(keyfile, "global", "TimeUpdates", NULL);

	time_value = string2time_updates(str);
	if (time_value != TIME_UPDATES_UNKNOWN)
		time_updates_config = time_value;

	g_free(str);

	str = g_key_file_get_string(keyfile, "global", "TimezoneUpdates",
			NULL);

	timezone_value = string2timezone_updates(str);
	if (timezone_value != TIMEZONE_UPDATES_UNKNOWN)
		timezone_updates_config = timezone_value;

	g_free(str);

	g_key_file_free(keyfile);
}

static void clock_properties_save(void)
{
	GKeyFile *keyfile;
	const char *str;

	keyfile = __connman_storage_load_global();
	if (!keyfile)
		keyfile = g_key_file_new();

	str = time_updates2string(time_updates_config);
	if (str)
		g_key_file_set_string(keyfile, "global", "TimeUpdates", str);
	else
		g_key_file_remove_key(keyfile, "global", "TimeUpdates", NULL);

	str = timezone_updates2string(timezone_updates_config);
	if (str)
		g_key_file_set_string(keyfile, "global", "TimezoneUpdates",
				str);
	else
		g_key_file_remove_key(keyfile, "global", "TimezoneUpdates",
				NULL);

	__connman_storage_save_global(keyfile);

	g_key_file_free(keyfile);
}

enum time_updates __connman_clock_timeupdates(void)
{
	return time_updates_config;
}

static void append_timeservers(DBusMessageIter *iter, void *user_data)
{
	int i;
	char **timeservers = __connman_timeserver_system_get();

	if (!timeservers)
		return;

	for (i = 0; timeservers[i]; i++) {
		dbus_message_iter_append_basic(iter,
				DBUS_TYPE_STRING, &timeservers[i]);
	}

	g_strfreev(timeservers);
}

static DBusMessage *get_properties(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	DBusMessageIter array, dict;
	struct timeval tv;
	const char *str;

	DBG("conn %p", conn);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &array);

	connman_dbus_dict_open(&array, &dict);

	if (gettimeofday(&tv, NULL) == 0) {
		dbus_uint64_t val = tv.tv_sec;

		connman_dbus_dict_append_basic(&dict, "Time",
						DBUS_TYPE_UINT64, &val);
	}

	str = time_updates2string(time_updates_config);
	if (str)
		connman_dbus_dict_append_basic(&dict, "TimeUpdates",
						DBUS_TYPE_STRING, &str);

	if (timezone_config)
		connman_dbus_dict_append_basic(&dict, "Timezone",
					DBUS_TYPE_STRING, &timezone_config);

	str = timezone_updates2string(timezone_updates_config);
	if (str)
		connman_dbus_dict_append_basic(&dict, "TimezoneUpdates",
						DBUS_TYPE_STRING, &str);

	connman_dbus_dict_append_array(&dict, "Timeservers",
				DBUS_TYPE_STRING, append_timeservers, NULL);

	connman_dbus_dict_close(&array, &dict);

	return reply;
}

static DBusMessage *set_property(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	DBusMessageIter iter, value;
	const char *name;
	int type;

	DBG("conn %p", conn);

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

	if (g_str_equal(name, "Time")) {
		struct timeval tv;
		dbus_uint64_t newval;

		if (type != DBUS_TYPE_UINT64)
			return __connman_error_invalid_arguments(msg);

		if (time_updates_config != TIME_UPDATES_MANUAL)
			return __connman_error_permission_denied(msg);

		dbus_message_iter_get_basic(&value, &newval);

		tv.tv_sec = newval;
		tv.tv_usec = 0;

		if (settimeofday(&tv, NULL) < 0)
			return __connman_error_invalid_arguments(msg);

		connman_dbus_property_changed_basic(CONNMAN_MANAGER_PATH,
				CONNMAN_CLOCK_INTERFACE, "Time",
				DBUS_TYPE_UINT64, &newval);
	} else if (g_str_equal(name, "TimeUpdates")) {
		const char *strval;
		enum time_updates newval;

		if (type != DBUS_TYPE_STRING)
			return __connman_error_invalid_arguments(msg);

		dbus_message_iter_get_basic(&value, &strval);
		newval = string2time_updates(strval);

		if (newval == TIME_UPDATES_UNKNOWN)
			return __connman_error_invalid_arguments(msg);

		if (newval == time_updates_config)
			return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);

		time_updates_config = newval;

		clock_properties_save();
		connman_dbus_property_changed_basic(CONNMAN_MANAGER_PATH,
				CONNMAN_CLOCK_INTERFACE, "TimeUpdates",
				DBUS_TYPE_STRING, &strval);
	} else if (g_str_equal(name, "Timezone")) {
		const char *strval;

		if (type != DBUS_TYPE_STRING)
			return __connman_error_invalid_arguments(msg);

		if (timezone_updates_config != TIMEZONE_UPDATES_MANUAL)
			return __connman_error_permission_denied(msg);

		dbus_message_iter_get_basic(&value, &strval);

		if (__connman_timezone_change(strval) < 0)
			return __connman_error_invalid_arguments(msg);
	} else if (g_str_equal(name, "TimezoneUpdates")) {
		const char *strval;
		enum timezone_updates newval;

		if (type != DBUS_TYPE_STRING)
			return __connman_error_invalid_arguments(msg);

		dbus_message_iter_get_basic(&value, &strval);
		newval = string2timezone_updates(strval);

		if (newval == TIMEZONE_UPDATES_UNKNOWN)
			return __connman_error_invalid_arguments(msg);

		if (newval == timezone_updates_config)
			return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);

		timezone_updates_config = newval;

		clock_properties_save();
		connman_dbus_property_changed_basic(CONNMAN_MANAGER_PATH,
				CONNMAN_CLOCK_INTERFACE, "TimezoneUpdates",
				DBUS_TYPE_STRING, &strval);
	} else if (g_str_equal(name, "Timeservers")) {
		DBusMessageIter entry;
		char **str = NULL;
		GSList *list = NULL;
		int count = 0;

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

		if (list) {
			str = g_new0(char *, count+1);

			while (list) {
				count--;
				str[count] = list->data;
				list = g_slist_delete_link(list, list);
			};
		}

		__connman_timeserver_system_set(str);

		if (str)
			g_strfreev(str);

		connman_dbus_property_changed_array(CONNMAN_MANAGER_PATH,
				CONNMAN_CLOCK_INTERFACE, "Timeservers",
				DBUS_TYPE_STRING, append_timeservers, NULL);
	} else
		return __connman_error_invalid_property(msg);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static const GDBusMethodTable clock_methods[] = {
	{ GDBUS_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			get_properties) },
	{ GDBUS_METHOD("SetProperty",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" }), NULL,
			set_property)   },
	{ },
};

static const GDBusSignalTable clock_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" })) },
	{ },
};

static DBusConnection *connection = NULL;

void __connman_clock_update_timezone(void)
{
	DBG("");

	g_free(timezone_config);
	timezone_config = __connman_timezone_lookup();

	if (!timezone_config)
		return;

	connman_dbus_property_changed_basic(CONNMAN_MANAGER_PATH,
				CONNMAN_CLOCK_INTERFACE, "Timezone",
				DBUS_TYPE_STRING, &timezone_config);
}

int __connman_clock_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();
	if (!connection)
		return -1;

	__connman_timezone_init();

	timezone_config = __connman_timezone_lookup();

	g_dbus_register_interface(connection, CONNMAN_MANAGER_PATH,
						CONNMAN_CLOCK_INTERFACE,
						clock_methods, clock_signals,
						NULL, NULL, NULL);

	clock_properties_load();
	return 0;
}

void __connman_clock_cleanup(void)
{
	DBG("");

	if (!connection)
		return;

	g_dbus_unregister_interface(connection, CONNMAN_MANAGER_PATH,
						CONNMAN_CLOCK_INTERFACE);

	dbus_connection_unref(connection);

	__connman_timezone_cleanup();

	g_free(timezone_config);
}
