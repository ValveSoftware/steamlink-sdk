// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use.h"

namespace data_use_measurement {

DataUse::DataUse() : total_bytes_sent_(0), total_bytes_received_(0) {}

DataUse::~DataUse() {}

void DataUse::MergeFrom(const DataUse& other) {
  total_bytes_sent_ += other.total_bytes_sent_;
  total_bytes_received_ += other.total_bytes_received_;
}

}  // namespace data_use_measurement
