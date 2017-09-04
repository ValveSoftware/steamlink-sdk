// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"

namespace {
// Test values to replace the values specified in preprocessor defines.
static const char kDefaultOrigin[] = "origin.net:80";
static const char kDefaultFallbackOrigin[] = "fallback.net:80";
static const char kDefaultSecureProxyCheckURL[] = "http://proxycheck.net/";

static const char kFlagOrigin[] = "https://origin.org:443";
static const char kFlagFallbackOrigin[] = "fallback.org:80";
static const char kFlagSecureProxyCheckURL[] = "http://proxycheck.org/";
}

namespace data_reduction_proxy {
TestDataReductionProxyParams::TestDataReductionProxyParams(
    int flags, unsigned int has_definitions)
    : DataReductionProxyParams(flags, false),
      has_definitions_(has_definitions) {
  init_result_ = Init(flags & DataReductionProxyParams::kAllowed,
                      flags & DataReductionProxyParams::kFallbackAllowed);
  }

bool TestDataReductionProxyParams::init_result() const {
  return init_result_;
}

void TestDataReductionProxyParams::SetProxiesForHttp(
    const std::vector<net::ProxyServer>& proxies) {
  proxies_for_http_ = proxies;
}
// Test values to replace the values specified in preprocessor defines.
std::string TestDataReductionProxyParams::DefaultOrigin() {
  return kDefaultOrigin;
}

std::string TestDataReductionProxyParams::DefaultFallbackOrigin() {
  return kDefaultFallbackOrigin;
}

std::string TestDataReductionProxyParams::DefaultSecureProxyCheckURL() {
  return kDefaultSecureProxyCheckURL;
}

std::string TestDataReductionProxyParams::FlagOrigin() {
  return kFlagOrigin;
}

std::string TestDataReductionProxyParams::FlagFallbackOrigin() {
  return kFlagFallbackOrigin;
}

std::string TestDataReductionProxyParams::FlagSecureProxyCheckURL() {
  return kFlagSecureProxyCheckURL;
}

std::string TestDataReductionProxyParams::GetDefaultOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_ORIGIN, kDefaultOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultFallbackOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      kDefaultFallbackOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultSecureProxyCheckURL()
    const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL,
      kDefaultSecureProxyCheckURL);
}

std::string TestDataReductionProxyParams::GetDefinition(
    unsigned int has_def,
    const std::string& definition) const {
  return ((has_definitions_ & has_def) ? definition : std::string());
}
}  // namespace data_reduction_proxy
