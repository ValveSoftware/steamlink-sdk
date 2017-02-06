// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/bookmarks_page_revisit_observer.h"

#include <algorithm>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

namespace sync_sessions {

BookmarksPageRevisitObserver::BookmarksPageRevisitObserver(
    std::unique_ptr<BookmarksByUrlProvider> provider)
    : provider_(std::move(provider)) {}

BookmarksPageRevisitObserver::~BookmarksPageRevisitObserver() {}

void BookmarksPageRevisitObserver::OnPageVisit(
    const GURL& url,
    const TransitionType transition) {
  base::TimeTicks start(base::TimeTicks::Now());

  std::vector<const bookmarks::BookmarkNode*> nodes;
  provider_->GetNodesByURL(url, &nodes);
  if (nodes.empty()) {
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitBookmarksMissTransition",
                              transition,
                              PageVisitObserver::kTransitionTypeLast);
  } else {
    auto last_added = std::max_element(
        nodes.begin(), nodes.end(),
        [](const bookmarks::BookmarkNode* a, const bookmarks::BookmarkNode* b) {
          return a->date_added() < b->date_added();
        });
    REVISIT_HISTOGRAM_AGE("Sync.PageRevisitBookmarksMatchAge",
                          (*last_added)->date_added());
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitBookmarksMatchTransition",
                              transition,
                              PageVisitObserver::kTransitionTypeLast);
  }

  base::TimeDelta duration(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_TIMES("Sync.PageRevisitBookmarksDuration", duration);
}

}  // namespace sync_sessions
