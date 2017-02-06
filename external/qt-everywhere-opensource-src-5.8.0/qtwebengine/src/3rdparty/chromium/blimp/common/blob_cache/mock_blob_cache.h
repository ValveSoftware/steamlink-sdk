// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_BLOB_CACHE_MOCK_BLOB_CACHE_H_
#define BLIMP_COMMON_BLOB_CACHE_MOCK_BLOB_CACHE_H_

#include "blimp/common/blob_cache/blob_cache.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {

class MockBlobCache : public BlobCache {
 public:
  MockBlobCache();
  ~MockBlobCache() override;

  MOCK_CONST_METHOD1(Contains, bool(const BlobId&));
  MOCK_METHOD2(Put, void(const BlobId&, BlobDataPtr));
  MOCK_CONST_METHOD1(Get, BlobDataPtr(const BlobId&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlobCache);
};

}  // namespace blimp

#endif  // BLIMP_COMMON_BLOB_CACHE_MOCK_BLOB_CACHE_H_
