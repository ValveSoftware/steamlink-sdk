// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_memory_controller.h"

#include <algorithm>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/small_map.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tuple.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/common/data_element.h"

using base::File;
using base::FilePath;

namespace storage {
namespace {
using FileCreationInfo = BlobMemoryController::FileCreationInfo;
using MemoryAllocation = BlobMemoryController::MemoryAllocation;
using QuotaAllocationTask = BlobMemoryController::QuotaAllocationTask;

File::Error CreateBlobDirectory(const FilePath& blob_storage_dir) {
  File::Error error = File::FILE_OK;
  base::CreateDirectoryAndGetError(blob_storage_dir, &error);
  UMA_HISTOGRAM_ENUMERATION("Storage.Blob.CreateDirectoryResult", -error,
                            -File::FILE_ERROR_MAX);
  return error;
}

void DestructFile(File infos_without_references) {}

// Used for new unpopulated file items. Caller must populate file reference in
// returned FileCreationInfos.
std::pair<std::vector<FileCreationInfo>, File::Error> CreateEmptyFiles(
    const FilePath& blob_storage_dir,
    scoped_refptr<base::TaskRunner> file_task_runner,
    std::vector<base::FilePath> file_paths) {
  base::ThreadRestrictions::AssertIOAllowed();

  File::Error dir_create_status = CreateBlobDirectory(blob_storage_dir);
  if (dir_create_status != File::FILE_OK)
    return std::make_pair(std::vector<FileCreationInfo>(), dir_create_status);

  std::vector<FileCreationInfo> result;
  for (const base::FilePath& file_path : file_paths) {
    FileCreationInfo creation_info;
    // Try to open our file.
    File file(file_path, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
    creation_info.path = std::move(file_path);
    creation_info.file_deletion_runner = file_task_runner;
    creation_info.error = file.error_details();
    if (creation_info.error != File::FILE_OK) {
      return std::make_pair(std::vector<FileCreationInfo>(),
                            creation_info.error);
    }

    // Grab the file info to get the "last modified" time and store the file.
    File::Info file_info;
    bool success = file.GetInfo(&file_info);
    creation_info.error = success ? File::FILE_OK : File::FILE_ERROR_FAILED;
    if (!success) {
      return std::make_pair(std::vector<FileCreationInfo>(),
                            creation_info.error);
    }
    creation_info.file = std::move(file);

    result.push_back(std::move(creation_info));
  }
  return std::make_pair(std::move(result), File::FILE_OK);
}

// Used to evict multiple memory items out to a single file. Caller must
// populate file reference in returned FileCreationInfo.
FileCreationInfo CreateFileAndWriteItems(
    const FilePath& blob_storage_dir,
    const FilePath& file_path,
    scoped_refptr<base::TaskRunner> file_task_runner,
    std::vector<DataElement*> items,
    size_t total_size_bytes) {
  DCHECK_NE(0u, total_size_bytes);
  UMA_HISTOGRAM_MEMORY_KB("Storage.Blob.PageFileSize", total_size_bytes / 1024);
  base::ThreadRestrictions::AssertIOAllowed();

  FileCreationInfo creation_info;
  creation_info.file_deletion_runner = std::move(file_task_runner);
  creation_info.error = CreateBlobDirectory(blob_storage_dir);
  if (creation_info.error != File::FILE_OK)
    return creation_info;

  // Create the page file.
  File file(file_path, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  creation_info.path = file_path;
  creation_info.error = file.error_details();
  if (creation_info.error != File::FILE_OK)
    return creation_info;

  // Write data.
  file.SetLength(total_size_bytes);
  int bytes_written = 0;
  for (DataElement* element : items) {
    DCHECK_EQ(DataElement::TYPE_BYTES, element->type());
    size_t length = base::checked_cast<size_t>(element->length());
    size_t bytes_left = length;
    while (bytes_left > 0) {
      bytes_written =
          file.WriteAtCurrentPos(element->bytes() + (length - bytes_left),
                                 base::saturated_cast<int>(bytes_left));
      if (bytes_written < 0)
        break;
      DCHECK_LE(static_cast<size_t>(bytes_written), bytes_left);
      bytes_left -= bytes_written;
    }
    if (bytes_written < 0)
      break;
  }

  File::Info info;
  bool success = file.GetInfo(&info);
  creation_info.error =
      bytes_written < 0 || !success ? File::FILE_ERROR_FAILED : File::FILE_OK;
  creation_info.last_modified = info.last_modified;
  return creation_info;
}

uint64_t GetTotalSizeAndFileSizes(
    const std::vector<scoped_refptr<ShareableBlobDataItem>>&
        unreserved_file_items,
    std::vector<uint64_t>* file_sizes_output) {
  uint64_t total_size_output = 0;
  base::SmallMap<std::map<uint64_t, uint64_t>> file_id_to_sizes;
  for (const auto& item : unreserved_file_items) {
    const DataElement& element = item->item()->data_element();
    uint64_t file_id = BlobDataBuilder::GetFutureFileID(element);
    auto it = file_id_to_sizes.find(file_id);
    if (it != file_id_to_sizes.end())
      it->second = std::max(it->second, element.offset() + element.length());
    else
      file_id_to_sizes[file_id] = element.offset() + element.length();
    total_size_output += element.length();
  }
  for (const auto& size_pair : file_id_to_sizes) {
    file_sizes_output->push_back(size_pair.second);
  }
  return total_size_output;
}

}  // namespace

FileCreationInfo::FileCreationInfo() {}
FileCreationInfo::~FileCreationInfo() {
  if (file.IsValid()) {
    DCHECK(file_deletion_runner);
    file_deletion_runner->PostTask(
        FROM_HERE, base::Bind(&DestructFile, base::Passed(&file)));
  }
}
FileCreationInfo::FileCreationInfo(FileCreationInfo&&) = default;
FileCreationInfo& FileCreationInfo::operator=(FileCreationInfo&&) = default;

MemoryAllocation::MemoryAllocation(
    base::WeakPtr<BlobMemoryController> controller,
    uint64_t item_id,
    size_t length)
    : controller(controller), item_id(item_id), length(length) {}

MemoryAllocation::~MemoryAllocation() {
  if (controller)
    controller->RevokeMemoryAllocation(item_id, length);
}

BlobMemoryController::QuotaAllocationTask::~QuotaAllocationTask() {}

class BlobMemoryController::MemoryQuotaAllocationTask
    : public BlobMemoryController::QuotaAllocationTask {
 public:
  MemoryQuotaAllocationTask(
      BlobMemoryController* controller,
      size_t quota_request_size,
      std::vector<scoped_refptr<ShareableBlobDataItem>> pending_items,
      MemoryQuotaRequestCallback done_callback)
      : controller_(controller),
        pending_items_(std::move(pending_items)),
        done_callback_(std::move(done_callback)),
        allocation_size_(quota_request_size),
        weak_factory_(this) {}

  ~MemoryQuotaAllocationTask() override = default;

  void RunDoneCallback(bool success) {
    // Make sure we clear the weak pointers we gave to the caller beforehand.
    weak_factory_.InvalidateWeakPtrs();
    if (success)
      controller_->GrantMemoryAllocations(&pending_items_, allocation_size_);
    done_callback_.Run(success);
  }

  base::WeakPtr<QuotaAllocationTask> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void Cancel() override {
    DCHECK_GE(controller_->pending_memory_quota_total_size_, allocation_size_);
    controller_->pending_memory_quota_total_size_ -= allocation_size_;
    // This call destroys this object.
    controller_->pending_memory_quota_tasks_.erase(my_list_position_);
  }

  // The my_list_position_ iterator is stored so that we can remove ourself
  // from the task list when we are cancelled.
  void set_my_list_position(
      PendingMemoryQuotaTaskList::iterator my_list_position) {
    my_list_position_ = my_list_position;
  }

  size_t allocation_size() const { return allocation_size_; }

 private:
  BlobMemoryController* controller_;
  std::vector<scoped_refptr<ShareableBlobDataItem>> pending_items_;
  MemoryQuotaRequestCallback done_callback_;

  size_t allocation_size_;
  PendingMemoryQuotaTaskList::iterator my_list_position_;

  base::WeakPtrFactory<MemoryQuotaAllocationTask> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(MemoryQuotaAllocationTask);
};

class BlobMemoryController::FileQuotaAllocationTask
    : public BlobMemoryController::QuotaAllocationTask {
 public:
  // We post a task to create the file for the items right away.
  FileQuotaAllocationTask(
      BlobMemoryController* memory_controller,
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_file_items,
      const FileQuotaRequestCallback& done_callback)
      : controller_(memory_controller),
        done_callback_(done_callback),
        weak_factory_(this) {
    // Get the file sizes and total size.
    std::vector<uint64_t> file_sizes;
    uint64_t total_size =
        GetTotalSizeAndFileSizes(unreserved_file_items, &file_sizes);
    DCHECK_LE(total_size, controller_->GetAvailableFileSpaceForBlobs());
    allocation_size_ = total_size;

    // Check & set our item states.
    for (auto& shareable_item : unreserved_file_items) {
      DCHECK_EQ(ShareableBlobDataItem::QUOTA_NEEDED, shareable_item->state());
      DCHECK_EQ(DataElement::TYPE_FILE, shareable_item->item()->type());
      shareable_item->set_state(ShareableBlobDataItem::QUOTA_REQUESTED);
    }
    pending_items_ = std::move(unreserved_file_items);

    // Increment disk usage and create our file references.
    controller_->disk_used_ += allocation_size_;
    std::vector<base::FilePath> file_paths;
    std::vector<scoped_refptr<ShareableFileReference>> references;
    for (size_t i = 0; i < file_sizes.size(); i++) {
      file_paths.push_back(controller_->GenerateNextPageFileName());
      references.push_back(ShareableFileReference::GetOrCreate(
          file_paths.back(), ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          controller_->file_runner_.get()));
      references.back()->AddFinalReleaseCallback(
          base::Bind(&BlobMemoryController::OnBlobFileDelete,
                     controller_->weak_factory_.GetWeakPtr(), file_sizes[i]));
    }

    // Send file creation task to file thread.
    base::PostTaskAndReplyWithResult(
        controller_->file_runner_.get(), FROM_HERE,
        base::Bind(&CreateEmptyFiles, controller_->blob_storage_dir_,
                   controller_->file_runner_, base::Passed(&file_paths)),
        base::Bind(&FileQuotaAllocationTask::OnCreateEmptyFiles,
                   weak_factory_.GetWeakPtr(), base::Passed(&references)));
    controller_->RecordTracingCounters();
  }
  ~FileQuotaAllocationTask() override {}

  void RunDoneCallback(bool success, std::vector<FileCreationInfo> file_info) {
    // Make sure we clear the weak pointers we gave to the caller beforehand.
    weak_factory_.InvalidateWeakPtrs();

    // We want to destroy this object on the exit of this method if we were
    // successful.
    std::unique_ptr<FileQuotaAllocationTask> this_object;
    if (success) {
      for (auto& item : pending_items_) {
        item->set_state(ShareableBlobDataItem::QUOTA_GRANTED);
      }
      this_object = std::move(*my_list_position_);
      controller_->pending_file_quota_tasks_.erase(my_list_position_);
    }

    done_callback_.Run(success, std::move(file_info));
  }

  base::WeakPtr<QuotaAllocationTask> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void Cancel() override {
    // This call destroys this object. We rely on ShareableFileReference's
    // final release callback for disk_usage_ accounting.
    controller_->pending_file_quota_tasks_.erase(my_list_position_);
  }

  void OnCreateEmptyFiles(
      std::vector<scoped_refptr<ShareableFileReference>> references,
      std::pair<std::vector<FileCreationInfo>, File::Error> files_and_error) {
    auto& files = files_and_error.first;
    if (files.empty()) {
      DCHECK_GE(controller_->disk_used_, allocation_size_);
      controller_->disk_used_ -= allocation_size_;
      // This will call our callback and delete the object correctly.
      controller_->DisableFilePaging(files_and_error.second);
      return;
    }
    DCHECK_EQ(files.size(), references.size());
    for (size_t i = 0; i < files.size(); i++) {
      files[i].file_reference = std::move(references[i]);
    }
    RunDoneCallback(true, std::move(files));
  }

  // The my_list_position_ iterator is stored so that we can remove ourself
  // from the task list when we are cancelled.
  void set_my_list_position(
      PendingFileQuotaTaskList::iterator my_list_position) {
    my_list_position_ = my_list_position;
  }

 private:
  BlobMemoryController* controller_;
  std::vector<scoped_refptr<ShareableBlobDataItem>> pending_items_;
  scoped_refptr<base::TaskRunner> file_runner_;
  FileQuotaRequestCallback done_callback_;

  uint64_t allocation_size_;
  PendingFileQuotaTaskList::iterator my_list_position_;

  base::WeakPtrFactory<FileQuotaAllocationTask> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileQuotaAllocationTask);
};

BlobMemoryController::BlobMemoryController(
    const base::FilePath& storage_directory,
    scoped_refptr<base::TaskRunner> file_runner)
    : file_paging_enabled_(file_runner.get() != nullptr),
      blob_storage_dir_(storage_directory),
      file_runner_(std::move(file_runner)),
      populated_memory_items_(
          base::MRUCache<uint64_t, ShareableBlobDataItem*>::NO_AUTO_EVICT),
      weak_factory_(this) {}

BlobMemoryController::~BlobMemoryController() {}

void BlobMemoryController::DisableFilePaging(base::File::Error reason) {
  UMA_HISTOGRAM_ENUMERATION("Storage.Blob.PagingDisabled", -reason,
                            -File::FILE_ERROR_MAX);
  file_paging_enabled_ = false;
  in_flight_memory_used_ = 0;
  items_paging_to_file_.clear();
  pending_evictions_ = 0;
  pending_memory_quota_total_size_ = 0;
  populated_memory_items_.Clear();
  populated_memory_items_bytes_ = 0;
  file_runner_ = nullptr;

  PendingMemoryQuotaTaskList old_memory_tasks;
  PendingFileQuotaTaskList old_file_tasks;
  std::swap(old_memory_tasks, pending_memory_quota_tasks_);
  std::swap(old_file_tasks, pending_file_quota_tasks_);

  // Don't call the callbacks until we have a consistent state.
  for (auto& memory_request : old_memory_tasks) {
    memory_request->RunDoneCallback(false);
  }
  for (auto& file_request : old_file_tasks) {
    file_request->RunDoneCallback(false, std::vector<FileCreationInfo>());
  }
}

BlobMemoryController::Strategy BlobMemoryController::DetermineStrategy(
    size_t preemptive_transported_bytes,
    uint64_t total_transportation_bytes) const {
  if (total_transportation_bytes == 0)
    return Strategy::NONE_NEEDED;
  if (!CanReserveQuota(total_transportation_bytes))
    return Strategy::TOO_LARGE;

  // Handle the case where we have all the bytes preemptively transported, and
  // we can also fit them.
  if (preemptive_transported_bytes == total_transportation_bytes &&
      pending_memory_quota_tasks_.empty() &&
      preemptive_transported_bytes < GetAvailableMemoryForBlobs()) {
    return Strategy::NONE_NEEDED;
  }
  if (file_paging_enabled_ &&
      (total_transportation_bytes > limits_.memory_limit_before_paging())) {
    return Strategy::FILE;
  }
  if (total_transportation_bytes > limits_.max_ipc_memory_size)
    return Strategy::SHARED_MEMORY;
  return Strategy::IPC;
}

bool BlobMemoryController::CanReserveQuota(uint64_t size) const {
  // We check each size independently as a blob can't be constructed in both
  // disk and memory.
  return size <= GetAvailableMemoryForBlobs() ||
         size <= GetAvailableFileSpaceForBlobs();
}

base::WeakPtr<QuotaAllocationTask> BlobMemoryController::ReserveMemoryQuota(
    std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
    const MemoryQuotaRequestCallback& done_callback) {
  if (unreserved_memory_items.empty()) {
    done_callback.Run(true);
    return base::WeakPtr<QuotaAllocationTask>();
  }

  base::CheckedNumeric<uint64_t> unsafe_total_bytes_needed = 0;
  for (auto& item : unreserved_memory_items) {
    DCHECK_EQ(ShareableBlobDataItem::QUOTA_NEEDED, item->state());
    DCHECK(item->item()->type() == DataElement::TYPE_BYTES_DESCRIPTION ||
           item->item()->type() == DataElement::TYPE_BYTES);
    DCHECK(item->item()->length() > 0);
    unsafe_total_bytes_needed += item->item()->length();
    item->set_state(ShareableBlobDataItem::QUOTA_REQUESTED);
  }

  uint64_t total_bytes_needed = unsafe_total_bytes_needed.ValueOrDie();
  DCHECK_GT(total_bytes_needed, 0ull);

  // If we're currently waiting for blobs to page already, then we add
  // ourselves to the end of the queue. Once paging is complete, we'll schedule
  // more paging for any more pending blobs.
  if (!pending_memory_quota_tasks_.empty()) {
    return AppendMemoryTask(total_bytes_needed,
                            std::move(unreserved_memory_items), done_callback);
  }

  // Store right away if we can.
  if (total_bytes_needed <= GetAvailableMemoryForBlobs()) {
    GrantMemoryAllocations(&unreserved_memory_items,
                           static_cast<size_t>(total_bytes_needed));
    MaybeScheduleEvictionUntilSystemHealthy();
    done_callback.Run(true);
    return base::WeakPtr<QuotaAllocationTask>();
  }

  // Size is larger than available memory.
  DCHECK(pending_memory_quota_tasks_.empty());
  DCHECK_EQ(0u, pending_memory_quota_total_size_);

  auto weak_ptr = AppendMemoryTask(
      total_bytes_needed, std::move(unreserved_memory_items), done_callback);
  MaybeScheduleEvictionUntilSystemHealthy();
  return weak_ptr;
}

base::WeakPtr<QuotaAllocationTask> BlobMemoryController::ReserveFileQuota(
    std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_file_items,
    const FileQuotaRequestCallback& done_callback) {
  pending_file_quota_tasks_.push_back(base::MakeUnique<FileQuotaAllocationTask>(
      this, std::move(unreserved_file_items), done_callback));
  pending_file_quota_tasks_.back()->set_my_list_position(
      --pending_file_quota_tasks_.end());
  return pending_file_quota_tasks_.back()->GetWeakPtr();
}

void BlobMemoryController::NotifyMemoryItemsUsed(
    const std::vector<scoped_refptr<ShareableBlobDataItem>>& items) {
  for (const auto& item : items) {
    if (item->item()->type() != DataElement::TYPE_BYTES ||
        item->state() != ShareableBlobDataItem::POPULATED_WITH_QUOTA) {
      continue;
    }
    // We don't want to re-add the item if we're currently paging it to disk.
    if (items_paging_to_file_.find(item->item_id()) !=
        items_paging_to_file_.end()) {
      return;
    }
    auto iterator = populated_memory_items_.Get(item->item_id());
    if (iterator == populated_memory_items_.end()) {
      populated_memory_items_bytes_ +=
          static_cast<size_t>(item->item()->length());
      populated_memory_items_.Put(item->item_id(), item.get());
    }
  }
  MaybeScheduleEvictionUntilSystemHealthy();
}

base::WeakPtr<QuotaAllocationTask> BlobMemoryController::AppendMemoryTask(
    uint64_t total_bytes_needed,
    std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
    const MemoryQuotaRequestCallback& done_callback) {
  DCHECK(file_paging_enabled_)
      << "Caller tried to reserve memory when CanReserveQuota("
      << total_bytes_needed << ") would have returned false.";

  pending_memory_quota_total_size_ += total_bytes_needed;
  pending_memory_quota_tasks_.push_back(
      base::MakeUnique<MemoryQuotaAllocationTask>(
          this, total_bytes_needed, std::move(unreserved_memory_items),
          std::move(done_callback)));
  pending_memory_quota_tasks_.back()->set_my_list_position(
      --pending_memory_quota_tasks_.end());

  return pending_memory_quota_tasks_.back()->GetWeakPtr();
}

void BlobMemoryController::MaybeGrantPendingMemoryRequests() {
  while (!pending_memory_quota_tasks_.empty() &&
         limits_.max_blob_in_memory_space - blob_memory_used_ >=
             pending_memory_quota_tasks_.front()->allocation_size()) {
    std::unique_ptr<MemoryQuotaAllocationTask> memory_task =
        std::move(pending_memory_quota_tasks_.front());
    pending_memory_quota_tasks_.pop_front();
    pending_memory_quota_total_size_ -= memory_task->allocation_size();
    memory_task->RunDoneCallback(true);
  }
  RecordTracingCounters();
}

size_t BlobMemoryController::CollectItemsForEviction(
    std::vector<scoped_refptr<ShareableBlobDataItem>>* output) {
  base::CheckedNumeric<size_t> total_items_size = 0;
  // Process the recent item list and remove items until we have at least a
  // minimum file size or we're at the end of our items to page to disk.
  while (total_items_size.ValueOrDie() < limits_.min_page_file_size &&
         !populated_memory_items_.empty()) {
    auto iterator = --populated_memory_items_.end();
    ShareableBlobDataItem* item = iterator->second;
    DCHECK_EQ(item->item()->type(), DataElement::TYPE_BYTES);
    populated_memory_items_.Erase(iterator);
    size_t size = base::checked_cast<size_t>(item->item()->length());
    populated_memory_items_bytes_ -= size;
    total_items_size += size;
    output->push_back(make_scoped_refptr(item));
  }
  return total_items_size.ValueOrDie();
}

void BlobMemoryController::MaybeScheduleEvictionUntilSystemHealthy() {
  // Don't do eviction when others are happening, as we don't change our
  // pending_memory_quota_total_size_ value until after the paging files have
  // been written.
  if (pending_evictions_ != 0 || !file_paging_enabled_)
    return;

  // We try to page items to disk until our current system size + requested
  // memory is below our size limit.
  while (pending_memory_quota_total_size_ + blob_memory_used_ >
         limits_.memory_limit_before_paging()) {
    // We only page when we have enough items to fill a whole page file.
    if (populated_memory_items_bytes_ < limits_.min_page_file_size)
      break;
    DCHECK_LE(limits_.min_page_file_size,
              static_cast<uint64_t>(blob_memory_used_));

    std::vector<scoped_refptr<ShareableBlobDataItem>> items_to_swap;
    size_t total_items_size = CollectItemsForEviction(&items_to_swap);
    if (total_items_size == 0)
      break;

    std::vector<DataElement*> items_for_paging;
    for (auto& shared_blob_item : items_to_swap) {
      items_paging_to_file_.insert(shared_blob_item->item_id());
      items_for_paging.push_back(shared_blob_item->item()->data_element_ptr());
    }

    // Update our bookkeeping.
    pending_evictions_++;
    disk_used_ += total_items_size;
    in_flight_memory_used_ += total_items_size;

    // Create our file reference.
    FilePath page_file_path = GenerateNextPageFileName();
    scoped_refptr<ShareableFileReference> file_reference =
        ShareableFileReference::GetOrCreate(
            page_file_path,
            ShareableFileReference::DELETE_ON_FINAL_RELEASE,
            file_runner_.get());
    // Add the release callback so we decrement our disk usage on file deletion.
    file_reference->AddFinalReleaseCallback(
        base::Bind(&BlobMemoryController::OnBlobFileDelete,
                   weak_factory_.GetWeakPtr(), total_items_size));

    // Post the file writing task.
    base::PostTaskAndReplyWithResult(
        file_runner_.get(), FROM_HERE,
        base::Bind(&CreateFileAndWriteItems, blob_storage_dir_,
                   base::Passed(&page_file_path), file_runner_,
                   base::Passed(&items_for_paging), total_items_size),
        base::Bind(&BlobMemoryController::OnEvictionComplete,
                   weak_factory_.GetWeakPtr(), base::Passed(&file_reference),
                   base::Passed(&items_to_swap), total_items_size));
  }
  RecordTracingCounters();
}

void BlobMemoryController::OnEvictionComplete(
    scoped_refptr<ShareableFileReference> file_reference,
    std::vector<scoped_refptr<ShareableBlobDataItem>> items,
    size_t total_items_size,
    FileCreationInfo result) {
  if (!file_paging_enabled_)
    return;

  if (result.error != File::FILE_OK) {
    DisableFilePaging(result.error);
    return;
  }

  DCHECK_LT(0, pending_evictions_);
  pending_evictions_--;

  // Switch item from memory to the new file.
  uint64_t offset = 0;
  for (const scoped_refptr<ShareableBlobDataItem>& shareable_item : items) {
    scoped_refptr<BlobDataItem> new_item(
        new BlobDataItem(base::WrapUnique(new DataElement()), file_reference));
    new_item->data_element_ptr()->SetToFilePathRange(
        file_reference->path(), offset, shareable_item->item()->length(),
        result.last_modified);
    DCHECK(shareable_item->memory_allocation_);
    shareable_item->set_memory_allocation(nullptr);
    shareable_item->set_item(new_item);
    items_paging_to_file_.erase(shareable_item->item_id());
    offset += shareable_item->item()->length();
  }
  in_flight_memory_used_ -= total_items_size;

  // We want callback on blobs up to the amount we've freed.
  MaybeGrantPendingMemoryRequests();

  // If we still have more blobs waiting and we're not waiting on more paging
  // operations, schedule more.
  MaybeScheduleEvictionUntilSystemHealthy();
}

FilePath BlobMemoryController::GenerateNextPageFileName() {
  std::string file_name = base::Uint64ToString(current_file_num_++);
  return blob_storage_dir_.Append(FilePath::FromUTF8Unsafe(file_name));
}

void BlobMemoryController::RecordTracingCounters() const {
  TRACE_COUNTER2("Blob", "MemoryUsage", "TotalStorage", blob_memory_used_,
                 "InFlightToDisk", in_flight_memory_used_);
  TRACE_COUNTER1("Blob", "DiskUsage", disk_used_);
  TRACE_COUNTER1("Blob", "TranfersPendingOnDisk",
                 pending_memory_quota_tasks_.size());
  TRACE_COUNTER1("Blob", "TranfersBytesPendingOnDisk",
                 pending_memory_quota_total_size_);
}

size_t BlobMemoryController::GetAvailableMemoryForBlobs() const {
  if (limits_.max_blob_in_memory_space < memory_usage())
    return 0;
  return limits_.max_blob_in_memory_space - memory_usage();
}

uint64_t BlobMemoryController::GetAvailableFileSpaceForBlobs() const {
  if (!file_paging_enabled_)
    return 0;
  // Sometimes we're only paging part of what we need for the new blob, so add
  // the rest of the size we need into our disk usage if this is the case.
  uint64_t total_disk_used = disk_used_;
  if (in_flight_memory_used_ < pending_memory_quota_total_size_) {
    total_disk_used +=
        pending_memory_quota_total_size_ - in_flight_memory_used_;
  }
  if (limits_.max_blob_disk_space < total_disk_used)
    return 0;
  return limits_.max_blob_disk_space - total_disk_used;
}

void BlobMemoryController::GrantMemoryAllocations(
    std::vector<scoped_refptr<ShareableBlobDataItem>>* items,
    size_t total_bytes) {
  // These metrics let us calculate the global distribution of blob storage by
  // subtracting the histograms.
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeBeforeAppend",
                       blob_memory_used_ / 1024);
  blob_memory_used_ += total_bytes;
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeAfterAppend",
                       blob_memory_used_ / 1024);

  for (auto& item : *items) {
    item->set_state(ShareableBlobDataItem::QUOTA_GRANTED);
    item->set_memory_allocation(base::MakeUnique<MemoryAllocation>(
        weak_factory_.GetWeakPtr(), item->item_id(),
        base::checked_cast<size_t>(item->item()->length())));
  }
}

void BlobMemoryController::RevokeMemoryAllocation(uint64_t item_id,
                                                  size_t length) {
  DCHECK_LE(length, blob_memory_used_);

  // These metrics let us calculate the global distribution of blob storage by
  // subtracting the histograms.
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeBeforeAppend",
                       blob_memory_used_ / 1024);
  blob_memory_used_ -= length;
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeAfterAppend",
                       blob_memory_used_ / 1024);

  auto iterator = populated_memory_items_.Get(item_id);
  if (iterator != populated_memory_items_.end()) {
    DCHECK_GE(populated_memory_items_bytes_, length);
    populated_memory_items_bytes_ -= length;
    populated_memory_items_.Erase(iterator);
  }
  MaybeGrantPendingMemoryRequests();
}

void BlobMemoryController::OnBlobFileDelete(uint64_t size,
                                            const FilePath& path) {
  DCHECK_LE(size, disk_used_);
  disk_used_ -= size;
}

}  // namespace storage
