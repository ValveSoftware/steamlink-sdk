// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_monitor.h"

#include <stdint.h>

#include <algorithm>

#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"

namespace storage {

// StorageObserverList:

StorageObserverList::ObserverState::ObserverState()
    : requires_update(false) {
}

StorageObserverList::StorageObserverList() {}

StorageObserverList::~StorageObserverList() {}

void StorageObserverList::AddObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  ObserverState& observer_state = observers_[observer];
  observer_state.origin = params.filter.origin;
  observer_state.rate = params.rate;
}

void StorageObserverList::RemoveObserver(StorageObserver* observer) {
  observers_.erase(observer);
}

int StorageObserverList::ObserverCount() const {
  return observers_.size();
}

void StorageObserverList::OnStorageChange(const StorageObserver::Event& event) {
  // crbug.com/349708
  TRACE_EVENT0("io",
               "HostStorageObserversStorageObserverList::OnStorageChange");

  for (StorageObserverStateMap::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    it->second.requires_update = true;
  }

  MaybeDispatchEvent(event);
}

void StorageObserverList::MaybeDispatchEvent(
    const StorageObserver::Event& event) {
  // crbug.com/349708
  TRACE_EVENT0("io", "StorageObserverList::MaybeDispatchEvent");

  notification_timer_.Stop();
  base::TimeDelta min_delay = base::TimeDelta::Max();
  bool all_observers_notified = true;

  for (StorageObserverStateMap::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (!it->second.requires_update)
      continue;

    base::TimeTicks current_time = base::TimeTicks::Now();
    base::TimeDelta delta = current_time - it->second.last_notification_time;
    if (it->second.last_notification_time.is_null() ||
        delta >= it->second.rate) {
      it->second.requires_update = false;
      it->second.last_notification_time = current_time;

      if (it->second.origin == event.filter.origin) {
        // crbug.com/349708
        TRACE_EVENT0("io",
                     "StorageObserverList::MaybeDispatchEvent OnStorageEvent1");

        it->first->OnStorageEvent(event);
      } else {
        // When the quota and usage of an origin is requested, QuotaManager
        // returns the quota and usage of the host. Multiple origins can map to
        // to the same host, so ensure the |origin| field in the dispatched
        // event matches the |origin| specified by the observer when it was
        // registered.
        StorageObserver::Event dispatch_event(event);
        dispatch_event.filter.origin = it->second.origin;

        // crbug.com/349708
        TRACE_EVENT0("io",
                     "StorageObserverList::MaybeDispatchEvent OnStorageEvent2");

        it->first->OnStorageEvent(dispatch_event);
      }
    } else {
      all_observers_notified = false;
      base::TimeDelta delay = it->second.rate - delta;
      if (delay < min_delay)
        min_delay = delay;
    }
  }

  // We need to respect the notification rate specified by observers. So if it
  // is too soon to dispatch an event to an observer, save the event and
  // dispatch it after a delay. If we simply drop the event, another one may
  // not arrive anytime soon and the observer will miss the most recent event.
  if (!all_observers_notified) {
    pending_event_ = event;
    notification_timer_.Start(
        FROM_HERE,
        min_delay,
        this,
        &StorageObserverList::DispatchPendingEvent);
  }
}

void StorageObserverList::ScheduleUpdateForObserver(StorageObserver* observer) {
  DCHECK(ContainsKey(observers_, observer));
  observers_[observer].requires_update = true;
}

void StorageObserverList::DispatchPendingEvent() {
  MaybeDispatchEvent(pending_event_);
}


// HostStorageObservers:

HostStorageObservers::HostStorageObservers(QuotaManager* quota_manager)
    : quota_manager_(quota_manager),
      initialized_(false),
      initializing_(false),
      event_occurred_before_init_(false),
      usage_deltas_during_init_(0),
      cached_usage_(0),
      cached_quota_(0),
      weak_factory_(this) {
}

HostStorageObservers::~HostStorageObservers() {}

void HostStorageObservers::AddObserver(
    StorageObserver* observer,
    const StorageObserver::MonitorParams& params) {
  observers_.AddObserver(observer, params);

  if (!params.dispatch_initial_state)
    return;

  if (initialized_) {
    StorageObserver::Event event(params.filter,
                                 std::max<int64_t>(cached_usage_, 0),
                                 std::max<int64_t>(cached_quota_, 0));
    observer->OnStorageEvent(event);
    return;
  }

  // Ensure the observer receives the initial storage state once initialization
  // is complete.
  observers_.ScheduleUpdateForObserver(observer);
  StartInitialization(params.filter);
}

void HostStorageObservers::RemoveObserver(StorageObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool HostStorageObservers::ContainsObservers() const {
  return observers_.ObserverCount() > 0;
}

void HostStorageObservers::NotifyUsageChange(
    const StorageObserver::Filter& filter,
    int64_t delta) {
  if (initialized_) {
    cached_usage_ += delta;
    DispatchEvent(filter, true);
    return;
  }

  // If a storage change occurs before initialization, ensure all observers will
  // receive an event once initialization is complete.
  event_occurred_before_init_ = true;

  // During QuotaManager::GetUsageAndQuotaForWebApps(), cached data is read
  // synchronously, but other data may be retrieved asynchronously. A usage
  // change may occur between the function call and callback. These deltas need
  // to be added to the usage received by GotHostUsageAndQuota() to ensure
  // |cached_usage_| is correctly initialized.
  if (initializing_) {
    usage_deltas_during_init_ += delta;
    return;
  }

  StartInitialization(filter);
}

void HostStorageObservers::StartInitialization(
    const StorageObserver::Filter& filter) {
  if (initialized_ || initializing_)
    return;
  // crbug.com/349708
  TRACE_EVENT0("io", "HostStorageObservers::StartInitialization");

  initializing_ = true;
  quota_manager_->GetUsageAndQuotaForWebApps(
      filter.origin,
      filter.storage_type,
      base::Bind(&HostStorageObservers::GotHostUsageAndQuota,
                 weak_factory_.GetWeakPtr(),
                 filter));
}

void HostStorageObservers::GotHostUsageAndQuota(
    const StorageObserver::Filter& filter,
    QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  initializing_ = false;
  if (status != kQuotaStatusOk)
    return;
  initialized_ = true;
  cached_quota_ = quota;
  cached_usage_ = usage + usage_deltas_during_init_;
  DispatchEvent(filter, event_occurred_before_init_);
}

void HostStorageObservers::DispatchEvent(
    const StorageObserver::Filter& filter, bool is_update) {
  StorageObserver::Event event(filter, std::max<int64_t>(cached_usage_, 0),
                               std::max<int64_t>(cached_quota_, 0));
  if (is_update)
    observers_.OnStorageChange(event);
  else
    observers_.MaybeDispatchEvent(event);
}


// StorageTypeObservers:

StorageTypeObservers::StorageTypeObservers(QuotaManager* quota_manager)
    : quota_manager_(quota_manager) {
}

StorageTypeObservers::~StorageTypeObservers() {
  STLDeleteValues(&host_observers_map_);
}

void StorageTypeObservers::AddObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  std::string host = net::GetHostOrSpecFromURL(params.filter.origin);
  if (host.empty())
    return;

  HostStorageObservers* host_observers = NULL;
  HostObserversMap::iterator it = host_observers_map_.find(host);
  if (it == host_observers_map_.end()) {
    host_observers = new HostStorageObservers(quota_manager_);
    host_observers_map_[host] = host_observers;
  } else {
    host_observers = it->second;
  }

  host_observers->AddObserver(observer, params);
}

void StorageTypeObservers::RemoveObserver(StorageObserver* observer) {
  for (HostObserversMap::iterator it = host_observers_map_.begin();
       it != host_observers_map_.end(); ) {
    it->second->RemoveObserver(observer);
    if (!it->second->ContainsObservers()) {
      delete it->second;
      host_observers_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void StorageTypeObservers::RemoveObserverForFilter(
    StorageObserver* observer, const StorageObserver::Filter& filter) {
  std::string host = net::GetHostOrSpecFromURL(filter.origin);
  HostObserversMap::iterator it = host_observers_map_.find(host);
  if (it == host_observers_map_.end())
    return;

  it->second->RemoveObserver(observer);
  if (!it->second->ContainsObservers()) {
    delete it->second;
    host_observers_map_.erase(it);
  }
}

const HostStorageObservers* StorageTypeObservers::GetHostObservers(
    const std::string& host) const {
  HostObserversMap::const_iterator it = host_observers_map_.find(host);
  if (it != host_observers_map_.end())
    return it->second;

  return NULL;
}

void StorageTypeObservers::NotifyUsageChange(
    const StorageObserver::Filter& filter,
    int64_t delta) {
  std::string host = net::GetHostOrSpecFromURL(filter.origin);
  HostObserversMap::iterator it = host_observers_map_.find(host);
  if (it == host_observers_map_.end())
    return;

  it->second->NotifyUsageChange(filter, delta);
}


// StorageMonitor:

StorageMonitor::StorageMonitor(QuotaManager* quota_manager)
    : quota_manager_(quota_manager) {
}

StorageMonitor::~StorageMonitor() {
  STLDeleteValues(&storage_type_observers_map_);
}

void StorageMonitor::AddObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  DCHECK(observer);

  // Check preconditions.
  if (params.filter.storage_type == kStorageTypeUnknown ||
      params.filter.storage_type == kStorageTypeQuotaNotManaged ||
      params.filter.origin.is_empty()) {
    NOTREACHED();
    return;
  }

  StorageTypeObservers* type_observers = NULL;
  StorageTypeObserversMap::iterator it =
      storage_type_observers_map_.find(params.filter.storage_type);
  if (it == storage_type_observers_map_.end()) {
    type_observers = new StorageTypeObservers(quota_manager_);
    storage_type_observers_map_[params.filter.storage_type] = type_observers;
  } else {
    type_observers = it->second;
  }

  type_observers->AddObserver(observer, params);
}

void StorageMonitor::RemoveObserver(StorageObserver* observer) {
  for (StorageTypeObserversMap::iterator it =
           storage_type_observers_map_.begin();
       it != storage_type_observers_map_.end(); ++it) {
    it->second->RemoveObserver(observer);
  }
}

void StorageMonitor::RemoveObserverForFilter(
    StorageObserver* observer, const StorageObserver::Filter& filter) {
  StorageTypeObserversMap::iterator it =
      storage_type_observers_map_.find(filter.storage_type);
  if (it == storage_type_observers_map_.end())
    return;

  it->second->RemoveObserverForFilter(observer, filter);
}

const StorageTypeObservers* StorageMonitor::GetStorageTypeObservers(
    StorageType storage_type) const {
  StorageTypeObserversMap::const_iterator it =
      storage_type_observers_map_.find(storage_type);
  if (it != storage_type_observers_map_.end())
    return it->second;

  return NULL;
}

void StorageMonitor::NotifyUsageChange(const StorageObserver::Filter& filter,
                                       int64_t delta) {
  // Check preconditions.
  if (filter.storage_type == kStorageTypeUnknown ||
      filter.storage_type == kStorageTypeQuotaNotManaged ||
      filter.origin.is_empty()) {
    NOTREACHED();
    return;
  }

  StorageTypeObserversMap::iterator it =
      storage_type_observers_map_.find(filter.storage_type);
  if (it == storage_type_observers_map_.end())
    return;

  it->second->NotifyUsageChange(filter, delta);
}

}  // namespace storage
