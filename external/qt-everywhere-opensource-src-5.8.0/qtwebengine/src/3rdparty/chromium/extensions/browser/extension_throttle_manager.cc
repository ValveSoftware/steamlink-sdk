// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_throttle_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "extensions/browser/extension_request_limiting_throttle.h"
#include "extensions/common/constants.h"
#include "net/base/url_util.h"
#include "net/log/net_log.h"
#include "net/url_request/url_request.h"

namespace extensions {

const unsigned int ExtensionThrottleManager::kMaximumNumberOfEntries = 1500;
const unsigned int ExtensionThrottleManager::kRequestsBetweenCollecting = 200;

ExtensionThrottleManager::ExtensionThrottleManager()
    : requests_since_last_gc_(0),
      enable_thread_checks_(false),
      logged_for_localhost_disabled_(false),
      registered_from_thread_(base::kInvalidThreadId),
      ignore_user_gesture_load_flag_for_tests_(false) {
  url_id_replacements_.ClearPassword();
  url_id_replacements_.ClearUsername();
  url_id_replacements_.ClearQuery();
  url_id_replacements_.ClearRef();

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

ExtensionThrottleManager::~ExtensionThrottleManager() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);

  // Since the manager object might conceivably go away before the
  // entries, detach the entries' back-pointer to the manager.
  UrlEntryMap::iterator i = url_entries_.begin();
  while (i != url_entries_.end()) {
    if (i->second.get() != NULL) {
      i->second->DetachManager();
    }
    ++i;
  }

  // Delete all entries.
  url_entries_.clear();
}

std::unique_ptr<content::ResourceThrottle>
ExtensionThrottleManager::MaybeCreateThrottle(const net::URLRequest* request) {
  if (request->first_party_for_cookies().scheme() !=
      extensions::kExtensionScheme) {
    return nullptr;
  }
  return base::WrapUnique(
      new extensions::ExtensionRequestLimitingThrottle(request, this));
}

scoped_refptr<ExtensionThrottleEntryInterface>
ExtensionThrottleManager::RegisterRequestUrl(const GURL& url) {
  DCHECK(!enable_thread_checks_ || CalledOnValidThread());

  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create a new NULL entry.
  scoped_refptr<ExtensionThrottleEntry>& entry = url_entries_[url_id];

  // If the entry exists but could be garbage collected at this point, we
  // start with a fresh entry so that we possibly back off a bit less
  // aggressively (i.e. this resets the error count when the entry's URL
  // hasn't been requested in long enough).
  if (entry.get() && entry->IsEntryOutdated()) {
    entry = NULL;
  }

  // Create the entry if needed.
  if (entry.get() == NULL) {
    if (backoff_policy_for_tests_) {
      entry = new ExtensionThrottleEntry(
          this, url_id, backoff_policy_for_tests_.get(),
          ignore_user_gesture_load_flag_for_tests_);
    } else {
      entry = new ExtensionThrottleEntry(
          this, url_id, ignore_user_gesture_load_flag_for_tests_);
    }

    // We only disable back-off throttling on an entry that we have
    // just constructed.  This is to allow unit tests to explicitly override
    // the entry for localhost URLs.
    std::string host = url.host();
    if (net::IsLocalhost(host)) {
      if (!logged_for_localhost_disabled_ && net::IsLocalhost(host)) {
        logged_for_localhost_disabled_ = true;
        net_log_.AddEvent(net::NetLog::TYPE_THROTTLING_DISABLED_FOR_HOST,
                          net::NetLog::StringCallback("host", &host));
      }

      // TODO(joi): Once sliding window is separate from back-off throttling,
      // we can simply return a dummy implementation of
      // ExtensionThrottleEntryInterface here that never blocks anything.
      entry->DisableBackoffThrottling();
    }
  }

  return entry;
}

void ExtensionThrottleManager::SetBackoffPolicyForTests(
    std::unique_ptr<net::BackoffEntry::Policy> policy) {
  backoff_policy_for_tests_ = std::move(policy);
}

void ExtensionThrottleManager::OverrideEntryForTests(
    const GURL& url,
    ExtensionThrottleEntry* entry) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  url_entries_[url_id] = entry;
}

void ExtensionThrottleManager::EraseEntryForTests(const GURL& url) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);
  url_entries_.erase(url_id);
}

void ExtensionThrottleManager::SetIgnoreUserGestureLoadFlagForTests(
    bool ignore_user_gesture_load_flag_for_tests) {
  ignore_user_gesture_load_flag_for_tests_ = true;
}

void ExtensionThrottleManager::set_enable_thread_checks(bool enable) {
  enable_thread_checks_ = enable;
}

bool ExtensionThrottleManager::enable_thread_checks() const {
  return enable_thread_checks_;
}

void ExtensionThrottleManager::set_net_log(net::NetLog* net_log) {
  DCHECK(net_log);
  net_log_ = net::BoundNetLog::Make(
      net_log, net::NetLog::SOURCE_EXPONENTIAL_BACKOFF_THROTTLING);
}

net::NetLog* ExtensionThrottleManager::net_log() const {
  return net_log_.net_log();
}

void ExtensionThrottleManager::OnIPAddressChanged() {
  OnNetworkChange();
}

void ExtensionThrottleManager::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  OnNetworkChange();
}

std::string ExtensionThrottleManager::GetIdFromUrl(const GURL& url) const {
  if (!url.is_valid())
    return url.possibly_invalid_spec();

  GURL id = url.ReplaceComponents(url_id_replacements_);
  return base::ToLowerASCII(id.spec());
}

void ExtensionThrottleManager::GarbageCollectEntriesIfNecessary() {
  requests_since_last_gc_++;
  if (requests_since_last_gc_ < kRequestsBetweenCollecting)
    return;
  requests_since_last_gc_ = 0;

  GarbageCollectEntries();
}

void ExtensionThrottleManager::GarbageCollectEntries() {
  UrlEntryMap::iterator i = url_entries_.begin();
  while (i != url_entries_.end()) {
    if ((i->second)->IsEntryOutdated()) {
      url_entries_.erase(i++);
    } else {
      ++i;
    }
  }

  // In case something broke we want to make sure not to grow indefinitely.
  while (url_entries_.size() > kMaximumNumberOfEntries) {
    url_entries_.erase(url_entries_.begin());
  }
}

void ExtensionThrottleManager::OnNetworkChange() {
  // Remove all entries.  Any entries that in-flight requests have a reference
  // to will live until those requests end, and these entries may be
  // inconsistent with new entries for the same URLs, but since what we
  // want is a clean slate for the new connection type, this is OK.
  url_entries_.clear();
  requests_since_last_gc_ = 0;
}

}  // namespace extensions
