// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "url/gurl.h"

namespace storage {
using BlobRegistryEntry = BlobStorageRegistry::Entry;
using BlobState = BlobStorageRegistry::BlobState;

BlobStorageContext::BlobStorageContext() : memory_usage_(0) {}

BlobStorageContext::~BlobStorageContext() {
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromUUID(
    const std::string& uuid) {
  BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  if (!entry) {
    return nullptr;
  }
  return base::WrapUnique(
      new BlobDataHandle(uuid, entry->content_type, entry->content_disposition,
                         this, base::ThreadTaskRunnerHandle::Get().get()));
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromPublicURL(
    const GURL& url) {
  std::string uuid;
  BlobRegistryEntry* entry = registry_.GetEntryFromURL(url, &uuid);
  if (!entry) {
    return nullptr;
  }
  return base::WrapUnique(
      new BlobDataHandle(uuid, entry->content_type, entry->content_disposition,
                         this, base::ThreadTaskRunnerHandle::Get().get()));
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobDataBuilder& external_builder) {
  TRACE_EVENT0("Blob", "Context::AddFinishedBlob");
  CreatePendingBlob(external_builder.uuid(), external_builder.content_type_,
                    external_builder.content_disposition_);
  CompletePendingBlob(external_builder);
  std::unique_ptr<BlobDataHandle> handle =
      GetBlobDataFromUUID(external_builder.uuid_);
  DecrementBlobRefCount(external_builder.uuid_);
  return handle;
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobDataBuilder* builder) {
  DCHECK(builder);
  return AddFinishedBlob(*builder);
}

bool BlobStorageContext::RegisterPublicBlobURL(const GURL& blob_url,
                                               const std::string& uuid) {
  if (!registry_.CreateUrlMapping(blob_url, uuid)) {
    return false;
  }
  IncrementBlobRefCount(uuid);
  return true;
}

void BlobStorageContext::RevokePublicBlobURL(const GURL& blob_url) {
  std::string uuid;
  if (!registry_.DeleteURLMapping(blob_url, &uuid)) {
    return;
  }
  DecrementBlobRefCount(uuid);
}

void BlobStorageContext::CreatePendingBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition) {
  DCHECK(!registry_.GetEntry(uuid) && !uuid.empty());
  registry_.CreateEntry(uuid, content_type, content_disposition);
}

void BlobStorageContext::CompletePendingBlob(
    const BlobDataBuilder& external_builder) {
  BlobRegistryEntry* entry = registry_.GetEntry(external_builder.uuid());
  DCHECK(entry);
  DCHECK(!entry->data.get()) << "Blob already constructed: "
                             << external_builder.uuid();
  // We want to handle storing our broken blob as well.
  switch (entry->state) {
    case BlobState::PENDING: {
      entry->data_builder.reset(new InternalBlobData::Builder());
      InternalBlobData::Builder* internal_data_builder =
          entry->data_builder.get();

      bool broken = false;
      for (const auto& blob_item : external_builder.items_) {
        IPCBlobCreationCancelCode error_code;
        if (!AppendAllocatedBlobItem(external_builder.uuid_, blob_item,
                                     internal_data_builder, &error_code)) {
          broken = true;
          memory_usage_ -= entry->data_builder->GetNonsharedMemoryUsage();
          entry->state = BlobState::BROKEN;
          entry->broken_reason = error_code;
          entry->data_builder.reset(new InternalBlobData::Builder());
          break;
        }
      }
      entry->data = entry->data_builder->Build();
      entry->data_builder.reset();
      entry->state = broken ? BlobState::BROKEN : BlobState::COMPLETE;
      break;
    }
    case BlobState::BROKEN: {
      InternalBlobData::Builder builder;
      entry->data = builder.Build();
      break;
    }
    case BlobState::COMPLETE:
      DCHECK(false) << "Blob already constructed: " << external_builder.uuid();
      return;
  }

  UMA_HISTOGRAM_COUNTS("Storage.Blob.ItemCount", entry->data->items().size());
  UMA_HISTOGRAM_BOOLEAN("Storage.Blob.Broken",
                        entry->state == BlobState::BROKEN);
  if (entry->state == BlobState::BROKEN) {
    UMA_HISTOGRAM_ENUMERATION(
        "Storage.Blob.BrokenReason", static_cast<int>(entry->broken_reason),
        (static_cast<int>(IPCBlobCreationCancelCode::LAST) + 1));
  }
  size_t total_memory = 0, nonshared_memory = 0;
  entry->data->GetMemoryUsage(&total_memory, &nonshared_memory);
  UMA_HISTOGRAM_COUNTS("Storage.Blob.TotalSize", total_memory / 1024);
  UMA_HISTOGRAM_COUNTS("Storage.Blob.TotalUnsharedSize",
                       nonshared_memory / 1024);
  TRACE_COUNTER1("Blob", "MemoryStoreUsageBytes", memory_usage_);

  auto runner = base::ThreadTaskRunnerHandle::Get();
  for (const auto& callback : entry->build_completion_callbacks) {
    runner->PostTask(FROM_HERE,
                     base::Bind(callback, entry->state == BlobState::COMPLETE,
                                entry->broken_reason));
  }
  entry->build_completion_callbacks.clear();
}

void BlobStorageContext::CancelPendingBlob(const std::string& uuid,
                                           IPCBlobCreationCancelCode reason) {
  BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry && entry->state == BlobState::PENDING);
  entry->state = BlobState::BROKEN;
  entry->broken_reason = reason;
  CompletePendingBlob(BlobDataBuilder(uuid));
}

void BlobStorageContext::IncrementBlobRefCount(const std::string& uuid) {
  BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  ++(entry->refcount);
}

void BlobStorageContext::DecrementBlobRefCount(const std::string& uuid) {
  BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  DCHECK_GT(entry->refcount, 0u);
  if (--(entry->refcount) == 0) {
    size_t memory_freeing = 0;
    if (entry->state == BlobState::COMPLETE) {
      memory_freeing = entry->data->GetUnsharedMemoryUsage();
      entry->data->RemoveBlobFromShareableItems(uuid);
    }
    DCHECK_LE(memory_freeing, memory_usage_);
    memory_usage_ -= memory_freeing;
    registry_.DeleteEntry(uuid);
  }
}

std::unique_ptr<BlobDataSnapshot> BlobStorageContext::CreateSnapshot(
    const std::string& uuid) {
  std::unique_ptr<BlobDataSnapshot> result;
  BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  if (entry->state != BlobState::COMPLETE) {
    return result;
  }

  const InternalBlobData& data = *entry->data;
  std::unique_ptr<BlobDataSnapshot> snapshot(new BlobDataSnapshot(
      uuid, entry->content_type, entry->content_disposition));
  snapshot->items_.reserve(data.items().size());
  for (const auto& shareable_item : data.items()) {
    snapshot->items_.push_back(shareable_item->item());
  }
  return snapshot;
}

bool BlobStorageContext::IsBroken(const std::string& uuid) const {
  const BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  if (!entry) {
    return true;
  }
  return entry->state == BlobState::BROKEN;
}

bool BlobStorageContext::IsBeingBuilt(const std::string& uuid) const {
  const BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  if (!entry) {
    return false;
  }
  return entry->state == BlobState::PENDING;
}

void BlobStorageContext::RunOnConstructionComplete(
    const std::string& uuid,
    const BlobConstructedCallback& done) {
  BlobRegistryEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  switch (entry->state) {
    case BlobState::COMPLETE:
      done.Run(true, IPCBlobCreationCancelCode::UNKNOWN);
      return;
    case BlobState::BROKEN:
      done.Run(false, entry->broken_reason);
      return;
    case BlobState::PENDING:
      entry->build_completion_callbacks.push_back(done);
      return;
  }
  NOTREACHED();
}

bool BlobStorageContext::AppendAllocatedBlobItem(
    const std::string& target_blob_uuid,
    scoped_refptr<BlobDataItem> blob_item,
    InternalBlobData::Builder* target_blob_builder,
    IPCBlobCreationCancelCode* error_code) {
  DCHECK(error_code);
  *error_code = IPCBlobCreationCancelCode::UNKNOWN;
  bool error = false;

  // The blob data is stored in the canonical way which only contains a
  // list of Data, File, and FileSystem items. Aggregated TYPE_BLOB items
  // are expanded into the primitive constituent types and reused if possible.
  // 1) The Data item is denoted by the raw data and length.
  // 2) The File item is denoted by the file path, the range and the expected
  //    modification time.
  // 3) The FileSystem File item is denoted by the FileSystem URL, the range
  //    and the expected modification time.
  // 4) The Blob item is denoted by the source blob and an offset and size.
  //    Internal items that are fully used by the new blob (not cut by the
  //    offset or size) are shared between the blobs.  Otherwise, the relevant
  //    portion of the item is copied.

  DCHECK(blob_item->data_element_ptr());
  const DataElement& data_element = blob_item->data_element();
  uint64_t length = data_element.length();
  uint64_t offset = data_element.offset();
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeBeforeAppend",
                       memory_usage_ / 1024);
  switch (data_element.type()) {
    case DataElement::TYPE_BYTES:
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.Bytes", length / 1024);
      DCHECK(!offset);
      if (memory_usage_ + length > kBlobStorageMaxMemoryUsage) {
        error = true;
        *error_code = IPCBlobCreationCancelCode::OUT_OF_MEMORY;
        break;
      }
      memory_usage_ += length;
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    case DataElement::TYPE_FILE: {
      bool full_file = (length == std::numeric_limits<uint64_t>::max());
      UMA_HISTOGRAM_BOOLEAN("Storage.BlobItemSize.File.Unknown", full_file);
      if (!full_file) {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.File",
                             (length - offset) / 1024);
      }
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    }
    case DataElement::TYPE_FILE_FILESYSTEM: {
      bool full_file = (length == std::numeric_limits<uint64_t>::max());
      UMA_HISTOGRAM_BOOLEAN("Storage.BlobItemSize.FileSystem.Unknown",
                            full_file);
      if (!full_file) {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.FileSystem",
                             (length - offset) / 1024);
      }
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    }
    case DataElement::TYPE_BLOB: {
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.Blob",
                           (length - offset) / 1024);
      // We grab the handle to ensure it stays around while we copy it.
      std::unique_ptr<BlobDataHandle> src =
          GetBlobDataFromUUID(data_element.blob_uuid());
      if (!src || src->IsBroken() || src->IsBeingBuilt()) {
        error = true;
        *error_code = IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN;
        break;
      }
      BlobRegistryEntry* other_entry =
          registry_.GetEntry(data_element.blob_uuid());
      DCHECK(other_entry->data);
      if (!AppendBlob(target_blob_uuid, *other_entry->data, offset, length,
                      target_blob_builder)) {
        error = true;
        *error_code = IPCBlobCreationCancelCode::OUT_OF_MEMORY;
      }
      break;
    }
    case DataElement::TYPE_DISK_CACHE_ENTRY: {
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.CacheEntry",
                           (length - offset) / 1024);
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    }
    case DataElement::TYPE_BYTES_DESCRIPTION:
    case DataElement::TYPE_UNKNOWN:
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeAfterAppend",
                       memory_usage_ / 1024);
  return !error;
}

bool BlobStorageContext::AppendBlob(
    const std::string& target_blob_uuid,
    const InternalBlobData& blob,
    uint64_t offset,
    uint64_t length,
    InternalBlobData::Builder* target_blob_builder) {
  DCHECK_GT(length, 0ull);

  const std::vector<scoped_refptr<ShareableBlobDataItem>>& items = blob.items();
  auto iter = items.begin();
  if (offset) {
    for (; iter != items.end(); ++iter) {
      const BlobDataItem& item = *(iter->get()->item());
      if (offset >= item.length())
        offset -= item.length();
      else
        break;
    }
  }

  for (; iter != items.end() && length > 0; ++iter) {
    scoped_refptr<ShareableBlobDataItem> shareable_item = iter->get();
    const BlobDataItem& item = *(shareable_item->item());
    uint64_t item_length = item.length();
    DCHECK_GT(item_length, offset);
    uint64_t current_length = item_length - offset;
    uint64_t new_length = current_length > length ? length : current_length;

    bool reusing_blob_item = offset == 0 && new_length == item.length();
    UMA_HISTOGRAM_BOOLEAN("Storage.Blob.ReusedItem", reusing_blob_item);
    if (reusing_blob_item) {
      shareable_item->referencing_blobs().insert(target_blob_uuid);
      target_blob_builder->AppendSharedBlobItem(shareable_item);
      length -= new_length;
      continue;
    }

    // We need to do copying of the items when we have a different offset or
    // length
    switch (item.type()) {
      case DataElement::TYPE_BYTES: {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.BlobSlice.Bytes",
                             new_length / 1024);
        if (memory_usage_ + new_length > kBlobStorageMaxMemoryUsage) {
          return false;
        }
        DCHECK(!item.offset());
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToBytes(item.bytes() + offset,
                            static_cast<int64_t>(new_length));
        memory_usage_ += new_length;
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid, new BlobDataItem(std::move(element))));
      } break;
      case DataElement::TYPE_FILE: {
        DCHECK_NE(item.length(), std::numeric_limits<uint64_t>::max())
            << "We cannot use a section of a file with an unknown length";
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.BlobSlice.File",
                             new_length / 1024);
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToFilePathRange(item.path(), item.offset() + offset,
                                    new_length,
                                    item.expected_modification_time());
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid,
            new BlobDataItem(std::move(element), item.data_handle_)));
      } break;
      case DataElement::TYPE_FILE_FILESYSTEM: {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.BlobSlice.FileSystem",
                             new_length / 1024);
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToFileSystemUrlRange(item.filesystem_url(),
                                         item.offset() + offset, new_length,
                                         item.expected_modification_time());
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid, new BlobDataItem(std::move(element))));
      } break;
      case DataElement::TYPE_DISK_CACHE_ENTRY: {
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToDiskCacheEntryRange(item.offset() + offset,
                                          new_length);
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid,
            new BlobDataItem(std::move(element), item.data_handle_,
                             item.disk_cache_entry(),
                             item.disk_cache_stream_index(),
                             item.disk_cache_side_stream_index())));
      } break;
      case DataElement::TYPE_BYTES_DESCRIPTION:
      case DataElement::TYPE_BLOB:
      case DataElement::TYPE_UNKNOWN:
        CHECK(false) << "Illegal blob item type: " << item.type();
    }
    length -= new_length;
    offset = 0;
  }
  return true;
}

}  // namespace storage
