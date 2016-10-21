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

#include <errno.h>

#include <curl/curl.h>

#include <glib.h>

#include "plugin.h"
#include "log.h"
#include "download.h"

struct download_data {
	char *url;
	CURL *easy;
	GString *content;
	pacrunner_download_cb callback;
	void *user_data;
};

static size_t write_callback(void *ptr, size_t size, size_t nitems,
							void *user_data)
{
	struct download_data *download = user_data;
	size_t realsize = size * nitems;

	DBG("size %zd nitems %zd realsize %zd", size, nitems, realsize);

	g_string_append_len(download->content, ptr, realsize);

	return realsize;
}

static void check_sockets(CURLM *multi, CURLMcode result, int handles)
{
	struct download_data *download;
	CURLMsg *msg;
	CURL *easy;
	int msgs_left;
	char *str, *eff_url;

	DBG("result %d handles %d", result, handles);

	do {
		msg = curl_multi_info_read(multi, &msgs_left);
		if (!msg)
			break;

		if (msg->msg != CURLMSG_DONE)
			continue;

		easy = msg->easy_handle;

		curl_easy_getinfo(easy, CURLINFO_PRIVATE, &str);
		download = (void *) str;

		curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);

		DBG("finished %s result %d", eff_url, msg->data.result);

		if (download->callback) {
			if (msg->data.result == 0) {
				str = g_string_free(download->content, FALSE);
				download->callback(str, download->user_data);
			} else {
				g_string_free(download->content, TRUE);
				download->callback(NULL, download->user_data);
			}
		} else
			g_string_free(download->content, TRUE);

		curl_multi_remove_handle(multi, easy);
		curl_easy_cleanup(easy);

		g_free(download->url);
		g_free(download);
	} while (msgs_left > 0);
}

static guint timeout_source = 0;

static gboolean event_callback(GIOChannel *channel,
				GIOCondition condition, gpointer user_data)
{
	CURLM *multi = user_data;
	CURLMcode result;
	int sockfd, handles, action = 0;

	DBG("condition %d", condition);

	sockfd = g_io_channel_unix_get_fd(channel);

	if (condition & G_IO_IN)
		action |= CURL_CSELECT_IN;
	if (condition & G_IO_OUT)
		action |= CURL_CSELECT_OUT;
	if (condition & (G_IO_HUP|G_IO_ERR|G_IO_NVAL))
		action |= CURL_CSELECT_ERR;

	do {
		result = curl_multi_socket_action(multi, sockfd, action, &handles);
		DBG("curl_multi_socket_action returns %d", result);

	} while (result == CURLM_CALL_MULTI_SOCKET);

	check_sockets(multi, result, handles);

	if (handles)
		return TRUE;

	if (timeout_source > 0) {
		g_source_remove(timeout_source);
		timeout_source = 0;
	}

	return FALSE;
}

static int socket_callback(CURL *easy, curl_socket_t sockfd, int what,
					void *user_data, void *socket_data)
{
	CURLM *multi = user_data;
	guint source = GPOINTER_TO_UINT(socket_data);
	GIOChannel *channel;

	DBG("what %d source %d", what, source);

	if (source > 0) {
		g_source_remove(source);
		source = 0;
	}

	switch (what) {
	case CURL_POLL_NONE:
		DBG("poll none");
		break;
	case CURL_POLL_IN:
		DBG("poll in");
		channel = g_io_channel_unix_new(sockfd);
		source = g_io_add_watch(channel,
					G_IO_IN|G_IO_HUP|G_IO_ERR|G_IO_NVAL,
					event_callback, multi);
		g_io_channel_unref(channel);

		curl_multi_assign(multi, sockfd, GUINT_TO_POINTER(source));
		break;
	case CURL_POLL_OUT:
		DBG("poll out");
		channel = g_io_channel_unix_new(sockfd);
		source = g_io_add_watch(channel,
					G_IO_OUT|G_IO_HUP|G_IO_ERR|G_IO_NVAL,
					event_callback, multi);
		g_io_channel_unref(channel);

		curl_multi_assign(multi, sockfd, GUINT_TO_POINTER(source));
		break;
	case CURL_POLL_INOUT:
		DBG("poll in/out");
		channel = g_io_channel_unix_new(sockfd);
		source = g_io_add_watch(channel,
					G_IO_IN|G_IO_OUT|G_IO_HUP|G_IO_ERR|G_IO_NVAL,
					event_callback, multi);
		g_io_channel_unref(channel);

		curl_multi_assign(multi, sockfd, GUINT_TO_POINTER(source));
		break;
	case CURL_POLL_REMOVE:
		DBG("poll remove");
		break;
	}

	DBG("source %d", source);

	return 0;
}

static int timer_callback(CURLM *multi, long timeout_ms, void *user_data);
static gboolean timeout_callback(gpointer user_data)
{
	CURLM *multi = user_data;
	CURLMcode result;
	int handles;

	DBG("");

	timeout_source = 0;

	do {
		result = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT,
						  0, &handles);
		DBG("curl_multi_socket_action returns %d", result);
	} while (result == CURLM_CALL_MULTI_SOCKET);

	check_sockets(multi, result, handles);

	/* Sometimes, curl doesn't set a new timeout when it should. */
	if (handles && !timeout_source) {
		long next_timer = -1;

		curl_multi_timeout(multi, &next_timer);

		/* From the curl_multi_timeout() man page:
		   Note: if libcurl returns a -1 timeout here, it just means
		   that libcurl currently has no stored timeout value. You
		   must not wait too long (more than a few seconds perhaps)
		   before you call curl_multi_perform() again.

		   With curl < 7.21.2 it's very true; we'll never poll for
		   completion of the asynchronous DNS lookup otherwise.
		*/
		if (next_timer == -1)
			next_timer = 500;

		timer_callback(multi, next_timer, NULL);
	}
	return FALSE;
}

static int timer_callback(CURLM *multi, long timeout_ms, void *user_data)
{
	guint interval = timeout_ms / 1000;

	DBG("interval %d", interval);

	if (timeout_source > 0) {
		g_source_remove(timeout_source);
		timeout_source = 0;
	}

	if (timeout_ms < 0)
		return 0;

	if (interval)
		timeout_source = g_timeout_add_seconds(interval,
						       timeout_callback, multi);
	else
		timeout_source = g_timeout_add(timeout_ms,
					       timeout_callback, multi);

	return 0;
}

static CURLM *multi;

static int curl_download(const char *interface, const char *url,
			pacrunner_download_cb callback, void *user_data)
{
	struct download_data *download;
	CURLMcode result;

	DBG("url %s", url);

	download = g_try_new0(struct download_data, 1);
	if (!download)
		return -ENOMEM;

	download->url = g_strdup(url);
	download->easy = curl_easy_init();
	download->content = g_string_sized_new(1024);

	download->callback = callback;
	download->user_data = user_data;

	curl_easy_setopt(download->easy, CURLOPT_PRIVATE, download);
	curl_easy_setopt(download->easy, CURLOPT_INTERFACE, interface);
	curl_easy_setopt(download->easy, CURLOPT_URL, download->url);

	curl_easy_setopt(download->easy, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(download->easy, CURLOPT_WRITEDATA, download);

	curl_easy_setopt(download->easy, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(download->easy, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(download->easy, CURLOPT_NOPROXY, "*");
	curl_easy_setopt(download->easy, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(download->easy, CURLOPT_CONNECTTIMEOUT, 30);
	curl_easy_setopt(download->easy, CURLOPT_USERAGENT, "pacrunner");

	result = curl_multi_add_handle(multi, download->easy);
	if (result != 0) {
		pacrunner_error("curl error %s", curl_multi_strerror(result));
		return -1;
	}

	return 0;
}

static struct pacrunner_download_driver curl_driver = {
	.name		= "curl",
	.download	= curl_download,
};

static int curl_init(void)
{
	DBG("");

	multi = curl_multi_init();

	curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, socket_callback);
	curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, multi);

	curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, timer_callback);
	curl_multi_setopt(multi, CURLMOPT_TIMERDATA, NULL);

	curl_multi_setopt(multi, CURLMOPT_PIPELINING, 1);

	pacrunner_download_driver_register(&curl_driver);

	return 0;
}

static void curl_exit(void)
{
	DBG("");

	pacrunner_download_driver_unregister(&curl_driver);

	if (timeout_source > 0) {
		g_source_remove(timeout_source);
		timeout_source = 0;
	}

	curl_multi_cleanup(multi);
}

PACRUNNER_PLUGIN_DEFINE(curl, curl_init, curl_exit)
