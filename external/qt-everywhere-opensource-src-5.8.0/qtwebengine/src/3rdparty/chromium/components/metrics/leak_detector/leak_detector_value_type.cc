// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector_value_type.h"

#include <stdio.h>

namespace metrics {
namespace leak_detector {

bool LeakDetectorValueType::operator==(
    const LeakDetectorValueType& other) const {
  if (type_ != other.type_)
    return false;

  switch (type_) {
    case TYPE_SIZE:
      return size_ == other.size_;
    case TYPE_CALL_STACK:
      return call_stack_ == other.call_stack_;
    case TYPE_NONE:
      // "NONE" types are considered to be all identical.
      return true;
  }
  return false;
}

bool LeakDetectorValueType::operator<(
    const LeakDetectorValueType& other) const {
  if (type_ != other.type_)
    return type_ < other.type_;

  switch (type_) {
    case TYPE_SIZE:
      return size_ < other.size_;
    case TYPE_CALL_STACK:
      return call_stack_ < other.call_stack_;
    case TYPE_NONE:
      // "NONE" types are considered to be all identical.
      return false;
  }
  return false;
}

}  // namespace leak_detector
}  // namespace metrics
