// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/data_element.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/strings/string_number_conversions.h"

namespace storage {

DataElement::DataElement()
    : type_(TYPE_UNKNOWN),
      bytes_(NULL),
      offset_(0),
      length_(std::numeric_limits<uint64_t>::max()) {}

DataElement::DataElement(const DataElement& other) = default;

DataElement::~DataElement() {}

void DataElement::SetToFilePathRange(
    const base::FilePath& path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  type_ = TYPE_FILE;
  path_ = path;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

void DataElement::SetToBlobRange(const std::string& blob_uuid,
                                 uint64_t offset,
                                 uint64_t length) {
  type_ = TYPE_BLOB;
  blob_uuid_ = blob_uuid;
  offset_ = offset;
  length_ = length;
}

void DataElement::SetToFileSystemUrlRange(
    const GURL& filesystem_url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  type_ = TYPE_FILE_FILESYSTEM;
  filesystem_url_ = filesystem_url;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

void DataElement::SetToDiskCacheEntryRange(uint64_t offset, uint64_t length) {
  type_ = TYPE_DISK_CACHE_ENTRY;
  offset_ = offset;
  length_ = length;
}

void PrintTo(const DataElement& x, std::ostream* os) {
  const uint64_t kMaxDataPrintLength = 40;
  *os << "<DataElement>{type: ";
  switch (x.type()) {
    case DataElement::TYPE_BYTES: {
      uint64_t length = std::min(x.length(), kMaxDataPrintLength);
      *os << "TYPE_BYTES, data: ["
          << base::HexEncode(x.bytes(), static_cast<size_t>(length));
      if (length < x.length()) {
        *os << "<...truncated due to length...>";
      }
      *os << "]";
      break;
    }
    case DataElement::TYPE_FILE:
      *os << "TYPE_FILE, path: " << x.path().AsUTF8Unsafe()
          << ", expected_modification_time: " << x.expected_modification_time();
      break;
    case DataElement::TYPE_BLOB:
      *os << "TYPE_BLOB, uuid: " << x.blob_uuid();
      break;
    case DataElement::TYPE_FILE_FILESYSTEM:
      *os << "TYPE_FILE_FILESYSTEM, filesystem_url: " << x.filesystem_url();
      break;
    case DataElement::TYPE_DISK_CACHE_ENTRY:
      *os << "TYPE_DISK_CACHE_ENTRY";
      break;
    case DataElement::TYPE_BYTES_DESCRIPTION:
      *os << "TYPE_BYTES_DESCRIPTION";
      break;
    case DataElement::TYPE_UNKNOWN:
      *os << "TYPE_UNKNOWN";
      break;
  }
  *os << ", length: " << x.length() << ", offset: " << x.offset() << "}";
}

bool operator==(const DataElement& a, const DataElement& b) {
  if (a.type() != b.type() || a.offset() != b.offset() ||
      a.length() != b.length())
    return false;
  switch (a.type()) {
    case DataElement::TYPE_BYTES:
      return memcmp(a.bytes(), b.bytes(), b.length()) == 0;
    case DataElement::TYPE_FILE:
      return a.path() == b.path() &&
             a.expected_modification_time() == b.expected_modification_time();
    case DataElement::TYPE_BLOB:
      return a.blob_uuid() == b.blob_uuid();
    case DataElement::TYPE_FILE_FILESYSTEM:
      return a.filesystem_url() == b.filesystem_url();
    case DataElement::TYPE_DISK_CACHE_ENTRY:
      // We compare only length and offset; we trust the entry itself was
      // compared at some higher level such as in BlobDataItem.
      return true;
    case DataElement::TYPE_BYTES_DESCRIPTION:
      return true;
    case DataElement::TYPE_UNKNOWN:
      NOTREACHED();
      return false;
  }
  return false;
}

bool operator!=(const DataElement& a, const DataElement& b) {
  return !(a == b);
}

}  // namespace storage
