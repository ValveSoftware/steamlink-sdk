// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_builder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace storage {

namespace {

const static int kInvalidDiskCacheSideStreamIndex = -1;

}  // namespace

const char BlobDataBuilder::kAppendFutureFileTemporaryFileName[] =
    "kFakeFilenameToBeChangedByPopulateFutureFile";

BlobDataBuilder::BlobDataBuilder(const std::string& uuid) : uuid_(uuid) {
}
BlobDataBuilder::~BlobDataBuilder() {
}

void BlobDataBuilder::AppendIPCDataElement(const DataElement& ipc_data) {
  uint64_t length = ipc_data.length();
  switch (ipc_data.type()) {
    case DataElement::TYPE_BYTES:
      DCHECK(!ipc_data.offset());
      AppendData(ipc_data.bytes(),
                 base::checked_cast<size_t, uint64_t>(length));
      break;
    case DataElement::TYPE_FILE:
      AppendFile(ipc_data.path(), ipc_data.offset(), length,
                 ipc_data.expected_modification_time());
      break;
    case DataElement::TYPE_FILE_FILESYSTEM:
      AppendFileSystemFile(ipc_data.filesystem_url(), ipc_data.offset(), length,
                           ipc_data.expected_modification_time());
      break;
    case DataElement::TYPE_BLOB:
      // This is a temporary item that will be deconstructed later in
      // BlobStorageContext.
      AppendBlob(ipc_data.blob_uuid(), ipc_data.offset(), ipc_data.length());
      break;
    case DataElement::TYPE_BYTES_DESCRIPTION:
    case DataElement::TYPE_UNKNOWN:
    case DataElement::TYPE_DISK_CACHE_ENTRY:  // This type can't be sent by IPC.
      NOTREACHED();
      break;
  }
}

void BlobDataBuilder::AppendData(const char* data, size_t length) {
  if (!length)
    return;
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToBytes(data, length);
  items_.push_back(new BlobDataItem(std::move(element)));
}

size_t BlobDataBuilder::AppendFutureData(size_t length) {
  CHECK_NE(length, 0u);
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToBytesDescription(length);
  items_.push_back(new BlobDataItem(std::move(element)));
  return items_.size() - 1;
}

bool BlobDataBuilder::PopulateFutureData(size_t index,
                                         const char* data,
                                         size_t offset,
                                         size_t length) {
  DCHECK_LT(index, items_.size());
  DCHECK(data);
  DataElement* element = items_.at(index)->data_element_ptr();

  // We lazily allocate our data buffer by waiting until the first
  // PopulateFutureData call.
  // Why? The reason we have the AppendFutureData  method is to create our Blob
  // record when the Renderer tells us about the blob without actually
  // allocating the memory yet, as we might not have the quota yet. So we don't
  // want to allocate the memory until we're actually receiving the data (which
  // the browser process only does when it has quota).
  if (element->type() == DataElement::TYPE_BYTES_DESCRIPTION) {
    element->SetToAllocatedBytes(element->length());
    // The type of the element is now TYPE_BYTES.
  }
  if (element->type() != DataElement::TYPE_BYTES) {
    DVLOG(1) << "Invalid item type.";
    return false;
  }
  base::CheckedNumeric<size_t> checked_end = offset;
  checked_end += length;
  if (!checked_end.IsValid() || checked_end.ValueOrDie() > element->length()) {
    DVLOG(1) << "Invalid offset or length.";
    return false;
  }
  std::memcpy(element->mutable_bytes() + offset, data, length);
  return true;
}

size_t BlobDataBuilder::AppendFutureFile(uint64_t offset, uint64_t length) {
  CHECK_NE(length, 0ull);
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToFilePathRange(base::FilePath::FromUTF8Unsafe(std::string(
                                  kAppendFutureFileTemporaryFileName)),
                              offset, length, base::Time());
  items_.push_back(new BlobDataItem(std::move(element)));
  return items_.size() - 1;
}

bool BlobDataBuilder::PopulateFutureFile(
    size_t index,
    const scoped_refptr<ShareableFileReference>& file_reference,
    const base::Time& expected_modification_time) {
  DCHECK_LT(index, items_.size());
  DataElement* old_element = items_.at(index)->data_element_ptr();

  if (old_element->type() != DataElement::TYPE_FILE) {
    DVLOG(1) << "Invalid item type.";
    return false;
  } else if (old_element->path().AsUTF8Unsafe() !=
             std::string(kAppendFutureFileTemporaryFileName)) {
    DVLOG(1) << "Item not created by AppendFutureFile";
    return false;
  }
  uint64_t length = old_element->length();
  uint64_t offset = old_element->offset();
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToFilePathRange(file_reference->path(), offset, length,
                              expected_modification_time);
  items_[index] = new BlobDataItem(std::move(element), file_reference);
  return true;
}

void BlobDataBuilder::AppendFile(const base::FilePath& file_path,
                                 uint64_t offset,
                                 uint64_t length,
                                 const base::Time& expected_modification_time) {
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToFilePathRange(file_path, offset, length,
                              expected_modification_time);
  items_.push_back(new BlobDataItem(std::move(element),
                                    ShareableFileReference::Get(file_path)));
}

void BlobDataBuilder::AppendBlob(const std::string& uuid,
                                 uint64_t offset,
                                 uint64_t length) {
  DCHECK_GT(length, 0ul);
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToBlobRange(uuid, offset, length);
  items_.push_back(new BlobDataItem(std::move(element)));
}

void BlobDataBuilder::AppendBlob(const std::string& uuid) {
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToBlob(uuid);
  items_.push_back(new BlobDataItem(std::move(element)));
}

void BlobDataBuilder::AppendFileSystemFile(
    const GURL& url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  DCHECK_GT(length, 0ul);
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToFileSystemUrlRange(url, offset, length,
                                   expected_modification_time);
  items_.push_back(new BlobDataItem(std::move(element)));
}

void BlobDataBuilder::AppendDiskCacheEntry(
    const scoped_refptr<DataHandle>& data_handle,
    disk_cache::Entry* disk_cache_entry,
    int disk_cache_stream_index) {
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToDiskCacheEntryRange(
      0U, disk_cache_entry->GetDataSize(disk_cache_stream_index));
  items_.push_back(new BlobDataItem(std::move(element), data_handle,
                                    disk_cache_entry, disk_cache_stream_index,
                                    kInvalidDiskCacheSideStreamIndex));
}

void BlobDataBuilder::AppendDiskCacheEntryWithSideData(
    const scoped_refptr<DataHandle>& data_handle,
    disk_cache::Entry* disk_cache_entry,
    int disk_cache_stream_index,
    int disk_cache_side_stream_index) {
  std::unique_ptr<DataElement> element(new DataElement());
  element->SetToDiskCacheEntryRange(
      0U, disk_cache_entry->GetDataSize(disk_cache_stream_index));
  items_.push_back(new BlobDataItem(std::move(element), data_handle,
                                    disk_cache_entry, disk_cache_stream_index,
                                    disk_cache_side_stream_index));
}

void BlobDataBuilder::Clear() {
  items_.clear();
  content_disposition_.clear();
  content_type_.clear();
  uuid_.clear();
}

void PrintTo(const BlobDataBuilder& x, std::ostream* os) {
  DCHECK(os);
  *os << "<BlobDataBuilder>{uuid: " << x.uuid()
      << ", content_type: " << x.content_type_
      << ", content_disposition: " << x.content_disposition_ << ", items: [";
  for (const auto& item : x.items_) {
    PrintTo(*item, os);
    *os << ", ";
  }
  *os << "]}";
}

}  // namespace storage
