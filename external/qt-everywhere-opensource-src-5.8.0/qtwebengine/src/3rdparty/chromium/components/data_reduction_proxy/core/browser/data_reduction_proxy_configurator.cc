// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "net/proxy/proxy_config.h"

namespace data_reduction_proxy {

DataReductionProxyConfigurator::DataReductionProxyConfigurator(
    net::NetLog* net_log,
    DataReductionProxyEventCreator* event_creator)
    : net_log_(net_log), data_reduction_proxy_event_creator_(event_creator) {
  DCHECK(net_log);
  DCHECK(event_creator);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfigurator::~DataReductionProxyConfigurator() {
}

void DataReductionProxyConfigurator::Enable(
    bool secure_transport_restricted,
    const std::vector<net::ProxyServer>& proxies_for_http) {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyConfig config =
      CreateProxyConfig(secure_transport_restricted, proxies_for_http);
  data_reduction_proxy_event_creator_->AddProxyEnabledEvent(
      net_log_, secure_transport_restricted, proxies_for_http);
  config_ = config;
}

net::ProxyConfig DataReductionProxyConfigurator::CreateProxyConfig(
    bool secure_transport_restricted,
    const std::vector<net::ProxyServer>& proxies_for_http) const {
  net::ProxyConfig config;
  config.proxy_rules().type =
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;

  for (const auto& http_proxy : proxies_for_http) {
    if (!secure_transport_restricted ||
        (http_proxy.scheme() != net::ProxyServer::SCHEME_HTTPS &&
         http_proxy.scheme() != net::ProxyServer::SCHEME_QUIC)) {
      config.proxy_rules().proxies_for_http.AddProxyServer(http_proxy);
    }
  }

  if (!config.proxy_rules().proxies_for_http.IsEmpty()) {
    config.proxy_rules().proxies_for_http.AddProxyServer(
        net::ProxyServer::Direct());
  }

  config.proxy_rules().bypass_rules.ParseFromString(
      base::JoinString(bypass_rules_, ", "));
  // The ID is set to a bogus value. It cannot be left uninitialized, else the
  // config will return invalid.
  net::ProxyConfig::ID unused_id = 1;
  config.set_id(unused_id);
  return config;
}

void DataReductionProxyConfigurator::Disable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyConfig config = net::ProxyConfig::CreateDirect();
  data_reduction_proxy_event_creator_->AddProxyDisabledEvent(net_log_);
  config_ = config;
}

void DataReductionProxyConfigurator::AddHostPatternToBypass(
    const std::string& pattern) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bypass_rules_.push_back(pattern);
}

const net::ProxyConfig& DataReductionProxyConfigurator::GetProxyConfig() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_;
}

}  // namespace data_reduction_proxy
