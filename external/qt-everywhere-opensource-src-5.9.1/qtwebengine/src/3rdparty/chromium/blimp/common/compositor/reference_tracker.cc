// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/compositor/reference_tracker.h"

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/logging.h"

namespace blimp {

ReferenceTracker::ReferenceTracker() {}

ReferenceTracker::~ReferenceTracker() {}

void ReferenceTracker::IncrementRefCount(uint32_t item) {
  ++active_ref_counts_[item];
}

void ReferenceTracker::DecrementRefCount(uint32_t item) {
  DCHECK_GT(active_ref_counts_[item], 0);
  --active_ref_counts_[item];
}

void ReferenceTracker::ClearRefCounts() {
  for (auto it = active_ref_counts_.begin(); it != active_ref_counts_.end();
       ++it) {
    it->second = 0;
  }
}

void ReferenceTracker::CommitRefCounts(std::vector<uint32_t>* added_entries,
                                       std::vector<uint32_t>* removed_entries) {
  DCHECK(added_entries);
  DCHECK(removed_entries);
  for (auto it = active_ref_counts_.begin(); it != active_ref_counts_.end();) {
    uint32_t key = it->first;
    uint32_t ref_count = it->second;
    bool is_committed = committed_.count(key) > 0u;
    if (ref_count > 0u) {
      if (!is_committed) {
        // The entry is new and has a positive reference count, so needs commit.
        committed_.insert(key);
        added_entries->push_back(key);
      }
      ++it;
    } else {
      if (is_committed) {
        // The entry has already been committed, but is not reference anymore.
        committed_.erase(key);
        removed_entries->push_back(key);
      }
      // The entry has no references, so should not be staged anymore.
      it = active_ref_counts_.erase(it);
    }
  }
}

}  // namespace blimp
