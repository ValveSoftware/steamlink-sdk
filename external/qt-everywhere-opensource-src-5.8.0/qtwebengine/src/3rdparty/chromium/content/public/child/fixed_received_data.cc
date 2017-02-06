// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/child/fixed_received_data.h"

namespace content {

FixedReceivedData::FixedReceivedData(const char* data,
                                     size_t length,
                                     int encoded_length)
    : data_(data, data + length), encoded_length_(encoded_length) {
}

FixedReceivedData::FixedReceivedData(ReceivedData* data)
    : FixedReceivedData(data->payload(),
                        data->length(),
                        data->encoded_length()) {
}

FixedReceivedData::FixedReceivedData(const std::vector<char>& data,
                                     int encoded_length)
    : data_(data), encoded_length_(encoded_length) {
}

FixedReceivedData::~FixedReceivedData() {
}

const char* FixedReceivedData::payload() const {
  return data_.empty() ? nullptr : &data_[0];
}

int FixedReceivedData::length() const {
  return static_cast<int>(data_.size());
}

int FixedReceivedData::encoded_length() const {
  return encoded_length_;
}

}  // namespace content
