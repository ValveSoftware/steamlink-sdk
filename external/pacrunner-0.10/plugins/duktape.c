/*
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright © 2013-2016  Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms and conditions of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>

#include <netdb.h>

#include "pacrunner.h"
#include "js.h"

#include "duktape/duktape.h"

struct pacrunner_duktape {
	struct pacrunner_proxy *proxy;
	pthread_mutex_t lock;
	duk_context *dctx;
};

static duk_ret_t myipaddress(duk_context *dkctx)
{
	struct pacrunner_duktape *ctx;
	duk_memory_functions fns;
	char address[NI_MAXHOST];

	duk_get_memory_functions(dkctx, &fns);
	ctx = fns.udata;

	if (__pacrunner_js_getipaddr(ctx->proxy, address, sizeof(address)) < 0)
		duk_push_string(dkctx, NULL);
	else
		duk_push_string(dkctx, address);

	DBG("address %s", address);

	return 1;
}

static duk_ret_t dnsresolve(duk_context *dkctx)
{
	struct pacrunner_duktape *ctx;
	const char *host = duk_require_string(dkctx, 0);
	duk_memory_functions fns;
	char address[NI_MAXHOST];

	DBG("host %s", host);

	duk_get_memory_functions(dkctx, &fns);
	ctx = fns.udata;

	address[0] = 0;
	if (__pacrunner_js_resolve(ctx->proxy, host, address,
				   sizeof(address)) < 0)
		duk_push_string(dkctx, NULL);
	else
		duk_push_string(dkctx, address);

	DBG("address %s", address);

	return 1;
}

static int create_object(struct pacrunner_proxy *proxy)
{
	struct pacrunner_duktape *ctx;
	const char *script;

	script = (char *) pacrunner_proxy_get_script(proxy);
	if (!script) {
		pacrunner_error("no script\n");
		return 0;
	}

	ctx = g_malloc0(sizeof(struct pacrunner_duktape));

	ctx->proxy = proxy;
	ctx->dctx = duk_create_heap(NULL, NULL, NULL, ctx, NULL);
	if (!ctx->dctx) {
		pacrunner_error("no headp\n");
		g_free(ctx);
		return -ENOMEM;
	}
	duk_push_global_object(ctx->dctx);

	/* Register the C functions so the script can use them */
	duk_push_c_function(ctx->dctx, myipaddress, 0);
	duk_put_prop_string(ctx->dctx, -2, "myIpAddress");

	duk_push_c_function(ctx->dctx, dnsresolve, 1);
	duk_put_prop_string(ctx->dctx, -2, "dnsResolve");

	if (duk_peval_string_noresult(ctx->dctx,
				      __pacrunner_js_routines) != 0 ||
	    duk_peval_string_noresult(ctx->dctx, script) != 0) {
		pacrunner_error("Error: %s\n",
				duk_safe_to_string(ctx->dctx, -1));
		duk_destroy_heap(ctx->dctx);
		g_free(ctx);
		return -EINVAL;
	}

	if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
		pacrunner_error("Failed to init mutex lock\n");
		duk_destroy_heap(ctx->dctx);
		g_free(ctx);
		return -EIO;
	}

	__pacrunner_proxy_set_jsctx(proxy, ctx);
	pacrunner_error("done %p\n", ctx);
	return 0;
}

static int duktape_clear_proxy(struct pacrunner_proxy *proxy)
{
	struct pacrunner_duktape *ctx = __pacrunner_proxy_get_jsctx(proxy);

	DBG("proxy %p ctx %p", proxy, ctx);

	if (!ctx)
		return -EINVAL;

	duk_destroy_heap(ctx->dctx);
	pthread_mutex_destroy(&ctx->lock);

	__pacrunner_proxy_set_jsctx(proxy, NULL);
	return 0;
}

static int duktape_set_proxy(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	if (!proxy)
		return 0;

	duktape_clear_proxy(proxy);

	return create_object(proxy);
}

static char *duktape_execute(struct pacrunner_proxy *proxy,
			     const char *url, const char *host)
{
	struct pacrunner_duktape *ctx = __pacrunner_proxy_get_jsctx(proxy);
	char *result;

	DBG("proxy %p ctx %p url %s host %s", proxy, ctx, url, host);

	if (!ctx)
		return NULL;

	pthread_mutex_lock(&ctx->lock);

	duk_get_prop_string(ctx->dctx, -1 /*index*/, "FindProxyForURL");

	duk_push_string(ctx->dctx, url);
	duk_push_string(ctx->dctx, host);
	if (duk_pcall(ctx->dctx, 2 /*nargs*/) != 0) {
		pacrunner_error("Error: %s\n",
				duk_safe_to_string(ctx->dctx, -1));
		result = NULL;
	} else {
		result = strdup(duk_safe_to_string(ctx->dctx, -1));
	}
	duk_pop(ctx->dctx);  /* pop result/error */

	if (result) {
		DBG("the return string is: %s\n", result);
	}

	pthread_mutex_unlock(&ctx->lock);

	return result;
}


static struct pacrunner_js_driver duktape_driver = {
	.name		= "duktape",
	.priority	= PACRUNNER_JS_PRIORITY_HIGH,
	.set_proxy	= duktape_set_proxy,
	.clear_proxy	= duktape_clear_proxy,
	.execute	= duktape_execute,
};

static int duktape_init(void)
{
	DBG("");

	return pacrunner_js_driver_register(&duktape_driver);
}


static void duktape_exit(void)
{
	DBG("");

	pacrunner_js_driver_unregister(&duktape_driver);

	duktape_set_proxy(NULL);
}

PACRUNNER_PLUGIN_DEFINE(duktape, duktape_init, duktape_exit)
