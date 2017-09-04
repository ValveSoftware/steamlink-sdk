// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

using testing::IsEmpty;
using testing::SizeIs;

namespace ntp_snippets {

namespace {

const char kBookmarkLastVisitDateOnMobileKey[] = "last_visited";
const char kBookmarkLastVisitDateOnDesktopKey[] = "last_visited_desktop";

void AddBookmarks(BookmarkModel* model,
                  int num,
                  const std::string& meta_key,
                  const std::string& meta_value) {
  for (int index = 0; index < num; ++index) {
    base::string16 title = base::ASCIIToUTF16(
        base::StringPrintf("title%s%d", meta_key.c_str(), index));
    GURL url(base::StringPrintf("http://url%s%d.com", meta_key.c_str(), index));
    const BookmarkNode* node =
        model->AddURL(model->bookmark_bar_node(), 0, title, url);

    if (!meta_key.empty()) {
      model->SetNodeMetaInfo(node, meta_key, meta_value);
    }
  }
}

void AddBookmarksRecentOnMobile(BookmarkModel* model,
                                int num,
                                const base::Time& threshold_time) {
  base::TimeDelta week = base::TimeDelta::FromDays(7);
  base::Time recent_time = threshold_time + week;
  std::string recent_time_string =
      base::Int64ToString(recent_time.ToInternalValue());

  AddBookmarks(model, num, kBookmarkLastVisitDateOnMobileKey,
               recent_time_string);
}

void AddBookmarksRecentOnDesktop(BookmarkModel* model,
                                 int num,
                                 const base::Time& threshold_time) {
  base::TimeDelta week = base::TimeDelta::FromDays(7);
  base::Time recent_time = threshold_time + week;
  std::string recent_time_string =
      base::Int64ToString(recent_time.ToInternalValue());

  AddBookmarks(model, num, kBookmarkLastVisitDateOnDesktopKey,
               recent_time_string);
}

void AddBookmarksNonRecentOnMobile(BookmarkModel* model,
                                   int num,
                                   const base::Time& threshold_time) {
  base::TimeDelta week = base::TimeDelta::FromDays(7);
  base::Time nonrecent_time = threshold_time - week;
  std::string nonrecent_time_string =
      base::Int64ToString(nonrecent_time.ToInternalValue());

  AddBookmarks(model, num, kBookmarkLastVisitDateOnMobileKey,
               nonrecent_time_string);
}

void AddBookmarksNonVisited(BookmarkModel* model, int num) {
  AddBookmarks(model, num, std::string(), std::string());
}

}  // namespace

class GetRecentlyVisitedBookmarksTest : public testing::Test {
 public:
  GetRecentlyVisitedBookmarksTest() {
    base::TimeDelta week = base::TimeDelta::FromDays(7);
    threshold_time_ = base::Time::UnixEpoch() + 52 * week;
  }

  const base::Time& threshold_time() const { return threshold_time_; }

 private:
  base::Time threshold_time_;

  DISALLOW_COPY_AND_ASSIGN(GetRecentlyVisitedBookmarksTest);
};

TEST_F(GetRecentlyVisitedBookmarksTest,
       WithoutDateFallbackShouldNotReturnMissing) {
  const int number_of_bookmarks = 3;
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  AddBookmarksNonVisited(model.get(), number_of_bookmarks);

  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), 0, number_of_bookmarks,
                                  threshold_time(),
                                  /*creation_date_fallback=*/false,
                                  /*consider_visits_from_desktop=*/false);
  EXPECT_THAT(result, IsEmpty());
}

TEST_F(GetRecentlyVisitedBookmarksTest,
       WithDateFallbackShouldReturnMissingUpToMinCount) {
  const int number_of_bookmarks = 3;
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  AddBookmarksNonVisited(model.get(), number_of_bookmarks);

  const int min_count = number_of_bookmarks - 1;
  const int max_count = min_count + 10;
  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), min_count, max_count,
                                  threshold_time(),
                                  /*creation_date_fallback=*/true,
                                  /*consider_visits_from_desktop=*/false);
  EXPECT_THAT(result, SizeIs(min_count));
}

TEST_F(GetRecentlyVisitedBookmarksTest,
       WithDateFallbackShouldReturnNonRecentUpToMinCount) {
  const int number_of_bookmarks = 3;
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  AddBookmarksNonRecentOnMobile(model.get(), number_of_bookmarks,
                                threshold_time());

  const int min_count = number_of_bookmarks - 1;
  const int max_count = min_count + 10;
  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), min_count, max_count,
                                  threshold_time(),
                                  /*creation_date_fallback=*/true,
                                  /*consider_visits_from_desktop=*/false);
  EXPECT_THAT(result, SizeIs(min_count));
}

TEST_F(GetRecentlyVisitedBookmarksTest, ShouldNotConsiderDesktopVisits) {
  const int number_of_bookmarks = 3;
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  AddBookmarksRecentOnDesktop(model.get(), number_of_bookmarks,
                              threshold_time());

  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), 0, number_of_bookmarks,
                                  threshold_time(),
                                  /*creation_date_fallback=*/false,
                                  /*consider_visits_from_desktop=*/false);
  EXPECT_THAT(result, IsEmpty());
}

TEST_F(GetRecentlyVisitedBookmarksTest, ShouldConsiderDesktopVisits) {
  const int number_of_bookmarks = 3;
  const int number_of_recent_desktop = 2;
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  AddBookmarksRecentOnDesktop(model.get(), number_of_recent_desktop,
                              threshold_time());
  AddBookmarksNonVisited(model.get(),
                         number_of_bookmarks - number_of_recent_desktop);

  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), 0, number_of_bookmarks,
                                  threshold_time(),
                                  /*creation_date_fallback=*/false,
                                  /*consider_visits_from_desktop=*/true);
  EXPECT_THAT(result, SizeIs(number_of_recent_desktop));
}

TEST_F(GetRecentlyVisitedBookmarksTest, ShouldReturnNotMoreThanMaxCount) {
  const int number_of_bookmarks = 3;
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  AddBookmarksRecentOnMobile(model.get(), number_of_bookmarks,
                             threshold_time());

  const int max_count = number_of_bookmarks - 1;
  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), max_count, max_count,
                                  threshold_time(),
                                  /*creation_date_fallback=*/false,
                                  /*consider_visits_from_desktop=*/false);
  EXPECT_THAT(result, SizeIs(max_count));
}

}  // namespace ntp_snippets
