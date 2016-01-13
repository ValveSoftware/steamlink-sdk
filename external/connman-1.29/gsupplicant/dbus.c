/*
 *
 *  WPA supplicant library with GLib integration
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <glib.h>

#include "dbus.h"

#define TIMEOUT 30000

static DBusConnection *connection;

static GSList *method_calls;

struct method_call_data {
	gpointer caller;
	DBusPendingCall *pending_call;
	supplicant_dbus_result_function function;
	void *user_data;
};

static void method_call_free(void *pointer)
{
	struct method_call_data *method_call = pointer;
	method_calls = g_slist_remove(method_calls, method_call);
	g_free(method_call);
}

static int find_method_call_by_caller(gconstpointer a, gconstpointer b)
{
	const struct method_call_data *method_call = a;
	gconstpointer caller = b;

	return method_call->caller != caller;
}

static GSList *property_calls;

struct property_call_data {
	gpointer caller;
	DBusPendingCall *pending_call;
	supplicant_dbus_property_function function;
	void *user_data;
};

static void property_call_free(void *pointer)
{
	struct property_call_data *property_call = pointer;
	property_calls = g_slist_remove(property_calls, property_call);
	g_free(property_call);
}

static int find_property_call_by_caller(gconstpointer a, gconstpointer b)
{
	const struct property_call_data *property_call = a;
	gconstpointer caller = b;

	return property_call->caller != caller;
}

void supplicant_dbus_setup(DBusConnection *conn)
{
	connection = conn;
	method_calls = NULL;
	property_calls = NULL;
}

void supplicant_dbus_array_foreach(DBusMessageIter *iter,
				supplicant_dbus_array_function function,
							void *user_data)
{
	DBusMessageIter entry;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &entry);

	while (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_INVALID) {
		if (function)
			function(&entry, user_data);

		dbus_message_iter_next(&entry);
	}
}

void supplicant_dbus_property_foreach(DBusMessageIter *iter,
				supplicant_dbus_property_function function,
							void *user_data)
{
	DBusMessageIter dict;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *key;

		dbus_message_iter_recurse(&dict, &entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
			return;

		dbus_message_iter_get_basic(&entry, &key);
		dbus_message_iter_next(&entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
			return;

		dbus_message_iter_recurse(&entry, &value);

		if (key) {
			if (strcmp(key, "Properties") == 0)
				supplicant_dbus_property_foreach(&value,
							function, user_data);
			else if (function)
				function(key, &value, user_data);
		}

		dbus_message_iter_next(&dict);
	}
}

void supplicant_dbus_property_call_cancel_all(gpointer caller)
{
	while (property_calls) {
		struct property_call_data *property_call;
		GSList *elem = g_slist_find_custom(property_calls, caller,
						find_property_call_by_caller);
		if (!elem)
			break;

		property_call = elem->data;
		property_calls = g_slist_delete_link(property_calls, elem);

		dbus_pending_call_cancel(property_call->pending_call);

		dbus_pending_call_unref(property_call->pending_call);
	}
}

static void property_get_all_reply(DBusPendingCall *call, void *user_data)
{
	struct property_call_data *property_call = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
		goto done;

	if (!dbus_message_iter_init(reply, &iter))
		goto done;

	supplicant_dbus_property_foreach(&iter, property_call->function,
						property_call->user_data);

	if (property_call->function)
		property_call->function(NULL, NULL, property_call->user_data);

done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

int supplicant_dbus_property_get_all(const char *path, const char *interface,
				supplicant_dbus_property_function function,
				void *user_data, gpointer caller)
{
	struct property_call_data *property_call = NULL;
	DBusMessage *message;
	DBusPendingCall *call;

	if (!connection)
		return -EINVAL;

	if (!path || !interface)
		return -EINVAL;

	property_call = g_try_new0(struct property_call_data, 1);
	if (!property_call)
		return -ENOMEM;

	message = dbus_message_new_method_call(SUPPLICANT_SERVICE, path,
					DBUS_INTERFACE_PROPERTIES, "GetAll");
	if (!message) {
		g_free(property_call);
		return -ENOMEM;
	}

	dbus_message_set_auto_start(message, FALSE);

	dbus_message_append_args(message, DBUS_TYPE_STRING, &interface, NULL);

	if (!dbus_connection_send_with_reply(connection, message,
						&call, TIMEOUT)) {
		dbus_message_unref(message);
		g_free(property_call);
		return -EIO;
	}

	if (!call) {
		dbus_message_unref(message);
		g_free(property_call);
		return -EIO;
	}

	property_call->caller = caller;
	property_call->pending_call = call;
	property_call->function = function;
	property_call->user_data = user_data;

	property_calls = g_slist_prepend(property_calls, property_call);

	dbus_pending_call_set_notify(call, property_get_all_reply,
				property_call, property_call_free);

	dbus_message_unref(message);

	return 0;
}

static void property_get_reply(DBusPendingCall *call, void *user_data)
{
	struct property_call_data *property_call = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
		goto done;

	if (!dbus_message_iter_init(reply, &iter))
		goto done;

	if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT) {
		DBusMessageIter variant;

		dbus_message_iter_recurse(&iter, &variant);

		if (property_call->function)
			property_call->function(NULL, &variant,
						property_call->user_data);
	}
done:
	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

int supplicant_dbus_property_get(const char *path, const char *interface,
				const char *method,
				supplicant_dbus_property_function function,
				void *user_data, gpointer caller)
{
	struct property_call_data *property_call = NULL;
	DBusMessage *message;
	DBusPendingCall *call;

	if (!connection)
		return -EINVAL;

	if (!path || !interface || !method)
		return -EINVAL;

	property_call = g_try_new0(struct property_call_data, 1);
	if (!property_call)
		return -ENOMEM;

	message = dbus_message_new_method_call(SUPPLICANT_SERVICE, path,
					DBUS_INTERFACE_PROPERTIES, "Get");

	if (!message) {
		g_free(property_call);
		return -ENOMEM;
	}

	dbus_message_set_auto_start(message, FALSE);

	dbus_message_append_args(message, DBUS_TYPE_STRING, &interface,
					DBUS_TYPE_STRING, &method, NULL);

	if (!dbus_connection_send_with_reply(connection, message,
						&call, TIMEOUT)) {
		dbus_message_unref(message);
		g_free(property_call);
		return -EIO;
	}

	if (!call) {
		dbus_message_unref(message);
		g_free(property_call);
		return -EIO;
	}

	property_call->caller = caller;
	property_call->pending_call = call;
	property_call->function = function;
	property_call->user_data = user_data;

	property_calls = g_slist_prepend(property_calls, property_call);

	dbus_pending_call_set_notify(call, property_get_reply,
					property_call, property_call_free);

	dbus_message_unref(message);

	return 0;
}

static void property_set_reply(DBusPendingCall *call, void *user_data)
{
	struct property_call_data *property_call = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;
	const char *error;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
		error = dbus_message_get_error_name(reply);
	else
		error = NULL;

	dbus_message_iter_init(reply, &iter);

	if (property_call->function)
		property_call->function(error, &iter, property_call->user_data);

	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

int supplicant_dbus_property_set(const char *path, const char *interface,
				const char *key, const char *signature,
				supplicant_dbus_setup_function setup,
				supplicant_dbus_result_function function,
				void *user_data, gpointer caller)
{
	struct property_call_data *property_call = NULL;
	DBusMessage *message;
	DBusMessageIter iter, value;
	DBusPendingCall *call;

	if (!connection)
		return -EINVAL;

	if (!path || !interface)
		return -EINVAL;

	if (!key || !signature || !setup)
		return -EINVAL;

	property_call = g_try_new0(struct property_call_data, 1);
	if (!property_call)
		return -ENOMEM;

	message = dbus_message_new_method_call(SUPPLICANT_SERVICE, path,
					DBUS_INTERFACE_PROPERTIES, "Set");
	if (!message) {
		g_free(property_call);
		return -ENOMEM;
	}

	dbus_message_set_auto_start(message, FALSE);

	dbus_message_iter_init_append(message, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
							signature, &value);
	setup(&value, user_data);
	dbus_message_iter_close_container(&iter, &value);

	if (!dbus_connection_send_with_reply(connection, message,
						&call, TIMEOUT)) {
		dbus_message_unref(message);
		g_free(property_call);
		return -EIO;
	}

	if (!call) {
		dbus_message_unref(message);
		g_free(property_call);
		return -EIO;
	}

	property_call->caller = caller;
	property_call->pending_call = call;
	property_call->function = function;
	property_call->user_data = user_data;

	property_calls = g_slist_prepend(property_calls, property_call);

	dbus_pending_call_set_notify(call, property_set_reply,
					property_call, property_call_free);

	dbus_message_unref(message);

	return 0;
}

void supplicant_dbus_method_call_cancel_all(gpointer caller)
{
	while (method_calls) {
		struct method_call_data *method_call;
		GSList *elem = g_slist_find_custom(method_calls, caller,
						find_method_call_by_caller);
		if (!elem)
			break;

		method_call = elem->data;
		method_calls = g_slist_delete_link(method_calls, elem);

		dbus_pending_call_cancel(method_call->pending_call);

		if (method_call->function)
			method_call->function("net.connman.Error.OperationAborted",
					NULL, method_call->user_data);

		dbus_pending_call_unref(method_call->pending_call);
	}
}

static void method_call_reply(DBusPendingCall *call, void *user_data)
{
	struct method_call_data *method_call = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;
	const char *error;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
		error = dbus_message_get_error_name(reply);
	else
		error = NULL;

	dbus_message_iter_init(reply, &iter);

	if (method_call && method_call->function)
		method_call->function(error, &iter, method_call->user_data);

	dbus_message_unref(reply);

	dbus_pending_call_unref(call);
}

int supplicant_dbus_method_call(const char *path,
				const char *interface, const char *method,
				supplicant_dbus_setup_function setup,
				supplicant_dbus_result_function function,
				void *user_data,
				gpointer caller)
{
	struct method_call_data *method_call = NULL;
	DBusMessage *message;
	DBusMessageIter iter;
	DBusPendingCall *call;

	if (!connection)
		return -EINVAL;

	if (!path || !interface || !method)
		return -EINVAL;

	method_call = g_try_new0(struct method_call_data, 1);
	if (!method_call)
		return -ENOMEM;

	message = dbus_message_new_method_call(SUPPLICANT_SERVICE, path,
							interface, method);
	if (!message) {
		g_free(method_call);
		return -ENOMEM;
	}

	dbus_message_set_auto_start(message, FALSE);

	dbus_message_iter_init_append(message, &iter);
	if (setup)
		setup(&iter, user_data);

	if (!dbus_connection_send_with_reply(connection, message,
						&call, TIMEOUT)) {
		dbus_message_unref(message);
		g_free(method_call);
		return -EIO;
	}

	if (!call) {
		dbus_message_unref(message);
		g_free(method_call);
		return -EIO;
	}

	method_call->caller = caller;
	method_call->pending_call = call;
	method_call->function = function;
	method_call->user_data = user_data;
	method_calls = g_slist_prepend(method_calls, method_call);

	dbus_pending_call_set_notify(call, method_call_reply, method_call,
				method_call_free);

	dbus_message_unref(message);

	return 0;
}

void supplicant_dbus_property_append_basic(DBusMessageIter *iter,
					const char *key, int type, void *val)
{
	DBusMessageIter value;
	const char *signature;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &key);

	switch (type) {
	case DBUS_TYPE_BOOLEAN:
		signature = DBUS_TYPE_BOOLEAN_AS_STRING;
		break;
	case DBUS_TYPE_STRING:
		signature = DBUS_TYPE_STRING_AS_STRING;
		break;
	case DBUS_TYPE_BYTE:
		signature = DBUS_TYPE_BYTE_AS_STRING;
		break;
	case DBUS_TYPE_UINT16:
		signature = DBUS_TYPE_UINT16_AS_STRING;
		break;
	case DBUS_TYPE_INT16:
		signature = DBUS_TYPE_INT16_AS_STRING;
		break;
	case DBUS_TYPE_UINT32:
		signature = DBUS_TYPE_UINT32_AS_STRING;
		break;
	case DBUS_TYPE_INT32:
		signature = DBUS_TYPE_INT32_AS_STRING;
		break;
	case DBUS_TYPE_OBJECT_PATH:
		signature = DBUS_TYPE_OBJECT_PATH_AS_STRING;
		break;
	default:
		signature = DBUS_TYPE_VARIANT_AS_STRING;
		break;
	}

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
							signature, &value);
	dbus_message_iter_append_basic(&value, type, val);
	dbus_message_iter_close_container(iter, &value);
}

void supplicant_dbus_property_append_fixed_array(DBusMessageIter *iter,
				const char *key, int type, void *val, int len)
{
	DBusMessageIter value, array;
	const char *variant_sig, *array_sig;

	switch (type) {
	case DBUS_TYPE_BYTE:
		variant_sig = DBUS_TYPE_ARRAY_AS_STRING
					DBUS_TYPE_BYTE_AS_STRING;
		array_sig = DBUS_TYPE_BYTE_AS_STRING;
		break;
	default:
		return;
	}

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
							variant_sig, &value);

	dbus_message_iter_open_container(&value, DBUS_TYPE_ARRAY,
							array_sig, &array);
	dbus_message_iter_append_fixed_array(&array, type, val, len);
	dbus_message_iter_close_container(&value, &array);

	dbus_message_iter_close_container(iter, &value);
}

void supplicant_dbus_property_append_array(DBusMessageIter *iter,
				const char *key, int type,
				supplicant_dbus_array_function function,
				void *user_data)
{
	DBusMessageIter value, array;
	const char *variant_sig, *array_sig;

	switch (type) {
	case DBUS_TYPE_STRING:
	case DBUS_TYPE_BYTE:
		variant_sig = DBUS_TYPE_ARRAY_AS_STRING
				DBUS_TYPE_ARRAY_AS_STRING
				DBUS_TYPE_BYTE_AS_STRING;
		array_sig = DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING;
		break;
	default:
		return;
	}

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
							variant_sig, &value);

	dbus_message_iter_open_container(&value, DBUS_TYPE_ARRAY,
							array_sig, &array);
	if (function)
		function(&array, user_data);

	dbus_message_iter_close_container(&value, &array);

	dbus_message_iter_close_container(iter, &value);
}
