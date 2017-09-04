// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_SET_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_SET_H_

#include <stddef.h>

#include <functional>  // for std::less
#include <set>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/leak_detector_value_type.h"
#include "components/metrics/leak_detector/stl_allocator.h"

namespace metrics {
namespace leak_detector {

// RankedSet lets you add entries consisting of a value-count pair, and
// automatically sorts them internally by count in descending order. This allows
// for the user of this container to insert value-count pairs without having to
// explicitly sort them by count.
class RankedSet {
 public:
  using ValueType = LeakDetectorValueType;

  // A single entry in the RankedSet. The RankedSet sorts entries by |count|
  // in descending order.
  struct Entry {
    ValueType value;
    int count;

    // This less-than comparator is used for sorting Entries within a sorted
    // container. It internally reverses the comparison so that higher-count
    // entries are sorted ahead of lower-count entries.
    bool operator<(const Entry& other) const;
  };

  // This class uses CustomAllocator to avoid recursive malloc hook invocation
  // when analyzing allocs and frees.
  using EntrySet =
      std::set<Entry, std::less<Entry>, STLAllocator<Entry, CustomAllocator>>;
  using const_iterator = EntrySet::const_iterator;

  explicit RankedSet(size_t max_size);
  ~RankedSet();

  // For move semantics.
  RankedSet(RankedSet&& other);
  RankedSet& operator=(RankedSet&& other);

  // Accessors for begin() and end() const iterators.
  const_iterator begin() const { return entries_.begin(); }
  const_iterator end() const { return entries_.end(); }

  size_t size() const { return entries_.size(); }
  size_t max_size() const { return max_size_; }

  // Add a new value-count pair to the container. Will overwrite any existing
  // entry with the same value and count. Will not overwrite an existing entry
  // with the same value but a different count, or different values with the
  // same count.
  //
  // Time complexity is O(log n).
  void Add(const ValueType& value, int count);

  // Helper functions to directly add a size or call stack to the RankedSet.
  void AddSize(size_t size, int count) { Add(ValueType(size), count); }
  void AddCallStack(const CallStack* call_stack, int count) {
    Add(ValueType(call_stack), count);
  }

  // Helper functions to directly search for an element with |value| equal to a
  // particular size or call stack. The time complexity is O(n) rather than the
  // O(log n) typical of std::set. These should be called sparingly in
  // performance-critical code.
  const_iterator FindSize(size_t size) const { return Find(ValueType(size)); }
  const_iterator FindCallStack(const CallStack* call_stack) const {
    return Find(ValueType(call_stack));
  }

 private:
  // Returns an iterator to the element in |entries_| with value=|value|, or to
  // entries_.end() if it was not found.
  const_iterator Find(const ValueType& value) const;

  // Max and min counts. Returns 0 if the list is empty.
  int max_count() const {
    return entries_.empty() ? 0 : entries_.begin()->count;
  }
  int min_count() const {
    return entries_.empty() ? 0 : entries_.rbegin()->count;
  }

  // Max number of items that can be stored in the list.
  size_t max_size_;

  // Actual storage container for entries.
  EntrySet entries_;

  DISALLOW_COPY_AND_ASSIGN(RankedSet);
};

}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_SET_H_
