// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace data_use_measurement {

// Class to store total network data used by some entity.
class DataUse {
 public:
  DataUse();
  ~DataUse();

  // Merge data use from another instance.
  void MergeFrom(const DataUse& other);

  const GURL& url() const { return url_; }

  void set_url(const GURL& url) { url_ = url; }

  const std::string& description() const { return description_; }

  void set_description(const std::string& description) {
    description_ = description;
  }

  int64_t total_bytes_received() const { return total_bytes_received_; }

  int64_t total_bytes_sent() const { return total_bytes_sent_; }

 private:
  friend class DataUseRecorder;

  GURL url_;
  std::string description_;

  int64_t total_bytes_sent_;
  int64_t total_bytes_received_;

  DISALLOW_COPY_AND_ASSIGN(DataUse);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_H_
