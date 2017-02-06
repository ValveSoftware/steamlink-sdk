// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include <vector>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace data_reduction_proxy {

std::unique_ptr<DataReductionProxyMutableConfigValues>
DataReductionProxyMutableConfigValues::CreateFromParams(
    const DataReductionProxyParams* params) {
  std::unique_ptr<DataReductionProxyMutableConfigValues> config_values(
      new DataReductionProxyMutableConfigValues());
  config_values->promo_allowed_ = params->promo_allowed();
  config_values->holdback_ = params->holdback();
  config_values->allowed_ = params->allowed();
  config_values->fallback_allowed_ = params->fallback_allowed();
  config_values->secure_proxy_check_url_ = params->secure_proxy_check_url();
  return config_values;
}

DataReductionProxyMutableConfigValues::DataReductionProxyMutableConfigValues()
    : promo_allowed_(false),
      holdback_(false),
      allowed_(false),
      fallback_allowed_(false),
      use_override_proxies_for_http_(false) {
  use_override_proxies_for_http_ =
      params::GetOverrideProxiesForHttpFromCommandLine(
          &override_proxies_for_http_);

  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyMutableConfigValues::
    ~DataReductionProxyMutableConfigValues() {
}

bool DataReductionProxyMutableConfigValues::promo_allowed() const {
  return promo_allowed_;
}

bool DataReductionProxyMutableConfigValues::holdback() const {
  return holdback_;
}

bool DataReductionProxyMutableConfigValues::allowed() const {
  return allowed_;
}

bool DataReductionProxyMutableConfigValues::fallback_allowed() const {
  return fallback_allowed_;
}

const std::vector<net::ProxyServer>&
DataReductionProxyMutableConfigValues::proxies_for_http() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_override_proxies_for_http_ && !proxies_for_http_.empty()) {
    // Only override the proxies if a non-empty list of proxies would have been
    // returned otherwise. This is to prevent use of the proxies when the config
    // has been invalidated, since attempting to use a Data Reduction Proxy
    // without valid credentials could cause a proxy bypass.
    return override_proxies_for_http_;
  }
  // TODO(sclittle): Support overriding individual proxies in the proxy list
  // according to field trials such as the DRP QUIC field trial and their
  // corresponding command line flags (crbug.com/533637).
  return proxies_for_http_;
}

const GURL& DataReductionProxyMutableConfigValues::secure_proxy_check_url()
    const {
  return secure_proxy_check_url_;
}

void DataReductionProxyMutableConfigValues::UpdateValues(
    const std::vector<net::ProxyServer>& proxies_for_http) {
  DCHECK(thread_checker_.CalledOnValidThread());
  proxies_for_http_ = proxies_for_http;
}

void DataReductionProxyMutableConfigValues::Invalidate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  proxies_for_http_.clear();
}

}  // namespace data_reduction_proxy
