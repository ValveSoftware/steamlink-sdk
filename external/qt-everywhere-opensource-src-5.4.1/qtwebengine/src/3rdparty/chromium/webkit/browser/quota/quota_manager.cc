// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/quota/quota_manager.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "net/base/net_util.h"
#include "webkit/browser/quota/quota_database.h"
#include "webkit/browser/quota/quota_manager_proxy.h"
#include "webkit/browser/quota/quota_temporary_storage_evictor.h"
#include "webkit/browser/quota/storage_monitor.h"
#include "webkit/browser/quota/usage_tracker.h"
#include "webkit/common/quota/quota_types.h"

#define UMA_HISTOGRAM_MBYTES(name, sample)          \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                      \
      (name), static_cast<int>((sample) / kMBytes), \
      1, 10 * 1024 * 1024 /* 10TB */, 100)

namespace quota {

namespace {

const int64 kMBytes = 1024 * 1024;
const int kMinutesInMilliSeconds = 60 * 1000;

const int64 kReportHistogramInterval = 60 * 60 * 1000;  // 1 hour
const double kTemporaryQuotaRatioToAvail = 1.0 / 3.0;  // 33%

}  // namespace

// Arbitrary for now, but must be reasonably small so that
// in-memory databases can fit.
// TODO(kinuko): Refer SysInfo::AmountOfPhysicalMemory() to determine this.
const int64 QuotaManager::kIncognitoDefaultQuotaLimit = 100 * kMBytes;

const int64 QuotaManager::kNoLimit = kint64max;

const int QuotaManager::kPerHostTemporaryPortion = 5;  // 20%

// Cap size for per-host persistent quota determined by the histogram.
// This is a bit lax value because the histogram says nothing about per-host
// persistent storage usage and we determined by global persistent storage
// usage that is less than 10GB for almost all users.
const int64 QuotaManager::kPerHostPersistentQuotaLimit = 10 * 1024 * kMBytes;

const char QuotaManager::kDatabaseName[] = "QuotaManager";

const int QuotaManager::kThresholdOfErrorsToBeBlacklisted = 3;

// Preserve kMinimumPreserveForSystem disk space for system book-keeping
// when returning the quota to unlimited apps/extensions.
// TODO(kinuko): This should be like 10% of the actual disk space.
// For now we simply use a constant as getting the disk size needs
// platform-dependent code. (http://crbug.com/178976)
int64 QuotaManager::kMinimumPreserveForSystem = 1024 * kMBytes;

const int QuotaManager::kEvictionIntervalInMilliSeconds =
    30 * kMinutesInMilliSeconds;

// Heuristics: assuming average cloud server allows a few Gigs storage
// on the server side and the storage needs to be shared for user data
// and by multiple apps.
int64 QuotaManager::kSyncableStorageDefaultHostQuota = 500 * kMBytes;

namespace {

void CountOriginType(const std::set<GURL>& origins,
                     SpecialStoragePolicy* policy,
                     size_t* protected_origins,
                     size_t* unlimited_origins) {
  DCHECK(protected_origins);
  DCHECK(unlimited_origins);
  *protected_origins = 0;
  *unlimited_origins = 0;
  if (!policy)
    return;
  for (std::set<GURL>::const_iterator itr = origins.begin();
       itr != origins.end();
       ++itr) {
    if (policy->IsStorageProtected(*itr))
      ++*protected_origins;
    if (policy->IsStorageUnlimited(*itr))
      ++*unlimited_origins;
  }
}

bool SetTemporaryGlobalOverrideQuotaOnDBThread(int64* new_quota,
                                               QuotaDatabase* database) {
  DCHECK(database);
  if (!database->SetQuotaConfigValue(
          QuotaDatabase::kTemporaryQuotaOverrideKey, *new_quota)) {
    *new_quota = -1;
    return false;
  }
  return true;
}

bool GetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64* quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  database->GetHostQuota(host, kStorageTypePersistent, quota);
  return true;
}

bool SetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64* new_quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  if (database->SetHostQuota(host, kStorageTypePersistent, *new_quota))
    return true;
  *new_quota = 0;
  return false;
}

bool InitializeOnDBThread(int64* temporary_quota_override,
                          int64* desired_available_space,
                          QuotaDatabase* database) {
  DCHECK(database);
  database->GetQuotaConfigValue(QuotaDatabase::kTemporaryQuotaOverrideKey,
                                temporary_quota_override);
  database->GetQuotaConfigValue(QuotaDatabase::kDesiredAvailableSpaceKey,
                                desired_available_space);
  return true;
}

bool GetLRUOriginOnDBThread(StorageType type,
                            std::set<GURL>* exceptions,
                            SpecialStoragePolicy* policy,
                            GURL* url,
                            QuotaDatabase* database) {
  DCHECK(database);
  database->GetLRUOrigin(type, *exceptions, policy, url);
  return true;
}

bool DeleteOriginInfoOnDBThread(const GURL& origin,
                                StorageType type,
                                QuotaDatabase* database) {
  DCHECK(database);
  return database->DeleteOriginInfo(origin, type);
}

bool InitializeTemporaryOriginsInfoOnDBThread(const std::set<GURL>* origins,
                                              QuotaDatabase* database) {
  DCHECK(database);
  if (database->IsOriginDatabaseBootstrapped())
    return true;

  // Register existing origins with 0 last time access.
  if (database->RegisterInitialOriginInfo(*origins, kStorageTypeTemporary)) {
    database->SetOriginDatabaseBootstrapped(true);
    return true;
  }
  return false;
}

bool UpdateAccessTimeOnDBThread(const GURL& origin,
                                StorageType type,
                                base::Time accessed_time,
                                QuotaDatabase* database) {
  DCHECK(database);
  return database->SetOriginLastAccessTime(origin, type, accessed_time);
}

bool UpdateModifiedTimeOnDBThread(const GURL& origin,
                                  StorageType type,
                                  base::Time modified_time,
                                  QuotaDatabase* database) {
  DCHECK(database);
  return database->SetOriginLastModifiedTime(origin, type, modified_time);
}

int64 CallSystemGetAmountOfFreeDiskSpace(const base::FilePath& profile_path) {
  // Ensure the profile path exists.
  if (!base::CreateDirectory(profile_path)) {
    LOG(WARNING) << "Create directory failed for path" << profile_path.value();
    return 0;
  }
  return base::SysInfo::AmountOfFreeDiskSpace(profile_path);
}

int64 CalculateTemporaryGlobalQuota(int64 global_limited_usage,
                                    int64 available_space) {
  DCHECK_GE(global_limited_usage, 0);
  int64 avail_space = available_space;
  if (avail_space < kint64max - global_limited_usage) {
    // We basically calculate the temporary quota by
    // [available_space + space_used_for_temp] * kTempQuotaRatio,
    // but make sure we'll have no overflow.
    avail_space += global_limited_usage;
  }
  return avail_space * kTemporaryQuotaRatioToAvail;
}

void DispatchTemporaryGlobalQuotaCallback(
    const QuotaCallback& callback,
    QuotaStatusCode status,
    const UsageAndQuota& usage_and_quota) {
  if (status != kQuotaStatusOk) {
    callback.Run(status, 0);
    return;
  }

  callback.Run(status, CalculateTemporaryGlobalQuota(
      usage_and_quota.global_limited_usage,
      usage_and_quota.available_disk_space));
}

int64 CalculateQuotaWithDiskSpace(
    int64 available_disk_space, int64 usage, int64 quota) {
  if (available_disk_space < QuotaManager::kMinimumPreserveForSystem) {
    LOG(WARNING)
        << "Running out of disk space for profile."
        << " QuotaManager starts forbidding further quota consumption.";
    return usage;
  }

  if (quota < usage) {
    // No more space; cap the quota to the current usage.
    return usage;
  }

  available_disk_space -= QuotaManager::kMinimumPreserveForSystem;
  if (available_disk_space < quota - usage)
    return available_disk_space + usage;

  return quota;
}

int64 CalculateTemporaryHostQuota(int64 host_usage,
                                  int64 global_quota,
                                  int64 global_limited_usage) {
  DCHECK_GE(global_limited_usage, 0);
  int64 host_quota = global_quota / QuotaManager::kPerHostTemporaryPortion;
  if (global_limited_usage > global_quota)
    host_quota = std::min(host_quota, host_usage);
  return host_quota;
}

void DispatchUsageAndQuotaForWebApps(
    StorageType type,
    bool is_incognito,
    bool is_unlimited,
    bool can_query_disk_size,
    const QuotaManager::GetUsageAndQuotaCallback& callback,
    QuotaStatusCode status,
    const UsageAndQuota& usage_and_quota) {
  if (status != kQuotaStatusOk) {
    callback.Run(status, 0, 0);
    return;
  }

  int64 usage = usage_and_quota.usage;
  int64 quota = usage_and_quota.quota;

  if (type == kStorageTypeTemporary && !is_unlimited) {
    quota = CalculateTemporaryHostQuota(
        usage, quota, usage_and_quota.global_limited_usage);
  }

  if (is_incognito) {
    quota = std::min(quota, QuotaManager::kIncognitoDefaultQuotaLimit);
    callback.Run(status, usage, quota);
    return;
  }

  // For apps with unlimited permission or can_query_disk_size is true (and not
  // in incognito mode).
  // We assume we can expose the actual disk size for them and cap the quota by
  // the available disk space.
  if (is_unlimited || can_query_disk_size) {
    callback.Run(
        status, usage,
        CalculateQuotaWithDiskSpace(
            usage_and_quota.available_disk_space,
            usage, quota));
    return;
  }

  callback.Run(status, usage, quota);
}

}  // namespace

UsageAndQuota::UsageAndQuota()
    : usage(0),
      global_limited_usage(0),
      quota(0),
      available_disk_space(0) {
}

UsageAndQuota::UsageAndQuota(
    int64 usage,
    int64 global_limited_usage,
    int64 quota,
    int64 available_disk_space)
    : usage(usage),
      global_limited_usage(global_limited_usage),
      quota(quota),
      available_disk_space(available_disk_space) {
}

class UsageAndQuotaCallbackDispatcher
    : public QuotaTask,
      public base::SupportsWeakPtr<UsageAndQuotaCallbackDispatcher> {
 public:
  explicit UsageAndQuotaCallbackDispatcher(QuotaManager* manager)
      : QuotaTask(manager),
        has_usage_(false),
        has_global_limited_usage_(false),
        has_quota_(false),
        has_available_disk_space_(false),
        status_(kQuotaStatusUnknown),
        usage_and_quota_(-1, -1, -1, -1),
        waiting_callbacks_(1) {}

  virtual ~UsageAndQuotaCallbackDispatcher() {}

  void WaitForResults(const QuotaManager::UsageAndQuotaCallback& callback) {
    callback_ = callback;
    Start();
  }

  void set_usage(int64 usage) {
    usage_and_quota_.usage = usage;
    has_usage_ = true;
  }

  void set_global_limited_usage(int64 global_limited_usage) {
    usage_and_quota_.global_limited_usage = global_limited_usage;
    has_global_limited_usage_ = true;
  }

  void set_quota(int64 quota) {
    usage_and_quota_.quota = quota;
    has_quota_ = true;
  }

  void set_available_disk_space(int64 available_disk_space) {
    usage_and_quota_.available_disk_space = available_disk_space;
    has_available_disk_space_ = true;
  }

  UsageCallback GetHostUsageCallback() {
    ++waiting_callbacks_;
    has_usage_ = true;
    return base::Bind(&UsageAndQuotaCallbackDispatcher::DidGetHostUsage,
                      AsWeakPtr());
  }

  UsageCallback GetGlobalLimitedUsageCallback() {
    ++waiting_callbacks_;
    has_global_limited_usage_ = true;
    return base::Bind(
        &UsageAndQuotaCallbackDispatcher::DidGetGlobalLimitedUsage,
        AsWeakPtr());
  }

  QuotaCallback GetQuotaCallback() {
    ++waiting_callbacks_;
    has_quota_ = true;
    return base::Bind(&UsageAndQuotaCallbackDispatcher::DidGetQuota,
                      AsWeakPtr());
  }

  QuotaCallback GetAvailableSpaceCallback() {
    ++waiting_callbacks_;
    has_available_disk_space_ = true;
    return base::Bind(&UsageAndQuotaCallbackDispatcher::DidGetAvailableSpace,
                      AsWeakPtr());
  }

 private:
  void DidGetHostUsage(int64 usage) {
    if (status_ == kQuotaStatusUnknown)
      status_ = kQuotaStatusOk;
    usage_and_quota_.usage = usage;
    CheckCompleted();
  }

  void DidGetGlobalLimitedUsage(int64 limited_usage) {
    if (status_ == kQuotaStatusUnknown)
      status_ = kQuotaStatusOk;
    usage_and_quota_.global_limited_usage = limited_usage;
    CheckCompleted();
  }

  void DidGetQuota(QuotaStatusCode status, int64 quota) {
    if (status_ == kQuotaStatusUnknown || status_ == kQuotaStatusOk)
      status_ = status;
    usage_and_quota_.quota = quota;
    CheckCompleted();
  }

  void DidGetAvailableSpace(QuotaStatusCode status, int64 space) {
    DCHECK_GE(space, 0);
    if (status_ == kQuotaStatusUnknown || status_ == kQuotaStatusOk)
      status_ = status;
    usage_and_quota_.available_disk_space = space;
    CheckCompleted();
  }

  virtual void Run() OVERRIDE {
    // We initialize waiting_callbacks to 1 so that we won't run
    // the completion callback until here even some of the callbacks
    // are dispatched synchronously.
    CheckCompleted();
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(kQuotaErrorAbort, UsageAndQuota());
    DeleteSoon();
  }

  virtual void Completed() OVERRIDE {
    DCHECK(!has_usage_ || usage_and_quota_.usage >= 0);
    DCHECK(!has_global_limited_usage_ ||
           usage_and_quota_.global_limited_usage >= 0);
    DCHECK(!has_quota_ || usage_and_quota_.quota >= 0);
    DCHECK(!has_available_disk_space_ ||
           usage_and_quota_.available_disk_space >= 0);

    callback_.Run(status_, usage_and_quota_);
    DeleteSoon();
  }

  void CheckCompleted() {
    if (--waiting_callbacks_ <= 0)
      CallCompleted();
  }

  // For sanity checks, they're checked only when DCHECK is on.
  bool has_usage_;
  bool has_global_limited_usage_;
  bool has_quota_;
  bool has_available_disk_space_;

  QuotaStatusCode status_;
  UsageAndQuota usage_and_quota_;
  QuotaManager::UsageAndQuotaCallback callback_;
  int waiting_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(UsageAndQuotaCallbackDispatcher);
};

class QuotaManager::GetUsageInfoTask : public QuotaTask {
 public:
  GetUsageInfoTask(
      QuotaManager* manager,
      const GetUsageInfoCallback& callback)
      : QuotaTask(manager),
        callback_(callback),
        weak_factory_(this) {
  }

 protected:
  virtual void Run() OVERRIDE {
    remaining_trackers_ = 3;
    // This will populate cached hosts and usage info.
    manager()->GetUsageTracker(kStorageTypeTemporary)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr(),
                   kStorageTypeTemporary));
    manager()->GetUsageTracker(kStorageTypePersistent)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr(),
                   kStorageTypePersistent));
    manager()->GetUsageTracker(kStorageTypeSyncable)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr(),
                   kStorageTypeSyncable));
  }

  virtual void Completed() OVERRIDE {
    callback_.Run(entries_);
    DeleteSoon();
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(UsageInfoEntries());
    DeleteSoon();
  }

 private:
  void AddEntries(StorageType type, UsageTracker* tracker) {
    std::map<std::string, int64> host_usage;
    tracker->GetCachedHostsUsage(&host_usage);
    for (std::map<std::string, int64>::const_iterator iter = host_usage.begin();
         iter != host_usage.end();
         ++iter) {
      entries_.push_back(UsageInfo(iter->first, type, iter->second));
    }
    if (--remaining_trackers_ == 0)
      CallCompleted();
  }

  void DidGetGlobalUsage(StorageType type, int64, int64) {
    DCHECK(manager()->GetUsageTracker(type));
    AddEntries(type, manager()->GetUsageTracker(type));
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  GetUsageInfoCallback callback_;
  UsageInfoEntries entries_;
  int remaining_trackers_;
  base::WeakPtrFactory<GetUsageInfoTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GetUsageInfoTask);
};

class QuotaManager::OriginDataDeleter : public QuotaTask {
 public:
  OriginDataDeleter(QuotaManager* manager,
                    const GURL& origin,
                    StorageType type,
                    int quota_client_mask,
                    const StatusCallback& callback)
      : QuotaTask(manager),
        origin_(origin),
        type_(type),
        quota_client_mask_(quota_client_mask),
        error_count_(0),
        remaining_clients_(-1),
        skipped_clients_(0),
        callback_(callback),
        weak_factory_(this) {}

 protected:
  virtual void Run() OVERRIDE {
    error_count_ = 0;
    remaining_clients_ = manager()->clients_.size();
    for (QuotaClientList::iterator iter = manager()->clients_.begin();
         iter != manager()->clients_.end(); ++iter) {
      if (quota_client_mask_ & (*iter)->id()) {
        (*iter)->DeleteOriginData(
            origin_, type_,
            base::Bind(&OriginDataDeleter::DidDeleteOriginData,
                       weak_factory_.GetWeakPtr()));
      } else {
        ++skipped_clients_;
        if (--remaining_clients_ == 0)
          CallCompleted();
      }
    }
  }

  virtual void Completed() OVERRIDE {
    if (error_count_ == 0) {
      // Only remove the entire origin if we didn't skip any client types.
      if (skipped_clients_ == 0)
        manager()->DeleteOriginFromDatabase(origin_, type_);
      callback_.Run(kQuotaStatusOk);
    } else {
      callback_.Run(kQuotaErrorInvalidModification);
    }
    DeleteSoon();
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(kQuotaErrorAbort);
    DeleteSoon();
  }

 private:
  void DidDeleteOriginData(QuotaStatusCode status) {
    DCHECK_GT(remaining_clients_, 0);

    if (status != kQuotaStatusOk)
      ++error_count_;

    if (--remaining_clients_ == 0)
      CallCompleted();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  GURL origin_;
  StorageType type_;
  int quota_client_mask_;
  int error_count_;
  int remaining_clients_;
  int skipped_clients_;
  StatusCallback callback_;

  base::WeakPtrFactory<OriginDataDeleter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(OriginDataDeleter);
};

class QuotaManager::HostDataDeleter : public QuotaTask {
 public:
  HostDataDeleter(QuotaManager* manager,
                  const std::string& host,
                  StorageType type,
                  int quota_client_mask,
                  const StatusCallback& callback)
      : QuotaTask(manager),
        host_(host),
        type_(type),
        quota_client_mask_(quota_client_mask),
        error_count_(0),
        remaining_clients_(-1),
        remaining_deleters_(-1),
        callback_(callback),
        weak_factory_(this) {}

 protected:
  virtual void Run() OVERRIDE {
    error_count_ = 0;
    remaining_clients_ = manager()->clients_.size();
    for (QuotaClientList::iterator iter = manager()->clients_.begin();
         iter != manager()->clients_.end(); ++iter) {
      (*iter)->GetOriginsForHost(
          type_, host_,
          base::Bind(&HostDataDeleter::DidGetOriginsForHost,
                     weak_factory_.GetWeakPtr()));
    }
  }

  virtual void Completed() OVERRIDE {
    if (error_count_ == 0) {
      callback_.Run(kQuotaStatusOk);
    } else {
      callback_.Run(kQuotaErrorInvalidModification);
    }
    DeleteSoon();
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(kQuotaErrorAbort);
    DeleteSoon();
  }

 private:
  void DidGetOriginsForHost(const std::set<GURL>& origins) {
    DCHECK_GT(remaining_clients_, 0);

    origins_.insert(origins.begin(), origins.end());

    if (--remaining_clients_ == 0) {
      if (!origins_.empty())
        ScheduleOriginsDeletion();
      else
        CallCompleted();
    }
  }

  void ScheduleOriginsDeletion() {
    remaining_deleters_ = origins_.size();
    for (std::set<GURL>::const_iterator p = origins_.begin();
         p != origins_.end();
         ++p) {
      OriginDataDeleter* deleter =
          new OriginDataDeleter(
              manager(), *p, type_, quota_client_mask_,
              base::Bind(&HostDataDeleter::DidDeleteOriginData,
                         weak_factory_.GetWeakPtr()));
      deleter->Start();
    }
  }

  void DidDeleteOriginData(QuotaStatusCode status) {
    DCHECK_GT(remaining_deleters_, 0);

    if (status != kQuotaStatusOk)
      ++error_count_;

    if (--remaining_deleters_ == 0)
      CallCompleted();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  std::string host_;
  StorageType type_;
  int quota_client_mask_;
  std::set<GURL> origins_;
  int error_count_;
  int remaining_clients_;
  int remaining_deleters_;
  StatusCallback callback_;

  base::WeakPtrFactory<HostDataDeleter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(HostDataDeleter);
};

class QuotaManager::GetModifiedSinceHelper {
 public:
  bool GetModifiedSinceOnDBThread(StorageType type,
                                  base::Time modified_since,
                                  QuotaDatabase* database) {
    DCHECK(database);
    return database->GetOriginsModifiedSince(type, &origins_, modified_since);
  }

  void DidGetModifiedSince(const base::WeakPtr<QuotaManager>& manager,
                           const GetOriginsCallback& callback,
                           StorageType type,
                           bool success) {
    if (!manager) {
      // The operation was aborted.
      callback.Run(std::set<GURL>(), type);
      return;
    }
    manager->DidDatabaseWork(success);
    callback.Run(origins_, type);
  }

 private:
  std::set<GURL> origins_;
};

class QuotaManager::DumpQuotaTableHelper {
 public:
  bool DumpQuotaTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpQuotaTable(
        base::Bind(&DumpQuotaTableHelper::AppendEntry, base::Unretained(this)));
  }

  void DidDumpQuotaTable(const base::WeakPtr<QuotaManager>& manager,
                         const DumpQuotaTableCallback& callback,
                         bool success) {
    if (!manager) {
      // The operation was aborted.
      callback.Run(QuotaTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    callback.Run(entries_);
  }

 private:
  bool AppendEntry(const QuotaTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  QuotaTableEntries entries_;
};

class QuotaManager::DumpOriginInfoTableHelper {
 public:
  bool DumpOriginInfoTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpOriginInfoTable(
        base::Bind(&DumpOriginInfoTableHelper::AppendEntry,
                   base::Unretained(this)));
  }

  void DidDumpOriginInfoTable(const base::WeakPtr<QuotaManager>& manager,
                              const DumpOriginInfoTableCallback& callback,
                              bool success) {
    if (!manager) {
      // The operation was aborted.
      callback.Run(OriginInfoTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    callback.Run(entries_);
  }

 private:
  bool AppendEntry(const OriginInfoTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  OriginInfoTableEntries entries_;
};

// QuotaManager ---------------------------------------------------------------

QuotaManager::QuotaManager(bool is_incognito,
                           const base::FilePath& profile_path,
                           base::SingleThreadTaskRunner* io_thread,
                           base::SequencedTaskRunner* db_thread,
                           SpecialStoragePolicy* special_storage_policy)
  : is_incognito_(is_incognito),
    profile_path_(profile_path),
    proxy_(new QuotaManagerProxy(
        this, io_thread)),
    db_disabled_(false),
    eviction_disabled_(false),
    io_thread_(io_thread),
    db_thread_(db_thread),
    temporary_quota_initialized_(false),
    temporary_quota_override_(-1),
    desired_available_space_(-1),
    special_storage_policy_(special_storage_policy),
    get_disk_space_fn_(&CallSystemGetAmountOfFreeDiskSpace),
    storage_monitor_(new StorageMonitor(this)),
    weak_factory_(this) {
}

void QuotaManager::GetUsageInfo(const GetUsageInfoCallback& callback) {
  LazyInitialize();
  GetUsageInfoTask* get_usage_info = new GetUsageInfoTask(this, callback);
  get_usage_info->Start();
}

void QuotaManager::GetUsageAndQuotaForWebApps(
    const GURL& origin,
    StorageType type,
    const GetUsageAndQuotaCallback& callback) {
  if (type != kStorageTypeTemporary &&
      type != kStorageTypePersistent &&
      type != kStorageTypeSyncable) {
    callback.Run(kQuotaErrorNotSupported, 0, 0);
    return;
  }

  DCHECK(origin == origin.GetOrigin());
  LazyInitialize();

  bool unlimited = IsStorageUnlimited(origin, type);
  bool can_query_disk_size = CanQueryDiskSize(origin);

  UsageAndQuotaCallbackDispatcher* dispatcher =
      new UsageAndQuotaCallbackDispatcher(this);

  UsageAndQuota usage_and_quota;
  if (unlimited) {
    dispatcher->set_quota(kNoLimit);
  } else {
    if (type == kStorageTypeTemporary) {
      GetUsageTracker(type)->GetGlobalLimitedUsage(
          dispatcher->GetGlobalLimitedUsageCallback());
      GetTemporaryGlobalQuota(dispatcher->GetQuotaCallback());
    } else if (type == kStorageTypePersistent) {
      GetPersistentHostQuota(net::GetHostOrSpecFromURL(origin),
                             dispatcher->GetQuotaCallback());
    } else {
      dispatcher->set_quota(kSyncableStorageDefaultHostQuota);
    }
  }

  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetHostUsage(net::GetHostOrSpecFromURL(origin),
                                      dispatcher->GetHostUsageCallback());

  if (!is_incognito_ && (unlimited || can_query_disk_size))
    GetAvailableSpace(dispatcher->GetAvailableSpaceCallback());

  dispatcher->WaitForResults(base::Bind(
      &DispatchUsageAndQuotaForWebApps,
      type, is_incognito_, unlimited, can_query_disk_size,
      callback));
}

void QuotaManager::GetUsageAndQuota(
    const GURL& origin, StorageType type,
    const GetUsageAndQuotaCallback& callback) {
  DCHECK(origin == origin.GetOrigin());

  if (IsStorageUnlimited(origin, type)) {
    callback.Run(kQuotaStatusOk, 0, kNoLimit);
    return;
  }

  GetUsageAndQuotaForWebApps(origin, type, callback);
}

void QuotaManager::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type) {
  DCHECK(origin == origin.GetOrigin());
  NotifyStorageAccessedInternal(client_id, origin, type, base::Time::Now());
}

void QuotaManager::NotifyStorageModified(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type, int64 delta) {
  DCHECK(origin == origin.GetOrigin());
  NotifyStorageModifiedInternal(client_id, origin, type, delta,
                                base::Time::Now());
}

void QuotaManager::NotifyOriginInUse(const GURL& origin) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  origins_in_use_[origin]++;
}

void QuotaManager::NotifyOriginNoLongerInUse(const GURL& origin) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(IsOriginInUse(origin));
  int& count = origins_in_use_[origin];
  if (--count == 0)
    origins_in_use_.erase(origin);
}

void QuotaManager::SetUsageCacheEnabled(QuotaClient::ID client_id,
                                        const GURL& origin,
                                        StorageType type,
                                        bool enabled) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->SetUsageCacheEnabled(client_id, origin, enabled);
}

void QuotaManager::DeleteOriginData(
    const GURL& origin, StorageType type, int quota_client_mask,
    const StatusCallback& callback) {
  LazyInitialize();

  if (origin.is_empty() || clients_.empty()) {
    callback.Run(kQuotaStatusOk);
    return;
  }

  DCHECK(origin == origin.GetOrigin());
  OriginDataDeleter* deleter =
      new OriginDataDeleter(this, origin, type, quota_client_mask, callback);
  deleter->Start();
}

void QuotaManager::DeleteHostData(const std::string& host,
                                  StorageType type,
                                  int quota_client_mask,
                                  const StatusCallback& callback) {
  LazyInitialize();

  if (host.empty() || clients_.empty()) {
    callback.Run(kQuotaStatusOk);
    return;
  }

  HostDataDeleter* deleter =
      new HostDataDeleter(this, host, type, quota_client_mask, callback);
  deleter->Start();
}

void QuotaManager::GetAvailableSpace(const AvailableSpaceCallback& callback) {
  if (!available_space_callbacks_.Add(callback))
    return;

  PostTaskAndReplyWithResult(db_thread_.get(),
                             FROM_HERE,
                             base::Bind(get_disk_space_fn_, profile_path_),
                             base::Bind(&QuotaManager::DidGetAvailableSpace,
                                        weak_factory_.GetWeakPtr()));
}

void QuotaManager::GetTemporaryGlobalQuota(const QuotaCallback& callback) {
  LazyInitialize();
  if (!temporary_quota_initialized_) {
    db_initialization_callbacks_.Add(base::Bind(
        &QuotaManager::GetTemporaryGlobalQuota,
        weak_factory_.GetWeakPtr(), callback));
    return;
  }

  if (temporary_quota_override_ > 0) {
    callback.Run(kQuotaStatusOk, temporary_quota_override_);
    return;
  }

  UsageAndQuotaCallbackDispatcher* dispatcher =
      new UsageAndQuotaCallbackDispatcher(this);
  GetUsageTracker(kStorageTypeTemporary)->
      GetGlobalLimitedUsage(dispatcher->GetGlobalLimitedUsageCallback());
  GetAvailableSpace(dispatcher->GetAvailableSpaceCallback());
  dispatcher->WaitForResults(
      base::Bind(&DispatchTemporaryGlobalQuotaCallback, callback));
}

void QuotaManager::SetTemporaryGlobalOverrideQuota(
    int64 new_quota, const QuotaCallback& callback) {
  LazyInitialize();

  if (new_quota < 0) {
    if (!callback.is_null())
      callback.Run(kQuotaErrorInvalidModification, -1);
    return;
  }

  if (db_disabled_) {
    if (!callback.is_null())
      callback.Run(kQuotaErrorInvalidAccess, -1);
    return;
  }

  int64* new_quota_ptr = new int64(new_quota);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&SetTemporaryGlobalOverrideQuotaOnDBThread,
                 base::Unretained(new_quota_ptr)),
      base::Bind(&QuotaManager::DidSetTemporaryGlobalOverrideQuota,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(new_quota_ptr)));
}

void QuotaManager::GetPersistentHostQuota(const std::string& host,
                                          const QuotaCallback& callback) {
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    // TODO(kinuko) We may want to respect --allow-file-access-from-files
    // command line switch.
    callback.Run(kQuotaStatusOk, 0);
    return;
  }

  if (!persistent_host_quota_callbacks_.Add(host, callback))
    return;

  int64* quota_ptr = new int64(0);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&GetPersistentHostQuotaOnDBThread,
                 host,
                 base::Unretained(quota_ptr)),
      base::Bind(&QuotaManager::DidGetPersistentHostQuota,
                 weak_factory_.GetWeakPtr(),
                 host,
                 base::Owned(quota_ptr)));
}

void QuotaManager::SetPersistentHostQuota(const std::string& host,
                                          int64 new_quota,
                                          const QuotaCallback& callback) {
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    callback.Run(kQuotaErrorNotSupported, 0);
    return;
  }

  if (new_quota < 0) {
    callback.Run(kQuotaErrorInvalidModification, -1);
    return;
  }

  if (kPerHostPersistentQuotaLimit < new_quota) {
    // Cap the requested size at the per-host quota limit.
    new_quota = kPerHostPersistentQuotaLimit;
  }

  if (db_disabled_) {
    callback.Run(kQuotaErrorInvalidAccess, -1);
    return;
  }

  int64* new_quota_ptr = new int64(new_quota);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&SetPersistentHostQuotaOnDBThread,
                 host,
                 base::Unretained(new_quota_ptr)),
      base::Bind(&QuotaManager::DidSetPersistentHostQuota,
                 weak_factory_.GetWeakPtr(),
                 host,
                 callback,
                 base::Owned(new_quota_ptr)));
}

void QuotaManager::GetGlobalUsage(StorageType type,
                                  const GlobalUsageCallback& callback) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetGlobalUsage(callback);
}

void QuotaManager::GetHostUsage(const std::string& host,
                                StorageType type,
                                const UsageCallback& callback) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetHostUsage(host, callback);
}

void QuotaManager::GetHostUsage(const std::string& host,
                                StorageType type,
                                QuotaClient::ID client_id,
                                const UsageCallback& callback) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  ClientUsageTracker* tracker =
      GetUsageTracker(type)->GetClientTracker(client_id);
  if (!tracker) {
    callback.Run(0);
    return;
  }
  tracker->GetHostUsage(host, callback);
}

bool QuotaManager::IsTrackingHostUsage(StorageType type,
                                       QuotaClient::ID client_id) const {
  UsageTracker* tracker = GetUsageTracker(type);
  return tracker && tracker->GetClientTracker(client_id);
}

void QuotaManager::GetStatistics(
    std::map<std::string, std::string>* statistics) {
  DCHECK(statistics);
  if (temporary_storage_evictor_) {
    std::map<std::string, int64> stats;
    temporary_storage_evictor_->GetStatistics(&stats);
    for (std::map<std::string, int64>::iterator p = stats.begin();
         p != stats.end();
         ++p)
      (*statistics)[p->first] = base::Int64ToString(p->second);
  }
}

bool QuotaManager::IsStorageUnlimited(const GURL& origin,
                                      StorageType type) const {
  // For syncable storage we should always enforce quota (since the
  // quota must be capped by the server limit).
  if (type == kStorageTypeSyncable)
    return false;
  if (type == kStorageTypeQuotaNotManaged)
    return true;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(origin);
}

void QuotaManager::GetOriginsModifiedSince(StorageType type,
                                           base::Time modified_since,
                                           const GetOriginsCallback& callback) {
  LazyInitialize();
  GetModifiedSinceHelper* helper = new GetModifiedSinceHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&GetModifiedSinceHelper::GetModifiedSinceOnDBThread,
                 base::Unretained(helper),
                 type,
                 modified_since),
      base::Bind(&GetModifiedSinceHelper::DidGetModifiedSince,
                 base::Owned(helper),
                 weak_factory_.GetWeakPtr(),
                 callback,
                 type));
}

bool QuotaManager::ResetUsageTracker(StorageType type) {
  DCHECK(GetUsageTracker(type));
  if (GetUsageTracker(type)->IsWorking())
    return false;
  switch (type) {
    case kStorageTypeTemporary:
      temporary_usage_tracker_.reset(new UsageTracker(
          clients_, kStorageTypeTemporary, special_storage_policy_.get(),
          storage_monitor_.get()));
      return true;
    case kStorageTypePersistent:
      persistent_usage_tracker_.reset(new UsageTracker(
          clients_, kStorageTypePersistent, special_storage_policy_.get(),
          storage_monitor_.get()));
      return true;
    case kStorageTypeSyncable:
      syncable_usage_tracker_.reset(new UsageTracker(
          clients_, kStorageTypeSyncable, special_storage_policy_.get(),
          storage_monitor_.get()));
      return true;
    default:
      NOTREACHED();
  }
  return true;
}

void QuotaManager::AddStorageObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  DCHECK(observer);
  storage_monitor_->AddObserver(observer, params);
}

void QuotaManager::RemoveStorageObserver(StorageObserver* observer) {
  DCHECK(observer);
  storage_monitor_->RemoveObserver(observer);
}

void QuotaManager::RemoveStorageObserverForFilter(
    StorageObserver* observer, const StorageObserver::Filter& filter) {
  DCHECK(observer);
  storage_monitor_->RemoveObserverForFilter(observer, filter);
}

QuotaManager::~QuotaManager() {
  proxy_->manager_ = NULL;
  std::for_each(clients_.begin(), clients_.end(),
                std::mem_fun(&QuotaClient::OnQuotaManagerDestroyed));
  if (database_)
    db_thread_->DeleteSoon(FROM_HERE, database_.release());
}

QuotaManager::EvictionContext::EvictionContext()
    : evicted_type(kStorageTypeUnknown) {
}

QuotaManager::EvictionContext::~EvictionContext() {
}

void QuotaManager::LazyInitialize() {
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_) {
    // Initialization seems to be done already.
    return;
  }

  // Use an empty path to open an in-memory only databse for incognito.
  database_.reset(new QuotaDatabase(is_incognito_ ? base::FilePath() :
      profile_path_.AppendASCII(kDatabaseName)));

  temporary_usage_tracker_.reset(new UsageTracker(
      clients_, kStorageTypeTemporary, special_storage_policy_.get(),
      storage_monitor_.get()));
  persistent_usage_tracker_.reset(new UsageTracker(
      clients_, kStorageTypePersistent, special_storage_policy_.get(),
      storage_monitor_.get()));
  syncable_usage_tracker_.reset(new UsageTracker(
      clients_, kStorageTypeSyncable, special_storage_policy_.get(),
      storage_monitor_.get()));

  int64* temporary_quota_override = new int64(-1);
  int64* desired_available_space = new int64(-1);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&InitializeOnDBThread,
                 base::Unretained(temporary_quota_override),
                 base::Unretained(desired_available_space)),
      base::Bind(&QuotaManager::DidInitialize,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(temporary_quota_override),
                 base::Owned(desired_available_space)));
}

void QuotaManager::RegisterClient(QuotaClient* client) {
  DCHECK(!database_.get());
  clients_.push_back(client);
}

UsageTracker* QuotaManager::GetUsageTracker(StorageType type) const {
  switch (type) {
    case kStorageTypeTemporary:
      return temporary_usage_tracker_.get();
    case kStorageTypePersistent:
      return persistent_usage_tracker_.get();
    case kStorageTypeSyncable:
      return syncable_usage_tracker_.get();
    case kStorageTypeQuotaNotManaged:
      return NULL;
    case kStorageTypeUnknown:
      NOTREACHED();
  }
  return NULL;
}

void QuotaManager::GetCachedOrigins(
    StorageType type, std::set<GURL>* origins) {
  DCHECK(origins);
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetCachedOrigins(origins);
}

void QuotaManager::NotifyStorageAccessedInternal(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type,
    base::Time accessed_time) {
  LazyInitialize();
  if (type == kStorageTypeTemporary && !lru_origin_callback_.is_null()) {
    // Record the accessed origins while GetLRUOrigin task is runing
    // to filter out them from eviction.
    access_notified_origins_.insert(origin);
  }

  if (db_disabled_)
    return;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&UpdateAccessTimeOnDBThread, origin, type, accessed_time),
      base::Bind(&QuotaManager::DidDatabaseWork,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::NotifyStorageModifiedInternal(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type,
    int64 delta,
    base::Time modified_time) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->UpdateUsageCache(client_id, origin, delta);

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&UpdateModifiedTimeOnDBThread, origin, type, modified_time),
      base::Bind(&QuotaManager::DidDatabaseWork,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DumpQuotaTable(const DumpQuotaTableCallback& callback) {
  DumpQuotaTableHelper* helper = new DumpQuotaTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&DumpQuotaTableHelper::DumpQuotaTableOnDBThread,
                 base::Unretained(helper)),
      base::Bind(&DumpQuotaTableHelper::DidDumpQuotaTable,
                 base::Owned(helper),
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void QuotaManager::DumpOriginInfoTable(
    const DumpOriginInfoTableCallback& callback) {
  DumpOriginInfoTableHelper* helper = new DumpOriginInfoTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&DumpOriginInfoTableHelper::DumpOriginInfoTableOnDBThread,
                 base::Unretained(helper)),
      base::Bind(&DumpOriginInfoTableHelper::DidDumpOriginInfoTable,
                 base::Owned(helper),
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void QuotaManager::StartEviction() {
  DCHECK(!temporary_storage_evictor_.get());
  temporary_storage_evictor_.reset(new QuotaTemporaryStorageEvictor(
      this, kEvictionIntervalInMilliSeconds));
  if (desired_available_space_ >= 0)
    temporary_storage_evictor_->set_min_available_disk_space_to_start_eviction(
        desired_available_space_);
  temporary_storage_evictor_->Start();
}

void QuotaManager::DeleteOriginFromDatabase(
    const GURL& origin, StorageType type) {
  LazyInitialize();
  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&DeleteOriginInfoOnDBThread, origin, type),
      base::Bind(&QuotaManager::DidDatabaseWork,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidOriginDataEvicted(QuotaStatusCode status) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  // We only try evict origins that are not in use, so basically
  // deletion attempt for eviction should not fail.  Let's record
  // the origin if we get error and exclude it from future eviction
  // if the error happens consistently (> kThresholdOfErrorsToBeBlacklisted).
  if (status != kQuotaStatusOk)
    origins_in_error_[eviction_context_.evicted_origin]++;

  eviction_context_.evict_origin_data_callback.Run(status);
  eviction_context_.evict_origin_data_callback.Reset();
}

void QuotaManager::ReportHistogram() {
  GetGlobalUsage(kStorageTypeTemporary,
                 base::Bind(
                     &QuotaManager::DidGetTemporaryGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
  GetGlobalUsage(kStorageTypePersistent,
                 base::Bind(
                     &QuotaManager::DidGetPersistentGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetTemporaryGlobalUsageForHistogram(
    int64 usage,
    int64 unlimited_usage) {
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfTemporaryStorage", usage);

  std::set<GURL> origins;
  GetCachedOrigins(kStorageTypeTemporary, &origins);

  size_t num_origins = origins.size();
  size_t protected_origins = 0;
  size_t unlimited_origins = 0;
  CountOriginType(origins,
                  special_storage_policy_.get(),
                  &protected_origins,
                  &unlimited_origins);

  UMA_HISTOGRAM_COUNTS("Quota.NumberOfTemporaryStorageOrigins",
                       num_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfProtectedTemporaryStorageOrigins",
                       protected_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfUnlimitedTemporaryStorageOrigins",
                       unlimited_origins);
}

void QuotaManager::DidGetPersistentGlobalUsageForHistogram(
    int64 usage,
    int64 unlimited_usage) {
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfPersistentStorage", usage);

  std::set<GURL> origins;
  GetCachedOrigins(kStorageTypePersistent, &origins);

  size_t num_origins = origins.size();
  size_t protected_origins = 0;
  size_t unlimited_origins = 0;
  CountOriginType(origins,
                  special_storage_policy_.get(),
                  &protected_origins,
                  &unlimited_origins);

  UMA_HISTOGRAM_COUNTS("Quota.NumberOfPersistentStorageOrigins",
                       num_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfProtectedPersistentStorageOrigins",
                       protected_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfUnlimitedPersistentStorageOrigins",
                       unlimited_origins);
}

void QuotaManager::GetLRUOrigin(
    StorageType type,
    const GetLRUOriginCallback& callback) {
  LazyInitialize();
  // This must not be called while there's an in-flight task.
  DCHECK(lru_origin_callback_.is_null());
  lru_origin_callback_ = callback;
  if (db_disabled_) {
    lru_origin_callback_.Run(GURL());
    lru_origin_callback_.Reset();
    return;
  }

  std::set<GURL>* exceptions = new std::set<GURL>;
  for (std::map<GURL, int>::const_iterator p = origins_in_use_.begin();
       p != origins_in_use_.end();
       ++p) {
    if (p->second > 0)
      exceptions->insert(p->first);
  }
  for (std::map<GURL, int>::const_iterator p = origins_in_error_.begin();
       p != origins_in_error_.end();
       ++p) {
    if (p->second > QuotaManager::kThresholdOfErrorsToBeBlacklisted)
      exceptions->insert(p->first);
  }

  GURL* url = new GURL;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&GetLRUOriginOnDBThread,
                 type,
                 base::Owned(exceptions),
                 special_storage_policy_,
                 base::Unretained(url)),
      base::Bind(&QuotaManager::DidGetLRUOrigin,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(url)));
}

void QuotaManager::EvictOriginData(
    const GURL& origin,
    StorageType type,
    const EvictOriginDataCallback& callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK_EQ(type, kStorageTypeTemporary);

  eviction_context_.evicted_origin = origin;
  eviction_context_.evicted_type = type;
  eviction_context_.evict_origin_data_callback = callback;

  DeleteOriginData(origin, type, QuotaClient::kAllClientsMask,
      base::Bind(&QuotaManager::DidOriginDataEvicted,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::GetUsageAndQuotaForEviction(
    const UsageAndQuotaCallback& callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  LazyInitialize();

  UsageAndQuotaCallbackDispatcher* dispatcher =
      new UsageAndQuotaCallbackDispatcher(this);
  GetUsageTracker(kStorageTypeTemporary)->
      GetGlobalLimitedUsage(dispatcher->GetGlobalLimitedUsageCallback());
  GetTemporaryGlobalQuota(dispatcher->GetQuotaCallback());
  GetAvailableSpace(dispatcher->GetAvailableSpaceCallback());
  dispatcher->WaitForResults(callback);
}

void QuotaManager::DidSetTemporaryGlobalOverrideQuota(
    const QuotaCallback& callback,
    const int64* new_quota,
    bool success) {
  QuotaStatusCode status = kQuotaErrorInvalidAccess;
  DidDatabaseWork(success);
  if (success) {
    temporary_quota_override_ = *new_quota;
    status = kQuotaStatusOk;
  }

  if (callback.is_null())
    return;

  callback.Run(status, *new_quota);
}

void QuotaManager::DidGetPersistentHostQuota(const std::string& host,
                                             const int64* quota,
                                             bool success) {
  DidDatabaseWork(success);
  persistent_host_quota_callbacks_.Run(
      host, MakeTuple(kQuotaStatusOk, *quota));
}

void QuotaManager::DidSetPersistentHostQuota(const std::string& host,
                                             const QuotaCallback& callback,
                                             const int64* new_quota,
                                             bool success) {
  DidDatabaseWork(success);
  callback.Run(success ? kQuotaStatusOk : kQuotaErrorInvalidAccess, *new_quota);
}

void QuotaManager::DidInitialize(int64* temporary_quota_override,
                                 int64* desired_available_space,
                                 bool success) {
  temporary_quota_override_ = *temporary_quota_override;
  desired_available_space_ = *desired_available_space;
  temporary_quota_initialized_ = true;
  DidDatabaseWork(success);

  histogram_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(
                             kReportHistogramInterval),
                         this, &QuotaManager::ReportHistogram);

  db_initialization_callbacks_.Run(MakeTuple());
  GetTemporaryGlobalQuota(
      base::Bind(&QuotaManager::DidGetInitialTemporaryGlobalQuota,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetLRUOrigin(const GURL* origin,
                                   bool success) {
  DidDatabaseWork(success);
  // Make sure the returned origin is (still) not in the origin_in_use_ set
  // and has not been accessed since we posted the task.
  if (origins_in_use_.find(*origin) != origins_in_use_.end() ||
      access_notified_origins_.find(*origin) != access_notified_origins_.end())
    lru_origin_callback_.Run(GURL());
  else
    lru_origin_callback_.Run(*origin);
  access_notified_origins_.clear();
  lru_origin_callback_.Reset();
}

void QuotaManager::DidGetInitialTemporaryGlobalQuota(
    QuotaStatusCode status, int64 quota_unused) {
  if (eviction_disabled_)
    return;

  std::set<GURL>* origins = new std::set<GURL>;
  temporary_usage_tracker_->GetCachedOrigins(origins);
  // This will call the StartEviction() when initial origin registration
  // is completed.
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&InitializeTemporaryOriginsInfoOnDBThread,
                 base::Owned(origins)),
      base::Bind(&QuotaManager::DidInitializeTemporaryOriginsInfo,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidInitializeTemporaryOriginsInfo(bool success) {
  DidDatabaseWork(success);
  if (success)
    StartEviction();
}

void QuotaManager::DidGetAvailableSpace(int64 space) {
  available_space_callbacks_.Run(MakeTuple(kQuotaStatusOk, space));
}

void QuotaManager::DidDatabaseWork(bool success) {
  db_disabled_ = !success;
}

void QuotaManager::DeleteOnCorrectThread() const {
  if (!io_thread_->BelongsToCurrentThread() &&
      io_thread_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

void QuotaManager::PostTaskAndReplyWithResultForDBThread(
    const tracked_objects::Location& from_here,
    const base::Callback<bool(QuotaDatabase*)>& task,
    const base::Callback<void(bool)>& reply) {
  // Deleting manager will post another task to DB thread to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.
  base::PostTaskAndReplyWithResult(
      db_thread_.get(),
      from_here,
      base::Bind(task, base::Unretained(database_.get())),
      reply);
}

}  // namespace quota
