// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/inter_process_time_ticks_converter.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace content {

InterProcessTimeTicksConverter::InterProcessTimeTicksConverter(
    const LocalTimeTicks& local_lower_bound,
    const LocalTimeTicks& local_upper_bound,
    const RemoteTimeTicks& remote_lower_bound,
    const RemoteTimeTicks& remote_upper_bound)
    : remote_lower_bound_(remote_lower_bound.value_),
      remote_upper_bound_(remote_upper_bound.value_) {
  int64 target_range = local_upper_bound.value_ - local_lower_bound.value_;
  int64 source_range = remote_upper_bound.value_ - remote_lower_bound.value_;
  DCHECK_GE(target_range, 0);
  DCHECK_GE(source_range, 0);
  if (source_range <= target_range) {
    // We fit!  Center the source range on the target range.
    numerator_ = 1;
    denominator_ = 1;
    local_base_time_ =
        local_lower_bound.value_ + (target_range - source_range) / 2;
    // When converting times, remote bounds should fall within local bounds.
    DCHECK_LE(local_lower_bound.value_,
              ToLocalTimeTicks(remote_lower_bound).value_);
    DCHECK_GE(local_upper_bound.value_,
              ToLocalTimeTicks(remote_upper_bound).value_);
    return;
  }

  // Interpolate values so that remote range will be will exactly fit into the
  // local range, if possible.
  numerator_ = target_range;
  denominator_ = source_range;
  local_base_time_ = local_lower_bound.value_;
  // When converting times, remote bounds should equal local bounds.
  DCHECK_EQ(local_lower_bound.value_,
            ToLocalTimeTicks(remote_lower_bound).value_);
  DCHECK_EQ(local_upper_bound.value_,
            ToLocalTimeTicks(remote_upper_bound).value_);
  DCHECK_EQ(target_range, Convert(source_range));
}

LocalTimeTicks InterProcessTimeTicksConverter::ToLocalTimeTicks(
    const RemoteTimeTicks& remote_ms) const {
  // If input time is "null", return another "null" time.
  if (remote_ms.value_ == 0)
    return LocalTimeTicks(0);
  DCHECK_LE(remote_lower_bound_, remote_ms.value_);
  DCHECK_GE(remote_upper_bound_, remote_ms.value_);
  RemoteTimeDelta remote_delta = remote_ms - remote_lower_bound_;
  return LocalTimeTicks(local_base_time_ +
                        ToLocalTimeDelta(remote_delta).value_);
}

LocalTimeDelta InterProcessTimeTicksConverter::ToLocalTimeDelta(
    const RemoteTimeDelta& remote_delta) const {
  DCHECK_GE(remote_upper_bound_, remote_lower_bound_ + remote_delta.value_);
  return LocalTimeDelta(Convert(remote_delta.value_));
}

int64 InterProcessTimeTicksConverter::Convert(int64 value) const {
  if (value <= 0) {
    return value;
  }
  return numerator_ * value / denominator_;
}

}  // namespace content
