// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_H_
#define COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/google/core/browser/google_url_tracker_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class PrefService;

namespace infobars {
class InfoBar;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This object is responsible for checking the Google URL once per network
// change.  The current value is saved to prefs.
//
// Most consumers should only call google_url().  Consumers who need to be
// notified when things change should register a callback that provides the
// original and updated values via RegisterCallback().
//
// To protect users' privacy and reduce server load, no updates will be
// performed (ever) unless at least one consumer registers interest by calling
// RequestServerCheck().
class GoogleURLTracker
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public KeyedService {
 public:
  // Callback that is called when the Google URL is updated. The arguments are
  // the old and new URLs.
  typedef base::Callback<void()> OnGoogleURLUpdatedCallback;
  typedef base::CallbackList<void()> CallbackList;
  typedef CallbackList::Subscription Subscription;

  // The constructor does different things depending on which of these values
  // you pass it.  Hopefully these are self-explanatory.
  enum Mode {
    NORMAL_MODE,
    UNIT_TEST_MODE,
  };

  static const char kDefaultGoogleHomepage[];

  // Only the GoogleURLTrackerFactory and tests should call this.
  GoogleURLTracker(std::unique_ptr<GoogleURLTrackerClient> client, Mode mode);

  ~GoogleURLTracker() override;

  // Register user preferences for GoogleURLTracker.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);


  // Returns the current Google homepage URL.
  const GURL& google_url() const { return google_url_; }

  // Requests that the tracker perform a server check to update the Google URL
  // as necessary.  If |force| is false, this will happen at most once per
  // network change, not sooner than five seconds after startup (checks
  // requested before that time will occur then; checks requested afterwards
  // will occur immediately, if no other checks have been made during this run).
  // If |force| is true, and the tracker has already performed any requested
  // check, it will check again.
  void RequestServerCheck(bool force);

  std::unique_ptr<Subscription> RegisterCallback(
      const OnGoogleURLUpdatedCallback& cb);

 private:
  friend class GoogleURLTrackerTest;
  friend class SyncTest;

  static const char kSearchDomainCheckURL[];

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // NetworkChangeNotifier::IPAddressObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // KeyedService:
  void Shutdown() override;

  // Sets |need_to_fetch_| and attempts to start a fetch.
  void SetNeedToFetch();

  // Called when the five second startup sleep has finished.  Runs any pending
  // fetch.
  void FinishSleep();

  // Starts the fetch of the up-to-date Google URL if we actually want to fetch
  // it and can currently do so.
  void StartFetchIfDesirable();

  CallbackList callback_list_;

  std::unique_ptr<GoogleURLTrackerClient> client_;

  GURL google_url_;
  std::unique_ptr<net::URLFetcher> fetcher_;
  int fetcher_id_;
  bool in_startup_sleep_;  // True if we're in the five-second "no fetching"
                           // period that begins at browser start.
  bool already_fetched_;   // True if we've already fetched a URL once this run;
                           // we won't fetch again until after a restart.
  bool need_to_fetch_;     // True if a consumer actually wants us to fetch an
                           // updated URL.  If this is never set, we won't
                           // bother to fetch anything.
                           // Consumers should register a callback via
                           // RegisterCallback().
  base::WeakPtrFactory<GoogleURLTracker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTracker);
};

#endif  // COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_H_
