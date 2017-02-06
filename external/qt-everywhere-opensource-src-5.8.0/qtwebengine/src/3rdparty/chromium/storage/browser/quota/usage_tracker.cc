// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/usage_tracker.h"

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/stl_util.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/storage_monitor.h"

namespace storage {

namespace {

void DidGetGlobalUsageForLimitedGlobalUsage(const UsageCallback& callback,
                                            int64_t total_global_usage,
                                            int64_t global_unlimited_usage) {
  callback.Run(total_global_usage - global_unlimited_usage);
}

}  // namespace

UsageTracker::UsageTracker(const QuotaClientList& clients,
                           StorageType type,
                           SpecialStoragePolicy* special_storage_policy,
                           StorageMonitor* storage_monitor)
    : type_(type),
      storage_monitor_(storage_monitor),
      weak_factory_(this) {
  for (const auto& client : clients) {
    if (client->DoesSupport(type)) {
      client_tracker_map_[client->id()] =
          new ClientUsageTracker(this, client, type, special_storage_policy,
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
  return nullptr;
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

  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetGlobalLimitedUsage(accumulator);

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

  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetGlobalUsage(accumulator);

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

  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetHostUsage(host, accumulator);

  // Fire the sentinel as we've now called GetHostUsage for all clients.
  accumulator.Run(0);
}

void UsageTracker::UpdateUsageCache(QuotaClient::ID client_id,
                                    const GURL& origin,
                                    int64_t delta) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);
  client_tracker->UpdateUsageCache(origin, delta);
}

void UsageTracker::GetCachedHostsUsage(
    std::map<std::string, int64_t>* host_usage) const {
  DCHECK(host_usage);
  host_usage->clear();
  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetCachedHostsUsage(host_usage);
}

void UsageTracker::GetCachedOriginsUsage(
    std::map<GURL, int64_t>* origin_usage) const {
  DCHECK(origin_usage);
  origin_usage->clear();
  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetCachedOriginsUsage(origin_usage);
}

void UsageTracker::GetCachedOrigins(std::set<GURL>* origins) const {
  DCHECK(origins);
  origins->clear();
  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetCachedOrigins(origins);
}

void UsageTracker::SetUsageCacheEnabled(QuotaClient::ID client_id,
                                        const GURL& origin,
                                        bool enabled) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);

  client_tracker->SetUsageCacheEnabled(origin, enabled);
}

void UsageTracker::AccumulateClientGlobalLimitedUsage(AccumulateInfo* info,
                                                      int64_t limited_usage) {
  info->usage += limited_usage;
  if (--info->pending_clients)
    return;

  // All the clients have returned their usage data.  Dispatch the
  // pending callbacks.
  global_limited_usage_callbacks_.Run(info->usage);
}

void UsageTracker::AccumulateClientGlobalUsage(AccumulateInfo* info,
                                               int64_t usage,
                                               int64_t unlimited_usage) {
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
  global_usage_callbacks_.Run(info->usage, info->unlimited_usage);
}

void UsageTracker::AccumulateClientHostUsage(AccumulateInfo* info,
                                             const std::string& host,
                                             int64_t usage) {
  info->usage += usage;
  if (--info->pending_clients)
    return;

  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  // All the clients have returned their usage data.  Dispatch the
  // pending callbacks.
  host_usage_callbacks_.Run(host, info->usage);
}

}  // namespace storage
