// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"

namespace data_reduction_proxy {
namespace prefs {

// A List pref that contains daily totals of the size of all HTTPS
// content received when the data reduction proxy was enabled.
const char kDailyContentLengthHttpsWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_https_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received when a bypass of more than 30 minutes is in effect.
const char kDailyContentLengthLongBypassWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_long_bypass_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received when a bypass of less than 30 minutes is in effect.
const char kDailyContentLengthShortBypassWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_short_bypass_with_"
    "data_reduction_proxy_enabled";

// TODO(bengr): what is this?
const char kDailyContentLengthUnknownWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_unknown_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxy[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with application mime-type received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxyApplication[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy_application";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with video mime-type received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxyVideo[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy_video";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with unknown mime-type received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxyUnknown[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy_unknown";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received while the data reduction proxy is enabled.
const char kDailyContentLengthWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with application mime-type received while the data reduction proxy is
// enabled.
const char kDailyContentLengthWithDataReductionProxyEnabledApplication[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled_"
    "application";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with video mime-type received while the data reduction proxy is
// enabled.
const char kDailyContentLengthWithDataReductionProxyEnabledVideo[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled_"
    "video";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with unknown mime-type received while the data reduction proxy is
// enabled.
const char kDailyContentLengthWithDataReductionProxyEnabledUnknown[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled_"
    "unknown";

// An int64_t pref that contains an internal representation of midnight on the
// date of the last update to |kDailyHttp{Original,Received}ContentLength|.
const char kDailyHttpContentLengthLastUpdateDate[] =
    "data_reduction.last_update_date";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received from the network.
const char kDailyHttpOriginalContentLength[] =
    "data_reduction.daily_original_length";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with application mime-type received from the network.
const char kDailyHttpOriginalContentLengthApplication[] =
    "data_reduction.daily_original_length_application";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with video mime-type received from the network.
const char kDailyHttpOriginalContentLengthVideo[] =
    "data_reduction.daily_original_length_video";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with unknown mime-type received from the network.
const char kDailyHttpOriginalContentLengthUnknown[] =
    "data_reduction.daily_original_length_unknown";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received from the network.
const char kDailyHttpReceivedContentLength[] =
    "data_reduction.daily_received_length";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received with application mime-type  from the network.
const char kDailyHttpReceivedContentLengthApplication[] =
    "data_reduction.daily_received_length_application";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received with video mime-type from the network.
const char kDailyHttpReceivedContentLengthVideo[] =
    "data_reduction.daily_received_length_video";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received with unknown mime-type  from the network.
const char kDailyHttpReceivedContentLengthUnknown[] =
    "data_reduction.daily_received_length_unknown";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxy[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with application mime-type received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxyApplication[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy_application";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with video mime-type received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxyVideo[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy_video";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with unknown mime-type received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxyUnknown[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy_unknown";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received while the data reduction proxy is enabled.
const char kDailyOriginalContentLengthWithDataReductionProxyEnabled[] =
    "data_reduction.daily_original_length_with_data_reduction_proxy_enabled";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with application mime type received while the data reduction proxy is
// enabled.
const char
    kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication[] =
        "data_reduction.daily_original_length_with_data_reduction_proxy_"
        "enabled_application";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with video mime type received while the data reduction proxy is
// enabled.
const char kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo[] =
    "data_reduction.daily_original_length_with_data_reduction_proxy_enabled_"
    "video";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with unknown mime type received while the data reduction proxy is
// enabled.
const char kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown[] =
    "data_reduction.daily_original_length_with_data_reduction_proxy_enabled_"
    "unknown";

// String that specifies the origin allowed to use data reduction proxy
// authentication, if any.
const char kDataReductionProxy[] = "auth.spdyproxy.origin";

// String that specifies a persisted Data Reduction Proxy configuration.
const char kDataReductionProxyConfig[] = "data_reduction.config";

// A boolean specifying whether data usage should be collected for reporting.
const char kDataUsageReportingEnabled[] = "data_usage_reporting.enabled";

// A boolean specifying whether the data reduction proxy was ever enabled
// before.
const char kDataReductionProxyWasEnabledBefore[] =
    "spdy_proxy.was_enabled_before";

// An int64_t pref that contains the total size of all HTTP content received
// from the network.
const char kHttpReceivedContentLength[] = "http_received_content_length";

// An int64_t pref that contains the total original size of all HTTP content
// received over the network.
const char kHttpOriginalContentLength[] = "http_original_content_length";

// An integer pref that contains the Lo-Fi epoch for the implicit opt-out rules.
// Any time this value is incremented via Finch,
// kLoFiConsecutiveSessionDisables is reset to zero.
const char kLoFiImplicitOptOutEpoch[] =
    "data_reduction_lo_fi.implicit_opt_out_epoch";

// An integer pref that contains the number of times that "Load images" Lo-Fi
// snackbar has been shown for the current session.
const char kLoFiSnackbarsShownPerSession[] =
    "data_reduction_lo_fi.load_images_snackbars_shown_per_session";

// An integer pref that contains the number of times that "Load images" has been
// requested on the Lo-Fi snackbar for the current session.
const char kLoFiLoadImagesPerSession[] =
    "data_reduction_lo_fi.load_images_requests_per_session";

// An integer pref that contains the number of consecutive sessions that LoFi
// has been disabled.
const char kLoFiConsecutiveSessionDisables[] =
    "data_reduction_lo_fi.consecutive_session_disables";

// A boolean pref specifying whether Lo-Fi was used this session.
const char kLoFiWasUsedThisSession[] =
    "data_reduction_lo_fi.was_used_this_session";

// Pref to store the retrieval time of the last simulated Data Reduction Proxy
// configuration. This is part of an experiment to see how many bytes are lost
// if the Data Reduction Proxy is not used due to configuration being expired
// or not available.
const char kSimulatedConfigRetrieveTime[] =
    "data_reduction.simulated_config_retrieve_time";

// A boolean specifying whether the data reduction proxy statistics preferences
// have migrated from local state to the profile.
const char kStatisticsPrefsMigrated[] =
    "data_reduction.statistics_prefs_migrated";

}  // namespace prefs
}  // namespace data_reduction_proxy
