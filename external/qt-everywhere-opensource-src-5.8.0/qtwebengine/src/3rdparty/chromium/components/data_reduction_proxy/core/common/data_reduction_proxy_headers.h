// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/proxy/proxy_service.h"

class GURL;

namespace net {

class HttpResponseHeaders;

}  // namespace net

namespace data_reduction_proxy {

class DataReductionProxyEventCreator;

// Values of the UMA DataReductionProxy.BypassType{Primary|Fallback} and
// DataReductionProxy.BlockType{Primary|Fallback} histograms. This enum must
// remain synchronized with the enum of the same name in
// metrics/histograms/histograms.xml.
enum DataReductionProxyBypassType {
#define BYPASS_EVENT_TYPE(label, value) BYPASS_EVENT_TYPE_ ## label = value,
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_type_list.h"
#undef BYPASS_EVENT_TYPE
};

// Values for the bypass actions that can be specified by the Data Reduction
// Proxy in response to a client request. These are explicit bypass actions
// specified by the Data Reduction Proxy in the Chrome-Proxy header, block-once,
// bypass=1, block=300, etc. These are not used for Chrome initiated bypasses
// due to a server error, missing Via header, etc.
enum DataReductionProxyBypassAction {
#define BYPASS_ACTION_TYPE(label, value) BYPASS_ACTION_TYPE_##label = value,
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_action_list.h"
#undef BYPASS_ACTION_TYPE
};

// Contains instructions contained in the Chrome-Proxy header.
struct DataReductionProxyInfo {
  DataReductionProxyInfo()
      : bypass_all(false),
        mark_proxies_as_bad(false),
        bypass_action(BYPASS_ACTION_TYPE_NONE) {}

  // True if Chrome should bypass all available data reduction proxies. False
  // if only the currently connected data reduction proxy should be bypassed.
  bool bypass_all;

  // True iff Chrome should mark the data reduction proxy or proxies as bad for
  // the period of time specified in |bypass_duration|.
  bool mark_proxies_as_bad;

  // Amount of time to bypass the data reduction proxy or proxies. This value is
  // ignored if |mark_proxies_as_bad| is false.
  base::TimeDelta bypass_duration;

  // The bypass action specified by the data reduction proxy.
  DataReductionProxyBypassAction bypass_action;
};

// Gets the header used for data reduction proxy requests and responses.
const char* chrome_proxy_header();

// Gets the Chrome-Proxy directive used by data reduction proxy Lo-Fi requests
// and responses.
const char* chrome_proxy_lo_fi_directive();

// Gets the Chrome-Proxy directive used by data reduction proxy Lo-Fi preview
// requests and responses.
const char* chrome_proxy_lo_fi_preview_directive();

// Gets the Chrome-Proxy directive used by data reduction proxy Lo-Fi preview
// experiment to ignore the blacklist.
const char* chrome_proxy_lo_fi_ignore_preview_blacklist_directive();

// Returns true if the Chrome-Proxy header is present and contains a bypass
// delay. Sets |proxy_info->bypass_duration| to the specified delay if greater
// than 0, and to 0 otherwise to indicate that the default proxy delay
// (as specified in |ProxyList::UpdateRetryInfoOnFallback|) should be used.
// If all available data reduction proxies should by bypassed, |bypass_all| is
// set to true. |proxy_info| must be non-NULL.
bool ParseHeadersForBypassInfo(const net::HttpResponseHeaders* headers,
                               DataReductionProxyInfo* proxy_info);

// Returns true if the response contains the data reduction proxy Via header
// value. If non-NULL, sets |has_intermediary| to true if another server added
// a Via header after the data reduction proxy, and to false otherwise. Used to
// check the integrity of data reduction proxy responses and whether there are
// other middleboxes between the data reduction proxy and the client.
bool HasDataReductionProxyViaHeader(const net::HttpResponseHeaders* headers,
                                    bool* has_intermediary);

// Returns the reason why the Chrome proxy should be bypassed or not, and
// populates |proxy_info| with information on how long to bypass if
// applicable.
DataReductionProxyBypassType GetDataReductionProxyBypassType(
    const net::HttpResponseHeaders* headers,
    DataReductionProxyInfo* proxy_info);

// Searches for the specified Chrome-Proxy action, and if present saves its
// value as a string in |action_value|. Only returns the first one and ignores
// the rest if multiple actions match |action_prefix|.
bool GetDataReductionProxyActionValue(const net::HttpResponseHeaders* headers,
                                      base::StringPiece action_prefix,
                                      std::string* action_value);

// Searches for the specified Chrome-Proxy action, and if present interprets
// its value as a duration in seconds.
bool ParseHeadersAndSetBypassDuration(const net::HttpResponseHeaders* headers,
                                      base::StringPiece action_prefix,
                                      base::TimeDelta* bypass_duration);

// Gets the fingerprint of the Chrome-Proxy header.
bool GetDataReductionProxyActionFingerprintChromeProxy(
    const net::HttpResponseHeaders* headers,
    std::string* chrome_proxy_fingerprint);

// Gets the fingerprint of the Via header.
bool GetDataReductionProxyActionFingerprintVia(
    const net::HttpResponseHeaders* headers,
    std::string* via_fingerprint);

// Gets the fingerprint of a list of headers.
bool GetDataReductionProxyActionFingerprintOtherHeaders(
    const net::HttpResponseHeaders* headers,
    std::string* other_headers_fingerprint);

// Gets the fingerprint of Content-Length header.
bool GetDataReductionProxyActionFingerprintContentLength(
    const net::HttpResponseHeaders* headers,
    std::string* content_length_fingerprint);

// Returns values of the Chrome-Proxy header, but with its fingerprint removed.
void GetDataReductionProxyHeaderWithFingerprintRemoved(
    const net::HttpResponseHeaders* headers,
    std::vector<std::string>* values);

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_
