// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace ntp_snippets {

namespace {

struct RecentBookmark {
  const bookmarks::BookmarkNode* node;
  base::Time last_visited;
  bool visited_recently;
};

const char* kBookmarksURLBlacklist[] = {"chrome://newtab/",
                                        "chrome-native://newtab/",
                                        "chrome://bookmarks/"};

const char kBookmarkLastVisitDateOnMobileKey[] = "last_visited";
const char kBookmarkLastVisitDateOnDesktopKey[] = "last_visited_desktop";
const char kBookmarkDismissedFromNTP[] = "dismissed_from_ntp";

std::string FormatLastVisitDate(const base::Time& date) {
  return base::Int64ToString(date.ToInternalValue());
}

bool ExtractLastVisitDate(const BookmarkNode& node,
                 const std::string& meta_info_key,
                 base::Time* out) {
  std::string last_visit_date_string;
  if (!node.GetMetaInfo(meta_info_key, &last_visit_date_string))
    return false;

  int64_t date = 0;
  if (!base::StringToInt64(last_visit_date_string, &date) || date < 0)
    return false;

  *out = base::Time::FromInternalValue(date);
  return true;
}

bool IsBlacklisted(const GURL& url) {
  for (const char* blacklisted : kBookmarksURLBlacklist) {
    if (url.spec() == blacklisted)
      return true;
  }
  return false;
}

std::vector<const BookmarkNode*>::const_iterator FindMostRecentBookmark(
    const std::vector<const BookmarkNode*>& bookmarks,
    bool creation_date_fallback,
    bool consider_visits_from_desktop) {
  auto most_recent = bookmarks.end();
  base::Time most_recent_last_visited = base::Time::UnixEpoch();

  for (auto iter = bookmarks.begin(); iter != bookmarks.end(); ++iter) {
    base::Time last_visited;
    if (GetLastVisitDateForNTPBookmark(*iter, creation_date_fallback,
                                       consider_visits_from_desktop,
                                       &last_visited) &&
        most_recent_last_visited <= last_visited) {
      most_recent = iter;
      most_recent_last_visited = last_visited;
    }
  }

  return most_recent;
}

}  // namespace

void UpdateBookmarkOnURLVisitedInMainFrame(BookmarkModel* bookmark_model,
                                           const GURL& url,
                                           bool is_mobile_platform) {
  // Skip URLs that are blacklisted.
  if (IsBlacklisted(url))
    return;

  // Skip URLs that are not bookmarked.
  std::vector<const BookmarkNode*> bookmarks_for_url;
  bookmark_model->GetNodesByURL(url, &bookmarks_for_url);
  if (bookmarks_for_url.empty())
    return;

  // If there are bookmarks for |url|, set their last visit date to now.
  std::string now = FormatLastVisitDate(base::Time::Now());
  for (const BookmarkNode* node : bookmarks_for_url) {
    bookmark_model->SetNodeMetaInfo(
        node, is_mobile_platform ? kBookmarkLastVisitDateOnMobileKey
                                 : kBookmarkLastVisitDateOnDesktopKey,
        now);
    // If the bookmark has been dismissed from NTP before, a new visit overrides
    // such a dismissal.
    bookmark_model->DeleteNodeMetaInfo(node, kBookmarkDismissedFromNTP);
  }
}

bool GetLastVisitDateForNTPBookmark(const BookmarkNode* node,
                                    bool creation_date_fallback,
                                    bool consider_visits_from_desktop,
                                    base::Time* out) {
  if (!node || IsDismissedFromNTPForBookmark(node)) {
    return false;
  }

  bool got_mobile_date =
      ExtractLastVisitDate(*node, kBookmarkLastVisitDateOnMobileKey, out);

  if (consider_visits_from_desktop) {
    // Consider the later visit from these two platform groups.
    base::Time last_visit_desktop;
    if (ExtractLastVisitDate(*node, kBookmarkLastVisitDateOnDesktopKey,
                             &last_visit_desktop)) {
      if (!got_mobile_date) {
        *out = last_visit_desktop;
      } else {
        *out = std::max(*out, last_visit_desktop);
      }
      return true;
    }
  }

  if (!got_mobile_date && creation_date_fallback) {
    *out = node->date_added();
    return true;
  }

  return got_mobile_date;
}

void MarkBookmarksDismissed(BookmarkModel* bookmark_model, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  bookmark_model->GetNodesByURL(url, &nodes);
  for (const BookmarkNode* node : nodes)
    bookmark_model->SetNodeMetaInfo(node, kBookmarkDismissedFromNTP, "1");
}

bool IsDismissedFromNTPForBookmark(const BookmarkNode* node) {
  if (!node)
    return false;

  std::string dismissed_from_ntp;
  bool result =
      node->GetMetaInfo(kBookmarkDismissedFromNTP, &dismissed_from_ntp);
  DCHECK(!result || dismissed_from_ntp == "1");
  return result;
}

void MarkAllBookmarksUndismissed(BookmarkModel* bookmark_model) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmarks;
  bookmark_model->GetBookmarks(&bookmarks);

  // Remove dismissed flag from all bookmarks
  for (const BookmarkModel::URLAndTitle& bookmark : bookmarks) {
    std::vector<const BookmarkNode*> nodes;
    bookmark_model->GetNodesByURL(bookmark.url, &nodes);
    for (const BookmarkNode* node : nodes)
      bookmark_model->DeleteNodeMetaInfo(node, kBookmarkDismissedFromNTP);
  }
}

std::vector<const BookmarkNode*> GetRecentlyVisitedBookmarks(
    BookmarkModel* bookmark_model,
    int min_count,
    int max_count,
    const base::Time& min_visit_time,
    bool creation_date_fallback,
    bool consider_visits_from_desktop) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmark_urls;
  bookmark_model->GetBookmarks(&bookmark_urls);

  std::vector<RecentBookmark> bookmarks;
  int recently_visited_count = 0;
  // Find for each bookmark the most recently visited BookmarkNode and find out
  // whether it is visited since |min_visit_time|.
  for (const BookmarkModel::URLAndTitle& url_and_title : bookmark_urls) {
    // Skip URLs that are blacklisted.
    if (IsBlacklisted(url_and_title.url)) {
      continue;
    }

    // Get all bookmarks for the given URL.
    std::vector<const BookmarkNode*> bookmarks_for_url;
    bookmark_model->GetNodesByURL(url_and_title.url, &bookmarks_for_url);
    DCHECK(!bookmarks_for_url.empty());

    // Find the most recently visited node for the given URL.
    auto most_recent =
        FindMostRecentBookmark(bookmarks_for_url, creation_date_fallback,
                               consider_visits_from_desktop);
    if (most_recent == bookmarks_for_url.end()) {
      continue;
    }

    // Extract the last visit of the node to use later for sorting.
    base::Time last_visit;
    if (!GetLastVisitDateForNTPBookmark(*most_recent, creation_date_fallback,
                                        consider_visits_from_desktop,
                                        &last_visit)) {
      continue;
    }

    // Has it been _visited_ recently enough (not considering creation date)?
    base::Time last_real_visit;
    if (GetLastVisitDateForNTPBookmark(
            *most_recent, /*creation_date_fallback=*/false,
            consider_visits_from_desktop, &last_real_visit) &&
        min_visit_time < last_real_visit) {
      recently_visited_count++;
      bookmarks.push_back(
          {*most_recent, last_visit, /*visited_recently=*/true});
    } else {
      bookmarks.push_back(
          {*most_recent, last_visit, /*visited_recently=*/false});
    }
  }

  if (recently_visited_count < min_count) {
    // There aren't enough recently-visited bookmarks. Fill the list up to
    // |min_count| with older bookmarks (in particular those with only a
    // creation date, if creation_date_fallback is true).
    max_count = min_count;
  } else {
    // Remove the bookmarks that are not recently visited; we do not need them.
    // (We might end up with fewer than |min_count| bookmarks if all the recent
    // ones are dismissed.)
    bookmarks.erase(
        std::remove_if(bookmarks.begin(), bookmarks.end(),
                       [](const RecentBookmark& bookmark) {
                         return !bookmark.visited_recently;
                       }),
        bookmarks.end());
  }

  // Sort the remaining entries by date.
  std::sort(bookmarks.begin(), bookmarks.end(),
            [creation_date_fallback, consider_visits_from_desktop](
                const RecentBookmark& a, const RecentBookmark& b) {
              return a.last_visited > b.last_visited;
            });

  // Insert the first |max_count| items from |bookmarks| into |result|.
  std::vector<const BookmarkNode*> result;
  for (const RecentBookmark& bookmark : bookmarks) {
    // TODO(jkrcal): The following break rule is in conflict with the comment
    // and code above that we give at least |min_count| bookmarks (irrespective
    // of creation_date_fallback). Discuss with treib@ and clear up.
    if (!creation_date_fallback && bookmark.last_visited < min_visit_time) {
      break;
    }

    result.push_back(bookmark.node);
    if (result.size() >= static_cast<size_t>(max_count))
      break;
  }
  return result;
}

std::vector<const BookmarkNode*> GetDismissedBookmarksForDebugging(
    BookmarkModel* bookmark_model) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmarks;
  bookmark_model->GetBookmarks(&bookmarks);

  // Remove the bookmark URLs which have at least one non-dismissed bookmark.
  bookmarks.erase(
      std::remove_if(
          bookmarks.begin(), bookmarks.end(),
          [&bookmark_model](const BookmarkModel::URLAndTitle& bookmark) {
            std::vector<const BookmarkNode*> bookmarks_for_url;
            bookmark_model->GetNodesByURL(bookmark.url, &bookmarks_for_url);
            DCHECK(!bookmarks_for_url.empty());

            for (const BookmarkNode* node : bookmarks_for_url) {
              if (!IsDismissedFromNTPForBookmark(node))
                return true;
            }
            return false;
          }),
      bookmarks.end());

  // Insert into |result|.
  std::vector<const BookmarkNode*> result;
  for (const BookmarkModel::URLAndTitle& bookmark : bookmarks) {
    result.push_back(
        bookmark_model->GetMostRecentlyAddedUserNodeForURL(bookmark.url));
  }
  return result;
}

void RemoveAllLastVisitDates(bookmarks::BookmarkModel* bookmark_model) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmark_urls;
  bookmark_model->GetBookmarks(&bookmark_urls);

  for (const BookmarkModel::URLAndTitle& url_and_title : bookmark_urls) {
    // Get all bookmarks for the given URL.
    std::vector<const BookmarkNode*> bookmarks_for_url;
    bookmark_model->GetNodesByURL(url_and_title.url, &bookmarks_for_url);

    for (const BookmarkNode* bookmark : bookmarks_for_url) {
      bookmark_model->DeleteNodeMetaInfo(bookmark,
                                         kBookmarkLastVisitDateOnMobileKey);
      bookmark_model->DeleteNodeMetaInfo(bookmark,
                                         kBookmarkLastVisitDateOnDesktopKey);
    }
  }
}

}  // namespace ntp_snippets
