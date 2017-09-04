// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/buffered_data_source_host_impl.h"

#include "media/base/timestamp_constants.h"

namespace media {

BufferedDataSourceHostImpl::BufferedDataSourceHostImpl()
    : total_bytes_(0),
      did_loading_progress_(false) { }

BufferedDataSourceHostImpl::~BufferedDataSourceHostImpl() { }

void BufferedDataSourceHostImpl::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
}

void BufferedDataSourceHostImpl::AddBufferedByteRange(int64_t start,
                                                      int64_t end) {
  const auto i = buffered_byte_ranges_.find(start);
  if (i.value() && i.interval_end() >= end) {
    // No change
    return;
  }
  buffered_byte_ranges_.SetInterval(start, end, 1);
  did_loading_progress_ = true;
}

static base::TimeDelta TimeForByteOffset(int64_t byte_offset,
                                         int64_t total_bytes,
                                         base::TimeDelta duration) {
  double position = static_cast<double>(byte_offset) / total_bytes;
  // Snap to the beginning/end where the approximation can look especially bad.
  if (position < 0.01)
    return base::TimeDelta();
  if (position > 0.99)
    return duration;
  return base::TimeDelta::FromMilliseconds(
      static_cast<int64_t>(position * duration.InMilliseconds()));
}

void BufferedDataSourceHostImpl::AddBufferedTimeRanges(
    Ranges<base::TimeDelta>* buffered_time_ranges,
    base::TimeDelta media_duration) const {
  DCHECK(media_duration != kNoTimestamp);
  DCHECK(media_duration != kInfiniteDuration);
  if (total_bytes_ && !buffered_byte_ranges_.empty()) {
    for (const auto i : buffered_byte_ranges_) {
      if (i.second) {
        int64_t start = i.first.begin;
        int64_t end = i.first.end;
        buffered_time_ranges->Add(
            TimeForByteOffset(start, total_bytes_, media_duration),
            TimeForByteOffset(end, total_bytes_, media_duration));
      }
    }
  }
}

bool BufferedDataSourceHostImpl::DidLoadingProgress() {
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

}  // namespace media
