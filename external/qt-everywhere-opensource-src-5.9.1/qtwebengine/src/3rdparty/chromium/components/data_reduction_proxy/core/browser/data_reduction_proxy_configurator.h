// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "net/proxy/proxy_config.h"

namespace net {
class NetLog;
class ProxyInfo;
class ProxyServer;
class ProxyService;
}

class PrefService;

namespace data_reduction_proxy {

class DataReductionProxyEventCreator;

class DataReductionProxyConfigurator {
 public:
  // Constructs a configurator. |net_log| and |event_creator| are used to
  // track network and Data Reduction Proxy events respectively, must not be
  // null, and must outlive this instance.
  DataReductionProxyConfigurator(net::NetLog* net_log,
                                 DataReductionProxyEventCreator* event_creator);

  virtual ~DataReductionProxyConfigurator();

  // Enables data reduction using the proxy servers in |proxies_for_http|.
  // |secure_transport_restricted| indicates that proxies going over secure
  // transports can not be used.
  virtual void Enable(bool secure_transport_restricted,
                      const std::vector<net::ProxyServer>& proxies_for_http);

  // Constructs a proxy configuration suitable for disabling the Data Reduction
  // proxy.
  virtual void Disable();

  // Adds a host pattern to bypass. This should follow the same syntax used
  // in net::ProxyBypassRules; that is, a hostname pattern, a hostname suffix
  // pattern, an IP literal, a CIDR block, or the magic string '<local>'.
  // Bypass settings persist for the life of this object and are applied
  // each time the proxy is enabled, but are not updated while it is enabled.
  virtual void AddHostPatternToBypass(const std::string& pattern);

  // Returns the current data reduction proxy config, even if it is not the
  // effective configuration used by the proxy service.
  const net::ProxyConfig& GetProxyConfig() const;

  // Constructs a proxy configuration suitable for enabling the Data Reduction
  // proxy. If true, |secure_transport_restricted| indicates that proxies going
  // over secure transports (HTTPS) should/can not be used.
  net::ProxyConfig CreateProxyConfig(
      bool secure_transport_restricted,
      const std::vector<net::ProxyServer>& proxies_for_http) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfiguratorTest, TestBypassList);

  // Rules for bypassing the Data Reduction Proxy.
  std::vector<std::string> bypass_rules_;

  // The Data Reduction Proxy's configuration. This contains the list of
  // acceptable data reduction proxies and bypass rules. It should be accessed
  // only on the IO thread.
  net::ProxyConfig config_;

  // Used for logging of network- and Data Reduction Proxy-related events.
  net::NetLog* net_log_;
  DataReductionProxyEventCreator* data_reduction_proxy_event_creator_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigurator);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
