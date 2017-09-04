// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

class DataReductionProxyParams;

// A |DataReductionProxyConfigValues| which is permitted to change its
// underlying values via the UpdateValues method.
class DataReductionProxyMutableConfigValues
    : public DataReductionProxyConfigValues {
 public:
  // Creates a new |DataReductionProxyMutableConfigValues| using |params| as
  // the basis for its initial values.
  static std::unique_ptr<DataReductionProxyMutableConfigValues>
  CreateFromParams(const DataReductionProxyParams* params);

  ~DataReductionProxyMutableConfigValues() override;

  // Updates |proxies_for_http_| with the provided values.
  // Virtual for testing.
  virtual void UpdateValues(
      const std::vector<net::ProxyServer>& proxies_for_http);

  // Invalidates |this| by clearing the stored Data Reduction Proxy servers.
  void Invalidate();

  // Overrides of |DataReductionProxyConfigValues|
  bool promo_allowed() const override;
  bool holdback() const override;
  bool allowed() const override;
  bool fallback_allowed() const override;
  const std::vector<net::ProxyServer>& proxies_for_http() const override;
  const GURL& secure_proxy_check_url() const override;

 protected:
  DataReductionProxyMutableConfigValues();

 private:
  bool promo_allowed_;
  bool holdback_;
  bool allowed_;
  bool fallback_allowed_;
  std::vector<net::ProxyServer> proxies_for_http_;
  GURL secure_proxy_check_url_;

  // Permits use of locally specified Data Reduction Proxy servers instead of
  // ones specified from the Data Saver API.
  bool use_override_proxies_for_http_;
  std::vector<net::ProxyServer> override_proxies_for_http_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMutableConfigValues);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_
