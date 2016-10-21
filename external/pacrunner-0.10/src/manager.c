/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
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

#include <string.h>

#include <gdbus.h>

#include "pacrunner.h"

struct proxy_config {
	char *path;
	char *sender;
	DBusConnection *conn;
	guint watch;

	char **nameservers;

	struct pacrunner_proxy *proxy;
};

static unsigned int next_config_number = 0;

static GHashTable *config_list;

static void destroy_config(gpointer data)
{
	struct proxy_config *config = data;

	DBG("path %s", config->path);

	pacrunner_proxy_disable(config->proxy);

	pacrunner_proxy_unref(config->proxy);

	if (config->watch > 0)
		g_dbus_remove_watch(config->conn, config->watch);

	g_strfreev(config->nameservers);

	g_free(config->sender);
	g_free(config->path);
	g_free(config);
}

static void disconnect_callback(DBusConnection *conn, void *user_data)
{
	struct proxy_config *config = user_data;

	DBG("path %s", config->path);

	config->watch = 0;

	g_hash_table_remove(config_list, config->path);
}

static struct proxy_config *create_config(DBusConnection *conn,
			const char *sender, const char *interface)
{
	struct proxy_config *config;

	config = g_try_new0(struct proxy_config, 1);
	if (!config)
		return NULL;

	config->proxy = pacrunner_proxy_create(interface);
	if (!config->proxy) {
		g_free(config);
		return NULL;
	}

	config->path = g_strdup_printf("%s/configuration%d", PACRUNNER_PATH,
							next_config_number++);
	config->sender = g_strdup(sender);

	DBG("path %s", config->path);

	config->conn = conn;
	config->watch = g_dbus_add_disconnect_watch(conn, sender,
					disconnect_callback, config, NULL);

	return config;
}

static char **extract_string_array(DBusMessageIter *array)
{
	char **str;
	int index;

	str = g_try_new(char *, 11);
	if (!str)
		return NULL;

	index = 0;

	while (dbus_message_iter_get_arg_type(array) == DBUS_TYPE_STRING) {
		const char *value = NULL;

		dbus_message_iter_get_basic(array, &value);

		if (!value || index > 9)
			break;

		str[index++] = g_strdup(value);

		dbus_message_iter_next(array);
	}

	str[index] = NULL;

	return str;
}

static DBusMessage *create_proxy_config(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	DBusMessage *reply;
	DBusMessageIter iter, array;
	struct proxy_config *config;
	const char *sender, *method = NULL, *interface = NULL;
	const char *url = NULL, *script = NULL;
	char **servers = NULL, **excludes = NULL;
	char **domains = NULL, **nameservers = NULL;
	gboolean browser_only = FALSE;

	sender = dbus_message_get_sender(msg);

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_recurse(&iter, &array);

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value, list;
		const char *key;

		dbus_message_iter_recurse(&array, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		switch (dbus_message_iter_get_arg_type(&value)) {
		case DBUS_TYPE_STRING:
			if (g_str_equal(key, "Method")) {
				dbus_message_iter_get_basic(&value, &method);
				if (strlen(method) == 0)
					method = NULL;
			} else if (g_str_equal(key, "URL")) {
				dbus_message_iter_get_basic(&value, &url);
				if (strlen(url) == 0)
					url = NULL;
			} else if (g_str_equal(key, "Script")) {
				dbus_message_iter_get_basic(&value, &script);
				if (strlen(script) == 0)
					script = NULL;
			} else if (g_str_equal(key, "Interface")) {
				dbus_message_iter_get_basic(&value, &interface);
				if (strlen(interface) == 0)
					interface = NULL;
			}
			break;
		case DBUS_TYPE_ARRAY:
			dbus_message_iter_recurse(&value, &list);

			if (dbus_message_iter_get_arg_type(&list) ==
							DBUS_TYPE_INVALID)
				break;

			if (g_str_equal(key, "Servers")) {
				g_strfreev(servers);
				servers = extract_string_array(&list);
			} else if (g_str_equal(key, "Excludes")) {
				g_strfreev(excludes);
				excludes = extract_string_array(&list);
			} else if (g_str_equal(key, "Domains")) {
				g_strfreev(domains);
				domains = extract_string_array(&list);
			} else if (g_str_equal(key, "Nameservers")) {
				g_strfreev(nameservers);
				nameservers = extract_string_array(&list);
			}
			break;
		case DBUS_TYPE_BOOLEAN:
			if (g_str_equal(key, "BrowserOnly"))
				dbus_message_iter_get_basic(&value,
								&browser_only);
			break;
		}

		dbus_message_iter_next(&array);
	}

	DBG("sender %s method %s interface %s", sender, method, interface);
	DBG("browser-only %u url %s script %p", browser_only, url, script);

	if (!method) {
		reply = g_dbus_create_error(msg,
					PACRUNNER_ERROR_INTERFACE ".Failed",
					"No proxy method specified");
		goto done;
	}

	config = create_config(conn, sender, interface);
	if (!config) {
		reply = g_dbus_create_error(msg,
					PACRUNNER_ERROR_INTERFACE ".Failed",
					"Memory allocation failed");
		goto done;
	}

	config->nameservers = nameservers;

	nameservers = NULL;

	if (pacrunner_proxy_set_domains(config->proxy, domains,
						browser_only) < 0)
		pacrunner_error("Failed to set proxy domains");

	if (g_str_equal(method, "direct")) {
		if (pacrunner_proxy_set_direct(config->proxy) < 0)
			pacrunner_error("Failed to set direct proxy");
	} else if (g_str_equal(method, "manual")) {
		if (pacrunner_proxy_set_manual(config->proxy,
						servers, excludes) < 0)
			pacrunner_error("Failed to set proxy servers");
	} else if (g_str_equal(method, "auto")) {
		if (pacrunner_proxy_set_auto(config->proxy, url, script) < 0)
			pacrunner_error("Failed to set auto proxy");
	} else {
		destroy_config(config);
		reply = g_dbus_create_error(msg,
					PACRUNNER_ERROR_INTERFACE ".Failed",
					"Invalid proxy method specified");
		goto done;
	}

	g_hash_table_insert(config_list, config->path, config);


	reply = g_dbus_create_reply(msg, DBUS_TYPE_OBJECT_PATH, &config->path,
							DBUS_TYPE_INVALID);

done:
	g_strfreev(servers);
	g_strfreev(excludes);

	g_strfreev(domains);
	g_strfreev(nameservers);

	return reply;
}

static DBusMessage *destroy_proxy_config(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	const char *sender, *path;

	sender = dbus_message_get_sender(msg);

	dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
							DBUS_TYPE_INVALID);

	DBG("sender %s path %s", sender, path);

	g_hash_table_remove(config_list, path);

	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);
}

static const GDBusMethodTable manager_methods[] = {
	{ GDBUS_METHOD("CreateProxyConfiguration",
			GDBUS_ARGS({ "settings", "a{sv}" }),
			GDBUS_ARGS({ "path", "o" }),
			create_proxy_config) },
	{ GDBUS_METHOD("DestroyProxyConfiguration",
			GDBUS_ARGS({ "configuration" , "o" }), NULL,
			destroy_proxy_config) },
	{ },
};

static DBusConnection *connection;

int __pacrunner_manager_init(DBusConnection *conn)
{
	DBG("");

	connection = dbus_connection_ref(conn);

	config_list = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, destroy_config);

	g_dbus_register_interface(connection, PACRUNNER_MANAGER_PATH,
						PACRUNNER_MANAGER_INTERFACE,
						manager_methods,
						NULL, NULL, NULL, NULL);

	return 0;
}

void __pacrunner_manager_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(config_list);

	g_dbus_unregister_interface(connection, PACRUNNER_MANAGER_PATH,
						PACRUNNER_MANAGER_INTERFACE);

	dbus_connection_unref(connection);
}
