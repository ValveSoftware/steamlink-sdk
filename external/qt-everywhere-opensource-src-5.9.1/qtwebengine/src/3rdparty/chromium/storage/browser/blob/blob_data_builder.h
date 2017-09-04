// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_DATA_BUILDER_H_
#define STORAGE_BROWSER_BLOB_BLOB_DATA_BUILDER_H_

#include <stddef.h>
#include <stdint.h>
#include <ostream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/storage_browser_export.h"

namespace disk_cache {
class Entry;
}

namespace storage {
class BlobStorageContext;
class ShareableFileReference;

// This class is used to build blobs. It also facilitates the operation of
// 'pending' data, where the user knows the size and existence of a file or
// bytes item, but we don't have the memory or file yet. See AppendFuture* and
// PopulateFuture* methods for more description. Use
// BlobDataHandle::GetBlobStatus to check for an error after creating the blob.
class STORAGE_EXPORT BlobDataBuilder {
 public:
  using DataHandle = BlobDataItem::DataHandle;
  // Visible for testing.
  static base::FilePath GetFutureFileItemPath(uint64_t file_id);

  // Returns if the given item was created by AppendFutureFile.
  static bool IsFutureFileItem(const DataElement& element);
  // Returns |file_id| given to AppendFutureFile.
  static uint64_t GetFutureFileID(const DataElement& element);

  explicit BlobDataBuilder(const std::string& uuid);
  BlobDataBuilder(BlobDataBuilder&&);
  BlobDataBuilder& operator=(BlobDataBuilder&&);
  ~BlobDataBuilder();

  const std::string& uuid() const { return uuid_; }

  // Validates the data element that was sent over IPC, and copies the data if
  // it's a 'bytes' element. Data elements of BYTES_DESCRIPTION or
  // DISK_CACHE_ENTRY types are not valid IPC data element types, and cannot be
  // given to this method.
  void AppendIPCDataElement(const DataElement& ipc_data);

  // Copies the given data into the blob.
  void AppendData(const std::string& data) {
    AppendData(data.c_str(), data.size());
  }

  // Copies the given data into the blob.
  void AppendData(const char* data, size_t length);

  // Adds an item that is flagged for future data population. The memory is not
  // allocated until the first call to PopulateFutureData. Returns the index of
  // the item (to be used in PopulateFutureData). |length| cannot be 0.
  size_t AppendFutureData(size_t length);

  // Populates a part of an item previously allocated with AppendFutureData.
  // The first call to PopulateFutureData lazily allocates the memory for the
  // data element.
  // Returns true if:
  // * The item was created by using AppendFutureData,
  // * The offset and length are valid, and
  // * data is a valid pointer.
  bool PopulateFutureData(size_t index,
                          const char* data,
                          size_t offset,
                          size_t length);

  // Adds an item that is flagged for future data population. Use
  // 'PopulateFutureFile' to set the file path and expected modification time
  // of this file. Returns the index of the item (to be used in
  // PopulateFutureFile). |length| cannot be 0.
  // Data for multiple items can be stored in the same 'future' file, just at
  // different offsets and lengths. The |file_id| is used to differentiate
  // between different 'future' files that will be used to store data for these
  // items.
  size_t AppendFutureFile(uint64_t offset, uint64_t length, uint64_t file_id);

  // Populates a part of an item previously allocated with AppendFutureFile.
  // Returns true if:
  // * The item was created by using AppendFutureFile and
  // * The filepath is valid.
  bool PopulateFutureFile(
      size_t index,
      const scoped_refptr<ShareableFileReference>& file_reference,
      const base::Time& expected_modification_time);

  // You must know the length of the file, you cannot use kuint64max to specify
  // the whole file.  This method creates a ShareableFileReference to the given
  // file, which is stored in this builder.
  void AppendFile(const base::FilePath& file_path,
                  uint64_t offset,
                  uint64_t length,
                  const base::Time& expected_modification_time);

  void AppendBlob(const std::string& uuid, uint64_t offset, uint64_t length);

  void AppendBlob(const std::string& uuid);

  void AppendFileSystemFile(const GURL& url,
                            uint64_t offset,
                            uint64_t length,
                            const base::Time& expected_modification_time);

  void AppendDiskCacheEntry(const scoped_refptr<DataHandle>& data_handle,
                            disk_cache::Entry* disk_cache_entry,
                            int disk_cache_stream_index);

  // The content of the side data is accessible with BlobReader::ReadSideData().
  void AppendDiskCacheEntryWithSideData(
      const scoped_refptr<DataHandle>& data_handle,
      disk_cache::Entry* disk_cache_entry,
      int disk_cache_stream_index,
      int disk_cache_side_stream_index);

  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

  void Clear();

 private:
  friend class BlobMemoryControllerTest;
  friend class BlobStorageContext;
  friend bool operator==(const BlobDataBuilder& a, const BlobDataBuilder& b);
  friend bool operator==(const BlobDataSnapshot& a, const BlobDataBuilder& b);
  friend STORAGE_EXPORT void PrintTo(const BlobDataBuilder& x,
                                     ::std::ostream* os);
  FRIEND_TEST_ALL_PREFIXES(BlobDataBuilderTest, TestFutureFiles);
  FRIEND_TEST_ALL_PREFIXES(BlobStorageContextTest, BuildBlobFuzzy);

  std::string uuid_;
  std::string content_type_;
  std::string content_disposition_;
  std::vector<scoped_refptr<BlobDataItem>> items_;

  DISALLOW_COPY_AND_ASSIGN(BlobDataBuilder);
};

#if defined(UNIT_TEST)
inline bool operator==(const BlobDataBuilder& a, const BlobDataBuilder& b) {
  if (a.content_type_ != b.content_type_)
    return false;
  if (a.content_disposition_ != b.content_disposition_)
    return false;
  if (a.items_.size() != b.items_.size())
    return false;
  for (size_t i = 0; i < a.items_.size(); ++i) {
    if (*(a.items_[i]) != *(b.items_[i]))
      return false;
  }
  return true;
}

inline bool operator==(const BlobDataSnapshot& a, const BlobDataBuilder& b) {
  if (a.content_type() != b.content_type_) {
    return false;
  }
  if (a.content_disposition() != b.content_disposition_) {
    return false;
  }
  if (a.items().size() != b.items_.size()) {
    return false;
  }
  for (size_t i = 0; i < a.items().size(); ++i) {
    if (*(a.items()[i]) != *(b.items_[i]))
      return false;
  }
  return true;
}

inline bool operator==(const BlobDataBuilder& a, const BlobDataSnapshot& b) {
  return b == a;
}

inline bool operator!=(const BlobDataSnapshot& a, const BlobDataBuilder& b) {
  return !(a == b);
}

inline bool operator!=(const BlobDataBuilder& a, const BlobDataBuilder& b) {
  return !(a == b);
}

inline bool operator!=(const BlobDataBuilder& a, const BlobDataSnapshot& b) {
  return b != a;
}

#endif  // defined(UNIT_TEST)

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_DATA_BUILDER_H_
