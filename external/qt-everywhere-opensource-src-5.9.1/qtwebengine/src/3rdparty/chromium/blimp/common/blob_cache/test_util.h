// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_BLOB_CACHE_TEST_UTIL_H_
#define BLIMP_COMMON_BLOB_CACHE_TEST_UTIL_H_

#include <string>

#include "blimp/common/blob_cache/blob_cache.h"

namespace blimp {

BlobDataPtr CreateBlobDataPtr(const std::string& data);

}  // namespace blimp

#endif  // BLIMP_COMMON_BLOB_CACHE_TEST_UTIL_H_
