// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RATE_ESTIMATOR_H_
#define CONTENT_BROWSER_DOWNLOAD_RATE_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// RateEstimator generates rate estimates based on recent activity.
//
// Internally it uses a fixed-size ring buffer, and develops estimates
// based on a small sliding window of activity.
class CONTENT_EXPORT RateEstimator {
 public:
  RateEstimator();
  RateEstimator(base::TimeDelta bucket_time,
                size_t num_buckets,
                base::TimeTicks now);
  ~RateEstimator();

  // Increment the counter by |count|. The first variant uses the current time,
  // the second variant provides the time that |count| is observed.
  void Increment(uint32_t count);
  void Increment(uint32_t count, base::TimeTicks now);

  // Get a rate estimate, in terms of counts/second. The first variant uses the
  // current time, the second variant provides the time.
  uint64_t GetCountPerSecond() const;
  uint64_t GetCountPerSecond(base::TimeTicks now) const;

 private:
  void ClearOldBuckets(base::TimeTicks now);
  void ResetBuckets(base::TimeTicks now);

  std::vector<uint32_t> history_;
  base::TimeDelta bucket_time_;
  size_t oldest_index_;
  size_t bucket_count_;
  base::TimeTicks oldest_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RATE_ESTIMATOR_H_
