// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/call_stack_table.h"

#include <algorithm>

#include "components/metrics/leak_detector/call_stack_manager.h"

namespace metrics {
namespace leak_detector {

namespace {

using ValueType = LeakDetectorValueType;

// During leak analysis, we only want to examine the top
// |kMaxCountOfSuspciousStacks| entries.
const int kMaxCountOfSuspciousStacks = 16;

const int kInitialHashTableSize = 1999;

}  // namespace

size_t CallStackTable::StoredHash::operator()(
    const CallStack* call_stack) const {
  // The call stack object should already have a hash computed when it was
  // created.
  //
  // This is NOT the actual hash computation function for a new call stack.
  return call_stack->hash;
}

CallStackTable::CallStackTable(int call_stack_suspicion_threshold)
    : num_allocs_(0),
      num_frees_(0),
      entry_map_(kInitialHashTableSize),
      leak_analyzer_(kMaxCountOfSuspciousStacks,
                     call_stack_suspicion_threshold) {}

CallStackTable::~CallStackTable() {}

void CallStackTable::Add(const CallStack* call_stack) {
  ++entry_map_[call_stack];
  ++num_allocs_;
}

void CallStackTable::Remove(const CallStack* call_stack) {
  auto iter = entry_map_.find(call_stack);
  if (iter == entry_map_.end())
    return;
  uint32_t& count_for_call_stack = iter->second;
  --count_for_call_stack;
  ++num_frees_;

  // Delete zero-alloc entries to free up space.
  if (count_for_call_stack == 0)
    entry_map_.erase(iter);
}

void CallStackTable::TestForLeaks() {
  // Add all entries to the ranked list.
  RankedSet ranked_entries(kMaxCountOfSuspciousStacks);
  GetTopCallStacks(&ranked_entries);
  leak_analyzer_.AddSample(std::move(ranked_entries));
}

void CallStackTable::GetTopCallStacks(RankedSet* top_entries) const {
  for (const auto& call_stack_and_count : entry_map_) {
    top_entries->AddCallStack(call_stack_and_count.first,
                              call_stack_and_count.second);
  }
}

}  // namespace leak_detector
}  // namespace metrics
