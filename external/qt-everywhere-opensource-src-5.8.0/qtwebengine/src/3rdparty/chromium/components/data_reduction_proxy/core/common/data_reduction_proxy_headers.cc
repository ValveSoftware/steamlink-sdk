// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

using base::StringPiece;
using base::TimeDelta;

namespace {

const char kChromeProxyHeader[] = "chrome-proxy";

const char kActionValueDelimiter = '=';

const char kChromeProxyLoFiDirective[] = "q=low";
const char kChromeProxyLoFiPreviewDirective[] = "q=preview";
const char kChromeProxyLoFiIngorePreviewBlacklistDirective[] =
    "exp=ignore_preview_blacklist";

const char kChromeProxyActionBlockOnce[] = "block-once";
const char kChromeProxyActionBlock[] = "block";
const char kChromeProxyActionBypass[] = "bypass";

// Actions for tamper detection fingerprints.
const char kChromeProxyActionFingerprintChromeProxy[]   = "fcp";
const char kChromeProxyActionFingerprintVia[]           = "fvia";
const char kChromeProxyActionFingerprintOtherHeaders[]  = "foh";
const char kChromeProxyActionFingerprintContentLength[] = "fcl";

const int kShortBypassMaxSeconds = 59;
const int kMediumBypassMaxSeconds = 300;

// Returns a random bypass duration between 1 and 5 minutes.
base::TimeDelta GetDefaultBypassDuration() {
  const int64_t delta_ms =
      base::RandInt(base::TimeDelta::FromMinutes(1).InMilliseconds(),
                    base::TimeDelta::FromMinutes(5).InMilliseconds());
  return TimeDelta::FromMilliseconds(delta_ms);
}

bool StartsWithActionPrefix(base::StringPiece header_value,
                            base::StringPiece action_prefix) {
  DCHECK(!action_prefix.empty());
  // A valid action does not include a trailing '='.
  DCHECK(action_prefix[action_prefix.size() - 1] != kActionValueDelimiter);

  return header_value.size() > action_prefix.size() + 1 &&
         header_value[action_prefix.size()] == kActionValueDelimiter &&
         base::StartsWith(header_value, action_prefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

namespace data_reduction_proxy {

const char* chrome_proxy_header() {
  return kChromeProxyHeader;
}

const char* chrome_proxy_lo_fi_directive() {
  return kChromeProxyLoFiDirective;
}

const char* chrome_proxy_lo_fi_preview_directive() {
  return kChromeProxyLoFiPreviewDirective;
}

const char* chrome_proxy_lo_fi_ignore_preview_blacklist_directive() {
  return kChromeProxyLoFiIngorePreviewBlacklistDirective;
}

bool GetDataReductionProxyActionValue(const net::HttpResponseHeaders* headers,
                                      base::StringPiece action_prefix,
                                      std::string* action_value) {
  DCHECK(headers);
  size_t iter = 0;
  std::string value;

  while (headers->EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (StartsWithActionPrefix(value, action_prefix)) {
      if (action_value)
        *action_value = value.substr(action_prefix.size() + 1);
      return true;
    }
  }
  return false;
}

bool ParseHeadersAndSetBypassDuration(const net::HttpResponseHeaders* headers,
                                      base::StringPiece action_prefix,
                                      base::TimeDelta* bypass_duration) {
  DCHECK(headers);
  size_t iter = 0;
  std::string value;

  while (headers->EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (StartsWithActionPrefix(value, action_prefix)) {
      int64_t seconds;
      if (!base::StringToInt64(
              StringPiece(value).substr(action_prefix.size() + 1), &seconds) ||
          seconds < 0) {
        continue;  // In case there is a well formed instruction.
      }
      if (seconds != 0) {
        *bypass_duration = TimeDelta::FromSeconds(seconds);
      } else {
        // Server deferred to us to choose a duration. Default to a random
        // duration between one and five minutes.
        *bypass_duration = GetDefaultBypassDuration();
      }
      return true;
    }
  }
  return false;
}

bool ParseHeadersForBypassInfo(const net::HttpResponseHeaders* headers,
                               DataReductionProxyInfo* proxy_info) {
  DCHECK(proxy_info);

  // Support header of the form Chrome-Proxy: bypass|block=<duration>, where
  // <duration> is the number of seconds to wait before retrying
  // the proxy. If the duration is 0, then the default proxy retry delay
  // (specified in |ProxyList::UpdateRetryInfoOnFallback|) will be used.
  // 'bypass' instructs Chrome to bypass the currently connected data reduction
  // proxy, whereas 'block' instructs Chrome to bypass all available data
  // reduction proxies.

  // 'block' takes precedence over 'bypass' and 'block-once', so look for it
  // first.
  // TODO(bengr): Reduce checks for 'block' and 'bypass' to a single loop.
  if (ParseHeadersAndSetBypassDuration(
          headers, kChromeProxyActionBlock, &proxy_info->bypass_duration)) {
    proxy_info->bypass_all = true;
    proxy_info->mark_proxies_as_bad = true;
    proxy_info->bypass_action = BYPASS_ACTION_TYPE_BLOCK;
    return true;
  }

  // Next, look for 'bypass'.
  if (ParseHeadersAndSetBypassDuration(
          headers, kChromeProxyActionBypass, &proxy_info->bypass_duration)) {
    proxy_info->bypass_all = false;
    proxy_info->mark_proxies_as_bad = true;
    proxy_info->bypass_action = BYPASS_ACTION_TYPE_BYPASS;
    return true;
  }

  // Lastly, look for 'block-once'. 'block-once' instructs Chrome to retry the
  // current request (if it's idempotent), bypassing all available data
  // reduction proxies. Unlike 'block', 'block-once' does not cause data
  // reduction proxies to be bypassed for an extended period of time;
  // 'block-once' only affects the retry of the current request.
  if (headers->HasHeaderValue(kChromeProxyHeader,
                              kChromeProxyActionBlockOnce)) {
    proxy_info->bypass_all = true;
    proxy_info->mark_proxies_as_bad = false;
    proxy_info->bypass_duration = TimeDelta();
    proxy_info->bypass_action = BYPASS_ACTION_TYPE_BLOCK_ONCE;
    return true;
  }

  return false;
}

bool HasDataReductionProxyViaHeader(const net::HttpResponseHeaders* headers,
                                    bool* has_intermediary) {
  static const size_t kVersionSize = 4;
  static const char kDataReductionProxyViaValue[] = "Chrome-Compression-Proxy";
  size_t iter = 0;
  std::string value;

  // Case-sensitive comparison of |value|. Assumes the received protocol and the
  // space following it are always |kVersionSize| characters. E.g.,
  // 'Via: 1.1 Chrome-Compression-Proxy'
  while (headers->EnumerateHeader(&iter, "via", &value)) {
    if (base::StringPiece(value).substr(
            kVersionSize, arraysize(kDataReductionProxyViaValue) - 1) ==
        kDataReductionProxyViaValue) {
      if (has_intermediary)
        // We assume intermediary exists if there is another Via header after
        // the data reduction proxy's Via header.
        *has_intermediary = !(headers->EnumerateHeader(&iter, "via", &value));
      return true;
    }
  }

  return false;
}

DataReductionProxyBypassType GetDataReductionProxyBypassType(
    const net::HttpResponseHeaders* headers,
    DataReductionProxyInfo* data_reduction_proxy_info) {
  DCHECK(data_reduction_proxy_info);
  if (ParseHeadersForBypassInfo(headers, data_reduction_proxy_info)) {
    // A chrome-proxy response header is only present in a 502. For proper
    // reporting, this check must come before the 5xx checks below.
    if (!data_reduction_proxy_info->mark_proxies_as_bad)
      return BYPASS_EVENT_TYPE_CURRENT;

    const TimeDelta& duration = data_reduction_proxy_info->bypass_duration;
    if (duration <= TimeDelta::FromSeconds(kShortBypassMaxSeconds))
      return BYPASS_EVENT_TYPE_SHORT;
    if (duration <= TimeDelta::FromSeconds(kMediumBypassMaxSeconds))
      return BYPASS_EVENT_TYPE_MEDIUM;
    return BYPASS_EVENT_TYPE_LONG;
  }

  // If a bypass is triggered by any of the following cases, then the data
  // reduction proxy should be bypassed for a random duration between 1 and 5
  // minutes.
  data_reduction_proxy_info->mark_proxies_as_bad = true;
  data_reduction_proxy_info->bypass_duration = GetDefaultBypassDuration();

  // Fall back if a 500, 502 or 503 is returned.
  if (headers->response_code() == net::HTTP_INTERNAL_SERVER_ERROR)
    return BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR;
  if (headers->response_code() == net::HTTP_BAD_GATEWAY)
    return BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY;
  if (headers->response_code() == net::HTTP_SERVICE_UNAVAILABLE)
    return BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE;
  // TODO(kundaji): Bypass if Proxy-Authenticate header value cannot be
  // interpreted by data reduction proxy.
  if (headers->response_code() == net::HTTP_PROXY_AUTHENTICATION_REQUIRED &&
      !headers->HasHeader("Proxy-Authenticate")) {
    return BYPASS_EVENT_TYPE_MALFORMED_407;
  }
  if (!HasDataReductionProxyViaHeader(headers, NULL) &&
      (headers->response_code() != net::HTTP_NOT_MODIFIED)) {
    // A Via header might not be present in a 304. Since the goal of a 304
    // response is to minimize information transfer, a sender in general
    // should not generate representation metadata other than Cache-Control,
    // Content-Location, Date, ETag, Expires, and Vary.

    // The proxy Via header might also not be present in a 4xx response.
    // Separate this case from other responses that are missing the header.
    if (headers->response_code() >= net::HTTP_BAD_REQUEST &&
        headers->response_code() < net::HTTP_INTERNAL_SERVER_ERROR) {
      // At this point, any 4xx response that is missing the via header
      // indicates an issue that is scoped to only the current request, so only
      // bypass the data reduction proxy for the current request.
      data_reduction_proxy_info->bypass_all = true;
      data_reduction_proxy_info->mark_proxies_as_bad = false;
      data_reduction_proxy_info->bypass_duration = TimeDelta();
      return BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX;
    }

    // Missing the via header should not trigger bypass if the client is
    // included in the tamper detection experiment.
    if (!params::IsIncludedInTamperDetectionExperiment())
      return BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER;
  }
  // There is no bypass event.
  return BYPASS_EVENT_TYPE_MAX;
}

bool GetDataReductionProxyActionFingerprintChromeProxy(
    const net::HttpResponseHeaders* headers,
    std::string* chrome_proxy_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintChromeProxy,
      chrome_proxy_fingerprint);
}

bool GetDataReductionProxyActionFingerprintVia(
    const net::HttpResponseHeaders* headers,
    std::string* via_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintVia,
      via_fingerprint);
}

bool GetDataReductionProxyActionFingerprintOtherHeaders(
    const net::HttpResponseHeaders* headers,
    std::string* other_headers_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintOtherHeaders,
      other_headers_fingerprint);
}

bool GetDataReductionProxyActionFingerprintContentLength(
    const net::HttpResponseHeaders* headers,
    std::string* content_length_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintContentLength,
      content_length_fingerprint);
}

void GetDataReductionProxyHeaderWithFingerprintRemoved(
    const net::HttpResponseHeaders* headers,
    std::vector<std::string>* values) {
  DCHECK(values);

  std::string value;
  size_t iter = 0;
  while (headers->EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (StartsWithActionPrefix(value, kChromeProxyActionFingerprintChromeProxy))
      continue;
    values->push_back(std::move(value));
  }
}

}  // namespace data_reduction_proxy
