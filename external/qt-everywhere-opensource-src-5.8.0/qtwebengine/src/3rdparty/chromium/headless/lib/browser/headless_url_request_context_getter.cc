// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_url_request_context_getter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace headless {

HeadlessURLRequestContextGetter::HeadlessURLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    content::ProtocolHandlerMap* protocol_handlers,
    ProtocolHandlerMap context_protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    HeadlessBrowser::Options* options)
    : io_task_runner_(std::move(io_task_runner)),
      file_task_runner_(std::move(file_task_runner)),
      user_agent_(options->user_agent),
      host_resolver_rules_(options->host_resolver_rules),
      proxy_server_(options->proxy_server),
      request_interceptors_(std::move(request_interceptors)) {
  // Must first be created on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::swap(protocol_handlers_, *protocol_handlers);
  for (auto& pair : options->protocol_handlers) {
    protocol_handlers_[pair.first] =
        linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            pair.second.release());
  }
  options->protocol_handlers.clear();
  for (auto& pair : context_protocol_handlers) {
    protocol_handlers_[pair.first] =
        linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            pair.second.release());
  }
  context_protocol_handlers.clear();

  // We must create the proxy config service on the UI loop on Linux because it
  // must synchronously run on the glib message loop. This will be passed to
  // the URLRequestContextStorage on the IO thread in GetURLRequestContext().
  if (proxy_server_.IsEmpty()) {
    proxy_config_service_ = net::ProxyService::CreateSystemProxyConfigService(
        io_task_runner_, file_task_runner_);
  }
}

HeadlessURLRequestContextGetter::~HeadlessURLRequestContextGetter() {}

net::URLRequestContext*
HeadlessURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;
    // TODO(skyostil): Make language settings configurable.
    builder.set_user_agent(user_agent_);
    // TODO(skyostil): Make these configurable.
    builder.set_data_enabled(true);
    builder.set_file_enabled(true);
    builder.SetFileTaskRunner(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::FILE));
    if (!proxy_server_.IsEmpty()) {
      builder.set_proxy_service(
          net::ProxyService::CreateFixed(proxy_server_.ToString()));
    } else {
      builder.set_proxy_config_service(std::move(proxy_config_service_));
    }

    if (!host_resolver_rules_.empty()) {
      std::unique_ptr<net::HostResolver> host_resolver(
          net::HostResolver::CreateDefaultResolver(nullptr /* net_log */));
      std::unique_ptr<net::MappedHostResolver> mapped_host_resolver(
          new net::MappedHostResolver(std::move(host_resolver)));
      mapped_host_resolver->SetRulesFromString(host_resolver_rules_);
      builder.set_host_resolver(std::move(mapped_host_resolver));
    }

    for (auto& pair : protocol_handlers_) {
      builder.SetProtocolHandler(pair.first,
                                 base::WrapUnique(pair.second.release()));
    }
    protocol_handlers_.clear();

    std::vector<std::unique_ptr<net::URLRequestInterceptor>>
        request_interceptors;
    for (auto it : request_interceptors_) {
      request_interceptors.push_back(base::WrapUnique(it));
    }
    request_interceptors_.weak_clear();
    builder.SetInterceptors(std::move(request_interceptors));

    url_request_context_ = builder.Build();
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessURLRequestContextGetter::GetNetworkTaskRunner() const {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::IO);
}

net::HostResolver* HeadlessURLRequestContextGetter::host_resolver() const {
  return url_request_context_->host_resolver();
}

}  // namespace headless
