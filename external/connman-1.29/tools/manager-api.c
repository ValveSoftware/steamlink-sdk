/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2011-2014  BMW Car IT GmbH.
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

#include "session-test.h"

static DBusMessage *set_property(DBusConnection *connection,
				const char *property, int type, void *value)
{
	DBusMessage *message, *reply;
	DBusError error;
	DBusMessageIter iter;

	message = dbus_message_new_method_call(CONNMAN_SERVICE,
						CONNMAN_MANAGER_PATH,
						CONNMAN_MANAGER_INTERFACE,
						"SetProperty");
	if (!message)
		return NULL;

	dbus_message_iter_init_append(message, &iter);
	connman_dbus_property_append_basic(&iter, property, type, value);

	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(connection,
							message, -1, &error);
	if (!reply) {
		if (dbus_error_is_set(&error)) {
			LOG("%s", error.message);
			dbus_error_free(&error);
		} else {
			LOG("Failed to get properties");
		}
		dbus_message_unref(message);
		return NULL;
	}

	dbus_message_unref(message);

	return reply;
}

DBusMessage *manager_get_services(DBusConnection *connection)
{
	DBusMessage *message, *reply;
	DBusError error;

	message = dbus_message_new_method_call(CONNMAN_SERVICE,
						CONNMAN_MANAGER_PATH,
						CONNMAN_MANAGER_INTERFACE,
							"GetServices");
	if (!message)
		return NULL;

	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(connection,
							message, -1, &error);
	if (!reply) {
		if (dbus_error_is_set(&error)) {
			LOG("%s", error.message);
			dbus_error_free(&error);
		} else {
			LOG("Failed to get properties");
		}
		dbus_message_unref(message);
		return NULL;
	}

	dbus_message_unref(message);

	return reply;
}

DBusMessage *manager_get_properties(DBusConnection *connection)
{
	DBusMessage *message, *reply;
	DBusError error;

	message = dbus_message_new_method_call(CONNMAN_SERVICE,
						CONNMAN_MANAGER_PATH,
						CONNMAN_MANAGER_INTERFACE,
							"GetProperties");
	if (!message)
		return NULL;

	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(connection,
							message, -1, &error);
	if (!reply) {
		if (dbus_error_is_set(&error)) {
			LOG("%s", error.message);
			dbus_error_free(&error);
		} else {
			LOG("%s", error.message);
		}
		dbus_message_unref(message);
		return NULL;
	}

	dbus_message_unref(message);

	return reply;
}

DBusMessage *manager_create_session(DBusConnection *connection,
					struct test_session_info *info,
					const char *notifier_path)
{
	DBusMessage *message, *reply;
	DBusError error;
	DBusMessageIter array, dict;

	message = dbus_message_new_method_call(CONNMAN_SERVICE,
						CONNMAN_MANAGER_PATH,
						CONNMAN_MANAGER_INTERFACE,
							"CreateSession");
	if (!message)
		return NULL;

	dbus_error_init(&error);

	dbus_message_iter_init_append(message, &array);

	connman_dbus_dict_open(&array, &dict);

	session_append_settings(&dict, info);

	connman_dbus_dict_close(&array, &dict);

	dbus_message_iter_append_basic(&array, DBUS_TYPE_OBJECT_PATH,
				&notifier_path);

	reply = dbus_connection_send_with_reply_and_block(connection,
							message, -1, &error);
	if (!reply) {
		if (dbus_error_is_set(&error)) {
			LOG("%s", error.message);
			dbus_error_free(&error);
		} else {
			LOG("Failed to get properties");
		}
		dbus_message_unref(message);
		return NULL;
	}

	dbus_message_unref(message);

	return reply;
}

DBusMessage *manager_destroy_session(DBusConnection *connection,
					const char *notifier_path)
{
	DBusMessage *message, *reply;
	DBusError error;
	DBusMessageIter array;

	message = dbus_message_new_method_call(CONNMAN_SERVICE,
						CONNMAN_MANAGER_PATH,
						CONNMAN_MANAGER_INTERFACE,
							"DestroySession");
	if (!message)
		return NULL;

	dbus_error_init(&error);

	dbus_message_iter_init_append(message, &array);

	dbus_message_iter_append_basic(&array, DBUS_TYPE_OBJECT_PATH,
				&notifier_path);

	reply = dbus_connection_send_with_reply_and_block(connection,
							message, -1, &error);
	if (!reply) {
		if (dbus_error_is_set(&error)) {
			LOG("%s", error.message);
			dbus_error_free(&error);
		} else {
			LOG("%s", error.message);
		}
		dbus_message_unref(message);
		return NULL;
	}

	dbus_message_unref(message);

	return reply;
}

DBusMessage *manager_set_session_mode(DBusConnection *connection,
					bool enable)
{
	dbus_bool_t val = enable;

	return set_property(connection, "SessionMode",
				DBUS_TYPE_BOOLEAN, &val);
}

int manager_parse_properties(DBusMessage *msg,
				struct test_manager *manager)
{
	DBusMessageIter iter, array;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_recurse(&iter, &array);

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(&array, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		switch (dbus_message_iter_get_arg_type(&value)) {
		case DBUS_TYPE_STRING:
			if (g_str_equal(key, "State")) {
				const char *val;
				dbus_message_iter_get_basic(&value, &val);

				if (manager->state)
					g_free(manager->state);

				LOG("State %s", val);

				manager->state = g_strdup(val);
			}
			break;
		default:
			break;
		}
		dbus_message_iter_next(&array);
	}

	return 0;
}
