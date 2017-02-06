// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/cellular_logic_helper.h"

#include "components/variations/variations_associated_data.h"
#include "net/base/network_change_notifier.h"

namespace metrics {

namespace {

// Standard interval between log uploads, in seconds.
#if defined(OS_ANDROID) || defined(OS_IOS)
const int kStandardUploadIntervalSeconds = 5 * 60;           // Five minutes.
const int kStandardUploadIntervalCellularSeconds = 15 * 60;  // Fifteen minutes.
#else
const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.
#endif

#if defined(OS_ANDROID)
const bool kDefaultCellularLogicEnabled = true;
const bool kDefaultCellularLogicOptimization = true;
#else
const bool kDefaultCellularLogicEnabled = false;
const bool kDefaultCellularLogicOptimization = false;
#endif

}  // namespace

base::TimeDelta GetUploadInterval() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  if (IsCellularLogicEnabled())
    return base::TimeDelta::FromSeconds(kStandardUploadIntervalCellularSeconds);
#endif
  return base::TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
}

// Returns true if current connection type is cellular and user is assigned to
// experimental group for enabled cellular uploads.
bool IsCellularLogicEnabled() {
  std::string enabled = variations::GetVariationParamValue(
      "UMA_EnableCellularLogUpload", "Enabled");
  std::string optimized = variations::GetVariationParamValue(
      "UMA_EnableCellularLogUpload", "Optimize");
  bool is_enabled = kDefaultCellularLogicEnabled;
  if (!enabled.empty())
    is_enabled = (enabled == "true");

  bool is_optimized = kDefaultCellularLogicOptimization;
  if (!optimized.empty())
    is_optimized = (optimized == "true");

  if (!is_enabled || !is_optimized)
    return false;

  return net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
}

}  // namespace metrics
