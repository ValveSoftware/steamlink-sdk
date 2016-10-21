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
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <netdb.h>

#pragma GCC diagnostic ignored "-Wredundant-decls"
#include <jsapi.h>
#pragma GCC diagnostic error "-Wredundant-decls"

#include "pacrunner.h"
#include "js.h"

static pthread_mutex_t mozjs_mutex = PTHREAD_MUTEX_INITIALIZER;

struct pacrunner_mozjs {
	struct pacrunner_proxy *proxy;
	JSContext *jsctx;
	JSObject *jsobj;
};

static JSBool myipaddress(JSContext *jsctx, uintN argc, jsval *vp)
{
	struct pacrunner_mozjs *ctx = JS_GetContextPrivate(jsctx);
	char address[NI_MAXHOST];

	DBG("");

	JS_SET_RVAL(jsctx, vp, JSVAL_NULL);

	if (!ctx)
		return JS_TRUE;

	if (__pacrunner_js_getipaddr(ctx->proxy, address, sizeof(address)) < 0)
		return JS_TRUE;

	DBG("address %s", address);

	JS_SET_RVAL(jsctx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(jsctx,
							       address)));

	return JS_TRUE;
}

static JSBool dnsresolve(JSContext *jsctx, uintN argc, jsval *vp)
{
	struct pacrunner_mozjs *ctx = JS_GetContextPrivate(jsctx);
	char address[NI_MAXHOST];
	jsval *argv = JS_ARGV(jsctx, vp);
	char *host = JS_EncodeString(jsctx, JS_ValueToString(jsctx, argv[0]));

	DBG("host %s", host);

	JS_SET_RVAL(jsctx, vp, JSVAL_NULL);

	if (!ctx)
		goto out;

	if (__pacrunner_js_resolve(ctx->proxy, host, address, sizeof(address)) < 0)
		goto out;

	DBG("address %s", address);

	JS_SET_RVAL(jsctx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(jsctx, address)));

 out:
	JS_free(jsctx, host);
	return JS_TRUE;

}

static JSClass jscls = {
	"global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static JSRuntime *jsrun;

static int create_object(struct pacrunner_proxy *proxy)
{
	struct pacrunner_mozjs *ctx;
	const char *script;
	jsval rval;

	script = pacrunner_proxy_get_script(proxy);
	if (!script)
		return 0;

	ctx = g_malloc0(sizeof(struct pacrunner_mozjs));

	ctx->proxy = proxy;
	ctx->jsctx = JS_NewContext(jsrun, 8 * 1024);
	if (!ctx->jsctx) {
		g_free(ctx);
		return -ENOMEM;
	}
	JS_SetContextPrivate(ctx->jsctx, ctx);
	__pacrunner_proxy_set_jsctx(proxy, ctx);

#if JS_VERSION >= 185
	ctx->jsobj = JS_NewCompartmentAndGlobalObject(ctx->jsctx, &jscls,
						      NULL);
#else
	ctx->jsobj = JS_NewObject(ctx->jsctx, &jscls, NULL, NULL);
#endif

	if (!JS_InitStandardClasses(ctx->jsctx, ctx->jsobj))
		pacrunner_error("Failed to init JS standard classes");

	JS_DefineFunction(ctx->jsctx, ctx->jsobj, "myIpAddress",
			  myipaddress, 0, 0);
	JS_DefineFunction(ctx->jsctx, ctx->jsobj,
			  "dnsResolve", dnsresolve, 1, 0);

	JS_EvaluateScript(ctx->jsctx, ctx->jsobj, __pacrunner_js_routines,
			  strlen(__pacrunner_js_routines), NULL, 0, &rval);

	JS_EvaluateScript(ctx->jsctx, ctx->jsobj, script, strlen(script),
			  "wpad.dat", 0, &rval);

	return 0;
}

static int mozjs_clear_proxy(struct pacrunner_proxy *proxy)
{
	struct pacrunner_mozjs *ctx = __pacrunner_proxy_get_jsctx(proxy);

	DBG("proxy %p ctx %p", proxy, ctx);

	if (!ctx)
		return -EINVAL;

	JS_DestroyContext(ctx->jsctx);
	__pacrunner_proxy_set_jsctx(proxy, NULL);

	return 0;
}

static int mozjs_set_proxy(struct pacrunner_proxy *proxy)
{
	DBG("proxy %p", proxy);

	if (!proxy)
		return 0;

	mozjs_clear_proxy(proxy);

	return create_object(proxy);
}

static char * mozjs_execute(struct pacrunner_proxy *proxy, const char *url,
			    const char *host)
{
	struct pacrunner_mozjs *ctx = __pacrunner_proxy_get_jsctx(proxy);
	JSBool result;
	jsval rval, args[2];
	char *answer, *g_answer;

	DBG("proxy %p ctx %p url %s host %s", proxy, ctx, url, host);

	if (!ctx)
		return NULL;

	pthread_mutex_lock(&mozjs_mutex);

	JS_BeginRequest(ctx->jsctx);

	args[0] = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx->jsctx, url));
	args[1] = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx->jsctx, host));

	result = JS_CallFunctionName(ctx->jsctx, ctx->jsobj,
				     "FindProxyForURL", 2, args, &rval);

	JS_EndRequest(ctx->jsctx);

	JS_MaybeGC(ctx->jsctx);

	pthread_mutex_unlock(&mozjs_mutex);

	if (result) {
		answer = JS_EncodeString(ctx->jsctx,
					 JS_ValueToString(ctx->jsctx, rval));
		g_answer = g_strdup(answer);
		JS_free(ctx->jsctx, answer);
		return g_answer;
	}

	return NULL;
}

static struct pacrunner_js_driver mozjs_driver = {
	.name		= "mozjs",
	.priority	= PACRUNNER_JS_PRIORITY_DEFAULT,
	.set_proxy	= mozjs_set_proxy,
	.clear_proxy	= mozjs_clear_proxy,
	.execute	= mozjs_execute,
};

static int mozjs_init(void)
{
	DBG("");

	jsrun = JS_NewRuntime(8 * 1024 * 1024);

	return pacrunner_js_driver_register(&mozjs_driver);
}

static void mozjs_exit(void)
{
	DBG("");

	pacrunner_js_driver_unregister(&mozjs_driver);

	JS_DestroyRuntime(jsrun);
}

PACRUNNER_PLUGIN_DEFINE(mozjs, mozjs_init, mozjs_exit)
