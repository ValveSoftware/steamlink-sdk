// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_service.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

ContentSuggestionsService::ContentSuggestionsService(
    State state,
    history::HistoryService* history_service,
    PrefService* pref_service)
    : state_(state),
      history_service_observer_(this),
      ntp_snippets_service_(nullptr),
      pref_service_(pref_service),
      user_classifier_(pref_service) {
  // Can be null in tests.
  if (history_service)
    history_service_observer_.Add(history_service);

  RestoreDismissedCategoriesFromPrefs();
}

ContentSuggestionsService::~ContentSuggestionsService() = default;

void ContentSuggestionsService::Shutdown() {
  ntp_snippets_service_ = nullptr;
  suggestions_by_category_.clear();
  providers_by_category_.clear();
  categories_.clear();
  providers_.clear();
  state_ = State::DISABLED;
  for (Observer& observer : observers_)
    observer.ContentSuggestionsServiceShutdown();
}

// static
void ContentSuggestionsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDismissedCategories);
}

CategoryStatus ContentSuggestionsService::GetCategoryStatus(
    Category category) const {
  if (state_ == State::DISABLED) {
    return CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED;
  }

  auto iterator = providers_by_category_.find(category);
  if (iterator == providers_by_category_.end())
    return CategoryStatus::NOT_PROVIDED;

  return iterator->second->GetCategoryStatus(category);
}

base::Optional<CategoryInfo> ContentSuggestionsService::GetCategoryInfo(
    Category category) const {
  auto iterator = providers_by_category_.find(category);
  if (iterator == providers_by_category_.end())
    return base::Optional<CategoryInfo>();
  return iterator->second->GetCategoryInfo(category);
}

const std::vector<ContentSuggestion>&
ContentSuggestionsService::GetSuggestionsForCategory(Category category) const {
  auto iterator = suggestions_by_category_.find(category);
  if (iterator == suggestions_by_category_.end())
    return no_suggestions_;
  return iterator->second;
}

void ContentSuggestionsService::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  if (!providers_by_category_.count(suggestion_id.category())) {
    LOG(WARNING) << "Requested image for suggestion " << suggestion_id
                 << " for unavailable category " << suggestion_id.category();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, gfx::Image()));
    return;
  }
  providers_by_category_[suggestion_id.category()]->FetchSuggestionImage(
      suggestion_id, callback);
}

void ContentSuggestionsService::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  for (const auto& provider : providers_) {
    provider->ClearHistory(begin, end, filter);
  }
}

void ContentSuggestionsService::ClearAllCachedSuggestions() {
  suggestions_by_category_.clear();
  for (const auto& category_provider_pair : providers_by_category_) {
    category_provider_pair.second->ClearCachedSuggestions(
        category_provider_pair.first);
    for (Observer& observer : observers_)
      observer.OnNewSuggestions(category_provider_pair.first);
  }
}

void ContentSuggestionsService::ClearCachedSuggestions(Category category) {
  suggestions_by_category_[category].clear();
  auto iterator = providers_by_category_.find(category);
  if (iterator != providers_by_category_.end())
    iterator->second->ClearCachedSuggestions(category);
}

void ContentSuggestionsService::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  auto iterator = providers_by_category_.find(category);
  if (iterator != providers_by_category_.end())
    iterator->second->GetDismissedSuggestionsForDebugging(category, callback);
  else
    callback.Run(std::vector<ContentSuggestion>());
}

void ContentSuggestionsService::ClearDismissedSuggestionsForDebugging(
    Category category) {
  auto iterator = providers_by_category_.find(category);
  if (iterator != providers_by_category_.end())
    iterator->second->ClearDismissedSuggestionsForDebugging(category);
}

void ContentSuggestionsService::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  if (!providers_by_category_.count(suggestion_id.category())) {
    LOG(WARNING) << "Dismissed suggestion " << suggestion_id
                 << " for unavailable category " << suggestion_id.category();
    return;
  }
  providers_by_category_[suggestion_id.category()]->DismissSuggestion(
      suggestion_id);

  // Remove the suggestion locally.
  bool removed = RemoveSuggestionByID(suggestion_id);
  DCHECK(removed) << "The dismissed suggestion " << suggestion_id
                  << " has already been removed. Providers must not call"
                  << " OnNewSuggestions in response to DismissSuggestion.";
}

void ContentSuggestionsService::DismissCategory(Category category) {
  auto providers_it = providers_by_category_.find(category);
  if (providers_it == providers_by_category_.end())
    return;

  ContentSuggestionsProvider* provider = providers_it->second;
  UnregisterCategory(category, provider);

  dismissed_providers_by_category_[category] = provider;
  StoreDismissedCategoriesToPrefs();
}

void ContentSuggestionsService::RestoreDismissedCategories() {
  // Make a copy as the original will be modified during iteration.
  auto dismissed_providers_by_category_copy = dismissed_providers_by_category_;
  for (const auto& category_provider_pair :
       dismissed_providers_by_category_copy) {
    RestoreDismissedCategory(category_provider_pair.first);
  }
  StoreDismissedCategoriesToPrefs();
  DCHECK(dismissed_providers_by_category_.empty());
}

void ContentSuggestionsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContentSuggestionsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContentSuggestionsService::RegisterProvider(
    std::unique_ptr<ContentSuggestionsProvider> provider) {
  DCHECK(state_ == State::ENABLED);
  providers_.push_back(std::move(provider));
}

void ContentSuggestionsService::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  auto providers_it = providers_by_category_.find(category);
  if (providers_it == providers_by_category_.end())
    return;

  providers_it->second->Fetch(category, known_suggestion_ids, callback);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void ContentSuggestionsService::OnNewSuggestions(
    ContentSuggestionsProvider* provider,
    Category category,
    std::vector<ContentSuggestion> suggestions) {
  // Providers shouldn't call this when they're in a non-available state.
  DCHECK(
      IsCategoryStatusInitOrAvailable(provider->GetCategoryStatus(category)));

  if (TryRegisterProviderForCategory(provider, category)) {
    NotifyCategoryStatusChanged(category);
  } else if (IsCategoryDismissed(category)) {
    // The category has been registered as a dismissed one. We need to
    // check if the dismissal can be cleared now that we received new data.
    if (suggestions.empty())
      return;

    RestoreDismissedCategory(category);
    StoreDismissedCategoriesToPrefs();

    NotifyCategoryStatusChanged(category);
  }

  if (!IsCategoryStatusAvailable(provider->GetCategoryStatus(category))) {
    // A provider shouldn't send us suggestions while it's not available.
    DCHECK(suggestions.empty());
    return;
  }

  suggestions_by_category_[category] = std::move(suggestions);

  for (Observer& observer : observers_)
    observer.OnNewSuggestions(category);
}

void ContentSuggestionsService::OnCategoryStatusChanged(
    ContentSuggestionsProvider* provider,
    Category category,
    CategoryStatus new_status) {
  if (new_status == CategoryStatus::NOT_PROVIDED) {
    UnregisterCategory(category, provider);
  } else {
    if (!IsCategoryStatusAvailable(new_status))
      suggestions_by_category_.erase(category);
    TryRegisterProviderForCategory(provider, category);
    DCHECK_EQ(new_status, provider->GetCategoryStatus(category));
  }

  if (!IsCategoryDismissed(category))
    NotifyCategoryStatusChanged(category);
}

void ContentSuggestionsService::OnSuggestionInvalidated(
    ContentSuggestionsProvider* provider,
    const ContentSuggestion::ID& suggestion_id) {
  RemoveSuggestionByID(suggestion_id);
  for (Observer& observer : observers_)
    observer.OnSuggestionInvalidated(suggestion_id);
}

// history::HistoryServiceObserver implementation.
void ContentSuggestionsService::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  // We don't care about expired entries.
  if (expired)
    return;

  // Redirect to ClearHistory().
  if (all_history) {
    base::Time begin = base::Time();
    base::Time end = base::Time::Max();
    base::Callback<bool(const GURL& url)> filter =
        base::Bind([](const GURL& url) { return true; });
    ClearHistory(begin, end, filter);
  } else {
    if (deleted_rows.empty())
      return;

    base::Time begin = deleted_rows[0].last_visit();
    base::Time end = deleted_rows[0].last_visit();
    std::set<GURL> deleted_urls;
    for (const history::URLRow& row : deleted_rows) {
      if (row.last_visit() < begin)
        begin = row.last_visit();
      if (row.last_visit() > end)
        end = row.last_visit();
      deleted_urls.insert(row.url());
    }
    base::Callback<bool(const GURL& url)> filter = base::Bind(
        [](const std::set<GURL>& set, const GURL& url) {
          return set.count(url) != 0;
        },
        deleted_urls);
    ClearHistory(begin, end, filter);
  }
}

void ContentSuggestionsService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
}

bool ContentSuggestionsService::TryRegisterProviderForCategory(
    ContentSuggestionsProvider* provider,
    Category category) {
  auto it = providers_by_category_.find(category);
  if (it != providers_by_category_.end()) {
    DCHECK_EQ(it->second, provider);
    return false;
  }

  auto dismissed_it = dismissed_providers_by_category_.find(category);
  if (dismissed_it != dismissed_providers_by_category_.end()) {
    // The initialisation of dismissed categories registers them with |nullptr|
    // for providers, we need to check for that to see if the provider is
    // already registered or not.
    if (!dismissed_it->second) {
      dismissed_it->second = provider;
    } else {
      DCHECK_EQ(dismissed_it->second, provider);
    }
    return false;
  }

  RegisterCategory(category, provider);
  return true;
}

void ContentSuggestionsService::RegisterCategory(
    Category category,
    ContentSuggestionsProvider* provider) {
  DCHECK(!base::ContainsKey(providers_by_category_, category));
  DCHECK(!IsCategoryDismissed(category));

  providers_by_category_[category] = provider;
  categories_.push_back(category);
  SortCategories();
  if (IsCategoryStatusAvailable(provider->GetCategoryStatus(category))) {
    suggestions_by_category_.insert(
        std::make_pair(category, std::vector<ContentSuggestion>()));
  }
}

void ContentSuggestionsService::UnregisterCategory(
    Category category,
    ContentSuggestionsProvider* provider) {
  auto providers_it = providers_by_category_.find(category);
  if (providers_it == providers_by_category_.end()) {
    DCHECK(IsCategoryDismissed(category));
    return;
  }

  DCHECK_EQ(provider, providers_it->second);
  providers_by_category_.erase(providers_it);
  categories_.erase(
      std::find(categories_.begin(), categories_.end(), category));
  suggestions_by_category_.erase(category);
}

bool ContentSuggestionsService::RemoveSuggestionByID(
    const ContentSuggestion::ID& suggestion_id) {
  std::vector<ContentSuggestion>* suggestions =
      &suggestions_by_category_[suggestion_id.category()];
  auto position =
      std::find_if(suggestions->begin(), suggestions->end(),
                   [&suggestion_id](const ContentSuggestion& suggestion) {
                     return suggestion_id == suggestion.id();
                   });
  if (position == suggestions->end())
    return false;
  suggestions->erase(position);

  return true;
}

void ContentSuggestionsService::NotifyCategoryStatusChanged(Category category) {
  for (Observer& observer : observers_)
    observer.OnCategoryStatusChanged(category, GetCategoryStatus(category));
}

void ContentSuggestionsService::SortCategories() {
  std::sort(categories_.begin(), categories_.end(),
            [this](const Category& left, const Category& right) {
              return category_factory_.CompareCategories(left, right);
            });
}

bool ContentSuggestionsService::IsCategoryDismissed(Category category) const {
  return base::ContainsKey(dismissed_providers_by_category_, category);
}

void ContentSuggestionsService::RestoreDismissedCategory(Category category) {
  auto dismissed_it = dismissed_providers_by_category_.find(category);
  DCHECK(base::ContainsKey(dismissed_providers_by_category_, category));

  // Keep the reference to the provider and remove it from the dismissed ones,
  // because the category registration enforces that it's not dismissed.
  ContentSuggestionsProvider* provider = dismissed_it->second;
  dismissed_providers_by_category_.erase(dismissed_it);

  if (provider)
    RegisterCategory(category, provider);
}

void ContentSuggestionsService::RestoreDismissedCategoriesFromPrefs() {
  // This must only be called at startup.
  DCHECK(dismissed_providers_by_category_.empty());
  DCHECK(providers_by_category_.empty());

  const base::ListValue* list =
      pref_service_->GetList(prefs::kDismissedCategories);
  for (const std::unique_ptr<base::Value>& entry : *list) {
    int id = 0;
    if (!entry->GetAsInteger(&id)) {
      DLOG(WARNING) << "Invalid category pref value: " << *entry;
      continue;
    }

    // When the provider is registered, it will be stored in this map.
    dismissed_providers_by_category_[category_factory()->FromIDValue(id)] =
        nullptr;
  }
}

void ContentSuggestionsService::StoreDismissedCategoriesToPrefs() {
  base::ListValue list;
  for (const auto& category_provider_pair : dismissed_providers_by_category_) {
    list.AppendInteger(category_provider_pair.first.id());
  }

  pref_service_->Set(prefs::kDismissedCategories, list);
}

}  // namespace ntp_snippets
