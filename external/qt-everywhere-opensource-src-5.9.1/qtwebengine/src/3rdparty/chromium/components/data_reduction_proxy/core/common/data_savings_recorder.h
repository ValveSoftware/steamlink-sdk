// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_SAVINGS_RECORDER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_SAVINGS_RECORDER_H_

#include <stdint.h>

#include <string>

namespace data_reduction_proxy {

// An interface that will record data savings based on the amount of data used,
// the original size of the data, and the request type.
class DataSavingsRecorder {
 public:
  // Records detailed data usage. Assumes |data_used| is recorded already by
  // previous handling of URLRequests. Also records daily data savings
  // statistics to prefs and reports data savings UMA. |data_used| and
  // |original_size| are measured in bytes.
  // Returns true if data savings was recorded.
  virtual bool UpdateDataSavings(const std::string& data_usage_host,
                                 int64_t data_used,
                                 int64_t original_size) = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_SAVINGS_RECORDER_H_
