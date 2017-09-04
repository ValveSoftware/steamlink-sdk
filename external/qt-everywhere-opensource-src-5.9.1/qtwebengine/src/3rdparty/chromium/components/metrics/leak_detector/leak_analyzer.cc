// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_analyzer.h"

#include <set>

namespace metrics {
namespace leak_detector {

namespace {

using RankedEntry = RankedSet::Entry;

// Increase suspicion scores by this much each time an entry is suspected as
// being a leak.
const int kSuspicionScoreIncrease = 1;

}  // namespace

LeakAnalyzer::LeakAnalyzer(uint32_t ranking_size,
                           uint32_t num_suspicions_threshold)
    : ranking_size_(ranking_size),
      score_threshold_(num_suspicions_threshold),
      ranked_entries_(ranking_size),
      prev_ranked_entries_(ranking_size) {
  suspected_leaks_.reserve(ranking_size);
}

LeakAnalyzer::~LeakAnalyzer() {}

void LeakAnalyzer::AddSample(RankedSet ranked_set) {
  // Save the ranked entries from the previous call.
  prev_ranked_entries_ = std::move(ranked_entries_);

  // Save the current entries.
  ranked_entries_ = std::move(ranked_set);

  RankedSet ranked_deltas(ranking_size_);
  for (const RankedEntry& entry : ranked_entries_) {
    // Determine what count was recorded for this value last time.
    uint32_t prev_count = 0;
    if (GetPreviousCountForValue(entry.value, &prev_count))
      ranked_deltas.Add(entry.value, entry.count - prev_count);
  }

  AnalyzeDeltas(ranked_deltas);
}

void LeakAnalyzer::AnalyzeDeltas(const RankedSet& ranked_deltas) {
  bool found_drop = false;
  RankedSet::const_iterator drop_position = ranked_deltas.end();

  if (ranked_deltas.size() > 1) {
    RankedSet::const_iterator entry_iter = ranked_deltas.begin();
    RankedSet::const_iterator next_entry_iter = ranked_deltas.begin();
    ++next_entry_iter;

    // If the first entry is 0, that means all deltas are 0 or negative. Do
    // not treat this as a suspicion of leaks; just quit.
    if (entry_iter->count > 0) {
      while (next_entry_iter != ranked_deltas.end()) {
        const RankedEntry& entry = *entry_iter;
        const RankedEntry& next_entry = *next_entry_iter;

        // Find the first major drop in values (i.e. by 50% or more).
        if (entry.count > next_entry.count * 2) {
          found_drop = true;
          drop_position = next_entry_iter;
          break;
        }
        ++entry_iter;
        ++next_entry_iter;
      }
    }
  }

  // All leak values before the drop are suspected during this analysis.
  std::set<ValueType, std::less<ValueType>, Allocator<ValueType>>
      current_suspects;
  if (found_drop) {
    for (RankedSet::const_iterator ranked_set_iter = ranked_deltas.begin();
         ranked_set_iter != drop_position; ++ranked_set_iter) {
      current_suspects.insert(ranked_set_iter->value);
    }
  }

  // Reset the score to 0 for all previously suspected leak values that did
  // not get suspected this time.
  auto iter = suspected_histogram_.begin();
  while (iter != suspected_histogram_.end()) {
    const ValueType& value = iter->first;
    // Erase entries whose suspicion score reaches 0.
    auto erase_iter = iter++;
    if (current_suspects.find(value) == current_suspects.end())
      suspected_histogram_.erase(erase_iter);
  }

  // For currently suspected values, increase the leak score.
  for (const ValueType& value : current_suspects) {
    auto histogram_iter = suspected_histogram_.find(value);
    if (histogram_iter != suspected_histogram_.end()) {
      histogram_iter->second += kSuspicionScoreIncrease;
    } else if (suspected_histogram_.size() < ranking_size_) {
      // Create a new entry if it didn't already exist.
      suspected_histogram_[value] = kSuspicionScoreIncrease;
    }
  }

  // Now check the leak suspicion scores. Make sure to erase the suspected
  // leaks from the previous call.
  suspected_leaks_.clear();
  for (const auto& entry : suspected_histogram_) {
    if (suspected_leaks_.size() > ranking_size_)
      break;

    // Only report suspected values that have accumulated a suspicion score.
    // This is achieved by maintaining suspicion for several cycles, with few
    // skips.
    if (entry.second >= score_threshold_)
      suspected_leaks_.emplace_back(entry.first);
  }
}

bool LeakAnalyzer::GetPreviousCountForValue(const ValueType& value,
                                            uint32_t* count) const {
  // Determine what count was recorded for this value last time.
  for (const RankedEntry& entry : prev_ranked_entries_) {
    if (entry.value == value) {
      *count = entry.count;
      return true;
    }
  }
  return false;
}

}  // namespace leak_detector
}  // namespace metrics
