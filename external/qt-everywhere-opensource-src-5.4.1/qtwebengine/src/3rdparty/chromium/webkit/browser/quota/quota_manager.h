// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_QUOTA_MANAGER_H_
#define WEBKIT_BROWSER_QUOTA_QUOTA_MANAGER_H_

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/browser/quota/quota_callbacks.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/quota/quota_database.h"
#include "webkit/browser/quota/quota_task.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/browser/quota/storage_observer.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace quota_internals {
class QuotaInternalsProxy;
}

namespace content {
class MockQuotaManager;
class MockStorageClient;
class QuotaManagerTest;
class StorageMonitorTest;

}

namespace quota {

class QuotaDatabase;
class QuotaManagerProxy;
class QuotaTemporaryStorageEvictor;
class StorageMonitor;
class UsageTracker;

struct QuotaManagerDeleter;

struct WEBKIT_STORAGE_BROWSER_EXPORT UsageAndQuota {
  int64 usage;
  int64 global_limited_usage;
  int64 quota;
  int64 available_disk_space;

  UsageAndQuota();
  UsageAndQuota(int64 usage,
                int64 global_limited_usage,
                int64 quota,
                int64 available_disk_space);
};

// An interface called by QuotaTemporaryStorageEvictor.
class WEBKIT_STORAGE_BROWSER_EXPORT QuotaEvictionHandler {
 public:
  typedef base::Callback<void(const GURL&)> GetLRUOriginCallback;
  typedef StatusCallback EvictOriginDataCallback;
  typedef base::Callback<void(QuotaStatusCode status,
                              const UsageAndQuota& usage_and_quota)>
      UsageAndQuotaCallback;

  // Returns the least recently used origin.  It might return empty
  // GURL when there are no evictable origins.
  virtual void GetLRUOrigin(
      StorageType type,
      const GetLRUOriginCallback& callback) = 0;

  virtual void EvictOriginData(
      const GURL& origin,
      StorageType type,
      const EvictOriginDataCallback& callback) = 0;

  virtual void GetUsageAndQuotaForEviction(
      const UsageAndQuotaCallback& callback) = 0;

 protected:
  virtual ~QuotaEvictionHandler() {}
};

struct UsageInfo {
  UsageInfo(const std::string& host, StorageType type, int64 usage)
      : host(host),
        type(type),
        usage(usage) {}
  std::string host;
  StorageType type;
  int64 usage;
};

// The quota manager class.  This class is instantiated per profile and
// held by the profile.  With the exception of the constructor and the
// proxy() method, all methods should only be called on the IO thread.
class WEBKIT_STORAGE_BROWSER_EXPORT QuotaManager
    : public QuotaTaskObserver,
      public QuotaEvictionHandler,
      public base::RefCountedThreadSafe<QuotaManager, QuotaManagerDeleter> {
 public:
  typedef base::Callback<void(QuotaStatusCode,
                              int64 /* usage */,
                              int64 /* quota */)>
      GetUsageAndQuotaCallback;

  static const int64 kIncognitoDefaultQuotaLimit;
  static const int64 kNoLimit;

  QuotaManager(bool is_incognito,
               const base::FilePath& profile_path,
               base::SingleThreadTaskRunner* io_thread,
               base::SequencedTaskRunner* db_thread,
               SpecialStoragePolicy* special_storage_policy);

  // Returns a proxy object that can be used on any thread.
  QuotaManagerProxy* proxy() { return proxy_.get(); }

  // Called by clients or webapps. Returns usage per host.
  void GetUsageInfo(const GetUsageInfoCallback& callback);

  // Called by Web Apps.
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuotaForWebApps(
      const GURL& origin,
      StorageType type,
      const GetUsageAndQuotaCallback& callback);

  // Called by StorageClients.
  // This method is declared as virtual to allow test code to override it.
  //
  // For UnlimitedStorage origins, this version skips usage and quota handling
  // to avoid extra query cost.
  // Do not call this method for apps/user-facing code.
  virtual void GetUsageAndQuota(
      const GURL& origin,
      StorageType type,
      const GetUsageAndQuotaCallback& callback);

  // Called by clients via proxy.
  // Client storage should call this method when storage is accessed.
  // Used to maintain LRU ordering.
  void NotifyStorageAccessed(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType type);

  // Called by clients via proxy.
  // Client storage must call this method whenever they have made any
  // modifications that change the amount of data stored in their storage.
  void NotifyStorageModified(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType type,
                             int64 delta);

  // Used to avoid evicting origins with open pages.
  // A call to NotifyOriginInUse must be balanced by a later call
  // to NotifyOriginNoLongerInUse.
  void NotifyOriginInUse(const GURL& origin);
  void NotifyOriginNoLongerInUse(const GURL& origin);
  bool IsOriginInUse(const GURL& origin) const {
    return origins_in_use_.find(origin) != origins_in_use_.end();
  }

  void SetUsageCacheEnabled(QuotaClient::ID client_id,
                            const GURL& origin,
                            StorageType type,
                            bool enabled);

  // DeleteOriginData and DeleteHostData (surprisingly enough) delete data of a
  // particular StorageType associated with either a specific origin or set of
  // origins. Each method additionally requires a |quota_client_mask| which
  // specifies the types of QuotaClients to delete from the origin. This is
  // specified by the caller as a bitmask built from QuotaClient::IDs. Setting
  // the mask to QuotaClient::kAllClientsMask will remove all clients from the
  // origin, regardless of type.
  virtual void DeleteOriginData(const GURL& origin,
                                StorageType type,
                                int quota_client_mask,
                                const StatusCallback& callback);
  void DeleteHostData(const std::string& host,
                      StorageType type,
                      int quota_client_mask,
                      const StatusCallback& callback);

  // Called by UI and internal modules.
  void GetAvailableSpace(const AvailableSpaceCallback& callback);
  void GetTemporaryGlobalQuota(const QuotaCallback& callback);

  // Ok to call with NULL callback.
  void SetTemporaryGlobalOverrideQuota(int64 new_quota,
                                       const QuotaCallback& callback);

  void GetPersistentHostQuota(const std::string& host,
                              const QuotaCallback& callback);
  void SetPersistentHostQuota(const std::string& host,
                              int64 new_quota,
                              const QuotaCallback& callback);
  void GetGlobalUsage(StorageType type, const GlobalUsageCallback& callback);
  void GetHostUsage(const std::string& host, StorageType type,
                    const UsageCallback& callback);
  void GetHostUsage(const std::string& host, StorageType type,
                    QuotaClient::ID client_id,
                    const UsageCallback& callback);

  bool IsTrackingHostUsage(StorageType type, QuotaClient::ID client_id) const;

  void GetStatistics(std::map<std::string, std::string>* statistics);

  bool IsStorageUnlimited(const GURL& origin, StorageType type) const;

  bool CanQueryDiskSize(const GURL& origin) const {
    return special_storage_policy_.get() &&
           special_storage_policy_->CanQueryDiskSize(origin);
  }

  virtual void GetOriginsModifiedSince(StorageType type,
                                       base::Time modified_since,
                                       const GetOriginsCallback& callback);

  bool ResetUsageTracker(StorageType type);

  // Used to register/deregister observers that wish to monitor storage events.
  void AddStorageObserver(StorageObserver* observer,
                          const StorageObserver::MonitorParams& params);
  void RemoveStorageObserver(StorageObserver* observer);
  void RemoveStorageObserverForFilter(StorageObserver* observer,
                                      const StorageObserver::Filter& filter);

  // Determines the portion of the temp pool that can be
  // utilized by a single host (ie. 5 for 20%).
  static const int kPerHostTemporaryPortion;

  static const int64 kPerHostPersistentQuotaLimit;

  static const char kDatabaseName[];

  static const int kThresholdOfErrorsToBeBlacklisted;

  static const int kEvictionIntervalInMilliSeconds;

  // These are kept non-const so that test code can change the value.
  // TODO(kinuko): Make this a real const value and add a proper way to set
  // the quota for syncable storage. (http://crbug.com/155488)
  static int64 kMinimumPreserveForSystem;
  static int64 kSyncableStorageDefaultHostQuota;

 protected:
  virtual ~QuotaManager();

 private:
  friend class base::DeleteHelper<QuotaManager>;
  friend class base::RefCountedThreadSafe<QuotaManager, QuotaManagerDeleter>;
  friend class content::QuotaManagerTest;
  friend class content::StorageMonitorTest;
  friend class content::MockQuotaManager;
  friend class content::MockStorageClient;
  friend class quota_internals::QuotaInternalsProxy;
  friend class QuotaManagerProxy;
  friend class QuotaTemporaryStorageEvictor;
  friend struct QuotaManagerDeleter;

  class GetUsageInfoTask;

  class OriginDataDeleter;
  class HostDataDeleter;

  class GetModifiedSinceHelper;
  class DumpQuotaTableHelper;
  class DumpOriginInfoTableHelper;

  typedef QuotaDatabase::QuotaTableEntry QuotaTableEntry;
  typedef QuotaDatabase::OriginInfoTableEntry OriginInfoTableEntry;
  typedef std::vector<QuotaTableEntry> QuotaTableEntries;
  typedef std::vector<OriginInfoTableEntry> OriginInfoTableEntries;

  // Function pointer type used to store the function which returns the
  // available disk space for the disk containing the given FilePath.
  typedef int64 (*GetAvailableDiskSpaceFn)(const base::FilePath&);

  typedef base::Callback<void(const QuotaTableEntries&)>
      DumpQuotaTableCallback;
  typedef base::Callback<void(const OriginInfoTableEntries&)>
      DumpOriginInfoTableCallback;

  struct EvictionContext {
    EvictionContext();
    virtual ~EvictionContext();
    GURL evicted_origin;
    StorageType evicted_type;

    EvictOriginDataCallback evict_origin_data_callback;
  };

  typedef QuotaEvictionHandler::UsageAndQuotaCallback
      UsageAndQuotaDispatcherCallback;

  // This initialization method is lazily called on the IO thread
  // when the first quota manager API is called.
  // Initialize must be called after all quota clients are added to the
  // manager by RegisterStorage.
  void LazyInitialize();

  // Called by clients via proxy.
  // Registers a quota client to the manager.
  // The client must remain valid until OnQuotaManagerDestored is called.
  void RegisterClient(QuotaClient* client);

  UsageTracker* GetUsageTracker(StorageType type) const;

  // Extract cached origins list from the usage tracker.
  // (Might return empty list if no origin is tracked by the tracker.)
  void GetCachedOrigins(StorageType type, std::set<GURL>* origins);

  // These internal methods are separately defined mainly for testing.
  void NotifyStorageAccessedInternal(
      QuotaClient::ID client_id,
      const GURL& origin,
      StorageType type,
      base::Time accessed_time);
  void NotifyStorageModifiedInternal(
      QuotaClient::ID client_id,
      const GURL& origin,
      StorageType type,
      int64 delta,
      base::Time modified_time);

  void DumpQuotaTable(const DumpQuotaTableCallback& callback);
  void DumpOriginInfoTable(const DumpOriginInfoTableCallback& callback);

  // Methods for eviction logic.
  void StartEviction();
  void DeleteOriginFromDatabase(const GURL& origin, StorageType type);

  void DidOriginDataEvicted(QuotaStatusCode status);

  void ReportHistogram();
  void DidGetTemporaryGlobalUsageForHistogram(int64 usage,
                                              int64 unlimited_usage);
  void DidGetPersistentGlobalUsageForHistogram(int64 usage,
                                               int64 unlimited_usage);

  // QuotaEvictionHandler.
  virtual void GetLRUOrigin(
      StorageType type,
      const GetLRUOriginCallback& callback) OVERRIDE;
  virtual void EvictOriginData(
      const GURL& origin,
      StorageType type,
      const EvictOriginDataCallback& callback) OVERRIDE;
  virtual void GetUsageAndQuotaForEviction(
      const UsageAndQuotaCallback& callback) OVERRIDE;

  void DidSetTemporaryGlobalOverrideQuota(const QuotaCallback& callback,
                                          const int64* new_quota,
                                          bool success);
  void DidGetPersistentHostQuota(const std::string& host,
                                 const int64* quota,
                                 bool success);
  void DidSetPersistentHostQuota(const std::string& host,
                                 const QuotaCallback& callback,
                                 const int64* new_quota,
                                 bool success);
  void DidInitialize(int64* temporary_quota_override,
                     int64* desired_available_space,
                     bool success);
  void DidGetLRUOrigin(const GURL* origin,
                       bool success);
  void DidGetInitialTemporaryGlobalQuota(QuotaStatusCode status,
                                         int64 quota_unused);
  void DidInitializeTemporaryOriginsInfo(bool success);
  void DidGetAvailableSpace(int64 space);
  void DidDatabaseWork(bool success);

  void DeleteOnCorrectThread() const;

  void PostTaskAndReplyWithResultForDBThread(
      const tracked_objects::Location& from_here,
      const base::Callback<bool(QuotaDatabase*)>& task,
      const base::Callback<void(bool)>& reply);

  const bool is_incognito_;
  const base::FilePath profile_path_;

  scoped_refptr<QuotaManagerProxy> proxy_;
  bool db_disabled_;
  bool eviction_disabled_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> db_thread_;
  mutable scoped_ptr<QuotaDatabase> database_;

  GetLRUOriginCallback lru_origin_callback_;
  std::set<GURL> access_notified_origins_;

  QuotaClientList clients_;

  scoped_ptr<UsageTracker> temporary_usage_tracker_;
  scoped_ptr<UsageTracker> persistent_usage_tracker_;
  scoped_ptr<UsageTracker> syncable_usage_tracker_;
  // TODO(michaeln): Need a way to clear the cache, drop and
  // reinstantiate the trackers when they're not handling requests.

  scoped_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  EvictionContext eviction_context_;

  ClosureQueue db_initialization_callbacks_;
  AvailableSpaceCallbackQueue available_space_callbacks_;
  GlobalQuotaCallbackQueue temporary_global_quota_callbacks_;
  HostQuotaCallbackMap persistent_host_quota_callbacks_;

  bool temporary_quota_initialized_;
  int64 temporary_quota_override_;

  int64 desired_available_space_;

  // Map from origin to count.
  std::map<GURL, int> origins_in_use_;
  // Map from origin to error count.
  std::map<GURL, int> origins_in_error_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  base::RepeatingTimer<QuotaManager> histogram_timer_;

  // Pointer to the function used to get the available disk space. This is
  // overwritten by QuotaManagerTest in order to attain a deterministic reported
  // value. The default value points to base::SysInfo::AmountOfFreeDiskSpace.
  GetAvailableDiskSpaceFn get_disk_space_fn_;

  scoped_ptr<StorageMonitor> storage_monitor_;

  base::WeakPtrFactory<QuotaManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManager);
};

struct QuotaManagerDeleter {
  static void Destruct(const QuotaManager* manager) {
    manager->DeleteOnCorrectThread();
  }
};

}  // namespace quota

#endif  // WEBKIT_BROWSER_QUOTA_QUOTA_MANAGER_H_
