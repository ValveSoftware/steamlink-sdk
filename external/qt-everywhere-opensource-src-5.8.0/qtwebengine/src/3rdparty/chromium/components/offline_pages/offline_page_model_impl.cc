// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model_impl.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/archive_manager.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_storage_manager.h"
#include "url/gurl.h"

using ArchiverResult = offline_pages::OfflinePageArchiver::ArchiverResult;
using ClearStorageCallback =
    offline_pages::OfflinePageStorageManager::ClearStorageCallback;
using ClearStorageResult =
    offline_pages::OfflinePageStorageManager::ClearStorageResult;

namespace offline_pages {

namespace {

// The delay used to schedule the first clear storage request for storage
// manager after the model is loaded.
const base::TimeDelta kStorageManagerStartingDelay =
    base::TimeDelta::FromSeconds(20);

// This enum is used in an UMA histogram. Hence the entries here shouldn't
// be deleted or re-ordered and new ones should be added to the end.
enum ClearAllStatus {
  CLEAR_ALL_SUCCEEDED,
  STORE_RESET_FAILED,
  STORE_RELOAD_FAILED,

  // NOTE: always keep this entry at the end.
  CLEAR_ALL_STATUS_COUNT
};

int64_t GenerateOfflineId() {
  return base::RandGenerator(std::numeric_limits<int64_t>::max()) + 1;
}

// The maximum histogram size for the metrics that measure time between views of
// a given page.
const base::TimeDelta kMaxOpenedPageHistogramBucket =
    base::TimeDelta::FromDays(90);

SavePageResult ToSavePageResult(ArchiverResult archiver_result) {
  SavePageResult result;
  switch (archiver_result) {
    case ArchiverResult::SUCCESSFULLY_CREATED:
      result = SavePageResult::SUCCESS;
      break;
    case ArchiverResult::ERROR_DEVICE_FULL:
      result = SavePageResult::DEVICE_FULL;
      break;
    case ArchiverResult::ERROR_CONTENT_UNAVAILABLE:
      result = SavePageResult::CONTENT_UNAVAILABLE;
      break;
    case ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED:
      result = SavePageResult::ARCHIVE_CREATION_FAILED;
      break;
    case ArchiverResult::ERROR_CANCELED:
      result = SavePageResult::CANCELLED;
      break;
    case ArchiverResult::ERROR_SECURITY_CERTIFICATE:
      result = SavePageResult::SECURITY_CERTIFICATE_ERROR;
      break;
    default:
      NOTREACHED();
      result = SavePageResult::CONTENT_UNAVAILABLE;
  }
  return result;
}

std::string AddHistogramSuffix(const ClientId& client_id,
                               const char* histogram_name) {
  if (client_id.name_space.empty()) {
    NOTREACHED();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += ".";
  adjusted_histogram_name += client_id.name_space;
  return adjusted_histogram_name;
}

void ReportStorageHistogramsAfterSave(
    const ArchiveManager::StorageStats& storage_stats) {
  const int kMB = 1024 * 1024;
  int free_disk_space_mb =
      static_cast<int>(storage_stats.free_disk_space / kMB);
  UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.SavePage.FreeSpaceMB",
                              free_disk_space_mb, 1, 500000, 50);

  int total_page_size_mb =
      static_cast<int>(storage_stats.total_archives_size / kMB);
  UMA_HISTOGRAM_COUNTS_10000("OfflinePages.TotalPageSize", total_page_size_mb);
}

void ReportStorageHistogramsAfterDelete(
    const ArchiveManager::StorageStats& storage_stats) {
  const int kMB = 1024 * 1024;
  int free_disk_space_mb =
      static_cast<int>(storage_stats.free_disk_space / kMB);
  UMA_HISTOGRAM_CUSTOM_COUNTS("OfflinePages.DeletePage.FreeSpaceMB",
                              free_disk_space_mb, 1, 500000, 50);

  int total_page_size_mb =
      static_cast<int>(storage_stats.total_archives_size / kMB);
  UMA_HISTOGRAM_COUNTS_10000("OfflinePages.TotalPageSize", total_page_size_mb);

  if (storage_stats.free_disk_space > 0) {
    int percentage_of_free = static_cast<int>(
        1.0 * storage_stats.total_archives_size /
        (storage_stats.total_archives_size + storage_stats.free_disk_space) *
        100);
    UMA_HISTOGRAM_PERCENTAGE(
        "OfflinePages.DeletePage.TotalPageSizeAsPercentageOfFreeSpace",
        percentage_of_free);
  }
}

void ReportSavePageResultHistogramAfterSave(const ClientId& client_id,
                                           SavePageResult result) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(client_id, "OfflinePages.SavePageResult"),
      1,
      static_cast<int>(SavePageResult::RESULT_COUNT),
      static_cast<int>(SavePageResult::RESULT_COUNT) + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<int>(result));
}

void ReportPageHistogramAfterSave(const OfflinePageItem& offline_page) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_TIMES
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      AddHistogramSuffix(offline_page.client_id, "OfflinePages.SavePageTime"),
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(10),
      50, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(base::Time::Now() - offline_page.creation_time);

  // The histogram below is an expansion of the UMA_HISTOGRAM_CUSTOM_COUNTS
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  // Reported as Kb between 1Kb and 10Mb.
  histogram = base::Histogram::FactoryGet(
      AddHistogramSuffix(offline_page.client_id, "OfflinePages.PageSize"),
      1, 10000, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(offline_page.file_size / 1024);
}

void ReportPageHistogramsAfterDelete(
    const std::map<int64_t, OfflinePageItem>& offline_pages,
    const std::vector<int64_t>& deleted_offline_ids) {
  const int max_minutes = base::TimeDelta::FromDays(365).InMinutes();
  base::Time now = base::Time::Now();
  int64_t total_size = 0;
  for (int64_t offline_id : deleted_offline_ids) {
    auto iter = offline_pages.find(offline_id);
    if (iter == offline_pages.end())
      continue;
    total_size += iter->second.file_size;
    ClientId client_id = iter->second.client_id;

    // The histograms below are an expansion of the UMA_HISTOGRAM_CUSTOM_COUNTS
    // macro adapted to allow for a dynamically suffixed histogram name.
    // Note: The factory creates and owns the histogram.
    base::HistogramBase* histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id, "OfflinePages.PageLifetime"),
        1, max_minutes, 100, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add((now - iter->second.creation_time).InMinutes());

    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(
            client_id, "OfflinePages.DeletePage.TimeSinceLastOpen"),
        1, max_minutes, 100, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add((now - iter->second.last_access_time).InMinutes());

    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(
            client_id, "OfflinePages.DeletePage.LastOpenToCreated"),
        1, max_minutes, 100, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(
        (iter->second.last_access_time - iter->second.creation_time).
            InMinutes());

    // Reported as Kb between 1Kb and 10Mb.
    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id, "OfflinePages.DeletePage.PageSize"),
        1, 10000, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(iter->second.file_size / 1024);

    histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id, "OfflinePages.DeletePage.AccessCount"),
        1, 1000000, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(iter->second.access_count);
  }

  if (deleted_offline_ids.size() > 1) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.BatchDelete.Count",
                         static_cast<int32_t>(deleted_offline_ids.size()));
    UMA_HISTOGRAM_MEMORY_KB(
        "OfflinePages.BatchDelete.TotalPageSize", total_size / 1024);
  }
}

void ReportPageHistogramsAfterAccess(const OfflinePageItem& offline_page_item) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_CUSTOM_COUNTS
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(
            offline_page_item.client_id,
            offline_page_item.access_count == 0 ?
                "OfflinePages.FirstOpenSinceCreated" :
                "OfflinePages.OpenSinceLastOpen"),
        1, kMaxOpenedPageHistogramBucket.InMinutes(), 50,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(
      (base::Time::Now() - offline_page_item.last_access_time).InMinutes());
}

}  // namespace

// protected
OfflinePageModelImpl::OfflinePageModelImpl()
    : OfflinePageModel(), is_loaded_(false), weak_ptr_factory_(this) {}

OfflinePageModelImpl::OfflinePageModelImpl(
    std::unique_ptr<OfflinePageMetadataStore> store,
    const base::FilePath& archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(std::move(store)),
      archives_dir_(archives_dir),
      is_loaded_(false),
      policy_controller_(new ClientPolicyController()),
      archive_manager_(new ArchiveManager(archives_dir, task_runner)),
      weak_ptr_factory_(this) {
  archive_manager_->EnsureArchivesDirCreated(
      base::Bind(&OfflinePageModelImpl::OnEnsureArchivesDirCreatedDone,
                 weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

OfflinePageModelImpl::~OfflinePageModelImpl() {}

void OfflinePageModelImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflinePageModelImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OfflinePageModelImpl::SavePage(
    const GURL& url,
    const ClientId& client_id,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& callback) {
  DCHECK(is_loaded_);

  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (!OfflinePageModel::CanSaveURL(url)) {
    InformSavePageDone(callback, SavePageResult::SKIPPED, client_id,
                       kInvalidOfflineId);
    return;
  }

  // The web contents is not available if archiver is not created and passed.
  if (!archiver.get()) {
    InformSavePageDone(callback, SavePageResult::CONTENT_UNAVAILABLE, client_id,
                       kInvalidOfflineId);
    return;
  }

  int64_t offline_id = GenerateOfflineId();

  archiver->CreateArchive(
      archives_dir_, offline_id,
      base::Bind(&OfflinePageModelImpl::OnCreateArchiveDone,
                 weak_ptr_factory_.GetWeakPtr(), url, offline_id, client_id,
                 base::Time::Now(), callback));
  pending_archivers_.push_back(std::move(archiver));
}

void OfflinePageModelImpl::MarkPageAccessed(int64_t offline_id) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::MarkPageAccessedWhenLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), offline_id));
}

void OfflinePageModelImpl::MarkPageAccessedWhenLoadDone(int64_t offline_id) {
  DCHECK(is_loaded_);

  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end() || iter->second.IsExpired())
    return;

  // Make a copy of the cached item and update it. The cached item should only
  // be updated upon the successful store operation.
  OfflinePageItem offline_page_item = iter->second;

  ReportPageHistogramsAfterAccess(offline_page_item);

  offline_page_item.last_access_time = base::Time::Now();
  offline_page_item.access_count++;

  store_->AddOrUpdateOfflinePage(
      offline_page_item,
      base::Bind(&OfflinePageModelImpl::OnMarkPageAccesseDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_page_item));
}

void OfflinePageModelImpl::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::DoDeletePagesByOfflineId,
                           weak_ptr_factory_.GetWeakPtr(), offline_ids,
                           callback));
}

void OfflinePageModelImpl::DoDeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);

  std::vector<base::FilePath> paths_to_delete;
  for (const auto& offline_id : offline_ids) {
    auto iter = offline_pages_.find(offline_id);
    if (iter != offline_pages_.end() && !iter->second.IsExpired()) {
      paths_to_delete.push_back(iter->second.file_path);
    }
  }

  // If there're no pages to delete, return early.
  if (paths_to_delete.empty()) {
    InformDeletePageDone(callback, DeletePageResult::SUCCESS);
    return;
  }

  archive_manager_->DeleteMultipleArchives(
      paths_to_delete,
      base::Bind(&OfflinePageModelImpl::OnDeleteArchiveFilesDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_ids, callback));
}

void OfflinePageModelImpl::ClearAll(const base::Closure& callback) {
  DCHECK(is_loaded_);

  std::vector<int64_t> offline_ids;
  for (const auto& id_page_pair : offline_pages_)
    offline_ids.push_back(id_page_pair.first);
  DeletePagesByOfflineId(
      offline_ids,
      base::Bind(&OfflinePageModelImpl::OnRemoveAllFilesDoneForClearAll,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModelImpl::DeletePagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::DoDeletePagesByURLPredicate,
                           weak_ptr_factory_.GetWeakPtr(), predicate,
                           callback));
}

void OfflinePageModelImpl::DoDeletePagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {
  DCHECK(is_loaded_);

  std::vector<int64_t> offline_ids;
  for (const auto& id_page_pair : offline_pages_) {
    if (predicate.Run(id_page_pair.second.url))
      offline_ids.push_back(id_page_pair.first);
  }
  DoDeletePagesByOfflineId(offline_ids, callback);
}

void OfflinePageModelImpl::HasPages(const std::string& name_space,
                                    const HasPagesCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::HasPagesAfterLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), name_space,
                           callback));
}

void OfflinePageModelImpl::HasPagesAfterLoadDone(
    const std::string& name_space,
    const HasPagesCallback& callback) const {
  DCHECK(is_loaded_);

  bool has_pages = false;

  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.client_id.name_space == name_space &&
        !id_page_pair.second.IsExpired()) {
      has_pages = true;
      break;
    }
  }

  callback.Run(has_pages);
}

void OfflinePageModelImpl::CheckPagesExistOffline(
    const std::set<GURL>& urls,
    const CheckPagesExistOfflineCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::CheckPagesExistOfflineAfterLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), urls, callback));
}

void OfflinePageModelImpl::CheckPagesExistOfflineAfterLoadDone(
    const std::set<GURL>& urls,
    const CheckPagesExistOfflineCallback& callback) {
  DCHECK(is_loaded_);
  CheckPagesExistOfflineResult result;
  for (const auto& id_page_pair : offline_pages_) {
    // TODO(dewittj): Remove the "Last N" restriction in favor of a better query
    // interface.  See https://crbug.com/622763 for information.
    if (id_page_pair.second.IsExpired() ||
        id_page_pair.second.client_id.name_space == kLastNNamespace)
      continue;
    auto iter = urls.find(id_page_pair.second.url);
    if (iter != urls.end())
      result.insert(*iter);
  }
  callback.Run(result);
}

void OfflinePageModelImpl::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {
  RunWhenLoaded(base::Bind(&OfflinePageModelImpl::GetAllPagesAfterLoadDone,
                           weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModelImpl::GetAllPagesAfterLoadDone(
    const MultipleOfflinePageItemCallback& callback) const {
  DCHECK(is_loaded_);

  MultipleOfflinePageItemResult offline_pages;
  for (const auto& id_page_pair : offline_pages_) {
    if (!id_page_pair.second.IsExpired())
      offline_pages.push_back(id_page_pair.second);
  }

  callback.Run(offline_pages);
}

void OfflinePageModelImpl::GetOfflineIdsForClientId(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetOfflineIdsForClientIdWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), client_id, callback));
}

void OfflinePageModelImpl::GetOfflineIdsForClientIdWhenLoadDone(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) const {
  callback.Run(MaybeGetOfflineIdsForClientId(client_id));
}

const std::vector<int64_t> OfflinePageModelImpl::MaybeGetOfflineIdsForClientId(
    const ClientId& client_id) const {
  DCHECK(is_loaded_);
  std::vector<int64_t> results;

  // We want only all pages, including those marked for deletion.
  // TODO(bburns): actually use an index rather than linear scan.
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.client_id == client_id &&
        !id_page_pair.second.IsExpired()) {
      results.push_back(id_page_pair.second.offline_id);
    }
  }
  return results;
}

void OfflinePageModelImpl::GetPageByOfflineId(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetPageByOfflineIdWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_id, callback));
}

void OfflinePageModelImpl::GetPageByOfflineIdWhenLoadDone(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) const {
  callback.Run(MaybeGetPageByOfflineId(offline_id));
}

const OfflinePageItem* OfflinePageModelImpl::MaybeGetPageByOfflineId(
    int64_t offline_id) const {
  const auto iter = offline_pages_.find(offline_id);
  return iter != offline_pages_.end() && !iter->second.IsExpired()
             ? &(iter->second)
             : nullptr;
}

void OfflinePageModelImpl::GetPageByOfflineURL(
    const GURL& offline_url,
    const SingleOfflinePageItemCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetPageByOfflineURLWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_url, callback));
}

void OfflinePageModelImpl::GetPageByOfflineURLWhenLoadDone(
    const GURL& offline_url,
    const SingleOfflinePageItemCallback& callback) const {
  // Getting pages by offline URL does not exclude expired pages, as the caller
  // already holds the offline URL and simply needs to look up a corresponding
  // online URL.
  const OfflinePageItem* result = nullptr;

  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.GetOfflineURL() == offline_url) {
      result = &id_page_pair.second;
      break;
    }
  }

  callback.Run(result);
}

const OfflinePageItem* OfflinePageModelImpl::MaybeGetPageByOfflineURL(
    const GURL& offline_url) const {
  // Getting pages by offline URL does not exclude expired pages, as the caller
  // already holds the offline URL and simply needs to look up a corresponding
  // online URL.
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.GetOfflineURL() == offline_url)
      return &(id_page_pair.second);
  }
  return nullptr;
}

void OfflinePageModelImpl::GetBestPageForOnlineURL(
    const GURL& online_url,
    const SingleOfflinePageItemCallback callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetBestPageForOnlineURLWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), online_url, callback));
}

void OfflinePageModelImpl::GetBestPageForOnlineURLWhenLoadDone(
    const GURL& online_url,
    const SingleOfflinePageItemCallback& callback) const {
  callback.Run(MaybeGetBestPageForOnlineURL(online_url));
}

void OfflinePageModelImpl::GetPagesByOnlineURL(
    const GURL& online_url,
    const MultipleOfflinePageItemCallback& callback) {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::GetPagesByOnlineURLWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr(), online_url, callback));
}

void OfflinePageModelImpl::GetPagesByOnlineURLWhenLoadDone(
    const GURL& online_url,
    const MultipleOfflinePageItemCallback& callback) const {
  std::vector<OfflinePageItem> result;

  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.url == online_url &&
        !id_page_pair.second.IsExpired()) {
      result.push_back(id_page_pair.second);
    }
  }

  callback.Run(result);
}

const OfflinePageItem* OfflinePageModelImpl::MaybeGetBestPageForOnlineURL(
    const GURL& online_url) const {
  const OfflinePageItem* result = nullptr;
  for (const auto& id_page_pair : offline_pages_) {
    if (id_page_pair.second.url == online_url &&
        !id_page_pair.second.IsExpired()) {
      if (!result || id_page_pair.second.creation_time > result->creation_time)
        result = &(id_page_pair.second);
    }
  }
  return result;
}

void OfflinePageModelImpl::CheckMetadataConsistency() {
  RunWhenLoaded(
      base::Bind(&OfflinePageModelImpl::CheckMetadataConsistencyWhenLoadDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageModelImpl::CheckMetadataConsistencyWhenLoadDone() {
  archive_manager_->GetAllArchives(
      base::Bind(&OfflinePageModelImpl::CheckMetadataConsistencyForArchivePaths,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageModelImpl::ExpirePages(
    const std::vector<int64_t>& offline_ids,
    const base::Time& expiration_time,
    const base::Callback<void(bool)>& callback) {
  std::vector<base::FilePath> paths_to_delete;
  for (int64_t offline_id : offline_ids) {
    auto iter = offline_pages_.find(offline_id);
    if (iter == offline_pages_.end())
      continue;

    OfflinePageItem offline_page = iter->second;
    paths_to_delete.push_back(offline_page.file_path);
    offline_page.expiration_time = expiration_time;

    store_->AddOrUpdateOfflinePage(
        offline_page, base::Bind(&OfflinePageModelImpl::OnExpirePageDone,
                                 weak_ptr_factory_.GetWeakPtr(), offline_id,
                                 expiration_time));
  }
  if (paths_to_delete.empty()) {
    callback.Run(true);
    return;
  }
  archive_manager_->DeleteMultipleArchives(paths_to_delete, callback);
}

void OfflinePageModelImpl::OnExpirePageDone(int64_t offline_id,
                                            const base::Time& expiration_time,
                                            bool success) {
  UMA_HISTOGRAM_BOOLEAN("OfflinePages.ExpirePage.StoreUpdateResult", success);
  if (!success)
    return;
  const auto& iter = offline_pages_.find(offline_id);
  if (iter != offline_pages_.end()) {
    iter->second.expiration_time = expiration_time;
    ClientId client_id = iter->second.client_id;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        AddHistogramSuffix(client_id, "OfflinePages.ExpirePage.PageLifetime")
            .c_str(),
        (expiration_time - iter->second.creation_time).InMinutes(), 1,
        base::TimeDelta::FromDays(30).InMinutes(), 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        AddHistogramSuffix(client_id,
                           "OfflinePages.ExpirePage.TimeSinceLastAccess")
            .c_str(),
        (expiration_time - iter->second.last_access_time).InMinutes(), 1,
        base::TimeDelta::FromDays(30).InMinutes(), 50);
  }
}

ClientPolicyController* OfflinePageModelImpl::GetPolicyController() {
  return policy_controller_.get();
}

OfflinePageMetadataStore* OfflinePageModelImpl::GetStoreForTesting() {
  return store_.get();
}

OfflinePageStorageManager* OfflinePageModelImpl::GetStorageManager() {
  return storage_manager_.get();
}

bool OfflinePageModelImpl::is_loaded() const {
  return is_loaded_;
}

OfflineEventLogger* OfflinePageModelImpl::GetLogger() {
  return &offline_event_logger_;
}

void OfflinePageModelImpl::OnCreateArchiveDone(const GURL& requested_url,
                                               int64_t offline_id,
                                               const ClientId& client_id,
                                               const base::Time& start_time,
                                               const SavePageCallback& callback,
                                               OfflinePageArchiver* archiver,
                                               ArchiverResult archiver_result,
                                               const GURL& url,
                                               const base::FilePath& file_path,
                                               int64_t file_size) {
  if (requested_url != url) {
    DVLOG(1) << "Saved URL does not match requested URL.";
    // TODO(fgorski): We have created an archive for a wrong URL. It should be
    // deleted from here, once archiver has the right functionality.
    InformSavePageDone(callback, SavePageResult::ARCHIVE_CREATION_FAILED,
                       client_id, offline_id);
    DeletePendingArchiver(archiver);
    return;
  }

  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ToSavePageResult(archiver_result);
    InformSavePageDone(callback, result, client_id, offline_id);
    DeletePendingArchiver(archiver);
    return;
  }
  OfflinePageItem offline_page_item(url, offline_id, client_id, file_path,
                                    file_size, start_time);
  store_->AddOrUpdateOfflinePage(
      offline_page_item, base::Bind(&OfflinePageModelImpl::OnAddOfflinePageDone,
                                    weak_ptr_factory_.GetWeakPtr(), archiver,
                                    callback, offline_page_item));
}

void OfflinePageModelImpl::OnAddOfflinePageDone(
    OfflinePageArchiver* archiver,
    const SavePageCallback& callback,
    const OfflinePageItem& offline_page,
    bool success) {
  SavePageResult result;
  if (success) {
    offline_pages_[offline_page.offline_id] = offline_page;
    result = SavePageResult::SUCCESS;
    ReportPageHistogramAfterSave(offline_page);
    offline_event_logger_.RecordPageSaved(
        offline_page.client_id.name_space, offline_page.url.spec(),
        std::to_string(offline_page.offline_id));
  } else {
    result = SavePageResult::STORE_FAILURE;
  }
  InformSavePageDone(callback, result, offline_page.client_id,
                     offline_page.offline_id);
  DeletePendingArchiver(archiver);

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelChanged(this));
}

void OfflinePageModelImpl::OnMarkPageAccesseDone(
    const OfflinePageItem& offline_page_item,
    bool success) {
  // Update the item in the cache only upon success.
  if (success)
    offline_pages_[offline_page_item.offline_id] = offline_page_item;

  // No need to fire OfflinePageModelChanged event since updating access info
  // should not have any impact to the UI.
}

void OfflinePageModelImpl::OnEnsureArchivesDirCreatedDone(
    const base::TimeTicks& start_time) {
  UMA_HISTOGRAM_TIMES("OfflinePages.Model.ArchiveDirCreationTime",
                      base::TimeTicks::Now() - start_time);

  store_->Load(base::Bind(&OfflinePageModelImpl::OnLoadDone,
                          weak_ptr_factory_.GetWeakPtr(), start_time));
}

void OfflinePageModelImpl::OnLoadDone(
    const base::TimeTicks& start_time,
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK(!is_loaded_);
  is_loaded_ = true;

  // TODO(jianli): rebuild the store upon failure.

  if (load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED)
    CacheLoadedData(offline_pages);

  UMA_HISTOGRAM_TIMES("OfflinePages.Model.ConstructionToLoadedEventTime",
                      base::TimeTicks::Now() - start_time);

  // Create Storage Manager.
  storage_manager_.reset(new OfflinePageStorageManager(
      this, GetPolicyController(), archive_manager_.get()));

  // Run all the delayed tasks.
  for (const auto& delayed_task : delayed_tasks_)
    delayed_task.Run();
  delayed_tasks_.clear();

  FOR_EACH_OBSERVER(Observer, observers_, OfflinePageModelLoaded(this));

  CheckMetadataConsistency();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&OfflinePageModelImpl::ClearStorageIfNeeded,
                            weak_ptr_factory_.GetWeakPtr(),
                            base::Bind(&OfflinePageModelImpl::OnStorageCleared,
                                       weak_ptr_factory_.GetWeakPtr())),
      kStorageManagerStartingDelay);
}

void OfflinePageModelImpl::InformSavePageDone(const SavePageCallback& callback,
                                              SavePageResult result,
                                              const ClientId& client_id,
                                              int64_t offline_id) {
  ReportSavePageResultHistogramAfterSave(client_id, result);
  archive_manager_->GetStorageStats(
      base::Bind(&ReportStorageHistogramsAfterSave));
  // Remove existing pages generated by the same policy and with same url.
  size_t pages_allowed =
      policy_controller_->GetPolicy(client_id.name_space).pages_allowed_per_url;
  if (result == SavePageResult::SUCCESS && pages_allowed != kUnlimitedPages) {
    GetPagesByOnlineURL(
        offline_pages_[offline_id].url,
        base::Bind(&OfflinePageModelImpl::OnPagesFoundWithSameURL,
                   weak_ptr_factory_.GetWeakPtr(), client_id, offline_id,
                   pages_allowed));
  } else {
    PostClearStorageIfNeededTask();
  }
  callback.Run(result, offline_id);
}

void OfflinePageModelImpl::OnPagesFoundWithSameURL(
    const ClientId& client_id,
    int64_t offline_id,
    size_t pages_allowed,
    const MultipleOfflinePageItemResult& items) {
  std::vector<OfflinePageItem> pages_to_delete;
  for (const auto& item : items) {
    if (item.offline_id != offline_id &&
        item.client_id.name_space == client_id.name_space) {
      pages_to_delete.push_back(item);
    }
  }
  // Only keep |pages_allowed|-1 of most fresh pages and delete others, by
  // sorting pages with the oldest  ones first and resize the vector.
  if (pages_to_delete.size() >= pages_allowed) {
    sort(pages_to_delete.begin(), pages_to_delete.end(),
         [](const OfflinePageItem& a, const OfflinePageItem& b) -> bool {
           return a.last_access_time < b.last_access_time;
         });
    pages_to_delete.resize(pages_to_delete.size() - pages_allowed + 1);
  }
  std::vector<int64_t> page_ids_to_delete;
  for (const auto& item : pages_to_delete)
    page_ids_to_delete.push_back(item.offline_id);
  DeletePagesByOfflineId(
      page_ids_to_delete,
      base::Bind(&OfflinePageModelImpl::OnDeleteOldPagesWithSameURL,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageModelImpl::OnDeleteOldPagesWithSameURL(
    DeletePageResult result) {
  // TODO(romax) Add UMAs for failure cases.
  PostClearStorageIfNeededTask();
}

void OfflinePageModelImpl::DeletePendingArchiver(
    OfflinePageArchiver* archiver) {
  pending_archivers_.erase(std::find(pending_archivers_.begin(),
                                     pending_archivers_.end(), archiver));
}

void OfflinePageModelImpl::OnDeleteArchiveFilesDone(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback,
    bool success) {
  if (!success) {
    InformDeletePageDone(callback, DeletePageResult::DEVICE_FAILURE);
    return;
  }

  store_->RemoveOfflinePages(
      offline_ids,
      base::Bind(&OfflinePageModelImpl::OnRemoveOfflinePagesDone,
                 weak_ptr_factory_.GetWeakPtr(), offline_ids, callback));
}

void OfflinePageModelImpl::OnRemoveOfflinePagesDone(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback,
    bool success) {
  ReportPageHistogramsAfterDelete(offline_pages_, offline_ids);

  for (int64_t offline_id : offline_ids) {
    offline_event_logger_.RecordPageDeleted(std::to_string(offline_id));
    auto iter = offline_pages_.find(offline_id);
    if (iter == offline_pages_.end())
      continue;
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OfflinePageDeleted(iter->second.offline_id, iter->second.client_id));
    offline_pages_.erase(iter);
  }

  // Deleting multiple pages always succeeds when it gets to this point.
  InformDeletePageDone(callback, (success || offline_ids.size() > 1)
                                     ? DeletePageResult::SUCCESS
                                     : DeletePageResult::STORE_FAILURE);
}

void OfflinePageModelImpl::InformDeletePageDone(
    const DeletePageCallback& callback,
    DeletePageResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.DeletePageResult",
                            static_cast<int>(result),
                            static_cast<int>(DeletePageResult::RESULT_COUNT));
  archive_manager_->GetStorageStats(
      base::Bind(&ReportStorageHistogramsAfterDelete));
  if (!callback.is_null())
    callback.Run(result);
}

void OfflinePageModelImpl::CheckMetadataConsistencyForArchivePaths(
    const std::set<base::FilePath>& archive_paths) {
  ExpirePagesMissingArchiveFile(archive_paths);
  DeleteOrphanedArchives(archive_paths);
}

void OfflinePageModelImpl::ExpirePagesMissingArchiveFile(
    const std::set<base::FilePath>& archive_paths) {
  std::vector<int64_t> ids_of_pages_missing_archive_file;
  for (const auto& id_page_pair : offline_pages_) {
    if (archive_paths.count(id_page_pair.second.file_path) == 0UL)
      ids_of_pages_missing_archive_file.push_back(id_page_pair.first);
  }

  if (ids_of_pages_missing_archive_file.empty())
    return;

  ExpirePages(
      ids_of_pages_missing_archive_file, base::Time::Now(),
      base::Bind(&OfflinePageModelImpl::OnExpirePagesMissingArchiveFileDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 ids_of_pages_missing_archive_file));
}

void OfflinePageModelImpl::OnExpirePagesMissingArchiveFileDone(
    const std::vector<int64_t>& offline_ids,
    bool success) {
  UMA_HISTOGRAM_COUNTS("OfflinePages.Consistency.PagesMissingArchiveFileCount",
                       static_cast<int32_t>(offline_ids.size()));
  UMA_HISTOGRAM_BOOLEAN(
      "OfflinePages.Consistency.ExpirePagesMissingArchiveFileResult", success);
}

void OfflinePageModelImpl::DeleteOrphanedArchives(
    const std::set<base::FilePath>& archive_paths) {
  // Archives are considered orphaned unless they are pointed to by some pages.
  std::set<base::FilePath> orphaned_archive_set(archive_paths);
  for (const auto& id_page_pair : offline_pages_)
    orphaned_archive_set.erase(id_page_pair.second.file_path);

  if (orphaned_archive_set.empty())
    return;

  std::vector<base::FilePath> orphaned_archives(orphaned_archive_set.begin(),
                                                orphaned_archive_set.end());
  archive_manager_->DeleteMultipleArchives(
      orphaned_archives,
      base::Bind(&OfflinePageModelImpl::OnDeleteOrphanedArchivesDone,
                 weak_ptr_factory_.GetWeakPtr(), orphaned_archives));
}

void OfflinePageModelImpl::OnDeleteOrphanedArchivesDone(
    const std::vector<base::FilePath>& archives,
    bool success) {
  UMA_HISTOGRAM_COUNTS("OfflinePages.Consistency.OrphanedArchivesCount",
                       static_cast<int32_t>(archives.size()));
  UMA_HISTOGRAM_BOOLEAN("OfflinePages.Consistency.DeleteOrphanedArchivesResult",
                        success);
}

void OfflinePageModelImpl::OnRemoveAllFilesDoneForClearAll(
    const base::Closure& callback,
    DeletePageResult result) {
  store_->Reset(base::Bind(&OfflinePageModelImpl::OnResetStoreDoneForClearAll,
                           weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModelImpl::OnResetStoreDoneForClearAll(
    const base::Closure& callback,
    bool success) {
  DCHECK(success);
  if (!success) {
    offline_event_logger_.RecordStoreClearError();
    UMA_HISTOGRAM_ENUMERATION("OfflinePages.ClearAllStatus2",
                              STORE_RESET_FAILED, CLEAR_ALL_STATUS_COUNT);
  }

  offline_pages_.clear();
  store_->Load(base::Bind(&OfflinePageModelImpl::OnReloadStoreDoneForClearAll,
                          weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageModelImpl::OnReloadStoreDoneForClearAll(
    const base::Closure& callback,
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK_EQ(OfflinePageMetadataStore::LOAD_SUCCEEDED, load_status);
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.ClearAllStatus2",
      load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED
          ? CLEAR_ALL_SUCCEEDED
          : STORE_RELOAD_FAILED,
      CLEAR_ALL_STATUS_COUNT);

  if (load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED) {
    offline_event_logger_.RecordStoreCleared();
  } else {
    offline_event_logger_.RecordStoreReloadError();
  }

  CacheLoadedData(offline_pages);
  callback.Run();
}

void OfflinePageModelImpl::CacheLoadedData(
    const std::vector<OfflinePageItem>& offline_pages) {
  offline_pages_.clear();
  for (const auto& offline_page : offline_pages)
    offline_pages_[offline_page.offline_id] = offline_page;
}

void OfflinePageModelImpl::ClearStorageIfNeeded(
    const ClearStorageCallback& callback) {
  storage_manager_->ClearPagesIfNeeded(callback);
}

void OfflinePageModelImpl::OnStorageCleared(size_t expired_page_count,
                                            ClearStorageResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.ClearStorageResult",
                            static_cast<int>(result),
                            static_cast<int>(ClearStorageResult::RESULT_COUNT));
  if (expired_page_count > 0) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.ExpirePage.BatchSize",
                         static_cast<int32_t>(expired_page_count));
  }
}

void OfflinePageModelImpl::PostClearStorageIfNeededTask() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OfflinePageModelImpl::ClearStorageIfNeeded,
                            weak_ptr_factory_.GetWeakPtr(),
                            base::Bind(&OfflinePageModelImpl::OnStorageCleared,
                                       weak_ptr_factory_.GetWeakPtr())));
}

void OfflinePageModelImpl::RunWhenLoaded(const base::Closure& task) {
  if (!is_loaded_) {
    delayed_tasks_.push_back(task);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
}

}  // namespace offline_pages
