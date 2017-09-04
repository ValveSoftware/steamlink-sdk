// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_OFFSET_TAB_MATCHER_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_OFFSET_TAB_MATCHER_H_

#include "base/macros.h"
#include "components/sync_sessions/revisit/page_equality.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"

namespace sessions {
struct SessionTab;
};  // namespace sessions

namespace sync_sessions {

// This class looks for tabs that have matching pages that are not the current
// navigation entry. This corresponds to the pages you would arrive at if you
// pressed the forward/backwards buttons. The goal is to emit metrics for the
// most recent/current entry. So to break ties of multiple entries that match,
// first the timestamp on the tab is used, followed by the index of the entry.
// This isn't necessarily perfect at determining the most recent, but it should
// be a reasonable enough approximation.
class OffsetTabMatcher {
 public:
  explicit OffsetTabMatcher(const PageEquality& page_equality);
  void Check(const sessions::SessionTab* tab);
  void Emit(const PageVisitObserver::TransitionType transition);

 private:
  int Clamp(int input, int lower, int upper);

  const PageEquality page_equality_;
  const sessions::SessionTab* best_tab_ = nullptr;
  // Invalid while best_tab_ is nullptr.
  int best_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OffsetTabMatcher);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_OFFSET_TAB_MATCHER_H_
