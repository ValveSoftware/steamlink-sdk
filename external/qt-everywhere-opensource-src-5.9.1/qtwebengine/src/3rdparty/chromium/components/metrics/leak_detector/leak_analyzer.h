// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_ANALYZER_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_ANALYZER_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/leak_detector_value_type.h"
#include "components/metrics/leak_detector/ranked_set.h"
#include "components/metrics/leak_detector/stl_allocator.h"

namespace metrics {
namespace leak_detector {

// This class looks for possible leak patterns in allocation data over time.
// Not thread-safe.
class LeakAnalyzer {
 public:
  using ValueType = LeakDetectorValueType;

  // This class uses CustomAllocator to avoid recursive malloc hook invocation
  // when analyzing allocs and frees.
  template <typename Type>
  using Allocator = STLAllocator<Type, CustomAllocator>;

  LeakAnalyzer(uint32_t ranking_size, uint32_t num_suspicions_threshold);
  ~LeakAnalyzer();

  // Take in a RankedSet of allocations, sorted by count. Removes the contents
  // of |ranked_entries| to be stored internally, which is why it is not passed
  // in as a const reference.
  void AddSample(RankedSet ranked_entries);

  // Used to report suspected leaks. Reported leaks are sorted by ValueType.
  const std::vector<ValueType, Allocator<ValueType>>& suspected_leaks() const {
    return suspected_leaks_;
  }

 private:
  // Analyze a list of allocation count deltas from the previous iteration. If
  // anything looks like a possible leak, update the suspicion scores.
  void AnalyzeDeltas(const RankedSet& ranked_deltas);

  // Returns the count for the given value from the previous analysis in
  // |count|. Returns true if the given value was present in the previous
  // analysis, or false if not.
  bool GetPreviousCountForValue(const ValueType& value, uint32_t* count) const;

  // Look for the top |ranking_size_| entries when analyzing leaks.
  const uint32_t ranking_size_;

  // Report suspected leaks when the suspicion score reaches this value.
  const uint32_t score_threshold_;

  // A mapping of allocation values to suspicion score. All allocations in this
  // container are suspected leaks. The score can increase or decrease over
  // time. Once the score  reaches |score_threshold_|, the entry is reported as
  // a suspected leak in |suspected_leaks_|.
  std::map<ValueType,
           uint32_t,
           std::less<ValueType>,
           Allocator<std::pair<const ValueType, uint32_t>>>
      suspected_histogram_;

  // Array of allocated values that passed the suspicion threshold and are being
  // reported.
  std::vector<ValueType, Allocator<ValueType>> suspected_leaks_;

  // The most recent allocation entries, since the last call to AddSample().
  RankedSet ranked_entries_;
  // The previous allocation entries, from before the last call to AddSample().
  RankedSet prev_ranked_entries_;

  DISALLOW_COPY_AND_ASSIGN(LeakAnalyzer);
};

}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_ANALYZER_H_
