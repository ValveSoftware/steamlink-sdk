// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_USAGE_TRACKER_H_
#define WEBKIT_BROWSER_QUOTA_USAGE_TRACKER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_callbacks.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/quota/quota_task.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/quota/quota_types.h"

namespace quota {

class ClientUsageTracker;
class StorageMonitor;

// A helper class that gathers and tracks the amount of data stored in
// all quota clients.
// An instance of this class is created per storage type.
class WEBKIT_STORAGE_BROWSER_EXPORT UsageTracker : public QuotaTaskObserver {
 public:
  UsageTracker(const QuotaClientList& clients, StorageType type,
               SpecialStoragePolicy* special_storage_policy,
               StorageMonitor* storage_monitor);
  virtual ~UsageTracker();

  StorageType type() const { return type_; }
  ClientUsageTracker* GetClientTracker(QuotaClient::ID client_id);

  void GetGlobalLimitedUsage(const UsageCallback& callback);
  void GetGlobalUsage(const GlobalUsageCallback& callback);
  void GetHostUsage(const std::string& host, const UsageCallback& callback);
  void UpdateUsageCache(QuotaClient::ID client_id,
                        const GURL& origin,
                        int64 delta);
  void GetCachedHostsUsage(std::map<std::string, int64>* host_usage) const;
  void GetCachedOrigins(std::set<GURL>* origins) const;
  bool IsWorking() const {
    return global_usage_callbacks_.HasCallbacks() ||
           host_usage_callbacks_.HasAnyCallbacks();
  }

  void SetUsageCacheEnabled(QuotaClient::ID client_id,
                            const GURL& origin,
                            bool enabled);

 private:
  struct AccumulateInfo {
    AccumulateInfo() : pending_clients(0), usage(0), unlimited_usage(0) {}
    int pending_clients;
    int64 usage;
    int64 unlimited_usage;
  };

  typedef std::map<QuotaClient::ID, ClientUsageTracker*> ClientTrackerMap;

  friend class ClientUsageTracker;
  void AccumulateClientGlobalLimitedUsage(AccumulateInfo* info,
                                          int64 limited_usage);
  void AccumulateClientGlobalUsage(AccumulateInfo* info,
                                   int64 usage,
                                   int64 unlimited_usage);
  void AccumulateClientHostUsage(AccumulateInfo* info,
                                 const std::string& host,
                                 int64 usage);

  const StorageType type_;
  ClientTrackerMap client_tracker_map_;

  UsageCallbackQueue global_limited_usage_callbacks_;
  GlobalUsageCallbackQueue global_usage_callbacks_;
  HostUsageCallbackMap host_usage_callbacks_;

  StorageMonitor* storage_monitor_;

  base::WeakPtrFactory<UsageTracker> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(UsageTracker);
};

// This class holds per-client usage tracking information and caches per-host
// usage data.  An instance of this class is created per client.
class ClientUsageTracker : public SpecialStoragePolicy::Observer,
                           public base::NonThreadSafe,
                           public base::SupportsWeakPtr<ClientUsageTracker> {
 public:
  typedef base::Callback<void(int64 limited_usage,
                              int64 unlimited_usage)> HostUsageAccumulator;
  typedef base::Callback<void(const GURL& origin,
                              int64 usage)> OriginUsageAccumulator;
  typedef std::map<std::string, std::set<GURL> > OriginSetByHost;

  ClientUsageTracker(UsageTracker* tracker,
                     QuotaClient* client,
                     StorageType type,
                     SpecialStoragePolicy* special_storage_policy,
                     StorageMonitor* storage_monitor);
  virtual ~ClientUsageTracker();

  void GetGlobalLimitedUsage(const UsageCallback& callback);
  void GetGlobalUsage(const GlobalUsageCallback& callback);
  void GetHostUsage(const std::string& host, const UsageCallback& callback);
  void UpdateUsageCache(const GURL& origin, int64 delta);
  void GetCachedHostsUsage(std::map<std::string, int64>* host_usage) const;
  void GetCachedOrigins(std::set<GURL>* origins) const;
  int64 GetCachedOriginsUsage(const std::set<GURL>& origins,
                              std::vector<GURL>* origins_not_in_cache);
  bool IsUsageCacheEnabledForOrigin(const GURL& origin) const;
  void SetUsageCacheEnabled(const GURL& origin, bool enabled);

 private:
  typedef CallbackQueueMap<HostUsageAccumulator, std::string,
                           Tuple2<int64, int64> > HostUsageAccumulatorMap;

  typedef std::set<std::string> HostSet;
  typedef std::map<GURL, int64> UsageMap;
  typedef std::map<std::string, UsageMap> HostUsageMap;

  struct AccumulateInfo {
    int pending_jobs;
    int64 limited_usage;
    int64 unlimited_usage;

    AccumulateInfo()
        : pending_jobs(0), limited_usage(0), unlimited_usage(0) {}
  };

  void AccumulateLimitedOriginUsage(AccumulateInfo* info,
                                    const UsageCallback& callback,
                                    int64 usage);
  void DidGetOriginsForGlobalUsage(const GlobalUsageCallback& callback,
                                   const std::set<GURL>& origins);
  void AccumulateHostUsage(AccumulateInfo* info,
                           const GlobalUsageCallback& callback,
                           int64 limited_usage,
                           int64 unlimited_usage);

  void DidGetOriginsForHostUsage(const std::string& host,
                                 const std::set<GURL>& origins);

  void GetUsageForOrigins(const std::string& host,
                          const std::set<GURL>& origins);
  void AccumulateOriginUsage(AccumulateInfo* info,
                             const std::string& host,
                             const GURL& origin,
                             int64 usage);

  void DidGetHostUsageAfterUpdate(const GURL& origin, int64 usage);

  // Methods used by our GatherUsage tasks, as a task makes progress
  // origins and hosts are added incrementally to the cache.
  void AddCachedOrigin(const GURL& origin, int64 usage);
  void AddCachedHost(const std::string& host);

  int64 GetCachedHostUsage(const std::string& host) const;
  int64 GetCachedGlobalUnlimitedUsage();
  bool GetCachedOriginUsage(const GURL& origin, int64* usage) const;

  // SpecialStoragePolicy::Observer overrides
  virtual void OnGranted(const GURL& origin, int change_flags) OVERRIDE;
  virtual void OnRevoked(const GURL& origin, int change_flags) OVERRIDE;
  virtual void OnCleared() OVERRIDE;

  bool IsStorageUnlimited(const GURL& origin) const;

  UsageTracker* tracker_;
  QuotaClient* client_;
  const StorageType type_;
  StorageMonitor* storage_monitor_;

  int64 global_limited_usage_;
  int64 global_unlimited_usage_;
  bool global_usage_retrieved_;
  HostSet cached_hosts_;
  HostUsageMap cached_usage_by_host_;

  OriginSetByHost non_cached_limited_origins_by_host_;
  OriginSetByHost non_cached_unlimited_origins_by_host_;

  GlobalUsageCallbackQueue global_usage_callback_;
  HostUsageAccumulatorMap host_usage_accumulators_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(ClientUsageTracker);
};

}  // namespace quota

#endif  // WEBKIT_BROWSER_QUOTA_USAGE_TRACKER_H_
