/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <netdb.h>

#include <gweb/gweb.h>

#define DEFAULT_URL  "http://www.connman.net/online/status.html"

static GTimer *timer;

static GMainLoop *main_loop;

static void web_debug(const char *str, void *data)
{
	g_print("%s: %s\n", (const char *) data, str);
}

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static const char *message_type_to_string(int message_type)
{
	switch (message_type) {
	case 100:
		return "Initial redirect message";
	case 110:
		return "Proxy notification";
	case 120:
		return "Authentication notification";
	case 130:
		return "Logoff notification";
	case 140:
		return "Response to Authentication Poll";
	case 150:
		return "Response to Abort Login";
	}

	return NULL;
}

static const char *response_code_to_string(int response_code)
{
	switch (response_code) {
	case 0:
		return "No error";
	case 50:
		return "Login succeeded";
	case 100:
		return "Login failed";
	case 102:
		return "RADIUS server error/timeout";
	case 105:
		return "RADIUS server not enabled";
	case 150:
		return "Logoff succeeded";
	case 151:
		return "Login aborted";
	case 200:
		return "Proxy detection/repeat operation";
	case 201:
		return "Authentication pending";
	case 255:
		return "Access gateway internal error";
	}

	return NULL;
}

struct wispr_msg {
	bool has_error;
	const char *current_element;
	int message_type;
	int response_code;
	char *login_url;
	char *abort_login_url;
	char *logoff_url;
	char *access_procedure;
	char *access_location;
	char *location_name;
};

static inline void wispr_msg_init(struct wispr_msg *msg)
{
	msg->has_error = false;
	msg->current_element = NULL;

	msg->message_type = -1;
	msg->response_code = -1;

	g_free(msg->login_url);
	msg->login_url = NULL;

	g_free(msg->abort_login_url);
	msg->abort_login_url = NULL;

	g_free(msg->logoff_url);
	msg->logoff_url = NULL;

	g_free(msg->access_procedure);
	msg->access_procedure = NULL;

	g_free(msg->access_location);
	msg->access_location = NULL;

	g_free(msg->location_name);
	msg->location_name = NULL;
}

struct wispr_session {
	GWeb *web;
	GWebParser *parser;
	guint request;
	struct wispr_msg msg;
	char *username;
	char *password;
	char *originurl;
	char *formdata;
};

static gboolean execute_login(gpointer user_data);

static struct {
	const char *str;
	enum {
		WISPR_ELEMENT_NONE,
		WISPR_ELEMENT_ACCESS_PROCEDURE,
		WISPR_ELEMENT_ACCESS_LOCATION,
		WISPR_ELEMENT_LOCATION_NAME,
		WISPR_ELEMENT_LOGIN_URL,
		WISPR_ELEMENT_ABORT_LOGIN_URL,
		WISPR_ELEMENT_MESSAGE_TYPE,
		WISPR_ELEMENT_RESPONSE_CODE,
		WISPR_ELEMENT_NEXT_URL,
		WISPR_ELEMENT_DELAY,
		WISPR_ELEMENT_REPLY_MESSAGE,
		WISPR_ELEMENT_LOGIN_RESULTS_URL,
		WISPR_ELEMENT_LOGOFF_URL,
	} element;
} wispr_element_map[] = {
	{ "AccessProcedure",	WISPR_ELEMENT_ACCESS_PROCEDURE	},
	{ "AccessLocation",	WISPR_ELEMENT_ACCESS_LOCATION	},
	{ "LocationName",	WISPR_ELEMENT_LOCATION_NAME	},
	{ "LoginURL",		WISPR_ELEMENT_LOGIN_URL		},
	{ "AbortLoginURL",	WISPR_ELEMENT_ABORT_LOGIN_URL	},
	{ "MessageType",	WISPR_ELEMENT_MESSAGE_TYPE	},
	{ "ResponseCode",	WISPR_ELEMENT_RESPONSE_CODE	},
	{ "NextURL",		WISPR_ELEMENT_NEXT_URL		},
	{ "Delay",		WISPR_ELEMENT_DELAY		},
	{ "ReplyMessage",	WISPR_ELEMENT_REPLY_MESSAGE	},
	{ "LoginResultsURL",	WISPR_ELEMENT_LOGIN_RESULTS_URL	},
	{ "LogoffURL",		WISPR_ELEMENT_LOGOFF_URL	},
	{ NULL,			WISPR_ELEMENT_NONE		},
};

static void start_element_handler(GMarkupParseContext *context,
					const gchar *element_name,
					const gchar **attribute_names,
					const gchar **attribute_values,
					gpointer user_data, GError **error)
{
	struct wispr_msg *msg = user_data;

	msg->current_element = element_name;
}

static void end_element_handler(GMarkupParseContext *context,
					const gchar *element_name,
					gpointer user_data, GError **error)
{
	struct wispr_msg *msg = user_data;

	msg->current_element = NULL;
}

static void text_handler(GMarkupParseContext *context,
					const gchar *text, gsize text_len,
					gpointer user_data, GError **error)
{
	struct wispr_msg *msg = user_data;
	int i;

	if (!msg->current_element)
		return;

	for (i = 0; wispr_element_map[i].str; i++) {
		if (!g_str_equal(wispr_element_map[i].str, msg->current_element))
			continue;

		switch (wispr_element_map[i].element) {
		case WISPR_ELEMENT_NONE:
		case WISPR_ELEMENT_ACCESS_PROCEDURE:
			g_free(msg->access_procedure);
			msg->access_procedure = g_strdup(text);
			break;
		case WISPR_ELEMENT_ACCESS_LOCATION:
			g_free(msg->access_location);
			msg->access_location = g_strdup(text);
			break;
		case WISPR_ELEMENT_LOCATION_NAME:
			g_free(msg->location_name);
			msg->location_name = g_strdup(text);
			break;
		case WISPR_ELEMENT_LOGIN_URL:
			g_free(msg->login_url);
			msg->login_url = g_strdup(text);
			break;
		case WISPR_ELEMENT_ABORT_LOGIN_URL:
			g_free(msg->abort_login_url);
			msg->abort_login_url = g_strdup(text);
			break;
		case WISPR_ELEMENT_MESSAGE_TYPE:
			msg->message_type = atoi(text);
			break;
		case WISPR_ELEMENT_RESPONSE_CODE:
			msg->response_code = atoi(text);
			break;
		case WISPR_ELEMENT_NEXT_URL:
		case WISPR_ELEMENT_DELAY:
		case WISPR_ELEMENT_REPLY_MESSAGE:
		case WISPR_ELEMENT_LOGIN_RESULTS_URL:
			break;
		case WISPR_ELEMENT_LOGOFF_URL:
			g_free(msg->logoff_url);
			msg->logoff_url = g_strdup(text);
			break;
		}
	}
}

static void error_handler(GMarkupParseContext *context,
					GError *error, gpointer user_data)
{
	struct wispr_msg *msg = user_data;

	msg->has_error = true;
}

static const GMarkupParser wispr_parser = {
	start_element_handler,
	end_element_handler,
	text_handler,
	NULL,
	error_handler,
};

static void parser_callback(const char *str, gpointer user_data)
{
	struct wispr_session *wispr = user_data;
	GMarkupParseContext *context;
	bool result;

	//printf("%s\n", str);

	context = g_markup_parse_context_new(&wispr_parser,
			G_MARKUP_TREAT_CDATA_AS_TEXT, &wispr->msg, NULL);

	result = g_markup_parse_context_parse(context, str, strlen(str), NULL);
	if (result)
		g_markup_parse_context_end_parse(context, NULL);

	g_markup_parse_context_free(context);
}

typedef void (*user_input_cb)(const char *value, gpointer user_data);

struct user_input_data {
	GString *str;
	user_input_cb cb;
	gpointer user_data;
	bool hidden;
	int fd;
	struct termios saved_termios;
};

static void user_callback(struct user_input_data *data)
{
	char *value;

	if (data->hidden) {
		ssize_t len;

		len = write(data->fd, "\n", 1);
		if (len < 0)
			return;
	}

	tcsetattr(data->fd, TCSADRAIN, &data->saved_termios);

	close(data->fd);

	value = g_string_free(data->str, FALSE);

	if (data->cb)
		data->cb(value, data->user_data);

	g_free(value);

	g_free(data);
}

static gboolean keyboard_input(GIOChannel *channel, GIOCondition condition,
							gpointer user_data)
{
	struct user_input_data *data = user_data;
	char buf[1];
	int len;

	len = read(data->fd, buf, 1);

	if (len != 1)
		return TRUE;

	if (buf[0] == '\n') {
		user_callback(data);
		return FALSE;
	}

	g_string_append_c(data->str, buf[0]);

	if (data->hidden)
		len = write(data->fd, "*", 1);

	return TRUE;
}

static bool user_input(const char *label, bool hidden,
				user_input_cb func, gpointer user_data)
{
	struct user_input_data *data;
	struct termios new_termios;
	GIOChannel *channel;
	guint watch;
	ssize_t len;

	data = g_try_new0(struct user_input_data, 1);
	if (!data)
		return false;

	data->str = g_string_sized_new(32);
	data->cb = func;
	data->user_data = user_data;
	data->hidden = hidden;

	data->fd = open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (data->fd < 0)
		goto error;

	if (tcgetattr(data->fd, &data->saved_termios) < 0) {
		close(data->fd);
		goto error;
	}

	new_termios = data->saved_termios;
	if (data->hidden)
		new_termios.c_lflag &= ~(ICANON|ECHO);
	else
		new_termios.c_lflag &= ~ICANON;
	new_termios.c_cc[VMIN] = 1;
	new_termios.c_cc[VTIME] = 0;

	tcsetattr(data->fd, TCSADRAIN, &new_termios);

	channel = g_io_channel_unix_new(data->fd);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	watch = g_io_add_watch(channel, G_IO_IN, keyboard_input, data);
	g_io_channel_unref(channel);

	if (watch == 0)
		goto error;

	len = write(data->fd, label, strlen(label));
	if (len < 0)
		goto error;

	len = write(data->fd, ": ", 2);
	if (len < 0)
		goto error;

	return true;

error:
	g_string_free(data->str, TRUE);
	g_free(data);

	return false;
}

static void password_callback(const char *value, gpointer user_data)
{
	struct wispr_session *wispr = user_data;

	g_free(wispr->password);
	wispr->password = g_strdup(value);

	printf("\n");

	execute_login(wispr);
}

static void username_callback(const char *value, gpointer user_data)
{
	struct wispr_session *wispr = user_data;

	g_free(wispr->username);
	wispr->username = g_strdup(value);

	if (!wispr->password) {
		user_input("Password", true, password_callback, wispr);
		return;
	}

	printf("\n");

	execute_login(wispr);
}

static bool wispr_input(const guint8 **data, gsize *length,
						gpointer user_data)
{
	struct wispr_session *wispr = user_data;
	GString *buf;
	gsize count;

	buf = g_string_sized_new(100);

	g_string_append(buf, "button=Login&UserName=");
	g_string_append_uri_escaped(buf, wispr->username, NULL, FALSE);
	g_string_append(buf, "&Password=");
	g_string_append_uri_escaped(buf, wispr->password, NULL, FALSE);
	g_string_append(buf, "&FNAME=0&OriginatingServer=");
	g_string_append_uri_escaped(buf, wispr->originurl, NULL, FALSE);

	count = buf->len;

	g_free(wispr->formdata);
	wispr->formdata = g_string_free(buf, FALSE);

	*data = (guint8 *) wispr->formdata;
	*length = count;

	return false;
}

static bool wispr_route(const char *addr, int ai_family, int if_index,
		gpointer user_data)
{
	char *family = "unknown";

	if (ai_family == AF_INET)
		family = "IPv4";
	else if (ai_family == AF_INET6)
		family = "IPv6";

	printf("Route request: %s %s index %d\n", family, addr, if_index);

	if (ai_family != AF_INET && ai_family != AF_INET6)
		return false;

	return true;
}

static bool wispr_result(GWebResult *result, gpointer user_data)
{
	struct wispr_session *wispr = user_data;
	const guint8 *chunk;
	gsize length;
	guint16 status;
	gdouble elapsed;

	g_web_result_get_chunk(result, &chunk, &length);

	if (length > 0) {
		//printf("%s\n", (char *) chunk);
		g_web_parser_feed_data(wispr->parser, chunk, length);
		return true;
	}

	g_web_parser_end_data(wispr->parser);

	status = g_web_result_get_status(result);

	g_print("status: %03u\n", status);

	elapsed = g_timer_elapsed(timer, NULL);

	g_print("elapse: %f seconds\n", elapsed);

	if (wispr->msg.message_type < 0) {
		const char *redirect;

		if (status != 302)
			goto done;

		if (!g_web_result_get_header(result, "Location", &redirect))
			goto done;

		printf("Redirect URL: %s\n", redirect);
		printf("\n");

		wispr->request = g_web_request_get(wispr->web, redirect,
				wispr_result, wispr_route, wispr);

		return false;
	}

	printf("Message type: %s (%d)\n",
			message_type_to_string(wispr->msg.message_type),
						wispr->msg.message_type);
	printf("Response code: %s (%d)\n",
			response_code_to_string(wispr->msg.response_code),
						wispr->msg.response_code);
	if (wispr->msg.access_procedure)
		printf("Access procedure: %s\n", wispr->msg.access_procedure);
	if (wispr->msg.access_location)
		printf("Access location: %s\n", wispr->msg.access_location);
	if (wispr->msg.location_name)
		printf("Location name: %s\n", wispr->msg.location_name);
	if (wispr->msg.login_url)
		printf("Login URL: %s\n", wispr->msg.login_url);
	if (wispr->msg.abort_login_url)
		printf("Abort login URL: %s\n", wispr->msg.abort_login_url);
	if (wispr->msg.logoff_url)
		printf("Logoff URL: %s\n", wispr->msg.logoff_url);
	printf("\n");

	if (status != 200 && status != 302 && status != 404)
		goto done;

	if (wispr->msg.message_type == 100) {
		if (!wispr->username) {
			user_input("Username", false,
				   username_callback, wispr);
			return false;
		}

		if (!wispr->password) {
			user_input("Password", true, password_callback, wispr);
			return false;
		}

		g_idle_add(execute_login, wispr);
		return false;
	} else if (wispr->msg.message_type == 120 ||
					wispr->msg.message_type == 140) {
		int code = wispr->msg.response_code;
		printf("Login process: %s\n",
					code == 50 ? "SUCCESS" : "FAILURE");
	}

	if (status == 302) {
		const char *redirect;

		if (!g_web_result_get_header(result, "Location", &redirect))
			goto done;

		printf("\n");
		printf("Redirect URL: %s\n", redirect);
		printf("\n");

		wispr->request = g_web_request_get(wispr->web, redirect,
				wispr_result, NULL, wispr);

		return false;
	}

done:
	g_main_loop_quit(main_loop);

	return false;
}

static gboolean execute_login(gpointer user_data)
{
	struct wispr_session *wispr = user_data;

	wispr->request = g_web_request_post(wispr->web, wispr->msg.login_url,
					"application/x-www-form-urlencoded",
					wispr_input, wispr_result, wispr);

	wispr_msg_init(&wispr->msg);

	return FALSE;
}

static bool option_debug = false;
static gchar *option_nameserver = NULL;
static gchar *option_username = NULL;
static gchar *option_password = NULL;
static gchar *option_url = NULL;

static GOptionEntry options[] = {
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug,
					"Enable debug output" },
	{ "nameserver", 'n', 0, G_OPTION_ARG_STRING, &option_nameserver,
					"Specify nameserver", "ADDRESS" },
	{ "username", 'u', 0, G_OPTION_ARG_STRING, &option_username,
					"Specify username", "USERNAME" },
	{ "password", 'p', 0, G_OPTION_ARG_STRING, &option_password,
					"Specify password", "PASSWORD" },
	{ "url", 'U', 0, G_OPTION_ARG_STRING, &option_url,
					"Specify arbitrary request", "URL" },
	{ NULL },
};

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	struct sigaction sa;
	struct wispr_session wispr;
	int index = 0;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");
		return 1;
	}

	g_option_context_free(context);

	memset(&wispr, 0, sizeof(wispr));
	wispr_msg_init(&wispr.msg);

	wispr.web = g_web_new(index);
	if (!wispr.web) {
		fprintf(stderr, "Failed to create web service\n");
		return 1;
	}

	if (option_debug)
		g_web_set_debug(wispr.web, web_debug, "WEB");

	main_loop = g_main_loop_new(NULL, FALSE);

	if (option_nameserver) {
		g_web_add_nameserver(wispr.web, option_nameserver);
		g_free(option_nameserver);
	}

	g_web_set_accept(wispr.web, NULL);
	g_web_set_user_agent(wispr.web, "SmartClient/%s wispr", VERSION);
	g_web_set_close_connection(wispr.web, TRUE);

	if (!option_url)
		option_url = g_strdup(DEFAULT_URL);

	wispr.username = option_username;
	wispr.password = option_password;
	wispr.originurl = option_url;

	timer = g_timer_new();

	wispr.parser = g_web_parser_new("<WISPAccessGatewayParam",
						"WISPAccessGatewayParam>",
						parser_callback, &wispr);

	wispr.request = g_web_request_get(wispr.web, option_url,
			wispr_result, wispr_route, &wispr);

	if (wispr.request == 0) {
		fprintf(stderr, "Failed to start request\n");
		return 1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_main_loop_run(main_loop);

	g_timer_destroy(timer);

	if (wispr.request > 0)
		g_web_cancel_request(wispr.web, wispr.request);

	g_web_parser_unref(wispr.parser);
	g_web_unref(wispr.web);

	g_main_loop_unref(main_loop);

	g_free(wispr.username);
	g_free(wispr.password);
	g_free(wispr.originurl);

	return 0;
}
