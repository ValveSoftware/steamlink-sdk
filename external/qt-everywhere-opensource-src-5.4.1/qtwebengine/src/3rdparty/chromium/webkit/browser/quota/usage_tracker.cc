// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/quota/usage_tracker.h"

#include <algorithm>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "net/base/net_util.h"
#include "webkit/browser/quota/storage_monitor.h"
#include "webkit/browser/quota/storage_observer.h"

namespace quota {

namespace {

typedef ClientUsageTracker::OriginUsageAccumulator OriginUsageAccumulator;
typedef ClientUsageTracker::OriginSetByHost OriginSetByHost;

void DidGetOriginUsage(const OriginUsageAccumulator& accumulator,
                       const GURL& origin,
                       int64 usage) {
  accumulator.Run(origin, usage);
}

void DidGetHostUsage(const UsageCallback& callback,
                     int64 limited_usage,
                     int64 unlimited_usage) {
  DCHECK_GE(limited_usage, 0);
  DCHECK_GE(unlimited_usage, 0);
  callback.Run(limited_usage + unlimited_usage);
}

bool EraseOriginFromOriginSet(OriginSetByHost* origins_by_host,
                              const std::string& host,
                              const GURL& origin) {
  OriginSetByHost::iterator found = origins_by_host->find(host);
  if (found == origins_by_host->end())
    return false;

  if (!found->second.erase(origin))
    return false;

  if (found->second.empty())
    origins_by_host->erase(host);
  return true;
}

bool OriginSetContainsOrigin(const OriginSetByHost& origins,
                             const std::string& host,
                             const GURL& origin) {
  OriginSetByHost::const_iterator itr = origins.find(host);
  return itr != origins.end() && ContainsKey(itr->second, origin);
}

void DidGetGlobalUsageForLimitedGlobalUsage(const UsageCallback& callback,
                                            int64 total_global_usage,
                                            int64 global_unlimited_usage) {
  callback.Run(total_global_usage - global_unlimited_usage);
}

}  // namespace

// UsageTracker ----------------------------------------------------------

UsageTracker::UsageTracker(const QuotaClientList& clients,
                           StorageType type,
                           SpecialStoragePolicy* special_storage_policy,
                           StorageMonitor* storage_monitor)
    : type_(type),
      storage_monitor_(storage_monitor),
      weak_factory_(this) {
  for (QuotaClientList::const_iterator iter = clients.begin();
      iter != clients.end();
      ++iter) {
    if ((*iter)->DoesSupport(type)) {
      client_tracker_map_[(*iter)->id()] =
          new ClientUsageTracker(this, *iter, type, special_storage_policy,
                                 storage_monitor_);
    }
  }
}

UsageTracker::~UsageTracker() {
  STLDeleteValues(&client_tracker_map_);
}

ClientUsageTracker* UsageTracker::GetClientTracker(QuotaClient::ID client_id) {
  ClientTrackerMap::iterator found = client_tracker_map_.find(client_id);
  if (found != client_tracker_map_.end())
    return found->second;
  return NULL;
}

void UsageTracker::GetGlobalLimitedUsage(const UsageCallback& callback) {
  if (global_usage_callbacks_.HasCallbacks()) {
    global_usage_callbacks_.Add(base::Bind(
        &DidGetGlobalUsageForLimitedGlobalUsage, callback));
    return;
  }

  if (!global_limited_usage_callbacks_.Add(callback))
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // Calling GetGlobalLimitedUsage(accumulator) may synchronously
  // return if the usage is cached, which may in turn dispatch
  // the completion callback before we finish looping over
  // all clients (because info->pending_clients may reach 0
  // during the loop).
  // To avoid this, we add one more pending client as a sentinel
  // and fire the sentinel callback at the end.
  info->pending_clients = client_tracker_map_.size() + 1;
  UsageCallback accumulator = base::Bind(
      &UsageTracker::AccumulateClientGlobalLimitedUsage,
      weak_factory_.GetWeakPtr(), base::Owned(info));

  for (ClientTrackerMap::iterator iter = client_tracker_map_.begin();
       iter != client_tracker_map_.end();
       ++iter)
    iter->second->GetGlobalLimitedUsage(accumulator);

  // Fire the sentinel as we've now called GetGlobalUsage for all clients.
  accumulator.Run(0);
}

void UsageTracker::GetGlobalUsage(const GlobalUsageCallback& callback) {
  if (!global_usage_callbacks_.Add(callback))
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // Calling GetGlobalUsage(accumulator) may synchronously
  // return if the usage is cached, which may in turn dispatch
  // the completion callback before we finish looping over
  // all clients (because info->pending_clients may reach 0
  // during the loop).
  // To avoid this, we add one more pending client as a sentinel
  // and fire the sentinel callback at the end.
  info->pending_clients = client_tracker_map_.size() + 1;
  GlobalUsageCallback accumulator = base::Bind(
      &UsageTracker::AccumulateClientGlobalUsage, weak_factory_.GetWeakPtr(),
      base::Owned(info));

  for (ClientTrackerMap::iterator iter = client_tracker_map_.begin();
       iter != client_tracker_map_.end();
       ++iter)
    iter->second->GetGlobalUsage(accumulator);

  // Fire the sentinel as we've now called GetGlobalUsage for all clients.
  accumulator.Run(0, 0);
}

void UsageTracker::GetHostUsage(const std::string& host,
                                const UsageCallback& callback) {
  if (!host_usage_callbacks_.Add(host, callback))
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // Calling GetHostUsage(accumulator) may synchronously
  // return if the usage is cached, which may in turn dispatch
  // the completion callback before we finish looping over
  // all clients (because info->pending_clients may reach 0
  // during the loop).
  // To avoid this, we add one more pending client as a sentinel
  // and fire the sentinel callback at the end.
  info->pending_clients = client_tracker_map_.size() + 1;
  UsageCallback accumulator = base::Bind(
      &UsageTracker::AccumulateClientHostUsage, weak_factory_.GetWeakPtr(),
      base::Owned(info), host);

  for (ClientTrackerMap::iterator iter = client_tracker_map_.begin();
       iter != client_tracker_map_.end();
       ++iter)
    iter->second->GetHostUsage(host, accumulator);

  // Fire the sentinel as we've now called GetHostUsage for all clients.
  accumulator.Run(0);
}

void UsageTracker::UpdateUsageCache(
    QuotaClient::ID client_id, const GURL& origin, int64 delta) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);
  client_tracker->UpdateUsageCache(origin, delta);
}

void UsageTracker::GetCachedHostsUsage(
    std::map<std::string, int64>* host_usage) const {
  DCHECK(host_usage);
  host_usage->clear();
  for (ClientTrackerMap::const_iterator iter = client_tracker_map_.begin();
       iter != client_tracker_map_.end(); ++iter) {
    iter->second->GetCachedHostsUsage(host_usage);
  }
}

void UsageTracker::GetCachedOrigins(std::set<GURL>* origins) const {
  DCHECK(origins);
  origins->clear();
  for (ClientTrackerMap::const_iterator iter = client_tracker_map_.begin();
       iter != client_tracker_map_.end(); ++iter) {
    iter->second->GetCachedOrigins(origins);
  }
}

void UsageTracker::SetUsageCacheEnabled(QuotaClient::ID client_id,
                                        const GURL& origin,
                                        bool enabled) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);

  client_tracker->SetUsageCacheEnabled(origin, enabled);
}

void UsageTracker::AccumulateClientGlobalLimitedUsage(AccumulateInfo* info,
                                                      int64 limited_usage) {
  info->usage += limited_usage;
  if (--info->pending_clients)
    return;

  // All the clients have returned their usage data.  Dispatch the
  // pending callbacks.
  global_limited_usage_callbacks_.Run(MakeTuple(info->usage));
}

void UsageTracker::AccumulateClientGlobalUsage(AccumulateInfo* info,
                                               int64 usage,
                                               int64 unlimited_usage) {
  info->usage += usage;
  info->unlimited_usage += unlimited_usage;
  if (--info->pending_clients)
    return;

  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  // TODO(michaeln): The unlimited number is not trustworthy, it
  // can get out of whack when apps are installed or uninstalled.
  if (info->unlimited_usage > info->usage)
    info->unlimited_usage = info->usage;
  else if (info->unlimited_usage < 0)
    info->unlimited_usage = 0;

  // All the clients have returned their usage data.  Dispatch the
  // pending callbacks.
  global_usage_callbacks_.Run(MakeTuple(info->usage, info->unlimited_usage));
}

void UsageTracker::AccumulateClientHostUsage(AccumulateInfo* info,
                                             const std::string& host,
                                             int64 usage) {
  info->usage += usage;
  if (--info->pending_clients)
    return;

  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  // All the clients have returned their usage data.  Dispatch the
  // pending callbacks.
  host_usage_callbacks_.Run(host, MakeTuple(info->usage));
}

// ClientUsageTracker ----------------------------------------------------

ClientUsageTracker::ClientUsageTracker(
    UsageTracker* tracker, QuotaClient* client, StorageType type,
    SpecialStoragePolicy* special_storage_policy,
    StorageMonitor* storage_monitor)
    : tracker_(tracker),
      client_(client),
      type_(type),
      storage_monitor_(storage_monitor),
      global_limited_usage_(0),
      global_unlimited_usage_(0),
      global_usage_retrieved_(false),
      special_storage_policy_(special_storage_policy) {
  DCHECK(tracker_);
  DCHECK(client_);
  if (special_storage_policy_.get())
    special_storage_policy_->AddObserver(this);
}

ClientUsageTracker::~ClientUsageTracker() {
  if (special_storage_policy_.get())
    special_storage_policy_->RemoveObserver(this);
}

void ClientUsageTracker::GetGlobalLimitedUsage(const UsageCallback& callback) {
  if (!global_usage_retrieved_) {
    GetGlobalUsage(base::Bind(&DidGetGlobalUsageForLimitedGlobalUsage,
                              callback));
    return;
  }

  if (non_cached_limited_origins_by_host_.empty()) {
    callback.Run(global_limited_usage_);
    return;
  }

  AccumulateInfo* info = new AccumulateInfo;
  info->pending_jobs = non_cached_limited_origins_by_host_.size() + 1;
  UsageCallback accumulator = base::Bind(
      &ClientUsageTracker::AccumulateLimitedOriginUsage, AsWeakPtr(),
      base::Owned(info), callback);

  for (OriginSetByHost::iterator host_itr =
           non_cached_limited_origins_by_host_.begin();
       host_itr != non_cached_limited_origins_by_host_.end(); ++host_itr) {
    for (std::set<GURL>::iterator origin_itr = host_itr->second.begin();
         origin_itr != host_itr->second.end(); ++origin_itr)
      client_->GetOriginUsage(*origin_itr, type_, accumulator);
  }

  accumulator.Run(global_limited_usage_);
}

void ClientUsageTracker::GetGlobalUsage(const GlobalUsageCallback& callback) {
  if (global_usage_retrieved_ &&
      non_cached_limited_origins_by_host_.empty() &&
      non_cached_unlimited_origins_by_host_.empty()) {
    callback.Run(global_limited_usage_ + global_unlimited_usage_,
                 global_unlimited_usage_);
    return;
  }

  client_->GetOriginsForType(type_, base::Bind(
      &ClientUsageTracker::DidGetOriginsForGlobalUsage, AsWeakPtr(),
      callback));
}

void ClientUsageTracker::GetHostUsage(
    const std::string& host, const UsageCallback& callback) {
  if (ContainsKey(cached_hosts_, host) &&
      !ContainsKey(non_cached_limited_origins_by_host_, host) &&
      !ContainsKey(non_cached_unlimited_origins_by_host_, host)) {
    // TODO(kinuko): Drop host_usage_map_ cache periodically.
    callback.Run(GetCachedHostUsage(host));
    return;
  }

  if (!host_usage_accumulators_.Add(
          host, base::Bind(&DidGetHostUsage, callback)))
    return;
  client_->GetOriginsForHost(type_, host, base::Bind(
      &ClientUsageTracker::DidGetOriginsForHostUsage, AsWeakPtr(), host));
}

void ClientUsageTracker::UpdateUsageCache(
    const GURL& origin, int64 delta) {
  std::string host = net::GetHostOrSpecFromURL(origin);
  if (cached_hosts_.find(host) != cached_hosts_.end()) {
    if (!IsUsageCacheEnabledForOrigin(origin))
      return;

    cached_usage_by_host_[host][origin] += delta;
    if (IsStorageUnlimited(origin))
      global_unlimited_usage_ += delta;
    else
      global_limited_usage_ += delta;
    DCHECK_GE(cached_usage_by_host_[host][origin], 0);
    DCHECK_GE(global_limited_usage_, 0);

    // Notify the usage monitor that usage has changed. The storage monitor may
    // be NULL during tests.
    if (storage_monitor_) {
      StorageObserver::Filter filter(type_, origin);
      storage_monitor_->NotifyUsageChange(filter, delta);
    }
    return;
  }

  // We don't know about this host yet, so populate our cache for it.
  GetHostUsage(host, base::Bind(&ClientUsageTracker::DidGetHostUsageAfterUpdate,
                                AsWeakPtr(), origin));
}

void ClientUsageTracker::GetCachedHostsUsage(
    std::map<std::string, int64>* host_usage) const {
  DCHECK(host_usage);
  for (HostUsageMap::const_iterator host_iter = cached_usage_by_host_.begin();
       host_iter != cached_usage_by_host_.end(); host_iter++) {
    const std::string& host = host_iter->first;
    (*host_usage)[host] += GetCachedHostUsage(host);
  }
}

void ClientUsageTracker::GetCachedOrigins(std::set<GURL>* origins) const {
  DCHECK(origins);
  for (HostUsageMap::const_iterator host_iter = cached_usage_by_host_.begin();
       host_iter != cached_usage_by_host_.end(); host_iter++) {
    const UsageMap& origin_map = host_iter->second;
    for (UsageMap::const_iterator origin_iter = origin_map.begin();
         origin_iter != origin_map.end(); origin_iter++) {
      origins->insert(origin_iter->first);
    }
  }
}

void ClientUsageTracker::SetUsageCacheEnabled(const GURL& origin,
                                              bool enabled) {
  std::string host = net::GetHostOrSpecFromURL(origin);
  if (!enabled) {
    // Erase |origin| from cache and subtract its usage.
    HostUsageMap::iterator found_host = cached_usage_by_host_.find(host);
    if (found_host != cached_usage_by_host_.end()) {
      UsageMap& cached_usage_for_host = found_host->second;

      UsageMap::iterator found = cached_usage_for_host.find(origin);
      if (found != cached_usage_for_host.end()) {
        int64 usage = found->second;
        UpdateUsageCache(origin, -usage);
        cached_usage_for_host.erase(found);
        if (cached_usage_for_host.empty()) {
          cached_usage_by_host_.erase(found_host);
          cached_hosts_.erase(host);
        }
      }
    }

    if (IsStorageUnlimited(origin))
      non_cached_unlimited_origins_by_host_[host].insert(origin);
    else
      non_cached_limited_origins_by_host_[host].insert(origin);
  } else {
    // Erase |origin| from |non_cached_origins_| and invalidate the usage cache
    // for the host.
    if (EraseOriginFromOriginSet(&non_cached_limited_origins_by_host_,
                                 host, origin) ||
        EraseOriginFromOriginSet(&non_cached_unlimited_origins_by_host_,
                                 host, origin)) {
      cached_hosts_.erase(host);
      global_usage_retrieved_ = false;
    }
  }
}

void ClientUsageTracker::AccumulateLimitedOriginUsage(
    AccumulateInfo* info,
    const UsageCallback& callback,
    int64 usage) {
  info->limited_usage += usage;
  if (--info->pending_jobs)
    return;

  callback.Run(info->limited_usage);
}

void ClientUsageTracker::DidGetOriginsForGlobalUsage(
    const GlobalUsageCallback& callback,
    const std::set<GURL>& origins) {
  OriginSetByHost origins_by_host;
  for (std::set<GURL>::const_iterator itr = origins.begin();
       itr != origins.end(); ++itr)
    origins_by_host[net::GetHostOrSpecFromURL(*itr)].insert(*itr);

  AccumulateInfo* info = new AccumulateInfo;
  // Getting host usage may synchronously return the result if the usage is
  // cached, which may in turn dispatch the completion callback before we finish
  // looping over all hosts (because info->pending_jobs may reach 0 during the
  // loop).  To avoid this, we add one more pending host as a sentinel and
  // fire the sentinel callback at the end.
  info->pending_jobs = origins_by_host.size() + 1;
  HostUsageAccumulator accumulator =
      base::Bind(&ClientUsageTracker::AccumulateHostUsage, AsWeakPtr(),
                 base::Owned(info), callback);

  for (OriginSetByHost::iterator itr = origins_by_host.begin();
       itr != origins_by_host.end(); ++itr) {
    if (host_usage_accumulators_.Add(itr->first, accumulator))
      GetUsageForOrigins(itr->first, itr->second);
  }

  // Fire the sentinel as we've now called GetUsageForOrigins for all clients.
  accumulator.Run(0, 0);
}

void ClientUsageTracker::AccumulateHostUsage(
    AccumulateInfo* info,
    const GlobalUsageCallback& callback,
    int64 limited_usage,
    int64 unlimited_usage) {
  info->limited_usage += limited_usage;
  info->unlimited_usage += unlimited_usage;
  if (--info->pending_jobs)
    return;

  DCHECK_GE(info->limited_usage, 0);
  DCHECK_GE(info->unlimited_usage, 0);

  global_usage_retrieved_ = true;
  callback.Run(info->limited_usage + info->unlimited_usage,
               info->unlimited_usage);
}

void ClientUsageTracker::DidGetOriginsForHostUsage(
    const std::string& host,
    const std::set<GURL>& origins) {
  GetUsageForOrigins(host, origins);
}

void ClientUsageTracker::GetUsageForOrigins(
    const std::string& host,
    const std::set<GURL>& origins) {
  AccumulateInfo* info = new AccumulateInfo;
  // Getting origin usage may synchronously return the result if the usage is
  // cached, which may in turn dispatch the completion callback before we finish
  // looping over all origins (because info->pending_jobs may reach 0 during the
  // loop).  To avoid this, we add one more pending origin as a sentinel and
  // fire the sentinel callback at the end.
  info->pending_jobs = origins.size() + 1;
  OriginUsageAccumulator accumulator =
      base::Bind(&ClientUsageTracker::AccumulateOriginUsage, AsWeakPtr(),
                 base::Owned(info), host);

  for (std::set<GURL>::const_iterator itr = origins.begin();
       itr != origins.end(); ++itr) {
    DCHECK_EQ(host, net::GetHostOrSpecFromURL(*itr));

    int64 origin_usage = 0;
    if (GetCachedOriginUsage(*itr, &origin_usage)) {
      accumulator.Run(*itr, origin_usage);
    } else {
      client_->GetOriginUsage(*itr, type_, base::Bind(
          &DidGetOriginUsage, accumulator, *itr));
    }
  }

  // Fire the sentinel as we've now called GetOriginUsage for all clients.
  accumulator.Run(GURL(), 0);
}

void ClientUsageTracker::AccumulateOriginUsage(AccumulateInfo* info,
                                               const std::string& host,
                                               const GURL& origin,
                                               int64 usage) {
  if (!origin.is_empty()) {
    if (usage < 0)
      usage = 0;

    if (IsStorageUnlimited(origin))
      info->unlimited_usage += usage;
    else
      info->limited_usage += usage;
    if (IsUsageCacheEnabledForOrigin(origin))
      AddCachedOrigin(origin, usage);
  }
  if (--info->pending_jobs)
    return;

  AddCachedHost(host);
  host_usage_accumulators_.Run(
      host, MakeTuple(info->limited_usage, info->unlimited_usage));
}

void ClientUsageTracker::DidGetHostUsageAfterUpdate(
    const GURL& origin, int64 usage) {
  if (!storage_monitor_)
    return;

  StorageObserver::Filter filter(type_, origin);
  storage_monitor_->NotifyUsageChange(filter, 0);
}

void ClientUsageTracker::AddCachedOrigin(
    const GURL& origin, int64 new_usage) {
  DCHECK(IsUsageCacheEnabledForOrigin(origin));

  std::string host = net::GetHostOrSpecFromURL(origin);
  int64* usage = &cached_usage_by_host_[host][origin];
  int64 delta = new_usage - *usage;
  *usage = new_usage;
  if (delta) {
    if (IsStorageUnlimited(origin))
      global_unlimited_usage_ += delta;
    else
      global_limited_usage_ += delta;
  }
  DCHECK_GE(*usage, 0);
  DCHECK_GE(global_limited_usage_, 0);
}

void ClientUsageTracker::AddCachedHost(const std::string& host) {
  cached_hosts_.insert(host);
}

int64 ClientUsageTracker::GetCachedHostUsage(const std::string& host) const {
  HostUsageMap::const_iterator found = cached_usage_by_host_.find(host);
  if (found == cached_usage_by_host_.end())
    return 0;

  int64 usage = 0;
  const UsageMap& map = found->second;
  for (UsageMap::const_iterator iter = map.begin();
       iter != map.end(); ++iter) {
    usage += iter->second;
  }
  return usage;
}

bool ClientUsageTracker::GetCachedOriginUsage(
    const GURL& origin,
    int64* usage) const {
  std::string host = net::GetHostOrSpecFromURL(origin);
  HostUsageMap::const_iterator found_host = cached_usage_by_host_.find(host);
  if (found_host == cached_usage_by_host_.end())
    return false;

  UsageMap::const_iterator found = found_host->second.find(origin);
  if (found == found_host->second.end())
    return false;

  DCHECK(IsUsageCacheEnabledForOrigin(origin));
  *usage = found->second;
  return true;
}

bool ClientUsageTracker::IsUsageCacheEnabledForOrigin(
    const GURL& origin) const {
  std::string host = net::GetHostOrSpecFromURL(origin);
  return !OriginSetContainsOrigin(non_cached_limited_origins_by_host_,
                                  host, origin) &&
      !OriginSetContainsOrigin(non_cached_unlimited_origins_by_host_,
                               host, origin);
}

void ClientUsageTracker::OnGranted(const GURL& origin,
                                   int change_flags) {
  DCHECK(CalledOnValidThread());
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64 usage = 0;
    if (GetCachedOriginUsage(origin, &usage)) {
      global_unlimited_usage_ += usage;
      global_limited_usage_ -= usage;
    }

    std::string host = net::GetHostOrSpecFromURL(origin);
    if (EraseOriginFromOriginSet(&non_cached_limited_origins_by_host_,
                                 host, origin))
      non_cached_unlimited_origins_by_host_[host].insert(origin);
  }
}

void ClientUsageTracker::OnRevoked(const GURL& origin,
                                   int change_flags) {
  DCHECK(CalledOnValidThread());
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64 usage = 0;
    if (GetCachedOriginUsage(origin, &usage)) {
      global_unlimited_usage_ -= usage;
      global_limited_usage_ += usage;
    }

    std::string host = net::GetHostOrSpecFromURL(origin);
    if (EraseOriginFromOriginSet(&non_cached_unlimited_origins_by_host_,
                                 host, origin))
      non_cached_limited_origins_by_host_[host].insert(origin);
  }
}

void ClientUsageTracker::OnCleared() {
  DCHECK(CalledOnValidThread());
  global_limited_usage_ += global_unlimited_usage_;
  global_unlimited_usage_ = 0;

  for (OriginSetByHost::const_iterator host_itr =
           non_cached_unlimited_origins_by_host_.begin();
       host_itr != non_cached_unlimited_origins_by_host_.end();
       ++host_itr) {
    for (std::set<GURL>::const_iterator origin_itr = host_itr->second.begin();
         origin_itr != host_itr->second.end();
         ++origin_itr)
      non_cached_limited_origins_by_host_[host_itr->first].insert(*origin_itr);
  }
  non_cached_unlimited_origins_by_host_.clear();
}

bool ClientUsageTracker::IsStorageUnlimited(const GURL& origin) const {
  if (type_ == kStorageTypeSyncable)
    return false;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(origin);
}

}  // namespace quota
