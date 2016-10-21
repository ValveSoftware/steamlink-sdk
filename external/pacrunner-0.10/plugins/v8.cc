/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2010  Intel Corporation. All rights reserved.
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

#include <netdb.h>

#include <v8.h>

extern "C" {
#include "pacrunner.h"
#include "js.h"
};

static struct pacrunner_proxy *current_proxy = NULL;
static v8::Persistent<v8::Context> jsctx;
static v8::Persistent<v8::Function> jsfn;
static guint gc_source = 0;

static gboolean v8_gc(gpointer user_data)
{
	v8::Locker lck;
	return !v8::V8::IdleNotification();
}

static v8::Handle<v8::Value> myipaddress(const v8::Arguments& args)
{
	const char *interface;
	char address[NI_MAXHOST];

	DBG("");

	if (current_proxy == NULL)
		return v8::ThrowException(v8::String::New("No current proxy"));

	if (__pacrunner_js_getipaddr(current_proxy, address, sizeof(address)) < 0)
		return v8::ThrowException(v8::String::New("Error fetching IP address"));

	DBG("address %s", address);

	return v8::String::New(address);
}

static v8::Handle<v8::Value> dnsresolve(const v8::Arguments& args)
{
	char address[NI_MAXHOST];
	v8::String::Utf8Value host(args[0]);
	char **split_res;

	if (args.Length() != 1)
		return v8::ThrowException(v8::String::New("Bad parameters"));

	DBG("host %s", *host);

	if (__pacrunner_js_resolve(current_proxy, *host, address, sizeof(address)) < 0)
		return v8::ThrowException(v8::String::New("Failed to resolve"));

	DBG("address %s", address);

	return v8::String::New(address);
}

static void create_object(void)
{
	if (!current_proxy)
		return;

	const char *pac = pacrunner_proxy_get_script(current_proxy);
	if (!pac) {
		printf("no script\n");
		return;
	}
	v8::HandleScope handle_scope;
	v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

	global->Set(v8::String::New("myIpAddress"),
		    v8::FunctionTemplate::New(myipaddress));
	global->Set(v8::String::New("dnsResolve"),
		    v8::FunctionTemplate::New(dnsresolve));

	jsctx = v8::Context::New(NULL, global);
	v8::Context::Scope context_scope(jsctx);

	v8::TryCatch exc;
	v8::Handle<v8::Script> script_scr;
	v8::Handle<v8::Value> result;

	script_scr = v8::Script::Compile(v8::String::New(__pacrunner_js_routines));
	if (script_scr.IsEmpty()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("Javascript failed to compile: %s", *err);
		jsctx.Dispose();
		return;
	}
	result = script_scr->Run();
	if (exc.HasCaught()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("Javascript library failed: %s", *err);
		jsctx.Dispose();
		return;
	}

	script_scr = v8::Script::Compile(v8::String::New(pac));
	if (script_scr.IsEmpty()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("PAC script failed to compile: %s", *err);
		jsctx.Dispose();
		return;
	}
	result = script_scr->Run();
	if (exc.HasCaught()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("PAC script failed: %s", *err);
		jsctx.Dispose();
		return;
	}

	v8::Handle<v8::String> fn_name = v8::String::New("FindProxyForURL");
	v8::Handle<v8::Value> fn_val = jsctx->Global()->Get(fn_name);

	if (!fn_val->IsFunction()) {
		DBG("FindProxyForUrl is not a function");
		jsctx.Dispose();
		return;
	}

	jsfn = v8::Persistent<v8::Function>::New(v8::Handle<v8::Function>::Cast(fn_val));
	return;
}

static void destroy_object(void)
{
	if (!jsfn.IsEmpty()) {
		jsfn.Dispose();
		jsfn.Clear();
	}
	if (!jsctx.IsEmpty()) {
		jsctx.Dispose();
		jsctx.Clear();
	}
}

static int v8_set_proxy(struct pacrunner_proxy *proxy)
{
	v8::Locker lck;

	DBG("proxy %p", proxy);

	if (current_proxy != NULL)
		destroy_object();

	current_proxy = proxy;

	if (current_proxy != NULL)
		create_object();

	if (!gc_source)
		gc_source = g_idle_add(v8_gc, NULL);

	return 0;
}


static char *v8_execute(struct pacrunner_proxy *proxy, const char *url,
			const char *host)
{
	DBG("proxy %p url %s host %s", proxy, url, host);

	if (current_proxy != proxy && v8_set_proxy(proxy))
		return NULL;

	v8::Locker lck;

	if (jsctx.IsEmpty() || jsfn.IsEmpty())
		return NULL;

	v8::HandleScope handle_scope;
	v8::Context::Scope context_scope(jsctx);

	v8::Handle<v8::Value> args[2] = {
		v8::String::New(url),
		v8::String::New(host)
	};

	v8::Handle<v8::Value> result;
	v8::TryCatch exc;

	result = jsfn->Call(jsctx->Global(), 2, args);
	if (exc.HasCaught()) {
		v8::Handle<v8::Message> msg = exc.Message();
		int line = msg->GetLineNumber();
		v8::String::Utf8Value err(msg->Get());
		DBG("Failed to run FindProxyForUrl(): at line %d: %s",
		    line, *err);
		return NULL;
	}

	if (!result->IsString()) {
		DBG("FindProxyForUrl() failed to return a string");
		return NULL;
	}

	char *retval = g_strdup(*v8::String::Utf8Value(result->ToString()));

	if (!gc_source)
		gc_source = g_idle_add(v8_gc, NULL);

	return retval;
}

static struct pacrunner_js_driver v8_driver = {
	"v8",
	PACRUNNER_JS_PRIORITY_LOW,
	v8_set_proxy,
	NULL,
	v8_execute,
};

static int v8_init(void)
{
	DBG("");

	return pacrunner_js_driver_register(&v8_driver);
}

static void v8_exit(void)
{
	DBG("");

	pacrunner_js_driver_unregister(&v8_driver);

	v8_set_proxy(NULL);

	if (gc_source) {
		g_source_remove(gc_source);
		gc_source = 0;
	}
	while (!v8::V8::IdleNotification())
		;
}

PACRUNNER_PLUGIN_DEFINE(v8, v8_init, v8_exit)
