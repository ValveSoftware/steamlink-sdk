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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include <gdbus.h>

#include "input.h"
#include "dbus_helpers.h"
#include "agent.h"

#define AGENT_INTERFACE      "net.connman.Agent"
#define VPN_AGENT_INTERFACE  "net.connman.vpn.Agent"

static DBusConnection *agent_connection;

struct agent_input_data {
	const char *attribute;
	bool requested;
	char *prompt;
	connmanctl_input_func_t func;
};

struct agent_data {
	struct agent_input_data *input;
	char *interface;
	bool registered;
	DBusMessage *message;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	GDBusMethodFunction pending_function;
};

static void request_input_ssid_return(char *input, void *user_data);
static void request_input_passphrase_return(char *input, void *user_data);
static void request_input_string_return(char *input, void *user_data);

enum requestinput {
	SSID                    = 0,
	IDENTITY                = 1,
	PASSPHRASE              = 2,
	WPS                     = 3,
	WISPR_USERNAME          = 4,
	WISPR_PASSPHRASE        = 5,
	REQUEST_INPUT_MAX       = 6,
};

static struct agent_input_data agent_input_handler[] = {
	{ "Name", false, "Hidden SSID name? ", request_input_ssid_return },
	{ "Identity", false, "EAP username? ", request_input_string_return },
	{ "Passphrase", false, "Passphrase? ",
	  request_input_passphrase_return },
	{ "WPS", false, "WPS PIN (empty line for pushbutton)? " ,
	  request_input_string_return },
	{ "Username", false, "WISPr username? ", request_input_string_return },
	{ "Password", false, "WISPr password? ", request_input_string_return },
	{ },
};

static struct agent_data agent_request = {
	agent_input_handler,
	AGENT_INTERFACE,
};

static struct agent_input_data vpnagent_input_handler[] = {
	{ "OpenConnect.Cookie", false, "OpenConnect Cookie? ",
	  request_input_string_return },
	{ "OpenConnect.ServerCert", false,
	  "OpenConnect server certificate hash? ",
	  request_input_string_return },
	{ "OpenConnect.VPNHost", false, "OpenConnect VPN server? ",
	  request_input_string_return },
	{ "Username", false, "VPN username? ", request_input_string_return },
	{ "Password", false, "VPN password? ", request_input_string_return },
	{ },
};

static struct agent_data vpn_agent_request = {
	vpnagent_input_handler,
	VPN_AGENT_INTERFACE,
};

static int confirm_input(char *input)
{
	int i;

	if (!input)
		return -1;

	for (i = 0; input[i] != '\0'; i++)
		if (isspace(input[i]) == 0)
			break;

	if (strcasecmp(&input[i], "yes") == 0 ||
			strcasecmp(&input[i], "y") == 0)
		return 1;

	if (strcasecmp(&input[i], "no") == 0 ||
			strcasecmp(&input[i], "n") == 0)
		return 0;

	return -1;
}

static char *strip_path(char *path)
{
	char *name = strrchr(path, '/');
	if (name)
		name++;
	else
		name = path;

	return name;
}

static char *agent_path(void)
{
	static char *path = NULL;

	if (!path)
		path = g_strdup_printf("/net/connman/connmanctl%d", getpid());

	return path;
}

static void pending_message_remove(struct agent_data *request)
{
	if (request->message) {
		dbus_message_unref(request->message);
		request->message = NULL;
	}

	if (request->reply) {
		dbus_message_unref(request->reply);
		request->reply = NULL;
	}
}

static void pending_command_complete(char *message)
{
	struct agent_data *next_request = NULL;
	DBusMessage *pending_message;
	GDBusMethodFunction pending_function;

	__connmanctl_save_rl();

	fprintf(stdout, "%s", message);

	__connmanctl_redraw_rl();

	if (__connmanctl_is_interactive() == true)
		__connmanctl_command_mode();
	else
		__connmanctl_agent_mode("", NULL, NULL);

	if (agent_request.message)
		next_request = &agent_request;
	else if (vpn_agent_request.message)
		next_request = &vpn_agent_request;

	if (!next_request)
		return;

	pending_message = next_request->message;
	pending_function = next_request->pending_function;
	next_request->pending_function = NULL;

	pending_function(agent_connection, next_request->message,
			next_request);

	dbus_message_unref(pending_message);
}

static bool handle_message(DBusMessage *message, struct agent_data *request,
		GDBusMethodFunction function)
{
	if (!agent_request.pending_function &&
			!vpn_agent_request.pending_function)
		return true;

	request->message = dbus_message_ref(message);
	request->pending_function = function;

	return false;
}

static DBusMessage *agent_release(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	struct agent_data *request = user_data;

	if (handle_message(message, request, agent_release) == false)
		return NULL;

	g_dbus_unregister_interface(connection, agent_path(),
			request->interface);
	request->registered = false;

	pending_message_remove(request);

	if (strcmp(request->interface, AGENT_INTERFACE) == 0)
		pending_command_complete("Agent unregistered by ConnMan\n");
	else
		pending_command_complete("VPN Agent unregistered by ConnMan "
				"VPNd\n");

	if (__connmanctl_is_interactive() == false)
		__connmanctl_quit();

	return dbus_message_new_method_return(message);
}

static DBusMessage *agent_cancel(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	struct agent_data *request = user_data;

	if (handle_message(message, request, agent_cancel) == false)
		return NULL;

	pending_message_remove(request);

	if (strcmp(request->interface, AGENT_INTERFACE) == 0)
		pending_command_complete("Agent request cancelled by "
				"ConnMan\n");
	else
		pending_command_complete("VPN Agent request cancelled by "
				"ConnMan VPNd\n");

	return dbus_message_new_method_return(message);
}

static void request_browser_return(char *input, void *user_data)
{
	struct agent_data *request = user_data;

	switch (confirm_input(input)) {
	case 1:
		g_dbus_send_reply(agent_connection, request->message,
				DBUS_TYPE_INVALID);
		break;
	case 0:
		g_dbus_send_error(agent_connection, request->message,
				"net.connman.Agent.Error.Canceled", NULL);
		break;
	default:
		return;
	}

	pending_message_remove(request);
	pending_command_complete("");
}

static DBusMessage *agent_request_browser(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	struct agent_data *request = user_data;
	DBusMessageIter iter;
	char *service, *url;

	if (handle_message(message, request, agent_request_browser) == false)
		return NULL;

	dbus_message_iter_init(message, &iter);

	dbus_message_iter_get_basic(&iter, &service);
	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &url);

	__connmanctl_save_rl();
	fprintf(stdout, "Agent RequestBrowser %s\n", strip_path(service));
	fprintf(stdout, "  %s\n", url);
	__connmanctl_redraw_rl();

	request->message = dbus_message_ref(message);
	__connmanctl_agent_mode("Connected (yes/no)? ",
			request_browser_return, request);

	return NULL;
}

static void report_error_return(char *input, void *user_data)
{
	struct agent_data *request = user_data;

	switch (confirm_input(input)) {
	case 1:
		if (strcmp(request->interface, AGENT_INTERFACE) == 0)
			g_dbus_send_error(agent_connection, request->message,
					"net.connman.Agent.Error.Retry", NULL);
		else
			g_dbus_send_error(agent_connection, request->message,
					"net.connman.vpn.Agent.Error.Retry",
					NULL);
		break;
	case 0:
		g_dbus_send_reply(agent_connection, request->message,
				DBUS_TYPE_INVALID);
		break;
	default:
		return;
	}

	pending_message_remove(request);
	pending_command_complete("");
}

static DBusMessage *agent_report_error(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	struct agent_data *request = user_data;
	DBusMessageIter iter;
	char *path, *service, *error;

	if (handle_message(message, request, agent_report_error) == false)
		return NULL;

	dbus_message_iter_init(message, &iter);

	dbus_message_iter_get_basic(&iter, &path);
	service = strip_path(path);

	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &error);

	__connmanctl_save_rl();
	if (strcmp(request->interface, AGENT_INTERFACE) == 0)
		fprintf(stdout, "Agent ReportError %s\n", service);
	else
		fprintf(stdout, "VPN Agent ReportError %s\n", service);
	fprintf(stdout, "  %s\n", error);
	__connmanctl_redraw_rl();

	request->message = dbus_message_ref(message);
	__connmanctl_agent_mode("Retry (yes/no)? ", report_error_return,
			request);

	return NULL;
}

static DBusMessage *agent_report_peer_error(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	struct agent_data *request = user_data;
	char *path, *peer, *error;
	DBusMessageIter iter;

	if (handle_message(message, request,
				agent_report_peer_error) == false)
		return NULL;

	dbus_message_iter_init(message, &iter);

	dbus_message_iter_get_basic(&iter, &path);
	peer = strip_path(path);

	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &error);

	__connmanctl_save_rl();
	fprintf(stdout, "Agent ReportPeerError %s\n", peer);
	fprintf(stdout, "  %s\n", error);
	__connmanctl_redraw_rl();

	request->message = dbus_message_ref(message);
	__connmanctl_agent_mode("Retry (yes/no)? ",
				report_error_return, request);
	return NULL;
}

static void request_input_next(struct agent_data *request)
{
	int i;

	for (i = 0; request->input[i].attribute; i++) {
		if (request->input[i].requested == true) {
			if (request->input[i].func)
				__connmanctl_agent_mode(request->input[i].prompt,
						request->input[i].func,
						request);
			else
				request->input[i].requested = false;
			return;
		}
	}

	dbus_message_iter_close_container(&request->iter, &request->dict);

	g_dbus_send_message(agent_connection, request->reply);
	request->reply = NULL;

	pending_message_remove(request);
	pending_command_complete("");

	__connmanctl_redraw_rl();
}

static void request_input_append(struct agent_data *request,
		const char *attribute, char *value)
{
	__connmanctl_dbus_append_dict_entry(&request->dict, attribute,
			DBUS_TYPE_STRING, &value);
}

static void request_input_ssid_return(char *input,
		void *user_data)
{
	struct agent_data *request = user_data;
	int len = 0;

	if (input)
		len = strlen(input);

	if (len > 0 && len <= 32) {
		request->input[SSID].requested = false;
		request_input_append(request, request->input[SSID].attribute,
				input);

		request_input_next(request);
	}
}

static void request_input_passphrase_return(char *input, void *user_data)
{
	struct agent_data *request = user_data;
	int len = 0;

	/* TBD passphrase length checking */

	if (input)
		len = strlen(input);

	if (len == 0 && request->input[WPS].requested == false)
		return;

	request->input[PASSPHRASE].requested = false;

	if (len > 0) {
		request_input_append(request,
				request->input[PASSPHRASE].attribute, input);

		request->input[WPS].requested = false;
	}

	request_input_next(request);
}

static void request_input_string_return(char *input, void *user_data)
{
	struct agent_data *request = user_data;
	int i;

	for (i = 0; request->input[i].attribute; i++) {
		if (request->input[i].requested == true) {
			request_input_append(request,
					request->input[i].attribute, input);
			request->input[i].requested = false;
			break;
		}
	}

	request_input_next(request);
}

static void parse_agent_request(struct agent_data *request,
						DBusMessageIter *iter)
{
	DBusMessageIter dict, entry, variant, dict_entry;
	DBusMessageIter field_entry, field_value;
	char *field, *argument, *value;
	char *attr_type = NULL;
	int i;

	dbus_message_iter_recurse(iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {

		dbus_message_iter_recurse(&dict, &entry);

		dbus_message_iter_get_basic(&entry, &field);

		dbus_message_iter_next(&entry);

		dbus_message_iter_recurse(&entry, &variant);
		dbus_message_iter_recurse(&variant, &dict_entry);

		while (dbus_message_iter_get_arg_type(&dict_entry)
				== DBUS_TYPE_DICT_ENTRY) {
			dbus_message_iter_recurse(&dict_entry, &field_entry);

			dbus_message_iter_get_basic(&field_entry, &argument);

			dbus_message_iter_next(&field_entry);

			dbus_message_iter_recurse(&field_entry, &field_value);

			if (strcmp(argument, "Type") == 0) {
				dbus_message_iter_get_basic(&field_value,
						&value);
				attr_type = g_strdup(value);
			}

			dbus_message_iter_next(&dict_entry);
		}

		for (i = 0; request->input[i].attribute; i++) {
			if (strcmp(field, request->input[i].attribute) == 0) {
				request->input[i].requested = true;
				break;
			}
		}

		g_free(attr_type);
		attr_type = NULL;

		dbus_message_iter_next(&dict);
	}
}

static DBusMessage *agent_request_input(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	struct agent_data *request = user_data;
	DBusMessageIter iter, dict;
	char *service, *str;

	if (handle_message(message, request, agent_request_input) == false)
		return NULL;

	dbus_message_iter_init(message, &iter);

	dbus_message_iter_get_basic(&iter, &str);
	service = strip_path(str);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &dict);

	__connmanctl_save_rl();
	if (strcmp(request->interface, AGENT_INTERFACE) == 0)
		fprintf(stdout, "Agent RequestInput %s\n", service);
	else
		fprintf(stdout, "VPN Agent RequestInput %s\n", service);
	__connmanctl_dbus_print(&dict, "  ", " = ", "\n");
	fprintf(stdout, "\n");

	parse_agent_request(request, &iter);

	request->reply = dbus_message_new_method_return(message);
	dbus_message_iter_init_append(request->reply, &request->iter);

	dbus_message_iter_open_container(&request->iter, DBUS_TYPE_ARRAY,
                        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                        DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
                        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
			&request->dict);

	request_input_next(request);

	return NULL;
}

static void request_authorization_return(char *input, void *user_data)
{
	struct agent_data *request = user_data;

	switch (confirm_input(input)) {
	case 1:
		request->reply = dbus_message_new_method_return(
							request->message);
		dbus_message_iter_init_append(request->reply, &request->iter);

		dbus_message_iter_open_container(&request->iter,
				DBUS_TYPE_ARRAY,
				DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
				DBUS_TYPE_STRING_AS_STRING
				DBUS_TYPE_VARIANT_AS_STRING
				DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
				&request->dict);
		dbus_message_iter_close_container(&request->iter,
							&request->dict);
		g_dbus_send_message(agent_connection, request->reply);
		request->reply = NULL;
		break;
	case 0:
		 g_dbus_send_error(agent_connection, request->message,
				 "net.connman.Agent.Error.Rejected", NULL);
		 break;
	default:
		 g_dbus_send_error(agent_connection, request->message,
				 "net.connman.Agent.Error.Canceled", NULL);
		 break;
	}

	pending_message_remove(request);
	pending_command_complete("");
}

static DBusMessage *
agent_request_peer_authorization(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	struct agent_data *request = user_data;
	DBusMessageIter iter, dict;
	char *peer, *str;
	bool input;
	int i;

	if (handle_message(message, request, agent_request_peer_authorization)
								== false)
		return NULL;

	dbus_message_iter_init(message, &iter);

	dbus_message_iter_get_basic(&iter, &str);
	peer = strip_path(str);

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &dict);

	__connmanctl_save_rl();
	fprintf(stdout, "Agent RequestPeerAuthorization %s\n", peer);
	__connmanctl_dbus_print(&dict, "  ", " = ", "\n");
	fprintf(stdout, "\n");

	parse_agent_request(request, &iter);

	for (input = false, i = 0; request->input[i].attribute; i++) {
		if (request->input[i].requested == true) {
			input = true;
			break;
		}
	}

	if (!input) {
		request->message = dbus_message_ref(message);
		__connmanctl_agent_mode("Accept connection (yes/no)? ",
					request_authorization_return, request);
		return NULL;
	}

	request->reply = dbus_message_new_method_return(message);
	dbus_message_iter_init_append(request->reply, &request->iter);

	dbus_message_iter_open_container(&request->iter, DBUS_TYPE_ARRAY,
                        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                        DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
                        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
			&request->dict);

	request_input_next(request);

	return NULL;
}

static const GDBusMethodTable agent_methods[] = {
	{ GDBUS_ASYNC_METHOD("Release", NULL, NULL, agent_release) },
	{ GDBUS_ASYNC_METHOD("Cancel", NULL, NULL, agent_cancel) },
	{ GDBUS_ASYNC_METHOD("RequestBrowser",
				GDBUS_ARGS({ "service", "o" },
					{ "url", "s" }),
				NULL, agent_request_browser) },
	{ GDBUS_ASYNC_METHOD("ReportError",
				GDBUS_ARGS({ "service", "o" },
					{ "error", "s" }),
				NULL, agent_report_error) },
	{ GDBUS_ASYNC_METHOD("ReportPeerError",
				GDBUS_ARGS({ "peer", "o" },
					{ "error", "s" }),
				NULL, agent_report_peer_error) },
	{ GDBUS_ASYNC_METHOD("RequestInput",
				GDBUS_ARGS({ "service", "o" },
					{ "fields", "a{sv}" }),
				GDBUS_ARGS({ "fields", "a{sv}" }),
				agent_request_input) },
	{ GDBUS_ASYNC_METHOD("RequestPeerAuthorization",
				GDBUS_ARGS({ "peer", "o" },
					{ "fields", "a{sv}" }),
				GDBUS_ARGS({ "fields", "a{sv}" }),
				agent_request_peer_authorization) },
	{ },
};

static int agent_register_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	DBusConnection *connection = user_data;

	if (error) {
		g_dbus_unregister_interface(connection, agent_path(),
				AGENT_INTERFACE);
		fprintf(stderr, "Error registering Agent: %s\n", error);
		return 0;
	}

	agent_request.registered = true;
	fprintf(stdout, "Agent registered\n");

	return -EINPROGRESS;
}

static void append_path(DBusMessageIter *iter, void *user_data)
{
	const char *path = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);
}

int __connmanctl_agent_register(DBusConnection *connection)
{
	char *path = agent_path();
	int result;

	if (agent_request.registered == true) {
		fprintf(stderr, "Agent already registered\n");
		return -EALREADY;
	}

	agent_connection = connection;

	if (!g_dbus_register_interface(connection, path,
					AGENT_INTERFACE, agent_methods,
					NULL, NULL, &agent_request, NULL)) {
		fprintf(stderr, "Error: Failed to register Agent callbacks\n");
		return 0;
	}

	result = __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE,
			CONNMAN_PATH, "net.connman.Manager", "RegisterAgent",
			agent_register_return, connection, append_path, path);

	if (result != -EINPROGRESS) {
		g_dbus_unregister_interface(connection, agent_path(),
				AGENT_INTERFACE);

		fprintf(stderr, "Error: Failed to register Agent\n");
	}

	return result;
}

static int agent_unregister_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	if (error) {
		fprintf(stderr, "Error unregistering Agent: %s\n", error);
		return 0;
	}

	agent_request.registered = false;
	fprintf(stdout, "Agent unregistered\n");

	return 0;
}

int __connmanctl_agent_unregister(DBusConnection *connection)
{
	char *path = agent_path();
	int result;

	if (agent_request.registered == false) {
		fprintf(stderr, "Agent not registered\n");
		return -EALREADY;
	}

	g_dbus_unregister_interface(connection, agent_path(), AGENT_INTERFACE);

	result = __connmanctl_dbus_method_call(connection, CONNMAN_SERVICE,
			CONNMAN_PATH, "net.connman.Manager", "UnregisterAgent",
			agent_unregister_return, NULL, append_path, path);

	if (result != -EINPROGRESS)
		fprintf(stderr, "Error: Failed to unregister Agent\n");

	return result;
}

static const GDBusMethodTable vpn_agent_methods[] = {
	{ GDBUS_ASYNC_METHOD("Release", NULL, NULL, agent_release) },
	{ GDBUS_ASYNC_METHOD("Cancel", NULL, NULL, agent_cancel) },
	{ GDBUS_ASYNC_METHOD("ReportError",
				GDBUS_ARGS({ "service", "o" },
					{ "error", "s" }),
				NULL, agent_report_error) },
	{ GDBUS_ASYNC_METHOD("RequestInput",
				GDBUS_ARGS({ "service", "o" },
					{ "fields", "a{sv}" }),
				GDBUS_ARGS({ "fields", "a{sv}" }),
				agent_request_input) },
	{ },
};

static int vpn_agent_register_return(DBusMessageIter *iter, const char *error,
		void *user_data)
{
	DBusConnection *connection = user_data;

	if (error) {
		g_dbus_unregister_interface(connection, agent_path(),
				VPN_AGENT_INTERFACE);
		fprintf(stderr, "Error registering VPN Agent: %s\n", error);
		return 0;
	}

	vpn_agent_request.registered = true;
	fprintf(stdout, "VPN Agent registered\n");

	return -EINPROGRESS;
}

int __connmanctl_vpn_agent_register(DBusConnection *connection)
{
	char *path = agent_path();
	int result;

	if (vpn_agent_request.registered == true) {
		fprintf(stderr, "VPN Agent already registered\n");
		return -EALREADY;
	}

	agent_connection = connection;

	if (!g_dbus_register_interface(connection, path,
			VPN_AGENT_INTERFACE, vpn_agent_methods,
			NULL, NULL, &vpn_agent_request, NULL)) {
		fprintf(stderr, "Error: Failed to register VPN Agent "
				"callbacks\n");
		return 0;
	}

	result = __connmanctl_dbus_method_call(connection, VPN_SERVICE,
			VPN_PATH, "net.connman.vpn.Manager", "RegisterAgent",
			vpn_agent_register_return, connection, append_path,
			path);

	if (result != -EINPROGRESS) {
		g_dbus_unregister_interface(connection, agent_path(),
				VPN_AGENT_INTERFACE);

		fprintf(stderr, "Error: Failed to register VPN Agent\n");
	}

	return result;
}

static int vpn_agent_unregister_return(DBusMessageIter *iter,
		const char *error, void *user_data)
{
	if (error) {
		fprintf(stderr, "Error unregistering VPN Agent: %s\n", error);
		return 0;
	}

	vpn_agent_request.registered = false;
	fprintf(stdout, "VPN Agent unregistered\n");

	return 0;
}

int __connmanctl_vpn_agent_unregister(DBusConnection *connection)
{
	char *path = agent_path();
	int result;

	if (vpn_agent_request.registered == false) {
		fprintf(stderr, "VPN Agent not registered\n");
		return -EALREADY;
	}

	g_dbus_unregister_interface(connection, agent_path(),
			VPN_AGENT_INTERFACE);

	result = __connmanctl_dbus_method_call(connection, VPN_SERVICE,
			VPN_PATH, "net.connman.vpn.Manager", "UnregisterAgent",
			vpn_agent_unregister_return, NULL, append_path, path);

	if (result != -EINPROGRESS)
		fprintf(stderr, "Error: Failed to unregister VPN Agent\n");

	return result;
}
