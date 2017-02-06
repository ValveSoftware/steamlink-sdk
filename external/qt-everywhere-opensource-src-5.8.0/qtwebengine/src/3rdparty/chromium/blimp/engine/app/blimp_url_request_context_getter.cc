// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_url_request_context_getter.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "blimp/engine/app/blimp_network_delegate.h"
#include "blimp/engine/common/blimp_user_agent.h"
#include "content/public/browser/browser_thread.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_interceptor.h"

namespace blimp {
namespace engine {

BlimpURLRequestContextGetter::BlimpURLRequestContextGetter(
    bool ignore_certificate_errors,
    const base::FilePath& base_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_loop_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_loop_task_runner,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    net::NetLog* net_log)
    : ignore_certificate_errors_(ignore_certificate_errors),
      base_path_(base_path),
      io_loop_task_runner_(io_loop_task_runner),
      file_loop_task_runner_(file_loop_task_runner),
      net_log_(net_log),
      request_interceptors_(std::move(request_interceptors)) {
  // Must first be created on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::swap(protocol_handlers_, *protocol_handlers);

  // We must create the proxy config service on the UI loop on Linux because it
  // must synchronously run on the glib message loop. This will be passed to
  // the URLRequestContextBuilder on the IO thread.
  proxy_config_service_ = net::ProxyService::CreateSystemProxyConfigService(
      io_loop_task_runner_, file_loop_task_runner_);
}

BlimpURLRequestContextGetter::~BlimpURLRequestContextGetter() {}

net::URLRequestContext* BlimpURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!url_request_context_.get()) {
    net::URLRequestContextBuilder builder;
    builder.set_accept_language("en-us,en");
    builder.set_user_agent(GetBlimpEngineUserAgent());
    builder.set_proxy_config_service(std::move(proxy_config_service_));
    builder.set_network_delegate(base::WrapUnique(new BlimpNetworkDelegate));
    builder.set_net_log(net_log_);
    builder.set_data_enabled(true);
    for (auto& scheme_handler : protocol_handlers_) {
      builder.SetProtocolHandler(
          scheme_handler.first,
          base::WrapUnique(scheme_handler.second.release()));
    }
    protocol_handlers_.clear();

    std::vector<std::unique_ptr<net::URLRequestInterceptor>>
        request_interceptors;
    for (auto i : request_interceptors_) {
      request_interceptors.push_back(base::WrapUnique(i));
    }
    request_interceptors_.weak_clear();
    builder.SetInterceptors(std::move(request_interceptors));

    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.type =
        net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    builder.EnableHttpCache(cache_params);

    net::URLRequestContextBuilder::HttpNetworkSessionParams params;
    params.ignore_certificate_errors = ignore_certificate_errors_;
    builder.set_http_network_session_params(params);

    url_request_context_ = builder.Build();
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
BlimpURLRequestContextGetter::GetNetworkTaskRunner() const {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::IO);
}

net::HostResolver* BlimpURLRequestContextGetter::host_resolver() {
  return url_request_context_->host_resolver();
}

}  // namespace engine
}  // namespace blimp
