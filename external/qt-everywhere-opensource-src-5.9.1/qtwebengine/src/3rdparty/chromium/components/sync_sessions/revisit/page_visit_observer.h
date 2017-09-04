// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_PAGE_VISIT_OBSERVER_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_PAGE_VISIT_OBSERVER_H_

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

class GURL;

namespace sync_sessions {

// The upper bound of 90 days and the bucket count of 100 try to strike a
// balance. We want multiple single minute digit buckets, but also notice
// differences that happen many weeks ago. It also helps that typed URLs age out
// around 90 days, which makes these values even more fitting. These might not
// handle bookmarks quite as elegantly, but we're less interested in knowing the
// age of very old objects. This must be defind as a macro instead of a static
// method because the histogram macro makes an inline static variable that must
// be unique for separately named histograms.
#define REVISIT_HISTOGRAM_AGE(name, timestamp)                                \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name,                                           \
                              (base::Time::Now() - timestamp).InMinutes(), 1, \
                              base::TimeDelta::FromDays(90).InMinutes(), 100)

// An interface that allows observers to be notified when a page is visited.
class PageVisitObserver {
 public:
  // This enum represents the most common ways to visit a new page/URL.
  enum TransitionType {
    kTransitionPage = 0,
    kTransitionOmniboxUrl = 1,
    kTransitionOmniboxDefaultSearch = 2,
    kTransitionOmniboxTemplateSearch = 3,
    kTransitionBookmark = 4,
    kTransitionCopyPaste = 5,
    kTransitionForwardBackward = 6,
    kTransitionRestore = 7,
    kTransitionUnknown = 8,
    kTransitionTypeLast = 9,
  };

  virtual ~PageVisitObserver() {}
  virtual void OnPageVisit(const GURL& url,
                           const TransitionType transition) = 0;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_PAGE_VISIT_OBSERVER_H_
