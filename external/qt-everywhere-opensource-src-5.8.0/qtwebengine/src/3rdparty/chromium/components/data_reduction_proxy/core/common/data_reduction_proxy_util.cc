// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/data_reduction_proxy/core/common/version.h"
#include "net/base/url_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"

#if defined(USE_GOOGLE_API_KEYS)
#include "google_apis/google_api_keys.h"
#endif

namespace data_reduction_proxy {

namespace {

#if defined(USE_GOOGLE_API_KEYS)
// Used in all Data Reduction Proxy URLs to specify API Key.
const char kApiKeyName[] = "key";
#endif
}  // namespace

namespace util {

const char* ChromiumVersion() {
  // Assert at compile time that the Chromium version is at least somewhat
  // properly formed, e.g. the version string is at least as long as "0.0.0.0",
  // and starts and ends with numeric digits. This is to prevent another
  // regression like http://crbug.com/595471.
  static_assert(arraysize(PRODUCT_VERSION) >= arraysize("0.0.0.0") &&
                    '0' <= PRODUCT_VERSION[0] && PRODUCT_VERSION[0] <= '9' &&
                    '0' <= PRODUCT_VERSION[arraysize(PRODUCT_VERSION) - 2] &&
                    PRODUCT_VERSION[arraysize(PRODUCT_VERSION) - 2] <= '9',
                "PRODUCT_VERSION must be a string of the form "
                "'MAJOR.MINOR.BUILD.PATCH', e.g. '1.2.3.4'. "
                "PRODUCT_VERSION='" PRODUCT_VERSION "' is badly formed.");

  return PRODUCT_VERSION;
}

void GetChromiumBuildAndPatch(const std::string& version_string,
                              std::string* build,
                              std::string* patch) {
  uint32_t build_number;
  uint32_t patch_number;
  GetChromiumBuildAndPatchAsInts(version_string, &build_number, &patch_number);
  *build = base::Uint64ToString(build_number);
  *patch = base::Uint64ToString(patch_number);
}

void GetChromiumBuildAndPatchAsInts(const std::string& version_string,
                                    uint32_t* build,
                                    uint32_t* patch) {
  base::Version version(version_string);
  DCHECK(version.IsValid());
  DCHECK_EQ(4U, version.components().size());
  *build = version.components()[2];
  *patch = version.components()[3];
}

const char* GetStringForClient(Client client) {
  switch (client) {
    case Client::UNKNOWN:
      return "";
    case Client::CRONET_ANDROID:
      return "cronet";
    case Client::WEBVIEW_ANDROID:
      return "webview";
    case Client::CHROME_ANDROID:
      return "android";
    case Client::CHROME_IOS:
      return "ios";
    case Client::CHROME_MAC:
      return "mac";
    case Client::CHROME_CHROMEOS:
      return "chromeos";
    case Client::CHROME_LINUX:
      return "linux";
    case Client::CHROME_WINDOWS:
      return "win";
    case Client::CHROME_FREEBSD:
      return "freebsd";
    case Client::CHROME_OPENBSD:
      return "openbsd";
    case Client::CHROME_SOLARIS:
      return "solaris";
    case Client::CHROME_QNX:
      return "qnx";
    default:
      NOTREACHED();
      return "";
  }
}

bool IsMethodIdempotent(const std::string& method) {
  return method == "GET" || method == "OPTIONS" || method == "HEAD" ||
         method == "PUT" || method == "DELETE" || method == "TRACE";
}

GURL AddApiKeyToUrl(const GURL& url) {
  GURL new_url = url;
#if defined(USE_GOOGLE_API_KEYS)
  std::string api_key = google_apis::GetAPIKey();
  if (google_apis::HasKeysConfigured() && !api_key.empty()) {
    new_url = net::AppendOrReplaceQueryParameter(url, kApiKeyName, api_key);
  }
#endif
  return net::AppendOrReplaceQueryParameter(new_url, "alt", "proto");
}

bool EligibleForDataReductionProxy(const net::ProxyInfo& proxy_info,
                                   const GURL& url,
                                   const std::string& method) {
  return proxy_info.is_direct() && proxy_info.proxy_list().size() == 1 &&
         !url.SchemeIsWSOrWSS() && IsMethodIdempotent(method);
}

bool ApplyProxyConfigToProxyInfo(const net::ProxyConfig& proxy_config,
                                 const net::ProxyRetryInfoMap& proxy_retry_info,
                                 const GURL& url,
                                 net::ProxyInfo* data_reduction_proxy_info) {
  DCHECK(data_reduction_proxy_info);
  if (!proxy_config.is_valid())
    return false;
  proxy_config.proxy_rules().Apply(url, data_reduction_proxy_info);
  data_reduction_proxy_info->DeprioritizeBadProxies(proxy_retry_info);
  return !data_reduction_proxy_info->proxy_server().is_direct();
}

}  // namespace util

namespace protobuf_parser {

net::ProxyServer::Scheme SchemeFromProxyScheme(
    ProxyServer_ProxyScheme proxy_scheme) {
  switch (proxy_scheme) {
    case ProxyServer_ProxyScheme_HTTP:
      return net::ProxyServer::SCHEME_HTTP;
    case ProxyServer_ProxyScheme_HTTPS:
      return net::ProxyServer::SCHEME_HTTPS;
    case ProxyServer_ProxyScheme_QUIC:
      return net::ProxyServer::SCHEME_QUIC;
    default:
      return net::ProxyServer::SCHEME_INVALID;
  }
}

ProxyServer_ProxyScheme ProxySchemeFromScheme(net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return ProxyServer_ProxyScheme_HTTP;
    case net::ProxyServer::SCHEME_HTTPS:
      return ProxyServer_ProxyScheme_HTTPS;
    case net::ProxyServer::SCHEME_QUIC:
      return ProxyServer_ProxyScheme_QUIC;
    default:
      return ProxyServer_ProxyScheme_UNSPECIFIED;
  }
}

void TimeDeltaToDuration(const base::TimeDelta& time_delta,
                         Duration* duration) {
  duration->set_seconds(time_delta.InSeconds());
  base::TimeDelta partial_seconds =
      time_delta - base::TimeDelta::FromSeconds(time_delta.InSeconds());
  duration->set_nanos(partial_seconds.InMicroseconds() *
                      base::Time::kNanosecondsPerMicrosecond);
}

base::TimeDelta DurationToTimeDelta(const Duration& duration) {
  return base::TimeDelta::FromSeconds(duration.seconds()) +
         base::TimeDelta::FromMicroseconds(
             duration.nanos() / base::Time::kNanosecondsPerMicrosecond);
}

void TimeToTimestamp(const base::Time& time, Timestamp* timestamp) {
  timestamp->set_seconds((time - base::Time::UnixEpoch()).InSeconds());
  timestamp->set_nanos(((time - base::Time::UnixEpoch()).InMicroseconds() %
                        base::Time::kMicrosecondsPerSecond) *
                       base::Time::kNanosecondsPerMicrosecond);
}

base::Time TimestampToTime(const Timestamp& timestamp) {
  base::Time t = base::Time::UnixEpoch();
  t += base::TimeDelta::FromSeconds(timestamp.seconds());
  t += base::TimeDelta::FromMicroseconds(
      timestamp.nanos() / base::Time::kNanosecondsPerMicrosecond);
  return t;
}

std::unique_ptr<Duration> CreateDurationFromTimeDelta(
    const base::TimeDelta& time_delta) {
  std::unique_ptr<Duration> duration(new Duration);
  TimeDeltaToDuration(time_delta, duration.get());
  return duration;
}

std::unique_ptr<Timestamp> CreateTimestampFromTime(const base::Time& time) {
  std::unique_ptr<Timestamp> timestamp(new Timestamp);
  TimeToTimestamp(time, timestamp.get());
  return timestamp;
}

}  // namespace protobuf_parser

}  // namespace data_reduction_proxy
