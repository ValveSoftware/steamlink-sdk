// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/blob_storage/blob_item_bytes_response.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/strings/string_number_conversions.h"

namespace storage {

BlobItemBytesResponse::BlobItemBytesResponse()
    : request_number(kInvalidIndex) {}

BlobItemBytesResponse::BlobItemBytesResponse(uint32_t request_number)
    : request_number(request_number) {}

BlobItemBytesResponse::BlobItemBytesResponse(
    const BlobItemBytesResponse& other) = default;

BlobItemBytesResponse::~BlobItemBytesResponse() {}

void PrintTo(const BlobItemBytesResponse& response, ::std::ostream* os) {
  const size_t kMaxDataPrintLength = 40;
  size_t length = std::min(response.inline_data.size(), kMaxDataPrintLength);
  *os << "{request_number: " << response.request_number
      << ", inline_data size: " << response.inline_data.size()
      << ", inline_data: [";
  if (length == 0) {
    *os << "<empty>";
  } else {
    *os << base::HexEncode(&response.inline_data[0], length);
    if (length < response.inline_data.size()) {
      *os << "<...truncated due to length...>";
    }
  }
  *os << "]}";
}

bool operator==(const BlobItemBytesResponse& a,
                const BlobItemBytesResponse& b) {
  return a.request_number == b.request_number &&
         a.inline_data.size() == b.inline_data.size() &&
         std::equal(a.inline_data.begin(),
                    a.inline_data.begin() + a.inline_data.size(),
                    b.inline_data.begin());
}

bool operator!=(const BlobItemBytesResponse& a,
                const BlobItemBytesResponse& b) {
  return !(a == b);
}

}  // namespace storage
