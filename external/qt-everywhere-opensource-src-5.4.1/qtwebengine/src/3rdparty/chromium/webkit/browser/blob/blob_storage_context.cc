// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/blob_storage_context.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "url/gurl.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/common/blob/blob_data.h"

namespace webkit_blob {

namespace {

// We can't use GURL directly for these hash fragment manipulations
// since it doesn't have specific knowlege of the BlobURL format. GURL
// treats BlobURLs as if they were PathURLs which don't support hash
// fragments.

bool BlobUrlHasRef(const GURL& url) {
  return url.spec().find('#') != std::string::npos;
}

GURL ClearBlobUrlRef(const GURL& url) {
  size_t hash_pos = url.spec().find('#');
  if (hash_pos == std::string::npos)
    return url;
  return GURL(url.spec().substr(0, hash_pos));
}

// TODO(michaeln): use base::SysInfo::AmountOfPhysicalMemoryMB() in some
// way to come up with a better limit.
static const int64 kMaxMemoryUsage = 500 * 1024 * 1024;  // Half a gig.

}  // namespace

BlobStorageContext::BlobMapEntry::BlobMapEntry()
    : refcount(0), flags(0) {
}

BlobStorageContext::BlobMapEntry::BlobMapEntry(
    int refcount, int flags, BlobData* data)
    : refcount(refcount), flags(flags), data(data) {
}

BlobStorageContext::BlobMapEntry::~BlobMapEntry() {
}

BlobStorageContext::BlobStorageContext()
    : memory_usage_(0) {
}

BlobStorageContext::~BlobStorageContext() {
}

scoped_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromUUID(
    const std::string& uuid) {
  scoped_ptr<BlobDataHandle> result;
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return result.Pass();
  if (found->second.flags & EXCEEDED_MEMORY)
    return result.Pass();
  DCHECK(!(found->second.flags & BEING_BUILT));
  result.reset(new BlobDataHandle(
      found->second.data.get(), this, base::MessageLoopProxy::current().get()));
  return result.Pass();
}

scoped_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromPublicURL(
    const GURL& url) {
  BlobURLMap::iterator found = public_blob_urls_.find(
      BlobUrlHasRef(url) ? ClearBlobUrlRef(url) : url);
  if (found == public_blob_urls_.end())
    return scoped_ptr<BlobDataHandle>();
  return GetBlobDataFromUUID(found->second);
}

scoped_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobData* data) {
  StartBuildingBlob(data->uuid());
  for (std::vector<BlobData::Item>::const_iterator iter =
           data->items().begin();
       iter != data->items().end(); ++iter) {
    AppendBlobDataItem(data->uuid(), *iter);
  }
  FinishBuildingBlob(data->uuid(), data->content_type());
  scoped_ptr<BlobDataHandle> handle = GetBlobDataFromUUID(data->uuid());
  DecrementBlobRefCount(data->uuid());
  return handle.Pass();
}

bool BlobStorageContext::RegisterPublicBlobURL(
    const GURL& blob_url, const std::string& uuid) {
  DCHECK(!BlobUrlHasRef(blob_url));
  DCHECK(IsInUse(uuid));
  DCHECK(!IsUrlRegistered(blob_url));
  if (!IsInUse(uuid) || IsUrlRegistered(blob_url))
    return false;
  IncrementBlobRefCount(uuid);
  public_blob_urls_[blob_url] = uuid;
  return true;
}

void BlobStorageContext::RevokePublicBlobURL(const GURL& blob_url) {
  DCHECK(!BlobUrlHasRef(blob_url));
  if (!IsUrlRegistered(blob_url))
    return;
  DecrementBlobRefCount(public_blob_urls_[blob_url]);
  public_blob_urls_.erase(blob_url);
}

void BlobStorageContext::StartBuildingBlob(const std::string& uuid) {
  DCHECK(!IsInUse(uuid) && !uuid.empty());
  blob_map_[uuid] = BlobMapEntry(1, BEING_BUILT, new BlobData(uuid));
}

void BlobStorageContext::AppendBlobDataItem(
    const std::string& uuid, const BlobData::Item& item) {
  DCHECK(IsBeingBuilt(uuid));
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  if (found->second.flags & EXCEEDED_MEMORY)
    return;
  BlobData* target_blob_data = found->second.data.get();
  DCHECK(target_blob_data);

  bool exceeded_memory = false;

  // The blob data is stored in the canonical way which only contains a
  // list of Data, File, and FileSystem items. Aggregated TYPE_BLOB items
  // are expanded into the primitive constituent types.
  // 1) The Data item is denoted by the raw data and length.
  // 2) The File item is denoted by the file path, the range and the expected
  //    modification time.
  // 3) The FileSystem File item is denoted by the FileSystem URL, the range
  //    and the expected modification time.
  // 4) The Blob items are expanded.
  // TODO(michaeln): Would be nice to avoid copying Data items when expanding.

  DCHECK(item.length() > 0);
  switch (item.type()) {
    case BlobData::Item::TYPE_BYTES:
      DCHECK(!item.offset());
      exceeded_memory = !AppendBytesItem(target_blob_data,
                                         item.bytes(),
                                         static_cast<int64>(item.length()));
      break;
    case BlobData::Item::TYPE_FILE:
      AppendFileItem(target_blob_data,
                     item.path(),
                     item.offset(),
                     item.length(),
                     item.expected_modification_time());
      break;
    case BlobData::Item::TYPE_FILE_FILESYSTEM:
      AppendFileSystemFileItem(target_blob_data,
                               item.filesystem_url(),
                               item.offset(),
                               item.length(),
                               item.expected_modification_time());
      break;
    case BlobData::Item::TYPE_BLOB: {
      scoped_ptr<BlobDataHandle> src = GetBlobDataFromUUID(item.blob_uuid());
      if (src)
        exceeded_memory = !ExpandStorageItems(target_blob_data,
                                              src->data(),
                                              item.offset(),
                                              item.length());
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  // If we're using too much memory, drop this blob's data.
  // TODO(michaeln): Blob memory storage does not yet spill over to disk,
  // as a stop gap, we'll prevent memory usage over a max amount.
  if (exceeded_memory) {
    memory_usage_ -= target_blob_data->GetMemoryUsage();
    found->second.flags |= EXCEEDED_MEMORY;
    found->second.data = new BlobData(uuid);
    return;
  }
}

void BlobStorageContext::FinishBuildingBlob(
    const std::string& uuid, const std::string& content_type) {
  DCHECK(IsBeingBuilt(uuid));
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  found->second.data->set_content_type(content_type);
  found->second.flags &= ~BEING_BUILT;
}

void BlobStorageContext::CancelBuildingBlob(const std::string& uuid) {
  DCHECK(IsBeingBuilt(uuid));
  DecrementBlobRefCount(uuid);
}

void BlobStorageContext::IncrementBlobRefCount(const std::string& uuid) {
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end()) {
    DCHECK(false);
    return;
  }
  ++(found->second.refcount);
}

void BlobStorageContext::DecrementBlobRefCount(const std::string& uuid) {
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  DCHECK_EQ(found->second.data->uuid(), uuid);
  if (--(found->second.refcount) == 0) {
    memory_usage_ -= found->second.data->GetMemoryUsage();
    blob_map_.erase(found);
  }
}

bool BlobStorageContext::ExpandStorageItems(
    BlobData* target_blob_data, BlobData* src_blob_data,
    uint64 offset, uint64 length) {
  DCHECK(target_blob_data && src_blob_data &&
         length != static_cast<uint64>(-1));

  std::vector<BlobData::Item>::const_iterator iter =
      src_blob_data->items().begin();
  if (offset) {
    for (; iter != src_blob_data->items().end(); ++iter) {
      if (offset >= iter->length())
        offset -= iter->length();
      else
        break;
    }
  }

  for (; iter != src_blob_data->items().end() && length > 0; ++iter) {
    uint64 current_length = iter->length() - offset;
    uint64 new_length = current_length > length ? length : current_length;
    if (iter->type() == BlobData::Item::TYPE_BYTES) {
      if (!AppendBytesItem(
              target_blob_data,
              iter->bytes() + static_cast<size_t>(iter->offset() + offset),
              static_cast<int64>(new_length))) {
        return false;  // exceeded memory
      }
    } else if (iter->type() == BlobData::Item::TYPE_FILE) {
      AppendFileItem(target_blob_data,
                     iter->path(),
                     iter->offset() + offset,
                     new_length,
                     iter->expected_modification_time());
    } else {
      DCHECK(iter->type() == BlobData::Item::TYPE_FILE_FILESYSTEM);
      AppendFileSystemFileItem(target_blob_data,
                               iter->filesystem_url(),
                               iter->offset() + offset,
                               new_length,
                               iter->expected_modification_time());
    }
    length -= new_length;
    offset = 0;
  }
  return true;
}

bool BlobStorageContext::AppendBytesItem(
    BlobData* target_blob_data, const char* bytes, int64 length) {
  if (length < 0) {
    DCHECK(false);
    return false;
  }
  if (memory_usage_ + length > kMaxMemoryUsage)
    return false;
  target_blob_data->AppendData(bytes, static_cast<size_t>(length));
  memory_usage_ += length;
  return true;
}

void BlobStorageContext::AppendFileItem(
    BlobData* target_blob_data,
    const base::FilePath& file_path, uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  target_blob_data->AppendFile(file_path, offset, length,
                               expected_modification_time);

  // It may be a temporary file that should be deleted when no longer needed.
  scoped_refptr<ShareableFileReference> shareable_file =
      ShareableFileReference::Get(file_path);
  if (shareable_file.get())
    target_blob_data->AttachShareableFileReference(shareable_file.get());
}

void BlobStorageContext::AppendFileSystemFileItem(
    BlobData* target_blob_data,
    const GURL& filesystem_url, uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  target_blob_data->AppendFileSystemFile(filesystem_url, offset, length,
                                         expected_modification_time);
}

bool BlobStorageContext::IsInUse(const std::string& uuid) {
  return blob_map_.find(uuid) != blob_map_.end();
}

bool BlobStorageContext::IsBeingBuilt(const std::string& uuid) {
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return false;
  return found->second.flags & BEING_BUILT;
}

bool BlobStorageContext::IsUrlRegistered(const GURL& blob_url) {
  return public_blob_urls_.find(blob_url) != public_blob_urls_.end();
}

}  // namespace webkit_blob
