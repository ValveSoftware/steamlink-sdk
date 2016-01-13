/*
 *
 *  ConnMan VPN daemon
 *
 *  Copyright (C) 2010,2013-2014  BMW Car IT GmbH.
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <net/if.h>

#include <dbus/dbus.h>
#include <glib.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/provider.h>
#include <connman/log.h>
#include <connman/task.h>
#include <connman/dbus.h>
#include <connman/inet.h>
#include <connman/agent.h>
#include <connman/setting.h>
#include <connman/vpn-dbus.h>

#include "../vpn-provider.h"
#include "../vpn-agent.h"

#include "vpn.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

enum {
	OPT_STRING = 1,
	OPT_BOOL = 2,
};

struct {
	const char *cm_opt;
	const char *pptp_opt;
	const char *vpnc_default;
	int type;
} pptp_options[] = {
	{ "PPTP.User", "user", NULL, OPT_STRING },
	{ "PPPD.EchoFailure", "lcp-echo-failure", "0", OPT_STRING },
	{ "PPPD.EchoInterval", "lcp-echo-interval", "0", OPT_STRING },
	{ "PPPD.Debug", "debug", NULL, OPT_STRING },
	{ "PPPD.RefuseEAP", "refuse-eap", NULL, OPT_BOOL },
	{ "PPPD.RefusePAP", "refuse-pap", NULL, OPT_BOOL },
	{ "PPPD.RefuseCHAP", "refuse-chap", NULL, OPT_BOOL },
	{ "PPPD.RefuseMSCHAP", "refuse-mschap", NULL, OPT_BOOL },
	{ "PPPD.RefuseMSCHAP2", "refuse-mschapv2", NULL, OPT_BOOL },
	{ "PPPD.NoBSDComp", "nobsdcomp", NULL, OPT_BOOL },
	{ "PPPD.NoDeflate", "nodeflate", NULL, OPT_BOOL },
	{ "PPPD.RequirMPPE", "require-mppe", NULL, OPT_BOOL },
	{ "PPPD.RequirMPPE40", "require-mppe-40", NULL, OPT_BOOL },
	{ "PPPD.RequirMPPE128", "require-mppe-128", NULL, OPT_BOOL },
	{ "PPPD.RequirMPPEStateful", "mppe-stateful", NULL, OPT_BOOL },
	{ "PPPD.NoVJ", "no-vj-comp", NULL, OPT_BOOL },
};

static DBusConnection *connection;

struct pptp_private_data {
	struct connman_task *task;
	char *if_name;
	vpn_provider_connect_cb_t cb;
	void *user_data;
};

static DBusMessage *pptp_get_sec(struct connman_task *task,
				DBusMessage *msg, void *user_data)
{
	const char *user, *passwd;
	struct vpn_provider *provider = user_data;
	DBusMessage *reply;

	if (dbus_message_get_no_reply(msg))
		return NULL;

	user = vpn_provider_get_string(provider, "PPTP.User");
	passwd = vpn_provider_get_string(provider, "PPTP.Password");
	if (!user || strlen(user) == 0 ||
				!passwd || strlen(passwd) == 0)
		return NULL;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_append_args(reply, DBUS_TYPE_STRING, &user,
				DBUS_TYPE_STRING, &passwd,
				DBUS_TYPE_INVALID);
	return reply;
}

static int pptp_notify(DBusMessage *msg, struct vpn_provider *provider)
{
	DBusMessageIter iter, dict;
	const char *reason, *key, *value;
	char *addressv4 = NULL, *netmask = NULL, *gateway = NULL;
	char *ifname = NULL, *nameservers = NULL;
	struct connman_ipaddress *ipaddress = NULL;

	dbus_message_iter_init(msg, &iter);

	dbus_message_iter_get_basic(&iter, &reason);
	dbus_message_iter_next(&iter);

	if (!provider) {
		connman_error("No provider found");
		return VPN_STATE_FAILURE;
	}

	if (strcmp(reason, "auth failed") == 0) {
		DBG("authentication failure");

		vpn_provider_set_string(provider, "PPTP.User", NULL);
		vpn_provider_set_string(provider, "PPTP.Password", NULL);

		return VPN_STATE_AUTH_FAILURE;
	}

	if (strcmp(reason, "connect"))
		return VPN_STATE_DISCONNECT;

	dbus_message_iter_recurse(&iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);
		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &value);

		DBG("%s = %s", key, value);

		if (!strcmp(key, "INTERNAL_IP4_ADDRESS"))
			addressv4 = g_strdup(value);

		if (!strcmp(key, "INTERNAL_IP4_NETMASK"))
			netmask = g_strdup(value);

		if (!strcmp(key, "INTERNAL_IP4_DNS"))
			nameservers = g_strdup(value);

		if (!strcmp(key, "INTERNAL_IFNAME"))
			ifname = g_strdup(value);

		dbus_message_iter_next(&dict);
	}

	if (vpn_set_ifname(provider, ifname) < 0) {
		g_free(ifname);
		g_free(addressv4);
		g_free(netmask);
		g_free(nameservers);
		return VPN_STATE_FAILURE;
	}

	if (addressv4)
		ipaddress = connman_ipaddress_alloc(AF_INET);

	g_free(ifname);

	if (!ipaddress) {
		connman_error("No IP address for provider");
		g_free(addressv4);
		g_free(netmask);
		g_free(nameservers);
		return VPN_STATE_FAILURE;
	}

	value = vpn_provider_get_string(provider, "HostIP");
	if (value) {
		vpn_provider_set_string(provider, "Gateway", value);
		gateway = g_strdup(value);
	}

	if (addressv4)
		connman_ipaddress_set_ipv4(ipaddress, addressv4, netmask,
					gateway);

	vpn_provider_set_ipaddress(provider, ipaddress);
	vpn_provider_set_nameservers(provider, nameservers);

	g_free(addressv4);
	g_free(netmask);
	g_free(gateway);
	g_free(nameservers);
	connman_ipaddress_free(ipaddress);

	return VPN_STATE_CONNECT;
}

static int pptp_save(struct vpn_provider *provider, GKeyFile *keyfile)
{
	const char *option;
	bool pptp_option, pppd_option;
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(pptp_options); i++) {
		pptp_option = pppd_option = false;

		if (strncmp(pptp_options[i].cm_opt, "PPTP.", 5) == 0)
			pptp_option = true;

		if (strncmp(pptp_options[i].cm_opt, "PPPD.", 5) == 0)
			pppd_option = true;

		if (pptp_option || pppd_option) {
			option = vpn_provider_get_string(provider,
							pptp_options[i].cm_opt);
			if (!option) {
				/*
				 * Check if the option prefix is PPTP as the
				 * PPPD options were using PPTP prefix earlier.
				 */
				char *pptp_str;

				if (!pppd_option)
					continue;

				pptp_str = g_strdup_printf("PPTP.%s",
						&pptp_options[i].cm_opt[5]);
				option = vpn_provider_get_string(provider,
								pptp_str);
				g_free(pptp_str);

				if (!option)
					continue;
			}

			g_key_file_set_string(keyfile,
					vpn_provider_get_save_group(provider),
					pptp_options[i].cm_opt, option);
		}
	}

	return 0;
}

static void pptp_write_bool_option(struct connman_task *task,
				const char *key, const char *value)
{
	if (key && value) {
		if (strcasecmp(value, "yes") == 0 ||
				strcasecmp(value, "true") == 0 ||
				strcmp(value, "1") == 0)
			connman_task_add_argument(task, key, NULL);
	}
}

struct request_input_reply {
	struct vpn_provider *provider;
	vpn_provider_password_cb_t callback;
	void *user_data;
};

static void request_input_reply(DBusMessage *reply, void *user_data)
{
	struct request_input_reply *pptp_reply = user_data;
	const char *error = NULL;
	char *username = NULL, *password = NULL;
	char *key;
	DBusMessageIter iter, dict;

	DBG("provider %p", pptp_reply->provider);

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		error = dbus_message_get_error_name(reply);
		goto done;
	}

	if (!vpn_agent_check_reply_has_dict(reply))
		goto done;

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &dict);
	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		const char *str;

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
			if (dbus_message_iter_get_arg_type(&value)
							!= DBUS_TYPE_STRING)
				break;
			dbus_message_iter_get_basic(&value, &str);
			username = g_strdup(str);
		}

		if (g_str_equal(key, "Password")) {
			dbus_message_iter_next(&entry);
			if (dbus_message_iter_get_arg_type(&entry)
							!= DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse(&entry, &value);
			if (dbus_message_iter_get_arg_type(&value)
							!= DBUS_TYPE_STRING)
				break;
			dbus_message_iter_get_basic(&value, &str);
			password = g_strdup(str);
		}

		dbus_message_iter_next(&dict);
	}

done:
	pptp_reply->callback(pptp_reply->provider, username, password, error,
				pptp_reply->user_data);

	g_free(username);
	g_free(password);

	g_free(pptp_reply);
}

typedef void (* request_cb_t)(struct vpn_provider *provider,
				const char *username, const char *password,
				const char *error, void *user_data);

static int request_input(struct vpn_provider *provider,
			request_cb_t callback, const char *dbus_sender,
			void *user_data)
{
	DBusMessage *message;
	const char *path, *agent_sender, *agent_path;
	DBusMessageIter iter;
	DBusMessageIter dict;
	struct request_input_reply *pptp_reply;
	int err;
	void *agent;

	agent = connman_agent_get_info(dbus_sender, &agent_sender,
							&agent_path);
	if (!provider || !agent || !agent_path || !callback)
		return -ESRCH;

	message = dbus_message_new_method_call(agent_sender, agent_path,
					VPN_AGENT_INTERFACE,
					"RequestInput");
	if (!message)
		return -ENOMEM;

	dbus_message_iter_init_append(message, &iter);

	path = vpn_provider_get_path(provider);
	dbus_message_iter_append_basic(&iter,
				DBUS_TYPE_OBJECT_PATH, &path);

	connman_dbus_dict_open(&iter, &dict);

	vpn_agent_append_user_info(&dict, provider, "PPTP.User");

	vpn_agent_append_host_and_name(&dict, provider);

	connman_dbus_dict_close(&iter, &dict);

	pptp_reply = g_try_new0(struct request_input_reply, 1);
	if (!pptp_reply) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	pptp_reply->provider = provider;
	pptp_reply->callback = callback;
	pptp_reply->user_data = user_data;

	err = connman_agent_queue_message(provider, message,
			connman_timeout_input_request(),
			request_input_reply, pptp_reply, agent);
	if (err < 0 && err != -EBUSY) {
		DBG("error %d sending agent request", err);
		dbus_message_unref(message);
		g_free(pptp_reply);
		return err;
	}

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static int run_connect(struct vpn_provider *provider,
			struct connman_task *task, const char *if_name,
			vpn_provider_connect_cb_t cb, void *user_data,
			const char *username, const char *password)
{
	const char *opt_s, *host;
	char *str;
	int err, i;

	host = vpn_provider_get_string(provider, "Host");
	if (!host) {
		connman_error("Host not set; cannot enable VPN");
		err = -EINVAL;
		goto done;
	}

	if (!username || !password) {
		DBG("Cannot connect username %s password %p",
						username, password);
		err = -EINVAL;
		goto done;
	}

	DBG("username %s password %p", username, password);

	str = g_strdup_printf("%s %s --nolaunchpppd --loglevel 2",
				PPTP, host);
	if (!str) {
		connman_error("can not allocate memory");
		err = -ENOMEM;
		goto done;
	}

	connman_task_add_argument(task, "pty", str);
	g_free(str);

	connman_task_add_argument(task, "nodetach", NULL);
	connman_task_add_argument(task, "lock", NULL);
	connman_task_add_argument(task, "usepeerdns", NULL);
	connman_task_add_argument(task, "noipdefault", NULL);
	connman_task_add_argument(task, "noauth", NULL);
	connman_task_add_argument(task, "nodefaultroute", NULL);
	connman_task_add_argument(task, "ipparam", "pptp_plugin");

	for (i = 0; i < (int)ARRAY_SIZE(pptp_options); i++) {
		opt_s = vpn_provider_get_string(provider,
					pptp_options[i].cm_opt);
		if (!opt_s)
			opt_s = pptp_options[i].vpnc_default;

		if (!opt_s)
			continue;

		if (pptp_options[i].type == OPT_STRING)
			connman_task_add_argument(task,
					pptp_options[i].pptp_opt, opt_s);
		else if (pptp_options[i].type == OPT_BOOL)
			pptp_write_bool_option(task,
					pptp_options[i].pptp_opt, opt_s);
	}

	connman_task_add_argument(task, "plugin",
				SCRIPTDIR "/libppp-plugin.so");

	err = connman_task_run(task, vpn_died, provider,
				NULL, NULL, NULL);
	if (err < 0) {
		connman_error("pptp failed to start");
		err = -EIO;
		goto done;
	}

done:
	if (cb)
		cb(provider, user_data, err);

	return err;
}

static void free_private_data(struct pptp_private_data *data)
{
	g_free(data->if_name);
	g_free(data);
}

static void request_input_cb(struct vpn_provider *provider,
			const char *username,
			const char *password,
			const char *error, void *user_data)
{
	struct pptp_private_data *data = user_data;

	if (!username || !password)
		DBG("Requesting username %s or password failed, error %s",
			username, error);
	else if (error)
		DBG("error %s", error);

	vpn_provider_set_string(provider, "PPTP.User", username);
	vpn_provider_set_string_hide_value(provider, "PPTP.Password",
								password);

	run_connect(provider, data->task, data->if_name, data->cb,
		data->user_data, username, password);

	free_private_data(data);
}

static int pptp_connect(struct vpn_provider *provider,
			struct connman_task *task, const char *if_name,
			vpn_provider_connect_cb_t cb, const char *dbus_sender,
			void *user_data)
{
	const char *username, *password;
	int err;

	DBG("iface %s provider %p user %p", if_name, provider, user_data);

	if (connman_task_set_notify(task, "getsec",
					pptp_get_sec, provider)) {
		err = -ENOMEM;
		goto error;
	}

	username = vpn_provider_get_string(provider, "PPTP.User");
	password = vpn_provider_get_string(provider, "PPTP.Password");

	DBG("user %s password %p", username, password);

	if (!username || !password) {
		struct pptp_private_data *data;

		data = g_try_new0(struct pptp_private_data, 1);
		if (!data)
			return -ENOMEM;

		data->task = task;
		data->if_name = g_strdup(if_name);
		data->cb = cb;
		data->user_data = user_data;

		err = request_input(provider, request_input_cb, dbus_sender,
									data);
		if (err != -EINPROGRESS) {
			free_private_data(data);
			goto done;
		}
		return err;
	}

done:
	return run_connect(provider, task, if_name, cb, user_data,
							username, password);

error:
	if (cb)
		cb(provider, user_data, err);

	return err;
}

static int pptp_error_code(struct vpn_provider *provider, int exit_code)
{

	switch (exit_code) {
	case 1:
		return CONNMAN_PROVIDER_ERROR_CONNECT_FAILED;
	case 2:
		return CONNMAN_PROVIDER_ERROR_LOGIN_FAILED;
	case 16:
		return CONNMAN_PROVIDER_ERROR_AUTH_FAILED;
	default:
		return CONNMAN_PROVIDER_ERROR_UNKNOWN;
	}
}

static void pptp_disconnect(struct vpn_provider *provider)
{
	vpn_provider_set_string(provider, "PPTP.Password", NULL);
}

static struct vpn_driver vpn_driver = {
	.flags		= VPN_FLAG_NO_TUN,
	.notify		= pptp_notify,
	.connect	= pptp_connect,
	.error_code     = pptp_error_code,
	.save		= pptp_save,
	.disconnect	= pptp_disconnect,
};

static int pptp_init(void)
{
	connection = connman_dbus_get_connection();

	return vpn_register("pptp", &vpn_driver, PPPD);
}

static void pptp_exit(void)
{
	vpn_unregister("pptp");

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(pptp, "pptp plugin", VERSION,
	CONNMAN_PLUGIN_PRIORITY_DEFAULT, pptp_init, pptp_exit)
