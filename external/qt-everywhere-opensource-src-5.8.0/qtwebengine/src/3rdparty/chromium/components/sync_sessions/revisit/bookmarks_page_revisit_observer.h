// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_BOOKMARKS_PAGE_REVISIT_OBSERVER_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_BOOKMARKS_PAGE_REVISIT_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_sessions {

// A simple interface to abstract away who is providing bookmarks.
class BookmarksByUrlProvider {
 public:
  // Fills the passed vector with all bookmark nodes for the given URL.
  virtual void GetNodesByURL(
      const GURL& url,
      std::vector<const bookmarks::BookmarkNode*>* nodes) = 0;
  virtual ~BookmarksByUrlProvider() {}
};

// Responds to OnPageVisit events by looking for bookmarks with matching URLs,
// and emits metrics about the results. If multiple bookmarks match, the most
// recently created one is used. Currently there isn't enough information to
// determine modificatoin times or remote vs local. This observer does all
// processing in task/thread, which is okay since it only accesses values in
// memory. Potential slow downs could occur when it fails to get bookmarks lock
// and when the number of matching Bookmarks for a single URL is very large.
class BookmarksPageRevisitObserver : public sync_sessions::PageVisitObserver {
 public:
  explicit BookmarksPageRevisitObserver(
      std::unique_ptr<BookmarksByUrlProvider> provider);
  ~BookmarksPageRevisitObserver() override;
  void OnPageVisit(const GURL& url, const TransitionType transition) override;

 private:
  std::unique_ptr<BookmarksByUrlProvider> provider_;
  DISALLOW_COPY_AND_ASSIGN(BookmarksPageRevisitObserver);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_BOOKMARKS_PAGE_REVISIT_OBSERVER_H_
