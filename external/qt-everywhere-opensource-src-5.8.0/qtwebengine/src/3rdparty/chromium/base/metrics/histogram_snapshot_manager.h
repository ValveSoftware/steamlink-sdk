// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_
#define BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"

namespace base {

class HistogramSamples;
class HistogramFlattener;

// HistogramSnapshotManager handles the logistics of gathering up available
// histograms for recording either to disk or for transmission (such as from
// renderer to browser, or from browser to UMA upload). Since histograms can sit
// in memory for an extended period of time, and are vulnerable to memory
// corruption, this class also validates as much rendundancy as it can before
// calling for the marginal change (a.k.a., delta) in a histogram to be
// recorded.
class BASE_EXPORT HistogramSnapshotManager {
 public:
  explicit HistogramSnapshotManager(HistogramFlattener* histogram_flattener);
  virtual ~HistogramSnapshotManager();

  // Snapshot all histograms, and ask |histogram_flattener_| to record the
  // delta. |flags_to_set| is used to set flags for each histogram.
  // |required_flags| is used to select histograms to be recorded.
  // Only histograms that have all the flags specified by the argument will be
  // chosen. If all histograms should be recorded, set it to
  // |Histogram::kNoFlags|. Though any "forward" iterator will work, the
  // histograms over which it iterates *must* remain valid until this method
  // returns; the iterator cannot deallocate histograms once it iterates past
  // them and FinishDeltas() has been called after.  StartDeltas() must be
  // called before.
  template <class ForwardHistogramIterator>
  void PrepareDeltasWithoutStartFinish(ForwardHistogramIterator begin,
                                       ForwardHistogramIterator end,
                                       HistogramBase::Flags flags_to_set,
                                       HistogramBase::Flags required_flags) {
    for (ForwardHistogramIterator it = begin; it != end; ++it) {
      (*it)->SetFlags(flags_to_set);
      if (((*it)->flags() & required_flags) == required_flags)
        PrepareDelta(*it);
    }
  }

  // As above but also calls StartDeltas() and FinishDeltas().
  template <class ForwardHistogramIterator>
  void PrepareDeltas(ForwardHistogramIterator begin,
                     ForwardHistogramIterator end,
                     HistogramBase::Flags flags_to_set,
                     HistogramBase::Flags required_flags) {
    StartDeltas();
    PrepareDeltasWithoutStartFinish(begin, end, flags_to_set, required_flags);
    FinishDeltas();
  }

  // When the collection is not so simple as can be done using a single
  // iterator, the steps can be performed separately. Call PerpareDelta()
  // as many times as necessary with a single StartDeltas() before and
  // a single FinishDeltas() after. All passed histograms must live
  // until FinishDeltas() completes. PrepareAbsolute() works the same
  // but assumes there were no previous logged values and no future deltas
  // will be created (and thus can work on read-only histograms).
  // PrepareFinalDelta() works like PrepareDelta() except that it does
  // not update the previous logged values and can thus be used with
  // read-only files.
  // Use Prepare*TakingOwnership() if it is desireable to have this class
  // automatically delete the histogram once it is "finished".
  void StartDeltas();
  void PrepareDelta(HistogramBase* histogram);
  void PrepareDeltaTakingOwnership(std::unique_ptr<HistogramBase> histogram);
  void PrepareAbsolute(const HistogramBase* histogram);
  void PrepareAbsoluteTakingOwnership(
      std::unique_ptr<const HistogramBase> histogram);
  void PrepareFinalDeltaTakingOwnership(
      std::unique_ptr<const HistogramBase> histogram);
  void FinishDeltas();

 private:
  FRIEND_TEST_ALL_PREFIXES(HistogramSnapshotManagerTest, CheckMerge);

  // During a snapshot, samples are acquired and aggregated. This structure
  // contains all the information collected for a given histogram. Once a
  // snapshot operation is finished, it is generally emptied except for
  // information that must persist from one report to the next, such as
  // the "inconsistencies".
  struct SampleInfo {
    // A histogram associated with this sample; it may be one of many if
    // several have been aggregated into the same "accumulated" sample set.
    // Ownership of the histogram remains elsewhere and this pointer is
    // cleared by FinishDeltas().
    const HistogramBase* histogram = nullptr;

    // The current snapshot-delta values being accumulated.
    // TODO(bcwhite): Change this to a scoped_ptr once all build architectures
    // support such as the value of a std::map.
    HistogramSamples* accumulated_samples = nullptr;

    // The set of inconsistencies (flags) already seen for the histogram.
    // See HistogramBase::Inconsistency for values.
    uint32_t inconsistencies = 0;
  };

  // Capture and hold samples from a histogram. This does all the heavy
  // lifting for PrepareDelta() and PrepareAbsolute().
  void PrepareSamples(const HistogramBase* histogram,
                      std::unique_ptr<HistogramSamples> samples);

  // Try to detect and fix count inconsistency of logged samples.
  void InspectLoggedSamplesInconsistency(
      const HistogramSamples& new_snapshot,
      HistogramSamples* logged_samples);

  // For histograms, track what has been previously seen, indexed
  // by the hash of the histogram name.
  std::map<uint64_t, SampleInfo> known_histograms_;

  // Collection of histograms of which ownership has been passed to this
  // object. They will be deleted by FinishDeltas().
  std::vector<std::unique_ptr<const HistogramBase>> owned_histograms_;

  // Indicates if deltas are currently being prepared.
  bool preparing_deltas_;

  // |histogram_flattener_| handles the logistics of recording the histogram
  // deltas.
  HistogramFlattener* histogram_flattener_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(HistogramSnapshotManager);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_
