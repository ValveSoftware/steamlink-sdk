// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_TABLE_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_TABLE_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>  // For std::equal_to.

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/leak_analyzer.h"
#include "components/metrics/leak_detector/ranked_set.h"
#include "components/metrics/leak_detector/stl_allocator.h"

namespace metrics {
namespace leak_detector {

struct CallStack;

// Contains a hash table where the key is the call stack and the value is the
// number of allocations from that call stack.
// Not thread-safe.
class CallStackTable {
 public:
  struct StoredHash {
    size_t operator()(const CallStack* call_stack) const;
  };

  explicit CallStackTable(int call_stack_suspicion_threshold);
  ~CallStackTable();

  // Add/Remove an allocation for the given call stack.
  // Note that this class does NOT own the CallStack objects. Instead, it
  // identifies different CallStacks by their hashes.
  void Add(const CallStack* call_stack);
  void Remove(const CallStack* call_stack);

  // Check for leak patterns in the allocation data.
  void TestForLeaks();

  // Get the top N entries in the CallStackTable, ranked by net number of
  // allocations. N is given by |top_entries->max_size()|, so |*top_entries|
  // must already be initialized with that number.
  void GetTopCallStacks(RankedSet* top_entries) const;

  // Updates information about the last seen drop in the number of allocations
  // and sets the |previous_count| for every stored call stack, both inside
  // |entry_map_|. It should be called in the analysis phase, before the
  // reports are generated, with |timestamp| describing current point in the
  // timeline.
  void UpdateLastDropInfo(size_t timestamp);

  // Retrieves the change in timestamp and allocation count since the last
  // observed drop in the number for allocations for the given call stack
  // (or since the oldest kept record for the call stacks if there were no
  // drops)
  void GetLastUptrendInfo(
      const CallStack* call_stack, size_t timestamp, size_t *timestamp_delta,
      uint32_t *count_delta) const;

  const LeakAnalyzer& leak_analyzer() const { return leak_analyzer_; }

  size_t size() const { return entry_map_.size(); }
  bool empty() const { return entry_map_.empty(); }

  uint32_t num_allocs() const { return num_allocs_; }
  uint32_t num_frees() const { return num_frees_; }

 private:
  struct CallStackCountInfo {
    uint32_t count, previous_count, last_drop_count;
    size_t last_drop_timestamp;
    CallStackCountInfo() : count(0), previous_count(0), last_drop_count(0),
                           last_drop_timestamp(0) {}
  };

  // Total number of allocs and frees in this table.
  uint32_t num_allocs_;
  uint32_t num_frees_;

  // Hash table containing entries. Uses CustomAllocator to avoid recursive
  // malloc hook invocation when analyzing allocs and frees.
  using TableEntryAllocator =
      STLAllocator<std::pair<const CallStack* const, CallStackCountInfo>,
                   CustomAllocator>;

  // Stores a mapping of each call stack to the number of recorded allocations
  // made from that call site.
  base::hash_map<const CallStack*,
                 CallStackCountInfo,
                 StoredHash,
                 std::equal_to<const CallStack*>,
                 TableEntryAllocator> entry_map_;

  // For detecting leak patterns in incoming allocations.
  LeakAnalyzer leak_analyzer_;

  DISALLOW_COPY_AND_ASSIGN(CallStackTable);
};

}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_TABLE_H_
