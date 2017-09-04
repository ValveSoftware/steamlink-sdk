// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/user_classifier.h"

class PrefService;
class PrefRegistrySimple;

namespace ntp_snippets {

class RemoteSuggestionsProvider;

// Retrieves suggestions from a number of ContentSuggestionsProviders and serves
// them grouped into categories. There can be at most one provider per category.
class ContentSuggestionsService : public KeyedService,
                                  public ContentSuggestionsProvider::Observer,
                                  public history::HistoryServiceObserver {
 public:
  class Observer {
   public:
    // Fired every time the service receives a new set of data for the given
    // |category|, replacing any previously available data (though in most cases
    // there will be an overlap and only a few changes within the data). The new
    // data is then available through |GetSuggestionsForCategory(category)|.
    virtual void OnNewSuggestions(Category category) = 0;

    // Fired when the status of a suggestions category changed. When the status
    // changes to an unavailable status, the suggestions of the respective
    // category have been invalidated, which means that they must no longer be
    // displayed to the user. The UI must immediately clear any suggestions of
    // that category.
    virtual void OnCategoryStatusChanged(Category category,
                                         CategoryStatus new_status) = 0;

    // Fired when a suggestion has been invalidated. The UI must immediately
    // clear the suggestion even from open NTPs. Invalidation happens, for
    // example, when the content that the suggestion refers to is gone.
    // Note that this event may be fired even if the corresponding category is
    // not currently AVAILABLE, because open UIs may still be showing the
    // suggestion that is to be removed. This event may also be fired for
    // |suggestion_id|s that never existed and should be ignored in that case.
    virtual void OnSuggestionInvalidated(
        const ContentSuggestion::ID& suggestion_id) = 0;

    // Sent when the service is shutting down. After the service has shut down,
    // it will not provide any data anymore, though calling the getters is still
    // safe.
    virtual void ContentSuggestionsServiceShutdown() = 0;

   protected:
    virtual ~Observer() = default;
  };

  enum State {
    ENABLED,
    DISABLED,
  };

  ContentSuggestionsService(State state,
                            history::HistoryService* history_service,
                            PrefService* pref_service);
  ~ContentSuggestionsService() override;

  // Inherited from KeyedService.
  void Shutdown() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  State state() { return state_; }

  // Gets all categories for which a provider is registered. The categories
  // may or may not be available, see |GetCategoryStatus()|.
  const std::vector<Category>& GetCategories() const { return categories_; }

  // Gets the status of a category.
  CategoryStatus GetCategoryStatus(Category category) const;

  // Gets the meta information of a category.
  base::Optional<CategoryInfo> GetCategoryInfo(Category category) const;

  // Gets the available suggestions for a category. The result is empty if the
  // category is available and empty, but also if the category is unavailable
  // for any reason, see |GetCategoryStatus()|.
  const std::vector<ContentSuggestion>& GetSuggestionsForCategory(
      Category category) const;

  // Fetches the image for the suggestion with the given |suggestion_id| and
  // runs the |callback|. If that suggestion doesn't exist or the fetch fails,
  // the callback gets an empty image. The callback will not be called
  // synchronously.
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const ImageFetchedCallback& callback);

  // Dismisses the suggestion with the given |suggestion_id|, if it exists.
  // This will not trigger an update through the observers.
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id);

  // Dismisses the given |category|, if it exists.
  // This will not trigger an update through the observers.
  void DismissCategory(Category category);

  // Restores all dismissed categories.
  // This will not trigger an update through the observers.
  void RestoreDismissedCategories();

  // Returns whether |category| is dismissed.
  bool IsCategoryDismissed(Category category) const;

  // Fetches additional contents for the given |category|. If the fetch was
  // completed, the given |callback| is called with the updated content.
  // This includes new and old data.
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             const FetchDoneCallback& callback);

  // Observer accessors.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Registers a new ContentSuggestionsProvider. It must be ensured that at most
  // one provider is registered for every category and that this method is
  // called only once per provider.
  void RegisterProvider(std::unique_ptr<ContentSuggestionsProvider> provider);

  // Removes history from the specified time range where the URL matches the
  // |filter| from all providers. The data removed depends on the provider. Note
  // that the data outside the time range may be deleted, for example
  // suggestions, which are based on history from that time range. Providers
  // should immediately clear any data related to history from the specified
  // time range where the URL matches the |filter|.
  void ClearHistory(base::Time begin,
                    base::Time end,
                    const base::Callback<bool(const GURL& url)>& filter);

  // Removes all suggestions from all caches or internal stores in all
  // providers. See |ClearCachedSuggestions|.
  void ClearAllCachedSuggestions();

  // Removes all suggestions of the given |category| from all caches or internal
  // stores in the service and the corresponding provider. It does, however, not
  // remove any suggestions from the provider's sources, so if its configuration
  // hasn't changed, it might return the same results when it fetches the next
  // time. In particular, calling this method will not mark any suggestions as
  // dismissed.
  void ClearCachedSuggestions(Category category);

  // Only for debugging use through the internals page.
  // Retrieves suggestions of the given |category| that have previously been
  // dismissed and are still stored in the respective provider. If the
  // provider doesn't store dismissed suggestions, the callback receives an
  // empty vector. The callback may be called synchronously.
  void GetDismissedSuggestionsForDebugging(
      Category category,
      const DismissedSuggestionsCallback& callback);

  // Only for debugging use through the internals page. Some providers
  // internally store a list of dismissed suggestions to prevent them from
  // reappearing. This function clears all suggestions of the given |category|
  // from such lists, making dismissed suggestions reappear (if the provider
  // supports it).
  void ClearDismissedSuggestionsForDebugging(Category category);

  CategoryFactory* category_factory() { return &category_factory_; }

  // The reference to the RemoteSuggestionsProvider provider should only be set
  // by the factory and only be used for scheduling, periodic fetching and
  // debugging.
  RemoteSuggestionsProvider* ntp_snippets_service() {
    return ntp_snippets_service_;
  }
  void set_ntp_snippets_service(
      RemoteSuggestionsProvider* ntp_snippets_service) {
    ntp_snippets_service_ = ntp_snippets_service;
  }

  UserClassifier* user_classifier() { return &user_classifier_; }

 private:
  friend class ContentSuggestionsServiceTest;

  // Implementation of ContentSuggestionsProvider::Observer.
  void OnNewSuggestions(ContentSuggestionsProvider* provider,
                        Category category,
                        std::vector<ContentSuggestion> suggestions) override;
  void OnCategoryStatusChanged(ContentSuggestionsProvider* provider,
                               Category category,
                               CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      ContentSuggestionsProvider* provider,
      const ContentSuggestion::ID& suggestion_id) override;

  // history::HistoryServiceObserver implementation.
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Registers the given |provider| for the given |category|, unless it is
  // already registered. Returns true if the category was newly registered or
  // false if it is dismissed or was present before.
  bool TryRegisterProviderForCategory(ContentSuggestionsProvider* provider,
                                      Category category);
  void RegisterCategory(Category category,
                        ContentSuggestionsProvider* provider);
  void UnregisterCategory(Category category,
                          ContentSuggestionsProvider* provider);

  // Removes a suggestion from the local store |suggestions_by_category_|, if it
  // exists. Returns true if a suggestion was removed.
  bool RemoveSuggestionByID(const ContentSuggestion::ID& suggestion_id);

  // Fires the OnCategoryStatusChanged event for the given |category|.
  void NotifyCategoryStatusChanged(Category category);

  void SortCategories();

  // Re-enables a dismissed category, making querying its provider possible.
  void RestoreDismissedCategory(Category category);

  void RestoreDismissedCategoriesFromPrefs();
  void StoreDismissedCategoriesToPrefs();

  // Whether the content suggestions feature is enabled.
  State state_;

  // Provides new and existing categories and an order for them.
  CategoryFactory category_factory_;

  // All registered providers, owned by the service.
  std::vector<std::unique_ptr<ContentSuggestionsProvider>> providers_;

  // All registered categories and their providers. A provider may be contained
  // multiple times, if it provides multiple categories. The keys of this map
  // are exactly the entries of |categories_| and the values are a subset of
  // |providers_|.
  std::map<Category, ContentSuggestionsProvider*, Category::CompareByID>
      providers_by_category_;

  // All dismissed categories and their providers. These may be restored by
  // RestoreDismissedCategories(). The provider can be null if the dismissed
  // category has received no updates since initialisation.
  // (see RestoreDismissedCategoriesFromPrefs())
  std::map<Category, ContentSuggestionsProvider*, Category::CompareByID>
      dismissed_providers_by_category_;

  // All current suggestion categories, in an order determined by the
  // |category_factory_|. This vector contains exactly the same categories as
  // |providers_by_category_|.
  std::vector<Category> categories_;

  // All current suggestions grouped by category. This contains an entry for
  // every category in |categories_| whose status is an available status. It may
  // contain an empty vector if the category is available but empty (or still
  // loading).
  std::map<Category, std::vector<ContentSuggestion>, Category::CompareByID>
      suggestions_by_category_;

  // Observer for the HistoryService. All providers are notified when history is
  // deleted.
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  base::ObserverList<Observer> observers_;

  const std::vector<ContentSuggestion> no_suggestions_;

  // Keep a direct reference to this special provider to redirect scheduling,
  // background fetching and debugging calls to it. If the
  // RemoteSuggestionsProvider is loaded, it is also present in |providers_|,
  // otherwise this is a nullptr.
  RemoteSuggestionsProvider* ntp_snippets_service_;

  PrefService* pref_service_;

  UserClassifier user_classifier_;

  DISALLOW_COPY_AND_ASSIGN(ContentSuggestionsService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_
