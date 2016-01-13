// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_interceptor.h"

#include "webkit/browser/appcache/appcache_backend_impl.h"
#include "webkit/browser/appcache/appcache_host.h"
#include "webkit/browser/appcache/appcache_request_handler.h"
#include "webkit/browser/appcache/appcache_service_impl.h"
#include "webkit/browser/appcache/appcache_url_request_job.h"
#include "webkit/common/appcache/appcache_interfaces.h"

using appcache::AppCacheBackendImpl;
using appcache::AppCacheHost;
using appcache::AppCacheRequestHandler;
using appcache::AppCacheServiceImpl;
using appcache::kAppCacheNoCacheId;
using appcache::kAppCacheNoHostId;

namespace content {

// static
AppCacheInterceptor* AppCacheInterceptor::GetInstance() {
  return Singleton<AppCacheInterceptor>::get();
}

void AppCacheInterceptor::SetHandler(
    net::URLRequest* request, AppCacheRequestHandler* handler) {
  request->SetUserData(GetInstance(), handler);  // request takes ownership
}

AppCacheRequestHandler* AppCacheInterceptor::GetHandler(
    net::URLRequest* request) {
  return reinterpret_cast<AppCacheRequestHandler*>(
      request->GetUserData(GetInstance()));
}

void AppCacheInterceptor::SetExtraRequestInfo(
    net::URLRequest* request, AppCacheServiceImpl* service, int process_id,
    int host_id, ResourceType::Type resource_type) {
  if (!service || (host_id == kAppCacheNoHostId))
    return;

  AppCacheBackendImpl* backend = service->GetBackend(process_id);
  if (!backend)
    return;

  // TODO(michaeln): An invalid host id is indicative of bad data
  // from a child process. How should we handle that here?
  AppCacheHost* host = backend->GetHost(host_id);
  if (!host)
    return;

  // Create a handler for this request and associate it with the request.
  AppCacheRequestHandler* handler =
      host->CreateRequestHandler(request, resource_type);
  if (handler)
    SetHandler(request, handler);
}

void AppCacheInterceptor::GetExtraResponseInfo(net::URLRequest* request,
                                               int64* cache_id,
                                               GURL* manifest_url) {
  DCHECK(*cache_id == kAppCacheNoCacheId);
  DCHECK(manifest_url->is_empty());
  AppCacheRequestHandler* handler = GetHandler(request);
  if (handler)
    handler->GetExtraResponseInfo(cache_id, manifest_url);
}

void AppCacheInterceptor::PrepareForCrossSiteTransfer(
    net::URLRequest* request,
    int old_process_id) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return;
  handler->PrepareForCrossSiteTransfer(old_process_id);
}

void AppCacheInterceptor::CompleteCrossSiteTransfer(
    net::URLRequest* request,
    int new_process_id,
    int new_host_id) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return;
  DCHECK_NE(kAppCacheNoHostId, new_host_id);
  handler->CompleteCrossSiteTransfer(new_process_id,
                                     new_host_id);
}

AppCacheInterceptor::AppCacheInterceptor() {
  net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
}

AppCacheInterceptor::~AppCacheInterceptor() {
  net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
}

net::URLRequestJob* AppCacheInterceptor::MaybeIntercept(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadResource(request, network_delegate);
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForRedirect(
      request, network_delegate, location);
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptResponse(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForResponse(request, network_delegate);
}

}  // namespace content
