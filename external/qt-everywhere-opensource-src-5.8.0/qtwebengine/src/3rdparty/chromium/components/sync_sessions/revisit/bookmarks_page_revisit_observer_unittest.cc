// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/bookmarks_page_revisit_observer.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sync_sessions {

namespace {

static const GURL kExampleGurl = GURL("http://www.example.com");

class TestBookmarksByUrlProvider : public BookmarksByUrlProvider {
 public:
  TestBookmarksByUrlProvider(
      const std::vector<const bookmarks::BookmarkNode*>& nodes)
      : nodes_(nodes) {}
  void GetNodesByURL(
      const GURL& url,
      std::vector<const bookmarks::BookmarkNode*>* nodes) override {
    *nodes = nodes_;
  }

 private:
  const std::vector<const bookmarks::BookmarkNode*>& nodes_;
};

}  // namespace

void RunObserver(const std::vector<const bookmarks::BookmarkNode*>& nodes) {
  BookmarksPageRevisitObserver observer(
      base::WrapUnique(new TestBookmarksByUrlProvider(nodes)));
  observer.OnPageVisit(kExampleGurl, PageVisitObserver::kTransitionPage);
}

void ExpectMiss(const std::vector<const bookmarks::BookmarkNode*>& nodes) {
  base::HistogramTester histogram_tester;
  RunObserver(nodes);
  histogram_tester.ExpectUniqueSample("Sync.PageRevisitBookmarksMissTransition",
                                      PageVisitObserver::kTransitionPage, 1);
  histogram_tester.ExpectTotalCount("Sync.PageRevisitBookmarksDuration", 1);
}

void ExpectMatch(const std::vector<const bookmarks::BookmarkNode*>& nodes) {
  base::HistogramTester histogram_tester;
  RunObserver(nodes);
  histogram_tester.ExpectTotalCount("Sync.PageRevisitBookmarksMatchAge", 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.PageRevisitBookmarksMatchTransition",
      PageVisitObserver::kTransitionPage, 1);
  histogram_tester.ExpectTotalCount("Sync.PageRevisitBookmarksDuration", 1);
}

TEST(BookmarksPageRevisitObserver, NoMatchingBookmarks) {
  std::vector<const bookmarks::BookmarkNode*> nodes;
  ExpectMiss(nodes);
}

TEST(BookmarksPageRevisitObserver, OneMatchingBookmark) {
  bookmarks::BookmarkNode node(kExampleGurl);
  std::vector<const bookmarks::BookmarkNode*> nodes;
  nodes.push_back(&node);
  ExpectMatch(nodes);
}

TEST(BookmarksPageRevisitObserver, MultipleMatchingBookmarks) {
  bookmarks::BookmarkNode node1(kExampleGurl);
  bookmarks::BookmarkNode node2(kExampleGurl);
  std::vector<const bookmarks::BookmarkNode*> nodes;
  nodes.push_back(&node1);
  nodes.push_back(&node2);
  ExpectMatch(nodes);
}

}  // namespace sync_sessions
