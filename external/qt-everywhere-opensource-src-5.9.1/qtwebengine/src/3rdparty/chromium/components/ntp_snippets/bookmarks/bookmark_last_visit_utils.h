// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BOOKMARKS_BOOKMARK_LAST_VISIT_UTILS_H_
#define COMPONENTS_NTP_SNIPPETS_BOOKMARKS_BOOKMARK_LAST_VISIT_UTILS_H_

#include <vector>

class GURL;

namespace base {
class Time;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ntp_snippets {

// If there is a bookmark for |url|, this function updates its last visit date
// to now. If there are multiple bookmarks for a given URL, it updates all of
// them. The last visit dates are kept separate for mobile and desktop,
// according to |is_mobile_platform|.
void UpdateBookmarkOnURLVisitedInMainFrame(
    bookmarks::BookmarkModel* bookmark_model,
    const GURL& url,
    bool is_mobile_platform);

// Reads the last visit date for a given bookmark |node|. On success, |out| is
// set to the extracted time and 'true' is returned. Otherwise returns false
// which also includes the case that the bookmark was dismissed from the NTP.
// As visits, we primarily understand visits on Android (the visit when the
// bookmark is created also counts). Visits on desktop platforms are considered
// only if |consider_visits_from_desktop|. If no info about last visit date is
// present and |creation_date_fallback| is true, the date when the bookmarks is
// created is used as the alst visit date (most likely, it is the date when the
// bookmark was synced to this device).
bool GetLastVisitDateForNTPBookmark(const bookmarks::BookmarkNode* node,
                                    bool creation_date_fallback,
                                    bool consider_visits_from_desktop,
                                    base::Time* out);

// Marks all bookmarks with the given URL as dismissed.
void MarkBookmarksDismissed(bookmarks::BookmarkModel* bookmark_model,
                            const GURL& url);

// Gets the dismissed flag for a given bookmark |node|. Defaults to false.
bool IsDismissedFromNTPForBookmark(const bookmarks::BookmarkNode* node);

// Removes the dismissed flag from all bookmarks (only for debugging).
void MarkAllBookmarksUndismissed(bookmarks::BookmarkModel* bookmark_model);

// Returns the list of most recently visited, non-dismissed bookmarks.
// For each bookmarked URL, it returns the most recently created bookmark.
// The result is ordered by visit time (the most recent first). Only bookmarks
// visited after |min_visit_time| are considered, at most |max_count| bookmarks
// are returned. If this results into less than |min_count| bookmarks, the list
// is filled up with older bookmarks sorted by their last visit / creation date.
// If |consider_visits_from_desktop|, also visits to bookmarks on synced desktop
// platforms are considered (and not only on this and other synced Android
// devices).
std::vector<const bookmarks::BookmarkNode*> GetRecentlyVisitedBookmarks(
    bookmarks::BookmarkModel* bookmark_model,
    int min_count,
    int max_count,
    const base::Time& min_visit_time,
    bool creation_date_fallback,
    bool consider_visits_from_desktop);

// Returns the list of all dismissed bookmarks. Only used for debugging.
std::vector<const bookmarks::BookmarkNode*> GetDismissedBookmarksForDebugging(
    bookmarks::BookmarkModel* bookmark_model);

// Removes last visited date metadata for all bookmarks.
void RemoveAllLastVisitDates(bookmarks::BookmarkModel* bookmark_model);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_BOOKMARKS_BOOKMARK_LAST_VISIT_UTILS_H_
