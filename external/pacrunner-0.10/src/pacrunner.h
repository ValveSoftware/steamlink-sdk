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

#include <dbus/dbus.h>
#include <glib.h>

#include <stdbool.h>

#define PACRUNNER_SERVICE	"org.pacrunner"
#define PACRUNNER_PATH		"/org/pacrunner"

#define PACRUNNER_ERROR_INTERFACE	PACRUNNER_SERVICE ".Error"

#define PACRUNNER_MANAGER_INTERFACE	PACRUNNER_SERVICE ".Manager"
#define PACRUNNER_MANAGER_PATH		PACRUNNER_PATH "/manager"

#define PACRUNNER_CLIENT_INTERFACE	PACRUNNER_SERVICE ".Client"
#define PACRUNNER_CLIENT_PATH		PACRUNNER_PATH "/client"


#include "log.h"

int __pacrunner_log_init(const char *debug, gboolean detach);
void __pacrunner_log_cleanup(void);

#include "plugin.h"

int __pacrunner_plugin_init(const char *pattern, const char *exclude);
void __pacrunner_plugin_cleanup(void);


enum pacrunner_proxy_method {
	PACRUNNER_PROXY_METHOD_UNKNOWN	= 0,
	PACRUNNER_PROXY_METHOD_DIRECT	= 1,
	PACRUNNER_PROXY_METHOD_MANUAL	= 2,
	PACRUNNER_PROXY_METHOD_AUTO	= 3,
};

struct pacrunner_proxy;

struct pacrunner_proxy *pacrunner_proxy_create(const char *interface);
struct pacrunner_proxy *pacrunner_proxy_ref(struct pacrunner_proxy *proxy);
void pacrunner_proxy_unref(struct pacrunner_proxy *proxy);

const char *pacrunner_proxy_get_interface(struct pacrunner_proxy *proxy);
const char *pacrunner_proxy_get_script(struct pacrunner_proxy *proxy);

int pacrunner_proxy_set_domains(struct pacrunner_proxy *proxy,
					char **domains, gboolean browser_only);
int pacrunner_proxy_set_direct(struct pacrunner_proxy *proxy);
int pacrunner_proxy_set_manual(struct pacrunner_proxy *proxy,
					char **servers, char **excludes);
int pacrunner_proxy_set_auto(struct pacrunner_proxy *proxy,
					const char *url, const char *script);

int pacrunner_proxy_enable(struct pacrunner_proxy *proxy);
int pacrunner_proxy_disable(struct pacrunner_proxy *proxy);

char *pacrunner_proxy_lookup(const char *url, const char *host);

void __pacrunner_proxy_set_jsctx(struct pacrunner_proxy *proxy, void *jsctx);
void *__pacrunner_proxy_get_jsctx(struct pacrunner_proxy *proxy);

int __pacrunner_proxy_init(void);
void __pacrunner_proxy_cleanup(void);


#include "download.h"

int __pacrunner_download_init(void);
void __pacrunner_download_cleanup(void);
int __pacrunner_download_update(const char *interface, const char *url,
			pacrunner_download_cb callback, void *user_data);

int __pacrunner_manager_init(DBusConnection *conn);
void __pacrunner_manager_cleanup();

int __pacrunner_client_init(DBusConnection *conn);
void __pacrunner_client_cleanup();

int __pacrunner_manual_init(void);
void __pacrunner_manual_cleanup(void);
GList **__pacrunner_manual_parse_servers(char **servers);
void __pacrunner_manual_destroy_servers(GList **servers);
GList **__pacrunner_manual_parse_excludes(char **excludes);
void __pacrunner_manual_destroy_excludes(GList **excludes);
char *__pacrunner_manual_execute(const char *url, const char *host,
				 GList **servers, GList **excludes);

int __pacrunner_mozjs_init(void);
void __pacrunner_mozjs_cleanup(void);
int __pacrunner_mozjs_set_proxy(struct pacrunner_proxy *proxy);
char *__pacrunner_mozjs_execute(const char *url, const char *host);

int __pacrunner_js_init(void);
void __pacrunner_js_cleanup(void);
int __pacrunner_js_set_proxy(struct pacrunner_proxy *proxy);
int __pacrunner_js_clear_proxy(struct pacrunner_proxy *proxy);
char *__pacrunner_js_execute(struct pacrunner_proxy *proxy, const char *url,
			     const char *host);
