// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_DATA_ELEMENT_H_
#define STORAGE_COMMON_DATA_ELEMENT_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <ostream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "storage/common/storage_common_export.h"
#include "url/gurl.h"

namespace storage {

// Represents a base Web data element. This could be either one of
// bytes, file or blob data.
class STORAGE_COMMON_EXPORT DataElement {
 public:
  enum Type {
    TYPE_UNKNOWN = -1,
    TYPE_BYTES,
    // Only used with BlobStorageMsg_StartBuildingBlob
    TYPE_BYTES_DESCRIPTION,
    TYPE_FILE,
    TYPE_BLOB,
    TYPE_FILE_FILESYSTEM,
    TYPE_DISK_CACHE_ENTRY,
  };

  DataElement();
  DataElement(const DataElement& other);
  ~DataElement();

  Type type() const { return type_; }
  const char* bytes() const { return bytes_ ? bytes_ : buf_.data(); }
  const base::FilePath& path() const { return path_; }
  const GURL& filesystem_url() const { return filesystem_url_; }
  const std::string& blob_uuid() const { return blob_uuid_; }
  uint64_t offset() const { return offset_; }
  uint64_t length() const { return length_; }
  const base::Time& expected_modification_time() const {
    return expected_modification_time_;
  }

  // For use with SetToAllocatedBytes. Should only be used after calling
  // SetToAllocatedBytes.
  char* mutable_bytes() { return &buf_[0]; }

  // Sets TYPE_BYTES data. This copies the given data into the element.
  void SetToBytes(const char* bytes, int bytes_len) {
    type_ = TYPE_BYTES;
    bytes_ = nullptr;
    buf_.assign(bytes, bytes + bytes_len);
    length_ = buf_.size();
  }

  // Sets TYPE_BYTES data, and clears the internal bytes buffer.
  // For use with AppendBytes.
  void SetToEmptyBytes() {
    type_ = TYPE_BYTES;
    buf_.clear();
    length_ = 0;
    bytes_ = nullptr;
  }

  // Copies and appends the given data into the element. SetToEmptyBytes or
  // SetToBytes must be called before this method.
  void AppendBytes(const char* bytes, int bytes_len) {
    DCHECK_EQ(type_, TYPE_BYTES);
    DCHECK_NE(length_, std::numeric_limits<uint64_t>::max());
    DCHECK(!bytes_);
    buf_.insert(buf_.end(), bytes, bytes + bytes_len);
    length_ = buf_.size();
  }

  void SetToBytesDescription(size_t bytes_len) {
    type_ = TYPE_BYTES_DESCRIPTION;
    bytes_ = nullptr;
    length_ = bytes_len;
  }

  // Sets TYPE_BYTES data. This does NOT copy the given data and the caller
  // should make sure the data is alive when this element is accessed.
  // You cannot use AppendBytes with this method.
  void SetToSharedBytes(const char* bytes, int bytes_len) {
    type_ = TYPE_BYTES;
    bytes_ = bytes;
    length_ = bytes_len;
  }

  // Sets TYPE_BYTES data. This allocates the space for the bytes in the
  // internal vector but does not populate it with anything.  The caller can
  // then use the bytes() method to access this buffer and populate it.
  void SetToAllocatedBytes(size_t bytes_len) {
    type_ = TYPE_BYTES;
    bytes_ = nullptr;
    buf_.resize(bytes_len);
    length_ = bytes_len;
  }

  // Sets TYPE_FILE data.
  void SetToFilePath(const base::FilePath& path) {
    SetToFilePathRange(path, 0, std::numeric_limits<uint64_t>::max(),
                       base::Time());
  }

  // Sets TYPE_BLOB data.
  void SetToBlob(const std::string& uuid) {
    SetToBlobRange(uuid, 0, std::numeric_limits<uint64_t>::max());
  }

  // Sets TYPE_FILE data with range.
  void SetToFilePathRange(const base::FilePath& path,
                          uint64_t offset,
                          uint64_t length,
                          const base::Time& expected_modification_time);

  // Sets TYPE_BLOB data with range.
  void SetToBlobRange(const std::string& blob_uuid,
                      uint64_t offset,
                      uint64_t length);

  // Sets TYPE_FILE_FILESYSTEM with range.
  void SetToFileSystemUrlRange(const GURL& filesystem_url,
                               uint64_t offset,
                               uint64_t length,
                               const base::Time& expected_modification_time);

  // Sets to TYPE_DISK_CACHE_ENTRY with range.
  void SetToDiskCacheEntryRange(uint64_t offset, uint64_t length);

 private:
  FRIEND_TEST_ALL_PREFIXES(BlobAsyncTransportStrategyTest, TestInvalidParams);
  friend STORAGE_COMMON_EXPORT void PrintTo(const DataElement& x,
                                            ::std::ostream* os);
  Type type_;
  std::vector<char> buf_;  // For TYPE_BYTES.
  const char* bytes_;  // For TYPE_BYTES.
  base::FilePath path_;  // For TYPE_FILE.
  GURL filesystem_url_;  // For TYPE_FILE_FILESYSTEM.
  std::string blob_uuid_;
  uint64_t offset_;
  uint64_t length_;
  base::Time expected_modification_time_;
};

STORAGE_COMMON_EXPORT bool operator==(const DataElement& a,
                                      const DataElement& b);
STORAGE_COMMON_EXPORT bool operator!=(const DataElement& a,
                                      const DataElement& b);

}  // namespace storage

#endif  // STORAGE_COMMON_DATA_ELEMENT_H_
