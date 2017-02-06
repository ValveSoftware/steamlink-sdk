// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_hints.h"
#include "net/base/address_list.h"
#include "net/base/load_flags.h"
#include "net/dns/host_resolver.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"

namespace content {

namespace {

// Note that the lifetime of |request| and |addresses| is managed by the caller.
void OnResolveComplete(net::SingleRequestHostResolver* request,
                       net::AddressList* addresses,
                       const net::CompletionCallback& callback,
                       int result) {
  // Plumb the resolution result into the callback if future consumers want
  // that information.
  callback.Run(result);
}

}  // namespace

void PreconnectUrl(content::ResourceContext* resource_context,
                   const GURL& url,
                   const GURL& first_party_for_cookies,
                   int count,
                   bool allow_credentials,
                   net::HttpRequestInfo::RequestMotivation motivation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(resource_context);

  net::URLRequestContext* context = resource_context->GetRequestContext();
  net::HttpTransactionFactory* factory = context->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();

  std::string user_agent;
  if (context->http_user_agent_settings())
    user_agent = context->http_user_agent_settings()->GetUserAgent();
  net::HttpRequestInfo request_info;
  request_info.url = url;
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                       user_agent);
  request_info.motivation = motivation;

  net::NetworkDelegate* delegate = context->network_delegate();
  if (delegate->CanEnablePrivacyMode(url, first_party_for_cookies))
    request_info.privacy_mode = net::PRIVACY_MODE_ENABLED;

  // TODO(yoav): Fix this layering violation, since when credentials are not
  // allowed we should turn on a flag indicating that, rather then turn on
  // private mode, even if lower layers would treat both the same.
  if (!allow_credentials) {
    request_info.privacy_mode = net::PRIVACY_MODE_ENABLED;
    request_info.load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                              net::LOAD_DO_NOT_SAVE_COOKIES |
                              net::LOAD_DO_NOT_SEND_AUTH_DATA;
  }

  net::HttpStreamFactory* http_stream_factory = session->http_stream_factory();
  http_stream_factory->PreconnectStreams(count, request_info);
}

int PreresolveUrl(content::ResourceContext* resource_context,
                  const GURL& url,
                  const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(resource_context);

  net::AddressList* addresses = new net::AddressList;
  net::SingleRequestHostResolver* resolver =
      new net::SingleRequestHostResolver(resource_context->GetHostResolver());
  net::HostResolver::RequestInfo resolve_info(net::HostPortPair::FromURL(url));
  return resolver->Resolve(resolve_info, net::IDLE, addresses,
                           base::Bind(&OnResolveComplete, base::Owned(resolver),
                                      base::Owned(addresses), callback),
                           net::BoundNetLog());
}

}  // namespace content
