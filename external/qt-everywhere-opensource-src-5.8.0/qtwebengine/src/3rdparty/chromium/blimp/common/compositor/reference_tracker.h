// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_COMPOSITOR_REFERENCE_TRACKER_H_
#define BLIMP_COMMON_COMPOSITOR_REFERENCE_TRACKER_H_

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "blimp/common/blimp_common_export.h"

namespace blimp {

// ReferenceTracker provides functionality to count the number of references to
// a given uint32_t, and a way of retrieving the delta in the number of items
// with a positive reference count since last call to CommitRefCounts().
// CommitRefCounts() functions provides the deltas since last call to Commit(),
// as sets of added and removed entries.
// The important thing for any given item is whether it is in a state of having
// positive reference count at the time of the call to CommitRefCounts(), not
// how the reference count changed overtime in-between two calls to
// CommitRefCounts().
class BLIMP_COMMON_EXPORT ReferenceTracker {
 public:
  ReferenceTracker();
  ~ReferenceTracker();

  // Increment the reference count for an |item|.
  void IncrementRefCount(uint32_t item);

  // Decrement the reference count for an |item|. Negative reference counts are
  // not allowed.
  void DecrementRefCount(uint32_t item);

  // Clears the reference count for all items.
  void ClearRefCounts();

  // Calculates the delta of items that still have a positive reference count
  // since last call to CommitRefCounts() and provides calculated delta in the
  // output params |added_items| and |removed_items|. This method changes
  // the internal state of the ReferenceTracker to be able to track what has
  // in fact been committed to make it possible to find a delta.
  void CommitRefCounts(std::vector<uint32_t>* added_items,
                       std::vector<uint32_t>* removed_items);

 private:
  // A reference count for all the given items. The key is the item, and the
  // value is the count for that item.
  std::unordered_map<uint32_t, int> active_ref_counts_;

  // The full set of all committed items.
  std::unordered_set<uint32_t> committed_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceTracker);
};

}  // namespace blimp

#endif  // BLIMP_COMMON_COMPOSITOR_REFERENCE_TRACKER_H_
