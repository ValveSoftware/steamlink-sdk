// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_BLOB_CACHE_ID_UTIL_H_
#define BLIMP_COMMON_BLOB_CACHE_ID_UTIL_H_

#include <string>
#include <vector>

#include "blimp/common/blimp_common_export.h"
#include "blimp/common/blob_cache/blob_cache.h"

namespace blimp {

// Returns a unique Id for the Blob, based on its content. All BlobIds have
// the same length.
BLIMP_COMMON_EXPORT const BlobId CalculateBlobId(const void* data,
                                                 size_t data_size);
BLIMP_COMMON_EXPORT const BlobId CalculateBlobId(const std::string& data);

// Returns a hexadecimal string representation of a BlobId. The input must
// be a valid BlobId.
BLIMP_COMMON_EXPORT const std::string BlobIdToString(const BlobId& id);

// Returns whether the BlobId is valid.
BLIMP_COMMON_EXPORT bool IsValidBlobId(const BlobId& id);

}  // namespace blimp

#endif  // BLIMP_COMMON_BLOB_CACHE_ID_UTIL_H_
