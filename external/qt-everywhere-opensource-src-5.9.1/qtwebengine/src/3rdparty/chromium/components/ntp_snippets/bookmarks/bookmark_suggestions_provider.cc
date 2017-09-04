// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_suggestions_provider.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace ntp_snippets {

namespace {

const int kMaxBookmarks = 10;
const int kMinBookmarks = 3;
const int kMaxBookmarkAgeInDays = 42;
const int kUseCreationDateFallbackForDays = 0;

const char* kMaxBookmarksParamName = "bookmarks_max_count";
const char* kMinBookmarksParamName = "bookmarks_min_count";
const char* kMaxBookmarkAgeInDaysParamName = "bookmarks_max_age_in_days";
const char* kUseCreationDateFallbackForDaysParamName =
    "bookmarks_creation_date_fallback_days";
const char* kConsiderDesktopVisitsParamName =
    "bookmarks_consider_desktop_visits";

// Any bookmark created or visited after this time will be considered recent.
// Note that bookmarks can be shown that do not meet this threshold.
base::Time GetThresholdTime() {
  return base::Time::Now() -
         base::TimeDelta::FromDays(GetParamAsInt(
             ntp_snippets::kBookmarkSuggestionsFeature,
             kMaxBookmarkAgeInDaysParamName, kMaxBookmarkAgeInDays));
}

// The number of days used as a threshold where if this is larger than the time
// since M54 started, then the creation timestamp of a bookmark be used as a
// fallback if no last visited timestamp is present.
int UseCreationDateFallbackForDays() {
  return GetParamAsInt(ntp_snippets::kBookmarkSuggestionsFeature,
                       kUseCreationDateFallbackForDaysParamName,
                       kUseCreationDateFallbackForDays);
}

// The maximum number of suggestions ever provided.
int GetMaxCount() {
  return GetParamAsInt(ntp_snippets::kBookmarkSuggestionsFeature,
                       kMaxBookmarksParamName, kMaxBookmarks);
}

// The minimum number of suggestions to try to provide. Depending on other
// parameters this may or not be respected. Currently creation date fallback
// must be active in order for older bookmarks to be incorporated to meet this
// min.
int GetMinCount() {
  return GetParamAsInt(ntp_snippets::kBookmarkSuggestionsFeature,
                       kMinBookmarksParamName, kMinBookmarks);
}

bool AreDesktopVisitsConsidered() {
  return GetParamAsBool(ntp_snippets::kBookmarkSuggestionsFeature,
                        kConsiderDesktopVisitsParamName, false);
}

}  // namespace

BookmarkSuggestionsProvider::BookmarkSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    bookmarks::BookmarkModel* bookmark_model,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(
          category_factory->FromKnownCategory(KnownCategories::BOOKMARKS)),
      bookmark_model_(bookmark_model),
      fetch_requested_(false),
      end_of_list_last_visit_date_(GetThresholdTime()),
      consider_bookmark_visits_from_desktop_(AreDesktopVisitsConsidered()) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
  base::Time first_m54_start;
  base::Time now = base::Time::Now();
  if (pref_service->HasPrefPath(prefs::kBookmarksFirstM54Start)) {
    first_m54_start = base::Time::FromInternalValue(
        pref_service->GetInt64(prefs::kBookmarksFirstM54Start));
  } else {
    first_m54_start = now;
    pref_service->SetInt64(prefs::kBookmarksFirstM54Start,
                           first_m54_start.ToInternalValue());
  }
  base::TimeDelta time_since_first_m54_start = now - first_m54_start;
  // Note: Setting the fallback timeout to zero effectively turns off the
  // fallback entirely.
  creation_date_fallback_ =
      time_since_first_m54_start.InDays() < UseCreationDateFallbackForDays();
  bookmark_model_->AddObserver(this);
  FetchBookmarks();
}

BookmarkSuggestionsProvider::~BookmarkSuggestionsProvider() {
  bookmark_model_->RemoveObserver(this);
}

// static
void BookmarkSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kBookmarksFirstM54Start, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

CategoryStatus BookmarkSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return category_status_;
}

CategoryInfo BookmarkSuggestionsProvider::GetCategoryInfo(Category category) {
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_BOOKMARK_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::MINIMAL_CARD,
      /*has_more_action=*/false,
      /*has_reload_action=*/false,
      /*has_view_all_action=*/true,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_BOOKMARK_SUGGESTIONS_SECTION_EMPTY));
}

void BookmarkSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK(bookmark_model_->loaded());
  GURL url(suggestion_id.id_within_category());
  MarkBookmarksDismissed(bookmark_model_, url);
}

void BookmarkSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void BookmarkSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  LOG(DFATAL) << "BookmarkSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          callback,
          Status(StatusCode::PERMANENT_ERROR,
                 "BookmarkSuggestionsProvider has no |Fetch| functionality!"),
          base::Passed(std::vector<ContentSuggestion>())));
}

void BookmarkSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // The last visit dates are not "owned" by the bookmark suggestion provider so
  // it is cleared directly from browsing_data_remover.cc.
  ClearDismissedSuggestionsForDebugging(provided_category_);
  FetchBookmarks();
  // Temporarily enter an "explicitly disabled" state, so that any open UIs
  // will clear the suggestions too.
  if (category_status_ != CategoryStatus::CATEGORY_EXPLICITLY_DISABLED) {
    CategoryStatus old_category_status = category_status_;
    NotifyStatusChanged(CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
    NotifyStatusChanged(old_category_status);
  }
}

void BookmarkSuggestionsProvider::ClearCachedSuggestions(Category category) {
  DCHECK_EQ(category, provided_category_);
  // Ignored.
}

void BookmarkSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(category, provided_category_);
  std::vector<const BookmarkNode*> bookmarks =
      GetDismissedBookmarksForDebugging(bookmark_model_);

  std::vector<ContentSuggestion> suggestions;
  for (const BookmarkNode* bookmark : bookmarks) {
    ConvertBookmark(bookmark, &suggestions);
  }
  callback.Run(std::move(suggestions));
}

void BookmarkSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  if (!bookmark_model_->loaded())
    return;
  MarkAllBookmarksUndismissed(bookmark_model_);
}

void BookmarkSuggestionsProvider::BookmarkModelLoaded(
    bookmarks::BookmarkModel* model,
    bool ids_reassigned) {
  DCHECK_EQ(bookmark_model_, model);
  if (fetch_requested_) {
    fetch_requested_ = false;
    FetchBookmarksInternal();
  }
}

void BookmarkSuggestionsProvider::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  // Store the last visit date of the node that is about to change.
  if (!GetLastVisitDateForNTPBookmark(node, creation_date_fallback_,
                                      consider_bookmark_visits_from_desktop_,
                                      &node_to_change_last_visit_date_)) {
    node_to_change_last_visit_date_ = base::Time::UnixEpoch();
  }
}

void BookmarkSuggestionsProvider::BookmarkMetaInfoChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  base::Time time;
  if (!GetLastVisitDateForNTPBookmark(node, creation_date_fallback_,
                                      consider_bookmark_visits_from_desktop_,
                                      &time)) {
    // Error in loading the last visit date after the change. This happens when
    // the bookmark just got dismissed. We must not update the suggestion in
    // such a case.
    return;
  }

  if (time == node_to_change_last_visit_date_ ||
      time < end_of_list_last_visit_date_) {
    // The last visit date has not changed or the change is irrelevant.
    return;
  }

  // Otherwise, we should update the suggestions.
  FetchBookmarks();
}

void BookmarkSuggestionsProvider::BookmarkNodeRemoved(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      int old_index,
      const bookmarks::BookmarkNode* node,
      const std::set<GURL>& no_longer_bookmarked) {
  base::Time time;
  if (GetLastVisitDateForNTPBookmark(node, creation_date_fallback_,
                                     consider_bookmark_visits_from_desktop_,
                                     &time) &&
      time < end_of_list_last_visit_date_) {
    // We know the node is too old to influence the list.
    return;
  }

  // Some node from our list got deleted, we should update the suggestions.
  FetchBookmarks();
}

void BookmarkSuggestionsProvider::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int index) {
  base::Time time;
  if (!GetLastVisitDateForNTPBookmark(
          parent->GetChild(index), creation_date_fallback_,
          consider_bookmark_visits_from_desktop_, &time) ||
      time < end_of_list_last_visit_date_) {
    // The new node has no last visited info or is too old to get into the list.
    return;
  }

  // Some relevant node got created (e.g. by sync), we should update the list.
  FetchBookmarks();
}

void BookmarkSuggestionsProvider::ConvertBookmark(
    const BookmarkNode* bookmark,
    std::vector<ContentSuggestion>* suggestions) {
  base::Time publish_date;
  if (!GetLastVisitDateForNTPBookmark(bookmark, creation_date_fallback_,
                                      consider_bookmark_visits_from_desktop_,
                                      &publish_date)) {
    return;
  }

  ContentSuggestion suggestion(provided_category_, bookmark->url().spec(),
                               bookmark->url());
  suggestion.set_title(bookmark->GetTitle());
  suggestion.set_snippet_text(base::string16());
  suggestion.set_publish_date(publish_date);
  suggestion.set_publisher_name(base::UTF8ToUTF16(bookmark->url().host()));

  suggestions->emplace_back(std::move(suggestion));
}

void BookmarkSuggestionsProvider::FetchBookmarksInternal() {
  DCHECK(bookmark_model_->loaded());

  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  base::Time threshold_time = GetThresholdTime();
  std::vector<const BookmarkNode*> bookmarks = GetRecentlyVisitedBookmarks(
      bookmark_model_, GetMinCount(), GetMaxCount(), threshold_time,
      creation_date_fallback_, consider_bookmark_visits_from_desktop_);

  std::vector<ContentSuggestion> suggestions;
  for (const BookmarkNode* bookmark : bookmarks) {
    ConvertBookmark(bookmark, &suggestions);
  }

  if (suggestions.empty()) {
    end_of_list_last_visit_date_ = threshold_time;
  } else {
    end_of_list_last_visit_date_ = suggestions.back().publish_date();
  }

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

void BookmarkSuggestionsProvider::FetchBookmarks() {
  if (bookmark_model_->loaded())
    FetchBookmarksInternal();
  else
    fetch_requested_ = true;
}

void BookmarkSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  if (category_status_ == new_status)
    return;
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

}  // namespace ntp_snippets
