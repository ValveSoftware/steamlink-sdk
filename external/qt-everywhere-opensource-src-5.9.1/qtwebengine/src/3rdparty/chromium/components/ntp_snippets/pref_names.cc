// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/pref_names.h"

namespace ntp_snippets {
namespace prefs {

const char kEnableSnippets[] = "ntp_snippets.enable";

const char kRemoteSuggestionCategories[] = "ntp_snippets.remote_categories";

const char kSnippetBackgroundFetchingIntervalWifi[] =
    "ntp_snippets.fetching_interval_wifi";

const char kSnippetBackgroundFetchingIntervalFallback[] =
    "ntp_snippets.fetching_interval_fallback";

const char kSnippetFetcherRequestCount[] =
    "ntp.request_throttler.suggestion_fetcher.count";
const char kSnippetFetcherInteractiveRequestCount[] =
    "ntp.request_throttler.suggestion_fetcher.interactive_count";
const char kSnippetFetcherRequestsDay[] =
    "ntp.request_throttler.suggestion_fetcher.day";

const char kSnippetThumbnailsRequestCount[] =
    "ntp.request_throttler.suggestion_thumbnails.count";
const char kSnippetThumbnailsInteractiveRequestCount[] =
    "ntp.request_throttler.suggestion_thumbnails.interactive_count";
const char kSnippetThumbnailsRequestsDay[] =
    "ntp.request_throttler.suggestion_thumbnails.day";

const char kDismissedAssetDownloadSuggestions[] =
    "ntp_suggestions.downloads.assets.dismissed_ids";
const char kDismissedRecentOfflineTabSuggestions[] =
    "ntp_suggestions.offline_pages.recent_tabs.dismissed_ids";
const char kDismissedOfflinePageDownloadSuggestions[] =
    "ntp_suggestions.downloads.offline_pages.dismissed_ids";
const char kDismissedForeignSessionsSuggestions[] =
    "ntp_suggestions.foreign_sessions.dismissed_ids";
const char kDismissedCategories[] = "ntp_suggestions.dismissed_categories";

const char kBookmarksFirstM54Start[] =
    "ntp_suggestions.bookmarks.first_M54_start";

const char kUserClassifierAverageNTPOpenedPerHour[] =
    "ntp_suggestions.user_classifier.average_ntp_opened_per_hour";
const char kUserClassifierAverageSuggestionsShownPerHour[] =
    "ntp_suggestions.user_classifier.average_suggestions_shown_per_hour";
const char kUserClassifierAverageSuggestionsUsedPerHour[] =
    "ntp_suggestions.user_classifier.average_suggestions_used_per_hour";

const char kUserClassifierLastTimeToOpenNTP[] =
    "ntp_suggestions.user_classifier.last_time_to_open_ntp";
const char kUserClassifierLastTimeToShowSuggestions[] =
    "ntp_suggestions.user_classifier.last_time_to_show_suggestions";
const char kUserClassifierLastTimeToUseSuggestions[] =
    "ntp_suggestions.user_classifier.last_time_to_use_suggestions";

}  // namespace prefs
}  // namespace ntp_snippets
