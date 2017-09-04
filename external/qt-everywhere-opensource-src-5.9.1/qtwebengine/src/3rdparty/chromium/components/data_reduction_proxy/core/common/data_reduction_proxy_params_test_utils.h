// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_

#include <vector>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace base {
class TimeDelta;
}

namespace net {
class ProxyConfig;
class ProxyServer;
class URLRequest;
}

namespace data_reduction_proxy {

class TestDataReductionProxyParams : public DataReductionProxyParams {
 public:
  // Used to emulate having constants defined by the preprocessor.
  enum HasNames {
    HAS_NOTHING = 0x0,
    HAS_ORIGIN = 0x2,
    HAS_FALLBACK_ORIGIN = 0x4,
    HAS_SECURE_PROXY_CHECK_URL = 0x40,
    HAS_EVERYTHING = 0xff,
  };

  TestDataReductionProxyParams(int flags,
                               unsigned int has_definitions);
  bool init_result() const;

  void SetProxiesForHttp(const std::vector<net::ProxyServer>& proxies);

  // Test values to replace the values specified in preprocessor defines.
  static std::string DefaultOrigin();
  static std::string DefaultFallbackOrigin();
  static std::string DefaultSecureProxyCheckURL();

  static std::string FlagOrigin();
  static std::string FlagFallbackOrigin();
  static std::string FlagSecureProxyCheckURL();

 protected:
  std::string GetDefaultOrigin() const override;

  std::string GetDefaultFallbackOrigin() const override;

  std::string GetDefaultSecureProxyCheckURL() const override;

 private:
  std::string GetDefinition(unsigned int has_def,
                            const std::string& definition) const;

  unsigned int has_definitions_;
  bool init_result_;
};
}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_
