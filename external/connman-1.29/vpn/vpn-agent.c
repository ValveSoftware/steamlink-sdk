/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
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
#include <connman/log.h>
#include <connman/agent.h>
#include <vpn/vpn-provider.h>

#include "vpn-agent.h"
#include "vpn.h"

bool vpn_agent_check_reply_has_dict(DBusMessage *reply)
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

static void request_input_append_name(DBusMessageIter *iter, void *user_data)
{
	struct vpn_provider *provider = user_data;
	const char *str = "string";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	str = "informational";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);

	str = vpn_provider_get_name(provider);
	connman_dbus_dict_append_basic(iter, "Value",
				DBUS_TYPE_STRING, &str);
}

static void request_input_append_host(DBusMessageIter *iter, void *user_data)
{
	struct vpn_provider *provider = user_data;
	const char *str = "string";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	str = "informational";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);

	str = vpn_provider_get_host(provider);
	connman_dbus_dict_append_basic(iter, "Value",
				DBUS_TYPE_STRING, &str);
}

void vpn_agent_append_host_and_name(DBusMessageIter *iter,
				struct vpn_provider *provider)
{
	connman_dbus_dict_append_dict(iter, "Host",
				request_input_append_host,
				provider);

	connman_dbus_dict_append_dict(iter, "Name",
				request_input_append_name,
				provider);
}

struct user_info_data {
	struct vpn_provider *provider;
	const char *username_str;
};

static void request_input_append_user_info(DBusMessageIter *iter,
						void *user_data)
{
	struct user_info_data *data = user_data;
	struct vpn_provider *provider = data->provider;
	const char *str = "string";

	connman_dbus_dict_append_basic(iter, "Type",
				DBUS_TYPE_STRING, &str);
	str = "mandatory";
	connman_dbus_dict_append_basic(iter, "Requirement",
				DBUS_TYPE_STRING, &str);

	if (data->username_str) {
		str = vpn_provider_get_string(provider, data->username_str);
		if (str)
			connman_dbus_dict_append_basic(iter, "Value",
						DBUS_TYPE_STRING, &str);
	}
}

void vpn_agent_append_user_info(DBusMessageIter *iter,
				struct vpn_provider *provider,
				const char *username_str)
{
	struct user_info_data data = {
		.provider = provider,
		.username_str = username_str
	};

	connman_dbus_dict_append_dict(iter, "Username",
				request_input_append_user_info,
				&data);

	data.username_str = NULL;
	connman_dbus_dict_append_dict(iter, "Password",
				request_input_append_user_info,
				&data);
}
