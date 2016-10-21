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

#include <stdlib.h>

#include <glib.h>

#include "pacrunner.h"

extern struct pacrunner_plugin_desc __pacrunner_builtin_mozjs;

static int mozjs_init(void) {
	return __pacrunner_builtin_mozjs.init();
}

static void mozjs_exit(void) {
	__pacrunner_builtin_mozjs.exit();
}


void pacrunner_error(const char *format, ...)
{
}

void pacrunner_debug(const char *format, ...)
{
}

#define MULTIPLE_COUNT	500
#define MASSIVE_COUNT	MULTIPLE_COUNT * 100

#define EXAMPLE_URL	"http://www.example.com/site/test.html"
#define EXAMPLE_HOST	"www.example.com"

#define DIRECT_PAC	"function FindProxyForURL(url, host)\n"		\
			"{\n"						\
			"	return \"DIRECT\";"			\
			"}\n"

#define EXAMPLE_PAC \
	"function FindProxyForURL(url, host)\n"				\
	"{\n"								\
	"  var me = myIpAddress();\n"					\
	"  var resolved_ip = dnsResolve(host);\n"			\
	"  if (me == \"127.0.0.1\") { return \"DIRECT\"; }\n"		\
	"  if (host == \"127.0.0.1\") { return \"DIRECT\"; }\n"		\
	"  if (host == \"localhost\") { return \"DIRECT\"; }\n"		\
	"  if (isPlainHostName(host)) { return \"DIRECT\"; }\n"		\
	"  return \"PROXY proxy.example.com\";\n"			\
	"}\n"

static struct pacrunner_proxy *proxy;

static void test_single_init(void)
{
	g_assert(mozjs_init() == 0);

	mozjs_exit();
}

static void test_multiple_init(void)
{
	int i;

	for (i = 0; i < MULTIPLE_COUNT; i++) {
		g_assert(mozjs_init() == 0);

		mozjs_exit();
	}
}

static void test_single_execute_without_pac(void)
{
	const char *result;

	g_assert(mozjs_init() == 0);

	result = __pacrunner_js_execute(proxy, EXAMPLE_URL, EXAMPLE_HOST);
	g_test_message("result: %s", result);

	mozjs_exit();
}

static void test_multiple_execute_without_pac(void)
{
	const char *result;
	int i;

	g_assert(mozjs_init() == 0);

	for (i = 0; i < MULTIPLE_COUNT; i++) {
		result = __pacrunner_js_execute(proxy, EXAMPLE_URL,
						EXAMPLE_HOST);
		g_test_message("result %d: %s", i, result);
	}

	mozjs_exit();
}

static void test_single_execute_with_direct_pac(void)
{
	const char *result;

	g_assert(mozjs_init() == 0);

	g_assert(pacrunner_proxy_set_auto(proxy, NULL, DIRECT_PAC) == 0);

	result = __pacrunner_js_execute(proxy, EXAMPLE_URL, EXAMPLE_HOST);
	g_test_message("result: %s", result);

	pacrunner_proxy_disable(proxy);

	mozjs_exit();
}

static void test_multiple_execute_with_direct_pac(void)
{
	const char *result;
	int i;

	mozjs_init();

	g_assert(pacrunner_proxy_set_auto(proxy, NULL, DIRECT_PAC) == 0);

	for (i = 0; i < MULTIPLE_COUNT; i++) {
		result = __pacrunner_js_execute(proxy, EXAMPLE_URL,
						EXAMPLE_HOST);
		g_test_message("result %d: %s", i, result);
	}

	pacrunner_proxy_disable(proxy);

	mozjs_exit();
}

static void test_massive_execute_with_direct_pac(void)
{
	const char *result;
	int i;

	mozjs_init();

	g_assert(pacrunner_proxy_set_auto(proxy, NULL, DIRECT_PAC) == 0);

	for (i = 0; i < MASSIVE_COUNT; i++) {
		result = __pacrunner_js_execute(proxy, EXAMPLE_URL,
						EXAMPLE_HOST);
		g_test_message("result %d: %s", i, result);
	}

	pacrunner_proxy_disable(proxy);

	mozjs_exit();
}

static void test_multiple_execute_with_example_pac(void)
{
	const char *result;
	int i;

	mozjs_init();

	g_assert(pacrunner_proxy_set_auto(proxy, NULL, EXAMPLE_PAC) == 0);

	for (i = 0; i < MULTIPLE_COUNT; i++) {
		result = __pacrunner_js_execute(proxy, EXAMPLE_URL,
						EXAMPLE_HOST);
		g_test_message("result %d: %s", i, result);
	}

	pacrunner_proxy_disable(proxy);

	mozjs_exit();
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);

	proxy = pacrunner_proxy_create("eth0");

	g_test_add_func("/mozjs/single-init", test_single_init);
	g_test_add_func("/mozjs/multiple-init", test_multiple_init);
	g_test_add_func("/mozjs/single-execute-without-pac",
				test_single_execute_without_pac);
	g_test_add_func("/mozjs/multiple-execute-without-pac",
				test_multiple_execute_without_pac);
	g_test_add_func("/mozjs/single-execute-with-direct-pac",
				test_single_execute_with_direct_pac);
	g_test_add_func("/mozjs/multiple-execute-with-direct-pac",
				test_multiple_execute_with_direct_pac);
	g_test_add_func("/mozjs/massive-execute-with-direct-pac",
				test_massive_execute_with_direct_pac);
	g_test_add_func("/mozjs/multiple-execute-with-example-pac",
				test_multiple_execute_with_example_pac);

	return g_test_run();
}
