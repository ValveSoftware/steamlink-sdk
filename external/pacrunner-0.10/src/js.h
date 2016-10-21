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

#define PACRUNNER_JS_PRIORITY_LOW      -100
#define PACRUNNER_JS_PRIORITY_DEFAULT     0
#define PACRUNNER_JS_PRIORITY_HIGH      100

struct pacrunner_js_driver {
	const char *name;
	int priority;
	int (*set_proxy) (struct pacrunner_proxy *proxy);
	int (*clear_proxy) (struct pacrunner_proxy *proxy);
	char *(*execute)(struct pacrunner_proxy *proxy, const char *url,
			 const char *host);
};

int pacrunner_js_driver_register(struct pacrunner_js_driver *driver);
void pacrunner_js_driver_unregister(struct pacrunner_js_driver *driver);

/* Common functions for JS plugins */
int __pacrunner_js_getipaddr(struct pacrunner_proxy *proxy, char *host,
			     size_t hostlen);
int __pacrunner_js_resolve(struct pacrunner_proxy *proxy, const char *node,
			   char *host, size_t hostlen);
extern const char __pacrunner_js_routines[];
