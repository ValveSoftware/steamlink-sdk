// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_url_tracker.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_switches.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"


const char GoogleURLTracker::kDefaultGoogleHomepage[] =
    "https://www.google.com/";
const char GoogleURLTracker::kSearchDomainCheckURL[] =
    "https://www.google.com/searchdomaincheck?format=domain&type=chrome";

GoogleURLTracker::GoogleURLTracker(
    std::unique_ptr<GoogleURLTrackerClient> client,
    Mode mode)
    : client_(std::move(client)),
      google_url_(mode == UNIT_TEST_MODE ? kDefaultGoogleHomepage
                                         : client_->GetPrefs()->GetString(
                                               prefs::kLastKnownGoogleURL)),
      fetcher_id_(0),
      in_startup_sleep_(true),
      already_fetched_(false),
      need_to_fetch_(false),
      weak_ptr_factory_(this) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  client_->set_google_url_tracker(this);

  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay five seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  //
  // In UNIT_TEST_MODE, where we want to explicitly control when the tracker
  // "wakes up", we do nothing at all.
  if (mode == NORMAL_MODE) {
    static const int kStartFetchDelayMS = 5000;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&GoogleURLTracker::FinishSleep,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kStartFetchDelayMS));
  }
}

GoogleURLTracker::~GoogleURLTracker() {
}

// static
void GoogleURLTracker::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kLastKnownGoogleURL,
                               GoogleURLTracker::kDefaultGoogleHomepage);
  registry->RegisterStringPref(prefs::kLastPromptedGoogleURL, std::string());
}

void GoogleURLTracker::RequestServerCheck(bool force) {
  // If this instance already has a fetcher, SetNeedToFetch() is unnecessary,
  // and changing |already_fetched_| is wrong.
  if (!fetcher_) {
    if (force)
      already_fetched_ = false;
    SetNeedToFetch();
  }
}

std::unique_ptr<GoogleURLTracker::Subscription>
GoogleURLTracker::RegisterCallback(const OnGoogleURLUpdatedCallback& cb) {
  return callback_list_.Add(cb);
}

void GoogleURLTracker::OnURLFetchComplete(const net::URLFetcher* source) {
  // Delete the fetcher on this function's exit.
  std::unique_ptr<net::URLFetcher> clean_up_fetcher(std::move(fetcher_));

  // Don't update the URL if the request didn't succeed.
  if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200)) {
    already_fetched_ = false;
    return;
  }

  // See if the response data was valid.  It should be ".google.<TLD>".
  std::string url_str;
  source->GetResponseAsString(&url_str);
  base::TrimWhitespaceASCII(url_str, base::TRIM_ALL, &url_str);
  if (!base::StartsWith(url_str, ".google.",
                        base::CompareCase::INSENSITIVE_ASCII))
    return;
  GURL url("https://www" + url_str);
  if (!url.is_valid() || (url.path().length() > 1) || url.has_query() ||
      url.has_ref() ||
      !google_util::IsGoogleDomainUrl(url, google_util::DISALLOW_SUBDOMAIN,
                                      google_util::DISALLOW_NON_STANDARD_PORTS))
    return;

  if (url != google_url_) {
    google_url_ = url;
    client_->GetPrefs()->SetString(prefs::kLastKnownGoogleURL,
                                   google_url_.spec());
    callback_list_.Notify();
  }
}

void GoogleURLTracker::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // Ignore destructive signals.
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;
  already_fetched_ = false;
  StartFetchIfDesirable();
}

void GoogleURLTracker::Shutdown() {
  client_.reset();
  fetcher_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void GoogleURLTracker::SetNeedToFetch() {
  need_to_fetch_ = true;
  StartFetchIfDesirable();
}

void GoogleURLTracker::FinishSleep() {
  in_startup_sleep_ = false;
  StartFetchIfDesirable();
}

void GoogleURLTracker::StartFetchIfDesirable() {
  // Bail if a fetch isn't appropriate right now.  This function will be called
  // again each time one of the preconditions changes, so we'll fetch
  // immediately once all of them are met.
  //
  // See comments in header on the class, on RequestServerCheck(), and on the
  // various members here for more detail on exactly what the conditions are.
  if (in_startup_sleep_ || already_fetched_ || !need_to_fetch_)
    return;

  // Some switches should disable the Google URL tracker entirely.  If we can't
  // do background networking, we can't do the necessary fetch, and if the user
  // specified a Google base URL manually, we shouldn't bother to look up any
  // alternatives or offer to switch to them.
  if (!client_->IsBackgroundNetworkingEnabled() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGoogleBaseURL))
    return;

  already_fetched_ = true;
  fetcher_ = net::URLFetcher::Create(fetcher_id_, GURL(kSearchDomainCheckURL),
                                     net::URLFetcher::GET, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher_.get(),
      data_use_measurement::DataUseUserData::GOOGLE_URL_TRACKER);
  ++fetcher_id_;
  // We don't want this fetch to set new entries in the cache or cookies, lest
  // we alarm the user.
  fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetRequestContext(client_->GetRequestContext());

  // Configure to retry at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher_->SetMaxRetriesOn5xx(kMaxRetries);

  // Also retry kMaxRetries times on network change errors. A network change can
  // propagate through Chrome in various stages, so it's possible for this code
  // to be reached via OnNetworkChanged(), and then have the fetch we kick off
  // be canceled due to e.g. the DNS server changing at a later time. In general
  // it's not possible to ensure that by the time we reach here any requests we
  // start won't be canceled in this fashion, so retrying is the best we can do.
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);

  fetcher_->Start();
}
