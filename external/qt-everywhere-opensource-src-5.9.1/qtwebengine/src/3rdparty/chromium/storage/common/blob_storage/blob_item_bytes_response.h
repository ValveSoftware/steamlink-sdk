// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_BLOB_STORAGE_BLOB_ITEM_BYTES_RESPONSE_H_
#define STORAGE_COMMON_BLOB_STORAGE_BLOB_ITEM_BYTES_RESPONSE_H_

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <ostream>
#include <vector>

#include "base/time/time.h"
#include "storage/common/storage_common_export.h"

namespace storage {

// This class is serialized over IPC to send blob item data, or to signal that
// the memory has been populated in shared memory or a file.
struct STORAGE_COMMON_EXPORT BlobItemBytesResponse {
  // not using std::numeric_limits<T>::max() because of non-C++11 builds.
  static const uint32_t kInvalidIndex = UINT32_MAX;

  BlobItemBytesResponse();
  explicit BlobItemBytesResponse(uint32_t request_number);
  BlobItemBytesResponse(const BlobItemBytesResponse& other);
  ~BlobItemBytesResponse();

  char* allocate_mutable_data(uint32_t size) {
    inline_data.resize(size);
    return &inline_data[0];
  }

  uint32_t request_number;
  std::vector<char> inline_data;
  base::Time time_file_modified;
};

STORAGE_COMMON_EXPORT void PrintTo(const BlobItemBytesResponse& response,
                                   std::ostream* os);

STORAGE_COMMON_EXPORT bool operator==(const BlobItemBytesResponse& a,
                                      const BlobItemBytesResponse& b);

STORAGE_COMMON_EXPORT bool operator!=(const BlobItemBytesResponse& a,
                                      const BlobItemBytesResponse& b);

}  // namespace storage

#endif  // STORAGE_COMMON_BLOB_STORAGE_BLOB_ITEM_BYTES_RESPONSE_H_
