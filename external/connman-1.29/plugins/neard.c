/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2013  Intel Corporation. All rights reserved.
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#include <glib.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/dbus.h>
#include <connman/technology.h>
#include <connman/provision.h>

#include <gdbus.h>

#define TIMEOUT 30000

#define NEARD_SERVICE "org.neard"
#define NEARD_PATH "/"
#define NEARD_MANAGER_INTERFACE "org.neard.Manager"
#define NEARD_AGENT_INTERFACE "org.neard.HandoverAgent"
#define NEARD_ERROR_INTERFACE "org.neard.HandoverAgent.Error"

#define AGENT_PATH "/net/connman/neard_handover_agent"
#define AGENT_TYPE "wifi"

#define NEARD_SERVICE_GROUP "service_neard_provision"

/* Configuration keys */
#define SERVICE_KEY_TYPE               "Type"
#define SERVICE_KEY_SSID               "SSID"
#define SERVICE_KEY_PASSPHRASE         "Passphrase"
#define SERVICE_KEY_HIDDEN             "Hidden"

struct data_elements {
	unsigned int value;
	unsigned int length;
	bool fixed_length;
};

typedef enum {
	DE_AUTHENTICATION_TYPE = 0,
	DE_NETWORK_KEY         = 1,
	DE_SSID                = 2,
	DE_MAX                 = 3,
} DEid;

static const struct data_elements  DEs[DE_MAX] = {
	{ 0x1003, 2,  TRUE  },
	{ 0x1027, 64, FALSE },
	{ 0x1045, 32, FALSE },
};

#define DE_VAL_OPEN                 0x0001
#define DE_VAL_PSK                  0x0022

struct wifi_sc {
	gchar *ssid;
	gchar *passphrase;
};

static DBusConnection *connection = NULL;
DBusPendingCall *register_call = NULL;
bool agent_registered = false;
static guint watch_id = 0;

static int set_2b_into_tlv(uint8_t *tlv_msg, uint16_t val)
{
	uint16_t ne_val;
	uint8_t *ins;

	ins = (uint8_t *) &ne_val;

	ne_val = htons(val);
	tlv_msg[0] = ins[0];
	tlv_msg[1] = ins[1];

	return 2;
}

static int set_byte_array_into_tlv(uint8_t *tlv_msg,
					uint8_t *array, int length)
{
	memcpy((void *)tlv_msg, (void *)array, length);
	return length;
}

static uint8_t *encode_to_tlv(const char *ssid, const char *psk, int *length)
{
	uint16_t ssid_len, psk_len;
	uint8_t *tlv_msg;
	int pos = 0;

	if (!ssid || !length)
		return NULL;

	ssid_len = strlen(ssid);

	*length = 6 + 4 + ssid_len;
	if (psk) {
		psk_len = strlen(psk);
		*length += 4 + psk_len;
	} else
		psk_len = 0;

	tlv_msg = g_try_malloc0(sizeof(uint8_t) * (*length));
	if (!tlv_msg)
		return NULL;

	pos += set_2b_into_tlv(tlv_msg+pos, DEs[DE_SSID].value);
	pos += set_2b_into_tlv(tlv_msg+pos, ssid_len);
	pos += set_byte_array_into_tlv(tlv_msg+pos, (uint8_t *)ssid, ssid_len);

	pos += set_2b_into_tlv(tlv_msg+pos, DEs[DE_AUTHENTICATION_TYPE].value);
	pos += set_2b_into_tlv(tlv_msg+pos,
					DEs[DE_AUTHENTICATION_TYPE].length);
	if (psk) {
		pos += set_2b_into_tlv(tlv_msg+pos, DE_VAL_PSK);
		pos += set_2b_into_tlv(tlv_msg+pos, DEs[DE_NETWORK_KEY].value);
		pos += set_2b_into_tlv(tlv_msg+pos, psk_len);
		pos += set_byte_array_into_tlv(tlv_msg+pos,
						(uint8_t *)psk, psk_len);
	} else
		pos += set_2b_into_tlv(tlv_msg+pos, DE_VAL_OPEN);

	return tlv_msg;
}

static DBusMessage *get_reply_on_error(DBusMessage *message, int error)
{
	if (error == ENOTSUP)
		return g_dbus_create_error(message,
					NEARD_ERROR_INTERFACE ".NotSupported",
					"Operation is not supported");

	return g_dbus_create_error(message,
		NEARD_ERROR_INTERFACE ".Failed", "Invalid parameters");
}

static int parse_request_oob_params(DBusMessage *message,
				const uint8_t **tlv_msg, int *length)
{
	DBusMessageIter iter;
	DBusMessageIter array;
	DBusMessageIter dict_entry;
	const char *key;
	int arg_type;

	if (!tlv_msg || !length)
		return -EINVAL;

	dbus_message_iter_init(message, &iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(&iter, &array);
	arg_type = dbus_message_iter_get_arg_type(&array);
	if (arg_type != DBUS_TYPE_DICT_ENTRY)
		return -EINVAL;

	while (arg_type != DBUS_TYPE_INVALID) {
		dbus_message_iter_recurse(&array, &dict_entry);
		if (dbus_message_iter_get_arg_type(&dict_entry) !=
							DBUS_TYPE_STRING)
			return -EINVAL;

		dbus_message_iter_get_basic(&dict_entry, &key);
		if (g_strcmp0(key, "WSC") == 0) {
			DBusMessageIter value;
			DBusMessageIter fixed_array;

			dbus_message_iter_next(&dict_entry);
			dbus_message_iter_recurse(&dict_entry, &value);

			if (dbus_message_iter_get_arg_type(&value) !=
							DBUS_TYPE_ARRAY)
				return -EINVAL;

			dbus_message_iter_recurse(&value, &fixed_array);
			dbus_message_iter_get_fixed_array(&fixed_array,
							tlv_msg, length);
			return 0;
		}

		dbus_message_iter_next(&array);
		arg_type = dbus_message_iter_get_arg_type(&array);
	}

	return -EINVAL;
}

static DBusMessage *create_request_oob_reply(DBusMessage *message)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *ssid, *psk;
	uint8_t *tlv_msg;
	int length;

	if (!connman_technology_get_wifi_tethering(&ssid, &psk))
		return get_reply_on_error(message, ENOTSUP);

	tlv_msg = encode_to_tlv(ssid, psk, &length);
	if (!tlv_msg)
		return get_reply_on_error(message, ENOTSUP);

	reply = dbus_message_new_method_return(message);
	if (!reply)
		goto out;

	dbus_message_iter_init_append(reply, &iter);

	connman_dbus_dict_open(&iter, &dict);

	connman_dbus_dict_append_fixed_array(&dict, "WSC",
					DBUS_TYPE_BYTE, &tlv_msg, length);

	dbus_message_iter_close_container(&iter, &dict);

out:
	g_free(tlv_msg);

	return reply;
}

static DBusMessage *request_oob_method(DBusConnection *dbus_conn,
					DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;

	DBG("");

	dbus_message_iter_init(message, &iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		return get_reply_on_error(message, EINVAL);

	return create_request_oob_reply(message);
}

static void free_wifi_sc(struct wifi_sc *wsc)
{
	if (!wsc)
		return;

	g_free(wsc->ssid);
	g_free(wsc->passphrase);

	g_free(wsc);
}

static inline int get_2b_from_tlv(const uint8_t *tlv_msg, uint16_t *val)
{
	*val = ntohs(tlv_msg[0] | (tlv_msg[1] << 8));

	return 2;
}

static uint8_t *get_byte_array_from_tlv(const uint8_t *tlv_msg, int length)
{
	uint8_t *array;

	array = g_try_malloc0(sizeof(uint8_t) * length);
	if (!array)
		return NULL;

	memcpy((void *)array, (void *)tlv_msg, length*sizeof(uint8_t));

	return array;
}

static char *get_string_from_tlv(const uint8_t *tlv_msg, int length)
{
	char *str;

	str = g_try_malloc0((sizeof(char) * length) + 1);
	if (!str)
		return NULL;

	memcpy((void *)str, (void *)tlv_msg, length*sizeof(char));

	return str;
}

static inline DEid get_de_id(uint16_t attr)
{
	int i;

	for (i = 0; i < DE_MAX && DEs[i].value != 0; i++) {
		if (attr == DEs[i].value)
			return i;
	}

	return DE_MAX;
}

static inline bool is_de_length_fine(DEid id, uint16_t length)
{
	if (DEs[id].fixed_length)
		return (length == DEs[id].length);

	return (length <= DEs[id].length);
}

static char *get_hexstr_from_byte_array(const uint8_t *bt, int length)
{
	char *hex_str;
	int i, j;

	hex_str = g_try_malloc0(((length*2)+1) * sizeof(char));
	if (!hex_str)
		return NULL;

	for (i = 0, j = 0; i < length; i++, j += 2)
		snprintf(hex_str+j, 3, "%02x", bt[i]);

	return hex_str;
}

static struct wifi_sc *decode_from_tlv(const uint8_t *tlv_msg, int length)
{
	struct wifi_sc *wsc;
	uint16_t attr, len;
	uint8_t *bt;
	int pos;
	DEid id;

	if (!tlv_msg || length == 0)
		return NULL;

	wsc = g_try_malloc0(sizeof(struct wifi_sc));
	if (!wsc)
		return NULL;

	pos = 0;
	while (pos < length) {
		pos += get_2b_from_tlv(tlv_msg+pos, &attr);
		pos += get_2b_from_tlv(tlv_msg+pos, &len);

		if (len > length - pos)
			goto error;

		id = get_de_id(attr);
		if (id == DE_MAX) {
			pos += len;
			continue;
		}

		if (!is_de_length_fine(id, len))
			goto error;

		switch (id) {
		case DE_AUTHENTICATION_TYPE:
			break;
		case DE_NETWORK_KEY:
			wsc->passphrase = get_string_from_tlv(tlv_msg+pos,
									len);
			break;
		case DE_SSID:
			bt = get_byte_array_from_tlv(tlv_msg+pos, len);
			wsc->ssid = get_hexstr_from_byte_array(bt, len);
			g_free(bt);

			break;
		case DE_MAX:
			/* Falling back. Should never happen though. */
		default:
			break;
		}

		pos += len;
	}

	return wsc;

error:
	free_wifi_sc(wsc);
	return NULL;
}

static int handle_wcs_data(const uint8_t *tlv_msg, int length)
{
	struct wifi_sc *wsc = NULL;
	GKeyFile *keyfile = NULL;
	int ret = -EINVAL;

	wsc = decode_from_tlv(tlv_msg, length);
	if (!wsc)
		return -EINVAL;

	if (!wsc->ssid)
		goto out;

	keyfile = g_key_file_new();
	if (!keyfile) {
		ret = -ENOMEM;
		goto out;
	}

	g_key_file_set_string(keyfile, NEARD_SERVICE_GROUP,
					SERVICE_KEY_TYPE, "wifi");
	g_key_file_set_string(keyfile, NEARD_SERVICE_GROUP,
					SERVICE_KEY_SSID, wsc->ssid);
	g_key_file_set_boolean(keyfile, NEARD_SERVICE_GROUP,
					SERVICE_KEY_HIDDEN, TRUE);

	if (wsc->passphrase)
		g_key_file_set_string(keyfile, NEARD_SERVICE_GROUP,
				SERVICE_KEY_PASSPHRASE, wsc->passphrase);

	ret = connman_config_provision_mutable_service(keyfile);

out:
	g_key_file_free(keyfile);
	free_wifi_sc(wsc);
	return ret;
}

static DBusMessage *push_oob_method(DBusConnection *dbus_conn,
					DBusMessage *message, void *user_data)
{
	const uint8_t *tlv_msg;
	int err = EINVAL;
	int length;

	DBG("");

	if (parse_request_oob_params(message, &tlv_msg, &length) != 0)
		return get_reply_on_error(message, err);

	err = handle_wcs_data(tlv_msg, length);
	if (err != 0)
		return get_reply_on_error(message, err);

	return g_dbus_create_reply(message, DBUS_TYPE_INVALID);
}

static DBusMessage *release_method(DBusConnection *dbus_conn,
					DBusMessage *message, void *user_data)
{
	DBG("");

	agent_registered = false;
	g_dbus_unregister_interface(connection,
					AGENT_PATH, NEARD_AGENT_INTERFACE);

	return g_dbus_create_reply(message, DBUS_TYPE_INVALID);
}

static const GDBusMethodTable neard_methods[] = {
	{ GDBUS_ASYNC_METHOD("RequestOOB",
		GDBUS_ARGS({ "data", "a{sv}" }),
		GDBUS_ARGS({ "data", "a{sv}" }), request_oob_method) },
	{ GDBUS_ASYNC_METHOD("PushOOB",
		GDBUS_ARGS({ "data", "a{sv}"}), NULL, push_oob_method) },
	{ GDBUS_METHOD("Release", NULL, NULL, release_method) },
	{ },
};

static void cleanup_register_call(void)
{
	if (register_call) {
		dbus_pending_call_cancel(register_call);
		dbus_pending_call_unref(register_call);
		register_call = NULL;
	}
}

static void register_agent_cb(DBusPendingCall *pending, void *user_data)
{
	DBusMessage *reply;

	if (!dbus_pending_call_get_completed(pending))
		return;

	register_call = NULL;

	reply = dbus_pending_call_steal_reply(pending);
	if (!reply)
		goto out;

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		g_dbus_unregister_interface(connection,
					AGENT_PATH, NEARD_AGENT_INTERFACE);
	} else
		agent_registered = true;

	dbus_message_unref(reply);
out:
	dbus_pending_call_unref(pending);
}

static void register_agent(void)
{
	const char *path = AGENT_PATH;
	const char *type = AGENT_TYPE;
	DBusMessage *message;

	message = dbus_message_new_method_call(NEARD_SERVICE, NEARD_PATH,
						NEARD_MANAGER_INTERFACE,
						"RegisterHandoverAgent");
	if (!message)
		return;

	dbus_message_append_args(message, DBUS_TYPE_OBJECT_PATH,
			&path, DBUS_TYPE_STRING, &type, DBUS_TYPE_INVALID);

	if (!dbus_connection_send_with_reply(connection, message,
						&register_call, TIMEOUT)) {
		dbus_message_unref(message);
		goto out;
	}

	if (!dbus_pending_call_set_notify(register_call,
						register_agent_cb, NULL, NULL))
		cleanup_register_call();

out:
	dbus_message_unref(message);
}

static void unregister_agent(void)
{
	const char *path = AGENT_PATH;
	const char *type = AGENT_TYPE;
	DBusMessage *message;

	if (!agent_registered)
		return cleanup_register_call();

	agent_registered = false;

	message = dbus_message_new_method_call(NEARD_SERVICE, NEARD_PATH,
						NEARD_MANAGER_INTERFACE,
						"UnregisterHandoverAgent");
	if (message) {
		dbus_message_append_args(message, DBUS_TYPE_OBJECT_PATH,
			&path, DBUS_TYPE_STRING, &type, DBUS_TYPE_INVALID);
		g_dbus_send_message(connection, message);
	}

	g_dbus_unregister_interface(connection,
					AGENT_PATH, NEARD_AGENT_INTERFACE);
}

static void neard_is_present(DBusConnection *conn, void *user_data)
{
	DBG("");

	if (agent_registered)
		return;

	if (g_dbus_register_interface(connection, AGENT_PATH,
					NEARD_AGENT_INTERFACE, neard_methods,
					NULL, NULL, NULL, NULL))
		register_agent();
}

static void neard_is_out(DBusConnection *conn, void *user_data)
{
	DBG("");

	if (agent_registered) {
		g_dbus_unregister_interface(connection,
					AGENT_PATH, NEARD_AGENT_INTERFACE);
		agent_registered = false;
	}

	cleanup_register_call();
}

static int neard_init(void)
{
	connection = connman_dbus_get_connection();
	if (!connection)
		return -EIO;

	watch_id = g_dbus_add_service_watch(connection, NEARD_SERVICE,
						neard_is_present, neard_is_out,
						NULL, NULL);
	if (watch_id == 0) {
		dbus_connection_unref(connection);
		return -ENOMEM;
	}

	return 0;
}

static void neard_exit(void)
{
	unregister_agent();

	if (watch_id != 0)
		g_dbus_remove_watch(connection, watch_id);
	if (connection)
		dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(neard, "Neard handover plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, neard_init, neard_exit)
