// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/offset_tab_matcher.h"

#include <stddef.h>

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_types.h"

namespace sync_sessions {

namespace {

// This is an upper bound of the max size of positive offset we will emit
// correct metrics for. Anything larger than this will be clamped to this value.
// This value doesn't exactly correspond to what we actually expect, this value
// is currently larger than expected. This value is more for the safety of our
// sparse histogram usage. It is assumed that the max negative offset is
// symmetrical and can be found by taking the negative of this value.
const int kMaxOffset = 10;

}  // namespace

OffsetTabMatcher::OffsetTabMatcher(const PageEquality& page_equality)
    : page_equality_(page_equality) {}

void OffsetTabMatcher::Check(const sessions::SessionTab* tab) {
  const int current_index = tab->normalized_navigation_index();
  for (std::size_t i = 0; i < tab->navigations.size(); ++i) {
    // Ignore the entry if it is the current entry. There's actually some
    // ambiguity here, the index of a tab is located in two places. Hopefully
    // they are equal, but it is possible for the index() accessor of an entry
    // to be different from the index in the tab's vector. Theoretically this
    // should not happen outside of tab construction logic, but to be safe all
    // matcher logic treats the index in the vector as the authoritative index.
    // We chose this because the other matcher wants efficient random access.
    if (current_index >= 0 && (std::size_t)current_index == i) {
      continue;
    }
    const int offset = i - current_index;
    if (page_equality_.IsSamePage(tab->navigations[i].virtual_url()) &&
        (best_tab_ == nullptr || best_tab_->timestamp < tab->timestamp ||
         (best_tab_->timestamp == tab->timestamp && best_offset_ < offset))) {
      best_tab_ = tab;
      best_offset_ = offset;
    }
  }
}

void OffsetTabMatcher::Emit(
    const PageVisitObserver::TransitionType transition) {
  if (best_tab_ == nullptr) {
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitNavigationMissTransition",
                              transition,
                              PageVisitObserver::kTransitionTypeLast);
  } else {
    // The sparse macro allows us to handle negative offsets. However, we need
    // to be careful when doing this because of the unrestricted nature of
    // sparse we could end up with a very large output space across many
    // clients. So we clamp on a resonable bound that's larger than we expect to
    // be sure no unexpected data causes problems.
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sync.PageRevisitNavigationMatchOffset",
                                Clamp(best_offset_, -kMaxOffset, kMaxOffset));
    REVISIT_HISTOGRAM_AGE("Sync.PageRevisitNavigationMatchAge",
                          best_tab_->timestamp);
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitNavigationMatchTransition",
                              transition,
                              PageVisitObserver::kTransitionTypeLast);
  }
}

int OffsetTabMatcher::Clamp(const int input, const int lower, const int upper) {
  return std::max(lower, std::min(upper, input));
}

}  // namespace sync_sessions
