// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"

namespace net {

const unsigned int URLRequestThrottlerManager::kMaximumNumberOfEntries = 1500;
const unsigned int URLRequestThrottlerManager::kRequestsBetweenCollecting = 200;

URLRequestThrottlerManager::URLRequestThrottlerManager()
    : requests_since_last_gc_(0),
      enable_thread_checks_(false),
      logged_for_localhost_disabled_(false),
      registered_from_thread_(base::kInvalidThreadId) {
  url_id_replacements_.ClearPassword();
  url_id_replacements_.ClearUsername();
  url_id_replacements_.ClearQuery();
  url_id_replacements_.ClearRef();

  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

URLRequestThrottlerManager::~URLRequestThrottlerManager() {
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);

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

scoped_refptr<URLRequestThrottlerEntryInterface>
    URLRequestThrottlerManager::RegisterRequestUrl(const GURL &url) {
  DCHECK(!enable_thread_checks_ || CalledOnValidThread());

  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create a new NULL entry.
  scoped_refptr<URLRequestThrottlerEntry>& entry = url_entries_[url_id];

  // If the entry exists but could be garbage collected at this point, we
  // start with a fresh entry so that we possibly back off a bit less
  // aggressively (i.e. this resets the error count when the entry's URL
  // hasn't been requested in long enough).
  if (entry.get() && entry->IsEntryOutdated()) {
    entry = NULL;
  }

  // Create the entry if needed.
  if (entry.get() == NULL) {
    entry = new URLRequestThrottlerEntry(this, url_id);

    // We only disable back-off throttling on an entry that we have
    // just constructed.  This is to allow unit tests to explicitly override
    // the entry for localhost URLs.  Given that we do not attempt to
    // disable throttling for entries already handed out (see comment
    // in AddToOptOutList), this is not a problem.
    std::string host = url.host();
    if (opt_out_hosts_.find(host) != opt_out_hosts_.end() ||
        IsLocalhost(host)) {
      if (!logged_for_localhost_disabled_ && IsLocalhost(host)) {
        logged_for_localhost_disabled_ = true;
        net_log_.AddEvent(NetLog::TYPE_THROTTLING_DISABLED_FOR_HOST,
                          NetLog::StringCallback("host", &host));
      }

      // TODO(joi): Once sliding window is separate from back-off throttling,
      // we can simply return a dummy implementation of
      // URLRequestThrottlerEntryInterface here that never blocks anything (and
      // not keep entries in url_entries_ for opted-out sites).
      entry->DisableBackoffThrottling();
    }
  }

  return entry;
}

void URLRequestThrottlerManager::AddToOptOutList(const std::string& host) {
  // There is an edge case here that we are not handling, to keep things
  // simple.  If a host starts adding the opt-out header to its responses
  // after there are already one or more entries in url_entries_ for that
  // host, the pre-existing entries may still perform back-off throttling.
  // In practice, this would almost never occur.
  if (opt_out_hosts_.find(host) == opt_out_hosts_.end()) {
    UMA_HISTOGRAM_COUNTS("Throttling.SiteOptedOut", 1);

    net_log_.EndEvent(NetLog::TYPE_THROTTLING_DISABLED_FOR_HOST,
                      NetLog::StringCallback("host", &host));
    opt_out_hosts_.insert(host);
  }
}

void URLRequestThrottlerManager::OverrideEntryForTests(
    const GURL& url,
    URLRequestThrottlerEntry* entry) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  url_entries_[url_id] = entry;
}

void URLRequestThrottlerManager::EraseEntryForTests(const GURL& url) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);
  url_entries_.erase(url_id);
}

void URLRequestThrottlerManager::set_enable_thread_checks(bool enable) {
  enable_thread_checks_ = enable;
}

bool URLRequestThrottlerManager::enable_thread_checks() const {
  return enable_thread_checks_;
}

void URLRequestThrottlerManager::set_net_log(NetLog* net_log) {
  DCHECK(net_log);
  net_log_ = BoundNetLog::Make(net_log,
                               NetLog::SOURCE_EXPONENTIAL_BACKOFF_THROTTLING);
}

NetLog* URLRequestThrottlerManager::net_log() const {
  return net_log_.net_log();
}

void URLRequestThrottlerManager::OnIPAddressChanged() {
  OnNetworkChange();
}

void URLRequestThrottlerManager::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  OnNetworkChange();
}

std::string URLRequestThrottlerManager::GetIdFromUrl(const GURL& url) const {
  if (!url.is_valid())
    return url.possibly_invalid_spec();

  GURL id = url.ReplaceComponents(url_id_replacements_);
  return StringToLowerASCII(id.spec()).c_str();
}

void URLRequestThrottlerManager::GarbageCollectEntriesIfNecessary() {
  requests_since_last_gc_++;
  if (requests_since_last_gc_ < kRequestsBetweenCollecting)
    return;
  requests_since_last_gc_ = 0;

  GarbageCollectEntries();
}

void URLRequestThrottlerManager::GarbageCollectEntries() {
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

void URLRequestThrottlerManager::OnNetworkChange() {
  // Remove all entries.  Any entries that in-flight requests have a reference
  // to will live until those requests end, and these entries may be
  // inconsistent with new entries for the same URLs, but since what we
  // want is a clean slate for the new connection type, this is OK.
  url_entries_.clear();
  requests_since_last_gc_ = 0;
}

}  // namespace net
