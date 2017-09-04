// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/blob_cache/id_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "crypto/sha2.h"

namespace blimp {

const BlobId CalculateBlobId(const void* data, size_t data_size) {
  return crypto::SHA256HashString(
      base::StringPiece(reinterpret_cast<const char*>(data), data_size));
}

const BlobId CalculateBlobId(const std::string& data) {
  return crypto::SHA256HashString(data);
}

const std::string BlobIdToString(const BlobId& id) {
  DCHECK(IsValidBlobId(id)) << "Invalid blob ID: " << id;
  return base::ToLowerASCII(base::HexEncode(id.data(), id.length()));
}

bool IsValidBlobId(const BlobId& id) {
  return crypto::kSHA256Length == id.length();
}

}  // namespace blimp
