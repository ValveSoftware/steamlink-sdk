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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <gdbus.h>
#include <connman/agent.h>
#include <connman/setting.h>
#include <connman/service.h>

#include "connman.h"

static bool check_reply_has_dict(DBusMessage *reply)
{
	const char *signature = DBUS_TYPE_ARRAY_AS_STRING
		DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
		DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_VARIANT_AS_STRING
		DBUS_DICT_ENTRY_END_CHAR_AS_STRING;

	if (dbus_message_has_signature(reply, signature))
		return true;

	connman_warn("Reply %s to %s from %s has wrong signature %s",
			signature,
			dbus_message_get_interface(reply),
			dbus_message_get_sender(reply),
			dbus_message_get_signature(reply));

	return false;
}

struct request_input_reply {
	struct connman_service *service;
	struct connman_peer *peer;
	union {
		authentication_cb_t service_callback;
		peer_wps_cb_t peer_callback;
	};
	bool wps_requested;
	void *user_data;
};

static void request_input_passphrase_reply(DBusMessage *reply, void *user_data)
{
	struct request_input_reply *passphrase_reply = user_data;
	bool values_received = false;
	bool wps = false;
	const char *error = NULL;
	char *identity = NULL;
	char *passphrase = NULL;
	char *wpspin = NULL;
	char *key;
	char *name = NULL;
	int name_len = 0;
	DBusMessageIter iter, dict;

	if (!reply) {
		error = CONNMAN_ERROR_INTERFACE ".OperationAborted";
		goto done;
	}

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		error = dbus_message_get_error_name(reply);
		goto done;
	}

	if (!check_reply_has_dict(reply))
		goto done;

	values_received = true;

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &dict);
	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;

		dbus_message_iter_recurse(&dict, &entry);
		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
			break;

		dbus_message_iter_get_basic(&entry, &key);

		if (g_str_equal(key, "Identity")) {
			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			dbus_message_iter_get_basic(&value, &identity);

		} else if (g_str_equal(key, "Passphrase")) {
			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			dbus_message_iter_get_basic(&value, &passphrase);

		} else if (g_str_equal(key, "WPS")) {
			wps = true;

			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			dbus_message_iter_get_basic(&value, &wpspin);
			break;
		} else if (g_str_equal(key, "Name")) {
			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			dbus_message_iter_get_basic(&value, &name);
			name_len = strlen(name);
		} else if (g_str_equal(key, "SSID")) {
			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			if (dbus_message_iter_get_arg_type(&value)
							!= DBUS_TYPE_VARIANT)
				break;
			if (dbus_message_iter_get_element_type(&value)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_get_fixed_array(&value, &name,
							&name_len);
		}
		dbus_message_iter_next(&dict);
	}

done:
	passphrase_reply->service_callback(passphrase_reply->service,
					values_received, name, name_len,
					identity, passphrase, wps, wpspin,
					error, passphrase_reply->user_data);

	g_free(passphrase_reply);
}

static void request_input_append_alternates(DBusMessageIter *iter,
							void *user_data)
{
	const char *str = user_data;
	char **alternates, **alternative;

	if (!str)
		return;

	alternates = g_strsplit(str, ",", 0);
	if (!alternates)
		return;

	for (alternative = alternates; *alternative; alternative++)
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING,
								alternative);

	g_strfreev(alternates);
}

static void request_input_append_identity(DBusMessageIter *iter,
							void *user_data)
{
	char *str = "string";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	str = "mandatory";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);
}

static void request_input_append_passphrase(DBusMessageIter *iter,
							void *user_data)
{
	struct connman_service *service = user_data;
	char *value;
	const char *phase2;

	switch (__connman_service_get_security(service)) {
	case CONNMAN_SERVICE_SECURITY_WEP:
		value = "wep";
		break;
	case CONNMAN_SERVICE_SECURITY_PSK:
		value = "psk";
		break;
	case CONNMAN_SERVICE_SECURITY_8021X:
		phase2 = __connman_service_get_phase2(service);

		if (phase2 && (
				g_str_has_suffix(phase2, "GTC") ||
				g_str_has_suffix(phase2, "OTP")))
			value = "response";
		else
			value = "passphrase";

		break;
	default:
		value = "string";
		break;
	}
	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &value);
	value = "mandatory";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &value);

	if (__connman_service_wps_enabled(service)) {
		connman_dbus_dict_append_array(iter, "Alternates",
					DBUS_TYPE_STRING,
					request_input_append_alternates,
					"WPS");
	}
}

struct request_wps_data {
	bool peer;
};

static void request_input_append_wps(DBusMessageIter *iter, void *user_data)
{
	struct request_wps_data *wps = user_data;
	const char *str = "wpspin";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	if (wps && wps->peer)
		str = "mandatory";
	else
		str = "alternate";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);
}

static void request_input_append_name(DBusMessageIter *iter, void *user_data)
{
	const char *str = "string";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	str = "mandatory";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);
	connman_dbus_dict_append_array(iter, "Alternates",
				DBUS_TYPE_STRING,
				request_input_append_alternates,
				"SSID");
}

static void request_input_append_ssid(DBusMessageIter *iter, void *user_data)
{
	const char *str = "ssid";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	str = "alternate";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);
}

static void request_input_append_password(DBusMessageIter *iter,
							void *user_data)
{
	char *str = "passphrase";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	str = "mandatory";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);
}

struct previous_passphrase_data {
	const char *passphrase;
	const char *type;
};

static void request_input_append_previouspassphrase(DBusMessageIter *iter,
							void *user_data)
{
	struct previous_passphrase_data *data = user_data;
	const char *requirement = "informational";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &data->type);

	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &requirement);

	connman_dbus_dict_append_basic(iter, "Value",
				DBUS_TYPE_STRING, &data->passphrase);
}

static void previous_passphrase_handler(DBusMessageIter *iter,
					struct connman_service *service)
{
	enum connman_service_security security;
	struct previous_passphrase_data data;
	struct connman_network *network;

	network = __connman_service_get_network(service);
	data.passphrase = connman_network_get_string(network, "WiFi.PinWPS");

	if (connman_network_get_bool(network, "WiFi.UseWPS") &&
						data.passphrase) {
		data.type = "wpspin";
	} else {
		data.passphrase = __connman_service_get_passphrase(service);
		if (!data.passphrase)
			return;

		security = __connman_service_get_security(service);
		switch (security) {
		case CONNMAN_SERVICE_SECURITY_WEP:
			data.type = "wep";
			break;
		case CONNMAN_SERVICE_SECURITY_PSK:
			data.type  = "psk";
			break;
		/*
		 * This should never happen: no passphrase is set if security
		 * is not one of the above. */
		default:
			break;
		}
	}

	connman_dbus_dict_append_dict(iter, "PreviousPassphrase",
			request_input_append_previouspassphrase, &data);
}

static void request_input_login_reply(DBusMessage *reply, void *user_data)
{
	struct request_input_reply *username_password_reply = user_data;
	const char *error = NULL;
	bool values_received = false;
	char *username = NULL;
	char *password = NULL;
	char *key;
	DBusMessageIter iter, dict;

	if (!reply) {
		error = CONNMAN_ERROR_INTERFACE ".OperationAborted";
		goto done;
	}

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		error = dbus_message_get_error_name(reply);
		goto done;
	}

	if (!check_reply_has_dict(reply))
		goto done;

	values_received = true;

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &dict);
	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;

		dbus_message_iter_recurse(&dict, &entry);
		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
			break;

		dbus_message_iter_get_basic(&entry, &key);

		if (g_str_equal(key, "Username")) {
			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			dbus_message_iter_get_basic(&value, &username);

		} else if (g_str_equal(key, "Password")) {
			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry) !=
							DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			dbus_message_iter_get_basic(&value, &password);
		}

		dbus_message_iter_next(&dict);
	}

done:
	username_password_reply->service_callback(
			username_password_reply->service, values_received,
			NULL, 0, username, password, FALSE, NULL, error,
			username_password_reply->user_data);

	g_free(username_password_reply);
}

int __connman_agent_request_passphrase_input(struct connman_service *service,
				authentication_cb_t callback,
				const char *dbus_sender, void *user_data)
{
	DBusMessage *message;
	const char *path, *agent_sender, *agent_path;
	DBusMessageIter iter;
	DBusMessageIter dict;
	struct request_input_reply *passphrase_reply;
	int err;
	void *agent;

	agent = connman_agent_get_info(dbus_sender, &agent_sender,
							&agent_path);

	DBG("agent %p service %p path %s", agent, service, agent_path);

	if (!service || !agent || !agent_path || !callback)
		return -ESRCH;

	message = dbus_message_new_method_call(agent_sender, agent_path,
					CONNMAN_AGENT_INTERFACE,
					"RequestInput");
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);

	path = __connman_service_get_path(service);
	dbus_message_iter_append_basic(&iter,
				DBUS_TYPE_OBJECT_PATH, &path);

	connman_dbus_dict_open(&iter, &dict);

	if (__connman_service_is_hidden(service)) {
		connman_dbus_dict_append_dict(&dict, "Name",
					request_input_append_name, NULL);
		connman_dbus_dict_append_dict(&dict, "SSID",
					request_input_append_ssid, NULL);
	}

	if (__connman_service_get_security(service) ==
			CONNMAN_SERVICE_SECURITY_8021X) {
		connman_dbus_dict_append_dict(&dict, "Identity",
					request_input_append_identity, service);
	}

	if (__connman_service_get_security(service) !=
			CONNMAN_SERVICE_SECURITY_NONE) {
		connman_dbus_dict_append_dict(&dict, "Passphrase",
				request_input_append_passphrase, service);

		previous_passphrase_handler(&dict, service);
	}

	if (__connman_service_wps_enabled(service))
		connman_dbus_dict_append_dict(&dict, "WPS",
				request_input_append_wps, NULL);

	connman_dbus_dict_close(&iter, &dict);

	passphrase_reply = g_try_new0(struct request_input_reply, 1);
	if (!passphrase_reply) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	passphrase_reply->service = service;
	passphrase_reply->service_callback = callback;
	passphrase_reply->user_data = user_data;

	err = connman_agent_queue_message(service, message,
			connman_timeout_input_request(),
			request_input_passphrase_reply,
			passphrase_reply, agent);

	if (err < 0 && err != -EBUSY) {
		DBG("error %d sending agent message", err);
		dbus_message_unref(message);
		g_free(passphrase_reply);
		return err;
	}

	dbus_message_unref(message);

	return -EINPROGRESS;
}

int __connman_agent_request_login_input(struct connman_service *service,
				authentication_cb_t callback, void *user_data)
{
	DBusMessage *message;
	const char *path, *agent_sender, *agent_path;
	DBusMessageIter iter;
	DBusMessageIter dict;
	struct request_input_reply *username_password_reply;
	int err;
	void *agent;

	agent = connman_agent_get_info(NULL, &agent_sender, &agent_path);

	if (!service || !agent || !agent_path || !callback)
		return -ESRCH;

	message = dbus_message_new_method_call(agent_sender, agent_path,
					CONNMAN_AGENT_INTERFACE,
					"RequestInput");
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);

	path = __connman_service_get_path(service);
	dbus_message_iter_append_basic(&iter,
				DBUS_TYPE_OBJECT_PATH, &path);

	connman_dbus_dict_open(&iter, &dict);

	connman_dbus_dict_append_dict(&dict, "Username",
				request_input_append_identity, service);

	connman_dbus_dict_append_dict(&dict, "Password",
				request_input_append_password, service);

	connman_dbus_dict_close(&iter, &dict);

	username_password_reply = g_try_new0(struct request_input_reply, 1);
	if (!username_password_reply) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	username_password_reply->service = service;
	username_password_reply->service_callback = callback;
	username_password_reply->user_data = user_data;

	err = connman_agent_queue_message(service, message,
			connman_timeout_input_request(),
			request_input_login_reply, username_password_reply,
			agent);
	if (err < 0 && err != -EBUSY) {
		DBG("error %d sending agent request", err);
		dbus_message_unref(message);
		g_free(username_password_reply);
		return err;
	}

	dbus_message_unref(message);

	return -EINPROGRESS;
}

struct request_browser_reply_data {
	struct connman_service *service;
	browser_authentication_cb_t callback;
	void *user_data;
};

static void request_browser_reply(DBusMessage *reply, void *user_data)
{
	struct request_browser_reply_data *browser_reply_data = user_data;
	bool result = false;
	const char *error = NULL;

	if (!reply) {
		error = CONNMAN_ERROR_INTERFACE ".OperationAborted";
		goto done;
	}

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		error = dbus_message_get_error_name(reply);
		goto done;
	}

	result = true;

done:
	browser_reply_data->callback(browser_reply_data->service, result,
					error, browser_reply_data->user_data);
	g_free(browser_reply_data);
}

int __connman_agent_request_browser(struct connman_service *service,
				browser_authentication_cb_t callback,
				const char *url, void *user_data)
{
	struct request_browser_reply_data *browser_reply_data;
	DBusMessage *message;
	DBusMessageIter iter;
	const char *path, *agent_sender, *agent_path;
	int err;
	void *agent;

	agent = connman_agent_get_info(NULL, &agent_sender, &agent_path);

	if (!service || !agent || !agent_path || !callback)
		return -ESRCH;

	if (!url)
		url = "";

	message = dbus_message_new_method_call(agent_sender, agent_path,
					CONNMAN_AGENT_INTERFACE,
					"RequestBrowser");
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);

	path = __connman_service_get_path(service);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);

	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &url);

	browser_reply_data = g_try_new0(struct request_browser_reply_data, 1);
	if (!browser_reply_data) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	browser_reply_data->service = service;
	browser_reply_data->callback = callback;
	browser_reply_data->user_data = user_data;

	err = connman_agent_queue_message(service, message,
				connman_timeout_browser_launch(),
				request_browser_reply, browser_reply_data,
				agent);

	if (err < 0 && err != -EBUSY) {
		DBG("error %d sending browser request", err);
		dbus_message_unref(message);
		g_free(browser_reply_data);
		return err;
	}

	dbus_message_unref(message);

	return -EINPROGRESS;
}

int __connman_agent_report_peer_error(struct connman_peer *peer,
					const char *path, const char *error,
					report_error_cb_t callback,
					const char *dbus_sender,
					void *user_data)
{
	return connman_agent_report_error_full(peer, path, "ReportPeerError",
				error, callback, dbus_sender, user_data);
}

static void request_peer_authorization_reply(DBusMessage *reply,
							void *user_data)
{
	struct request_input_reply *auth_reply = user_data;
	DBusMessageIter iter, dict;
	const char *error = NULL;
	bool choice_done = false;
	char *wpspin = NULL;
	char *key;

	if (!reply) {
		error = CONNMAN_ERROR_INTERFACE ".OperationAborted";
		goto done;
	}

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		error = dbus_message_get_error_name(reply);
		goto done;
	}

	if (!check_reply_has_dict(reply))
		goto done;

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &dict);
	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;

		dbus_message_iter_recurse(&dict, &entry);
		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
			break;

		dbus_message_iter_get_basic(&entry, &key);

		if (g_str_equal(key, "WPS")) {
			choice_done = true;

			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			dbus_message_iter_get_basic(&value, &wpspin);
			break;
		}
		dbus_message_iter_next(&dict);
	}

	if (!auth_reply->wps_requested)
		choice_done = true;

done:
	auth_reply->peer_callback(auth_reply->peer, choice_done, wpspin,
						error, auth_reply->user_data);

	g_free(auth_reply);
}

int __connman_agent_request_peer_authorization(struct connman_peer *peer,
						peer_wps_cb_t callback,
						bool wps_requested,
						const char *dbus_sender,
						void *user_data)
{
	struct request_wps_data wps = { .peer = true };
	const char *path, *agent_sender, *agent_path;
	struct request_input_reply *auth_reply;
	DBusMessageIter dict, iter;
	DBusMessage *message;
	void *agent;
	int err;

	agent = connman_agent_get_info(dbus_sender, &agent_sender,
							&agent_path);
	DBG("agent %p peer %p path %s", agent, peer, agent_path);

	if (!peer || !agent || !agent_path || !callback)
		return -ESRCH;

	message = dbus_message_new_method_call(agent_sender, agent_path,
			CONNMAN_AGENT_INTERFACE, "RequestPeerAuthorization");
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);

	path = __connman_peer_get_path(peer);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);

	connman_dbus_dict_open(&iter, &dict);

	if (wps_requested)
		connman_dbus_dict_append_dict(&dict, "WPS",
					request_input_append_wps, &wps);

	connman_dbus_dict_close(&iter, &dict);

	auth_reply = g_try_new0(struct request_input_reply, 1);
	if (!auth_reply) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	auth_reply->peer = peer;
	auth_reply->peer_callback = callback;
	auth_reply->wps_requested = wps_requested;
	auth_reply->user_data = user_data;

	err = connman_agent_queue_message(peer, message,
					connman_timeout_input_request(),
					request_peer_authorization_reply,
					auth_reply, agent);
	if (err < 0 && err != -EBUSY) {
		DBG("error %d sending agent message", err);
		dbus_message_unref(message);
		g_free(auth_reply);
		return err;
	}

	dbus_message_unref(message);

	return -EINPROGRESS;
}
