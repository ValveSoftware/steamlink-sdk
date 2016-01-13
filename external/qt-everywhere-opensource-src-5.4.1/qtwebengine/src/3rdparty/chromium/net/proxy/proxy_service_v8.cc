// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service_v8.h"

#include "base/logging.h"
#include "net/proxy/network_delegate_error_observer.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"
#include "net/proxy/proxy_service.h"

namespace net {

// static
ProxyService* CreateProxyServiceUsingV8ProxyResolver(
    ProxyConfigService* proxy_config_service,
    ProxyScriptFetcher* proxy_script_fetcher,
    DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher,
    HostResolver* host_resolver,
    NetLog* net_log,
    NetworkDelegate* network_delegate) {
  DCHECK(proxy_config_service);
  DCHECK(proxy_script_fetcher);
  DCHECK(dhcp_proxy_script_fetcher);
  DCHECK(host_resolver);

  ProxyResolverErrorObserver* error_observer = new NetworkDelegateErrorObserver(
      network_delegate, base::MessageLoopProxy::current().get());

  ProxyResolver* proxy_resolver =
      new ProxyResolverV8Tracing(host_resolver, error_observer, net_log);

  ProxyService* proxy_service =
      new ProxyService(proxy_config_service, proxy_resolver, net_log);

  // Configure fetchers to use for PAC script downloads and auto-detect.
  proxy_service->SetProxyScriptFetchers(proxy_script_fetcher,
                                        dhcp_proxy_script_fetcher);

  return proxy_service;
}

}  // namespace net
