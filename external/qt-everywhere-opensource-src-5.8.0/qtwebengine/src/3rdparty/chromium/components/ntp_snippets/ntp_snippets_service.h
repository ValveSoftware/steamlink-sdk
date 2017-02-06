// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/ntp_snippets_status_service.h"
#include "components/suggestions/suggestions_service.h"
#include "components/sync_driver/sync_service_observer.h"

class PrefRegistrySimple;
class PrefService;
class SigninManagerBase;

namespace base {
class RefCountedMemory;
class Value;
}

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageDecoder;
class ImageFetcher;
}

namespace suggestions {
class SuggestionsProfile;
}

namespace sync_driver {
class SyncService;
}

namespace ntp_snippets {

class NTPSnippetsDatabase;
class NTPSnippetsServiceObserver;

// Stores and vends fresh content data for the NTP.
class NTPSnippetsService : public KeyedService,
                           public image_fetcher::ImageFetcherDelegate {
 public:
  using ImageFetchedCallback =
      base::Callback<void(const std::string& snippet_id, const gfx::Image&)>;

  // |application_language_code| should be a ISO 639-1 compliant string, e.g.
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (English language with US locale) and 'en-GB_US'
  // (British English person in the US) are not language codes.
  NTPSnippetsService(bool enabled,
                     PrefService* pref_service,
                     suggestions::SuggestionsService* suggestions_service,
                     const std::string& application_language_code,
                     NTPSnippetsScheduler* scheduler,
                     std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher,
                     std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
                     std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
                     std::unique_ptr<NTPSnippetsDatabase> database,
                     std::unique_ptr<NTPSnippetsStatusService> status_service);

  ~NTPSnippetsService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Inherited from KeyedService.
  void Shutdown() override;

  // Returns whether the service is ready. While this is false, the list of
  // snippets will be empty, and all modifications to it (fetch, discard, etc)
  // will be ignored.
  bool ready() const { return state_ == State::READY; }

  // Returns whether the service is initialized. While this is false, some
  // calls may trigger DCHECKs.
  bool initialized() const { return ready() || state_ == State::DISABLED; }

  // Fetches snippets from the server and adds them to the current ones.
  void FetchSnippets();
  // Fetches snippets from the server for specified hosts (overriding
  // suggestions from the suggestion service) and adds them to the current ones.
  // Only called from chrome://snippets-internals, DO NOT USE otherwise!
  // Ignored while |loaded()| is false.
  void FetchSnippetsFromHosts(const std::set<std::string>& hosts);

  // Available snippets.
  const NTPSnippet::PtrVector& snippets() const { return snippets_; }

  // Returns the list of snippets previously discarded by the user (that are
  // not expired yet).
  const NTPSnippet::PtrVector& discarded_snippets() const {
    return discarded_snippets_;
  }

  const NTPSnippetsFetcher* snippets_fetcher() const {
    return snippets_fetcher_.get();
  }

  // Returns a reason why the service is disabled, or DisabledReason::NONE
  // if it's not.
  DisabledReason disabled_reason() const {
    return snippets_status_service_->disabled_reason();
  }

  // (Re)schedules the periodic fetching of snippets. This is necessary because
  // the schedule depends on the time of day.
  void RescheduleFetching();

  // Fetches the image for the snippet with the given |snippet_id| and runs the
  // |callback|. If that snippet doesn't exist or the fetch fails, the callback
  // gets a null image.
  void FetchSnippetImage(const std::string& snippet_id,
                         const ImageFetchedCallback& callback);

  // Deletes all currently stored snippets.
  void ClearSnippets();

  // Discards the snippet with the given |snippet_id|, if it exists. Returns
  // true iff a snippet was discarded.
  bool DiscardSnippet(const std::string& snippet_id);

  // Clears the lists of snippets previously discarded by the user.
  void ClearDiscardedSnippets();

  // Returns the lists of suggestion hosts the snippets are restricted to.
  std::set<std::string> GetSuggestionsHosts() const;

  // Observer accessors.
  void AddObserver(NTPSnippetsServiceObserver* observer);
  void RemoveObserver(NTPSnippetsServiceObserver* observer);

  // Returns the maximum number of snippets that will be shown at once.
  static int GetMaxSnippetCountForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsServiceTest, HistorySyncStateChanges);

  // Possible state transitions:
  //  +------- NOT_INITED ------+
  //  |        /       \        |
  //  |   READY <--> DISABLED <-+
  //  |       \        /
  //  +-----> SHUT_DOWN
  enum class State {
    // The service has just been created. Can change to states:
    // - DISABLED: if the constructor was called with |enabled == false| . In
    //             that case the service will stay disabled until it is shut
    //             down. It can also enter this state after the database is
    //             done loading and GetStateForDependenciesStatus identifies
    //             the next state to be DISABLED.
    // - READY: if GetStateForDependenciesStatus returns it, after the database
    //          is done loading.
    NOT_INITED,

    // The service registered observers, timers, etc. and is ready to answer to
    // queries, fetch snippets... Can change to states:
    // - DISABLED: when the global Chrome state changes, for example after
    //             |OnStateChanged| is called and sync is disabled.
    // - SHUT_DOWN: when |Shutdown| is called, during the browser shutdown.
    READY,

    // The service is disabled and unregistered the related resources.
    // Can change to states:
    // - READY: when the global Chrome state changes, for example after
    //          |OnStateChanged| is called and sync is enabled.
    // - SHUT_DOWN: when |Shutdown| is called, during the browser shutdown.
    DISABLED,

    // The service shutdown and can't be used anymore. This state is checked
    // for early exit in callbacks from observers.
    SHUT_DOWN
  };

  // image_fetcher::ImageFetcherDelegate implementation.
  void OnImageDataFetched(const std::string& snippet_id,
                          const std::string& image_data) override;

  // Callbacks for the NTPSnippetsDatabase.
  void OnDatabaseLoaded(NTPSnippet::PtrVector snippets);
  void OnDatabaseError();

  // Callback for the SuggestionsService.
  void OnSuggestionsChanged(const suggestions::SuggestionsProfile& suggestions);

  // Callback for the NTPSnippetsFetcher.
  void OnFetchFinished(NTPSnippetsFetcher::OptionalSnippets snippets);

  // Merges newly available snippets with the previously available list.
  void MergeSnippets(NTPSnippet::PtrVector new_snippets);

  std::set<std::string> GetSnippetHostsFromPrefs() const;
  void StoreSnippetHostsToPrefs(const std::set<std::string>& hosts);

  // Removes the expired snippets (including discarded) from the service and the
  // database, and schedules another pass for the next expiration.
  void ClearExpiredSnippets();

  // Completes the initialization phase of the service, registering the last
  // observers. This is done after construction, once the database is loaded.
  void FinishInitialization();

  void LoadingSnippetsFinished();

  void OnSnippetImageFetchedFromDatabase(const std::string& snippet_id,
                                         const ImageFetchedCallback& callback,
                                         std::string data);

  void OnSnippetImageDecoded(const std::string& snippet_id,
                             const ImageFetchedCallback& callback,
                             const gfx::Image& image);

  void FetchSnippetImageFromNetwork(const std::string& snippet_id,
                                    const ImageFetchedCallback& callback);

  // Triggers a state transition depending on the provided reason to be
  // disabled (or lack thereof). This method is called when a change is detected
  // by |snippets_status_service_|
  void UpdateStateForStatus(DisabledReason disabled_reason);

  // Verifies state transitions (see |State|'s documentation) and applies them.
  // Does nothing if called with the current state.
  void EnterState(State state);

  // Enables the service and triggers a fetch if required. Do not call directly,
  // use |EnterState| instead.
  void EnterStateEnabled(bool fetch_snippets);

  // Disables the service. Do not call directly, use |EnterState| instead.
  void EnterStateDisabled();

  // Applies the effects of the transition to the SHUT_DOWN state. Do not call
  // directly, use |EnterState| instead.
  void EnterStateShutdown();

  void ClearDeprecatedPrefs();

  State state_;

  PrefService* pref_service_;

  suggestions::SuggestionsService* suggestions_service_;

  // All current suggestions (i.e. not discarded ones).
  NTPSnippet::PtrVector snippets_;

  // Suggestions that the user discarded. We keep these around until they expire
  // so we won't re-add them on the next fetch.
  NTPSnippet::PtrVector discarded_snippets_;

  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;

  // The observers.
  base::ObserverList<NTPSnippetsServiceObserver> observers_;

  // Scheduler for fetching snippets. Not owned.
  NTPSnippetsScheduler* scheduler_;

  // The subscription to the SuggestionsService. When the suggestions change,
  // SuggestionsService will call |OnSuggestionsChanged|, which triggers an
  // update to the set of snippets.
  using SuggestionsSubscription =
      suggestions::SuggestionsService::ResponseCallbackList::Subscription;
  std::unique_ptr<SuggestionsSubscription> suggestions_service_subscription_;

  // The snippets fetcher.
  std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher_;

  // Timer that calls us back when the next snippet expires.
  base::OneShotTimer expiry_timer_;

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  // The database for persisting snippets.
  std::unique_ptr<NTPSnippetsDatabase> database_;

  // The service that provides events and data about the signin and sync state.
  std::unique_ptr<NTPSnippetsStatusService> snippets_status_service_;

  // Set to true if FetchSnippets is called before the database has been loaded.
  // The fetch will be executed after the database load finishes.
  bool fetch_after_load_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsService);
};

class NTPSnippetsServiceObserver {
 public:
  // Sent every time the service loads a new set of data.
  virtual void NTPSnippetsServiceLoaded() = 0;

  // Sent when the service is shutting down.
  virtual void NTPSnippetsServiceShutdown() = 0;

  // Sent when the state of the service is changing. Something changed in its
  // dependencies so it's notifying observers about incoming data changes.
  // If the service might be enabled, DisabledReason::NONE will be provided.
  virtual void NTPSnippetsServiceDisabledReasonChanged(DisabledReason) = 0;

 protected:
  virtual ~NTPSnippetsServiceObserver() {}
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
