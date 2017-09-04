// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_RECENT_TAB_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_RECENT_TAB_SUGGESTIONS_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/offline_pages/offline_page_model.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Image;
}  // namespace gfx

namespace ntp_snippets {

// Provides recent tabs content suggestions from the offline pages model
// obtaining the data through OfflinePageProxy.
class RecentTabSuggestionsProvider
    : public ContentSuggestionsProvider,
      public offline_pages::OfflinePageModel::Observer {
 public:
  RecentTabSuggestionsProvider(
      ContentSuggestionsProvider::Observer* observer,
      CategoryFactory* category_factory,
      offline_pages::OfflinePageModel* offline_page_model,
      PrefService* pref_service);
  ~RecentTabSuggestionsProvider() override;

  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             const FetchDoneCallback& callback) override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions(Category category) override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      const DismissedSuggestionsCallback& callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class RecentTabSuggestionsProviderTest;

  void GetPagesMatchingQueryCallbackForGetDismissedSuggestions(
      const DismissedSuggestionsCallback& callback,
      const std::vector<offline_pages::OfflinePageItem>& offline_pages) const;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override;
  void OfflinePageModelChanged(offline_pages::OfflinePageModel* model) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const offline_pages::ClientId& client_id) override;

  void GetPagesMatchingQueryCallbackForFetchRecentTabs(
      const std::vector<offline_pages::OfflinePageItem>& offline_pages);

  // Updates the |category_status_| of the |provided_category_| and notifies the
  // |observer_|, if necessary.
  void NotifyStatusChanged(CategoryStatus new_status);

  // Manually requests all offline pages and updates the suggestions.
  void FetchRecentTabs();

  // Converts an OfflinePageItem to a ContentSuggestion for the
  // |provided_category_|.
  ContentSuggestion ConvertOfflinePage(
      const offline_pages::OfflinePageItem& offline_page) const;

  // Gets the |kMaxSuggestionsCount| most recently visited OfflinePageItems from
  // the list, orders them by last visit date and converts them to
  // ContentSuggestions for the |provided_category_|.
  std::vector<ContentSuggestion> GetMostRecentlyVisited(
      std::vector<const offline_pages::OfflinePageItem*> offline_page_items)
      const;

  // Fires the |OnSuggestionInvalidated| event for the suggestion corresponding
  // to the given |offline_id| and clears it from the dismissed IDs list, if
  // necessary.
  void InvalidateSuggestion(int64_t offline_id);

  // Reads dismissed IDs from Prefs.
  std::set<std::string> ReadDismissedIDsFromPrefs() const;

  // Writes |dismissed_ids| into Prefs.
  void StoreDismissedIDsToPrefs(const std::set<std::string>& dismissed_ids);

  CategoryStatus category_status_;
  const Category provided_category_;
  offline_pages::OfflinePageModel* offline_page_model_;

  PrefService* pref_service_;

  base::WeakPtrFactory<RecentTabSuggestionsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_RECENT_TAB_SUGGESTIONS_PROVIDER_H_
