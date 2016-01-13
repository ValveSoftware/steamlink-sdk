/*
 *
 *  ConnMan VPN daemon
 *
 *  Copyright (C) 2010,2013  BMW Car IT GmbH.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

enum {
	OPT_ALL = 1,
	OPT_L2G = 2,
	OPT_L2	= 3,
	OPT_PPPD = 4,
};

struct {
	const char *cm_opt;
	const char *pppd_opt;
	int sub;
	const char *vpn_default;
	int type;
} pppd_options[] = {
	{ "L2TP.User", "name", OPT_ALL, NULL, OPT_STRING },
	{ "L2TP.BPS", "bps", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.TXBPS", "tx bps", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.RXBPS", "rx bps", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.LengthBit", "length bit", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.Challenge", "challenge", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.DefaultRoute", "defaultroute", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.FlowBit", "flow bit", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.TunnelRWS", "tunnel rws", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.Exclusive", "exclusive", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.Autodial", "autodial", OPT_L2, "yes", OPT_STRING },
	{ "L2TP.Redial", "redial", OPT_L2, "yes", OPT_STRING },
	{ "L2TP.RedialTimeout", "redial timeout", OPT_L2, "10", OPT_STRING },
	{ "L2TP.MaxRedials", "max redials", OPT_L2, NULL, OPT_STRING },
	{ "L2TP.RequirePAP", "require pap", OPT_L2, "no", OPT_STRING },
	{ "L2TP.RequireCHAP", "require chap", OPT_L2, "yes", OPT_STRING },
	{ "L2TP.ReqAuth", "require authentication", OPT_L2, "no", OPT_STRING },
	{ "L2TP.AccessControl", "access control", OPT_L2G, "yes", OPT_STRING },
	{ "L2TP.AuthFile", "auth file", OPT_L2G, NULL, OPT_STRING },
	{ "L2TP.ForceUserSpace", "force userspace", OPT_L2G, NULL, OPT_STRING },
	{ "L2TP.ListenAddr", "listen-addr", OPT_L2G, NULL, OPT_STRING },
	{ "L2TP.Rand Source", "rand source", OPT_L2G, NULL, OPT_STRING },
	{ "L2TP.IPsecSaref", "ipsec saref", OPT_L2G, NULL, OPT_STRING },
	{ "L2TP.Port", "port", OPT_L2G, NULL, OPT_STRING },
	{ "PPPD.EchoFailure", "lcp-echo-failure", OPT_PPPD, "0", OPT_STRING },
	{ "PPPD.EchoInterval", "lcp-echo-interval", OPT_PPPD, "0", OPT_STRING },
	{ "PPPD.Debug", "debug", OPT_PPPD, NULL, OPT_STRING },
	{ "PPPD.RefuseEAP", "refuse-eap", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.RefusePAP", "refuse-pap", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.RefuseCHAP", "refuse-chap", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.RefuseMSCHAP", "refuse-mschap", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.RefuseMSCHAP2", "refuse-mschapv2", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.NoBSDComp", "nobsdcomp", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.NoPcomp", "nopcomp", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.UseAccomp", "accomp", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.NoDeflate", "nodeflate", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.ReqMPPE", "require-mppe", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.ReqMPPE40", "require-mppe-40", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.ReqMPPE128", "require-mppe-128", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.ReqMPPEStateful", "mppe-stateful", OPT_PPPD, NULL, OPT_BOOL },
	{ "PPPD.NoVJ", "no-vj-comp", OPT_PPPD, NULL, OPT_BOOL },
};

static DBusConnection *connection;

struct l2tp_private_data {
	struct connman_task *task;
	char *if_name;
	vpn_provider_connect_cb_t cb;
	void *user_data;
};

static DBusMessage *l2tp_get_sec(struct connman_task *task,
			DBusMessage *msg, void *user_data)
{
	const char *user, *passwd;
	struct vpn_provider *provider = user_data;

	if (!dbus_message_get_no_reply(msg)) {
		DBusMessage *reply;

		user = vpn_provider_get_string(provider, "L2TP.User");
		passwd = vpn_provider_get_string(provider, "L2TP.Password");

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

	return NULL;
}

static int l2tp_notify(DBusMessage *msg, struct vpn_provider *provider)
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

		vpn_provider_set_string(provider, "L2TP.User", NULL);
		vpn_provider_set_string(provider, "L2TP.Password", NULL);

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

static int l2tp_save(struct vpn_provider *provider, GKeyFile *keyfile)
{
	const char *option;
	bool l2tp_option, pppd_option;
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(pppd_options); i++) {
		l2tp_option = pppd_option = false;

		if (strncmp(pppd_options[i].cm_opt, "L2TP.", 5) == 0)
			l2tp_option = true;

		if (strncmp(pppd_options[i].cm_opt, "PPPD.", 5) == 0)
			pppd_option = true;

		if (l2tp_option || pppd_option) {
			option = vpn_provider_get_string(provider,
						pppd_options[i].cm_opt);
			if (!option) {
				/*
				 * Check if the option prefix is L2TP as the
				 * PPPD options were using L2TP prefix earlier.
				 */
				char *l2tp_str;

				if (!pppd_option)
					continue;

				l2tp_str = g_strdup_printf("L2TP.%s",
						&pppd_options[i].cm_opt[5]);
				option = vpn_provider_get_string(provider,
								l2tp_str);
				g_free(l2tp_str);

				if (!option)
					continue;
			}

			g_key_file_set_string(keyfile,
					vpn_provider_get_save_group(provider),
					pppd_options[i].cm_opt, option);
		}
	}

	return 0;
}

static ssize_t full_write(int fd, const void *buf, size_t len)
{
	ssize_t byte_write;

	while (len) {
		byte_write = write(fd, buf, len);
		if (byte_write < 0) {
			connman_error("failed to write config to l2tp: %s\n",
					strerror(errno));
			return byte_write;
		}
		len -= byte_write;
		buf += byte_write;
	}

	return 0;
}

static ssize_t l2tp_write_bool_option(int fd,
					const char *key, const char *value)
{
	gchar *buf;
	ssize_t ret = 0;

	if (key && value) {
		if (strcasecmp(value, "yes") == 0 ||
				strcasecmp(value, "true") == 0 ||
				strcmp(value, "1") == 0) {
			buf = g_strdup_printf("%s\n", key);
			ret = full_write(fd, buf, strlen(buf));

			g_free(buf);
		}
	}

	return ret;
}

static int l2tp_write_option(int fd, const char *key, const char *value)
{
	gchar *buf;
	ssize_t ret = 0;

	if (key) {
		if (value)
			buf = g_strdup_printf("%s %s\n", key, value);
		else
			buf = g_strdup_printf("%s\n", key);

		ret = full_write(fd, buf, strlen(buf));

		g_free(buf);
	}

	return ret;
}

static int l2tp_write_section(int fd, const char *key, const char *value)
{
	gchar *buf;
	ssize_t ret = 0;

	if (key && value) {
		buf = g_strdup_printf("%s = %s\n", key, value);
		ret = full_write(fd, buf, strlen(buf));

		g_free(buf);
	}

	return ret;
}

static int write_pppd_option(struct vpn_provider *provider, int fd)
{
	int i;
	const char *opt_s;

	l2tp_write_option(fd, "nodetach", NULL);
	l2tp_write_option(fd, "lock", NULL);
	l2tp_write_option(fd, "usepeerdns", NULL);
	l2tp_write_option(fd, "noipdefault", NULL);
	l2tp_write_option(fd, "noauth", NULL);
	l2tp_write_option(fd, "nodefaultroute", NULL);
	l2tp_write_option(fd, "ipparam", "l2tp_plugin");

	for (i = 0; i < (int)ARRAY_SIZE(pppd_options); i++) {
		if (pppd_options[i].sub != OPT_ALL &&
			pppd_options[i].sub != OPT_PPPD)
			continue;

		opt_s = vpn_provider_get_string(provider,
					pppd_options[i].cm_opt);
		if (!opt_s)
			opt_s = pppd_options[i].vpn_default;

		if (!opt_s)
			continue;

		if (pppd_options[i].type == OPT_STRING)
			l2tp_write_option(fd,
				pppd_options[i].pppd_opt, opt_s);
		else if (pppd_options[i].type == OPT_BOOL)
			l2tp_write_bool_option(fd,
				pppd_options[i].pppd_opt, opt_s);
	}

	l2tp_write_option(fd, "plugin",
				SCRIPTDIR "/libppp-plugin.so");

	return 0;
}


static int l2tp_write_fields(struct vpn_provider *provider,
						int fd, int sub)
{
	int i;
	const char *opt_s;

	for (i = 0; i < (int)ARRAY_SIZE(pppd_options); i++) {
		if (pppd_options[i].sub != sub)
			continue;

		opt_s = vpn_provider_get_string(provider,
					pppd_options[i].cm_opt);
		if (!opt_s)
			opt_s = pppd_options[i].vpn_default;

		if (!opt_s)
			continue;

		if (pppd_options[i].type == OPT_STRING)
			l2tp_write_section(fd,
				pppd_options[i].pppd_opt, opt_s);
		else if (pppd_options[i].type == OPT_BOOL)
			l2tp_write_bool_option(fd,
				pppd_options[i].pppd_opt, opt_s);
	}

	return 0;
}

static int l2tp_write_config(struct vpn_provider *provider,
					const char *pppd_name, int fd)
{
	const char *option;

	l2tp_write_option(fd, "[global]", NULL);
	l2tp_write_fields(provider, fd, OPT_L2G);

	l2tp_write_option(fd, "[lac l2tp]", NULL);

	option = vpn_provider_get_string(provider, "Host");
	l2tp_write_option(fd, "lns =", option);

	l2tp_write_fields(provider, fd, OPT_ALL);
	l2tp_write_fields(provider, fd, OPT_L2);

	l2tp_write_option(fd, "pppoptfile =", pppd_name);

	return 0;
}

static void l2tp_died(struct connman_task *task, int exit_code, void *user_data)
{
	char *conf_file;

	vpn_died(task, exit_code, user_data);

	conf_file = g_strdup_printf(VPN_STATEDIR "/connman-xl2tpd.conf");
	unlink(conf_file);
	g_free(conf_file);

	conf_file = g_strdup_printf(VPN_STATEDIR "/connman-ppp-option.conf");
	unlink(conf_file);
	g_free(conf_file);
}

struct request_input_reply {
	struct vpn_provider *provider;
	vpn_provider_password_cb_t callback;
	void *user_data;
};

static void request_input_reply(DBusMessage *reply, void *user_data)
{
	struct request_input_reply *l2tp_reply = user_data;
	const char *error = NULL;
	char *username = NULL, *password = NULL;
	char *key;
	DBusMessageIter iter, dict;

	DBG("provider %p", l2tp_reply->provider);

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
	l2tp_reply->callback(l2tp_reply->provider, username, password, error,
				l2tp_reply->user_data);

	g_free(username);
	g_free(password);

	g_free(l2tp_reply);
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
	struct request_input_reply *l2tp_reply;
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

	vpn_agent_append_user_info(&dict, provider, "L2TP.User");

	vpn_agent_append_host_and_name(&dict, provider);

	connman_dbus_dict_close(&iter, &dict);

	l2tp_reply = g_try_new0(struct request_input_reply, 1);
	if (!l2tp_reply) {
		dbus_message_unref(message);
		return -ENOMEM;
	}

	l2tp_reply->provider = provider;
	l2tp_reply->callback = callback;
	l2tp_reply->user_data = user_data;

	err = connman_agent_queue_message(provider, message,
			connman_timeout_input_request(),
			request_input_reply, l2tp_reply, agent);
	if (err < 0 && err != -EBUSY) {
		DBG("error %d sending agent request", err);
		dbus_message_unref(message);
		g_free(l2tp_reply);
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
	char *l2tp_name, *pppd_name;
	int l2tp_fd, pppd_fd;
	int err;

	if (!username || !password) {
		DBG("Cannot connect username %s password %p",
						username, password);
		err = -EINVAL;
		goto done;
	}

	DBG("username %s password %p", username, password);

	l2tp_name = g_strdup_printf(VPN_STATEDIR "/connman-xl2tpd.conf");

	l2tp_fd = open(l2tp_name, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
	if (l2tp_fd < 0) {
		g_free(l2tp_name);
		connman_error("Error writing l2tp config");
		err = -EIO;
		goto done;
	}

	pppd_name = g_strdup_printf(VPN_STATEDIR "/connman-ppp-option.conf");

	pppd_fd = open(pppd_name, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
	if (pppd_fd < 0) {
		connman_error("Error writing pppd config");
		g_free(l2tp_name);
		g_free(pppd_name);
		close(l2tp_fd);
		err = -EIO;
		goto done;
	}

	l2tp_write_config(provider, pppd_name, l2tp_fd);

	write_pppd_option(provider, pppd_fd);

	connman_task_add_argument(task, "-D", NULL);
	connman_task_add_argument(task, "-c", l2tp_name);

	g_free(l2tp_name);
	g_free(pppd_name);
	close(l2tp_fd);
	close(pppd_fd);

	err = connman_task_run(task, l2tp_died, provider,
				NULL, NULL, NULL);
	if (err < 0) {
		connman_error("l2tp failed to start");
		err = -EIO;
		goto done;
	}

done:
	if (cb)
		cb(provider, user_data, err);

	return err;
}

static void free_private_data(struct l2tp_private_data *data)
{
	g_free(data->if_name);
	g_free(data);
}

static void request_input_cb(struct vpn_provider *provider,
			const char *username,
			const char *password,
			const char *error, void *user_data)
{
	struct l2tp_private_data *data = user_data;

	if (!username || !password)
		DBG("Requesting username %s or password failed, error %s",
			username, error);
	else if (error)
		DBG("error %s", error);

	vpn_provider_set_string(provider, "L2TP.User", username);
	vpn_provider_set_string_hide_value(provider, "L2TP.Password",
								password);

	run_connect(provider, data->task, data->if_name, data->cb,
		data->user_data, username, password);

	free_private_data(data);
}

static int l2tp_connect(struct vpn_provider *provider,
			struct connman_task *task, const char *if_name,
			vpn_provider_connect_cb_t cb, const char *dbus_sender,
			void *user_data)
{
	const char *username, *password;
	int err;

	if (connman_task_set_notify(task, "getsec",
					l2tp_get_sec, provider) != 0) {
		err = -ENOMEM;
		goto error;
	}

	username = vpn_provider_get_string(provider, "L2TP.User");
	password = vpn_provider_get_string(provider, "L2TP.Password");

	DBG("user %s password %p", username, password);

	if (!username || !password) {
		struct l2tp_private_data *data;

		data = g_try_new0(struct l2tp_private_data, 1);
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

static int l2tp_error_code(struct vpn_provider *provider, int exit_code)
{
	switch (exit_code) {
	case 1:
		return CONNMAN_PROVIDER_ERROR_CONNECT_FAILED;
	default:
		return CONNMAN_PROVIDER_ERROR_UNKNOWN;
	}
}

static void l2tp_disconnect(struct vpn_provider *provider)
{
	vpn_provider_set_string(provider, "L2TP.Password", NULL);
}

static struct vpn_driver vpn_driver = {
	.flags		= VPN_FLAG_NO_TUN,
	.notify		= l2tp_notify,
	.connect	= l2tp_connect,
	.error_code	= l2tp_error_code,
	.save		= l2tp_save,
	.disconnect	= l2tp_disconnect,
};

static int l2tp_init(void)
{
	connection = connman_dbus_get_connection();

	return vpn_register("l2tp", &vpn_driver, L2TP);
}

static void l2tp_exit(void)
{
	vpn_unregister("l2tp");

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(l2tp, "l2tp plugin", VERSION,
	CONNMAN_PLUGIN_PRIORITY_DEFAULT, l2tp_init, l2tp_exit)
