// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/blob_cache/id_util.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

const char kBlobdata[] = "\xde\xad\xbe\xef\x4b\x1d\xc0\xd3\xff\xfe";

// Caution: If these expected outputs ever change, it means that all client-side
// caches will effectively be invalidated.
const char kBlobId[] =
    "\x31\xda\x00\xaf\x5e\xd6\x64\xd6\x5f\xb1\xe6\x34\x99\xd3\xf5\x12\x21\xc8"
    "\xf8\x51\x19\xfd\x1d\x17\xaa\x0b\x5e\x85\x10\x4f\x17\x15";
const char kBlobIdString[] =
    "31da00af5ed664d65fb1e63499d3f51221c8f85119fd1d17aa0b5e85104f1715";

TEST(IdUtilTest, BlobIdIsCorrect) {
  BlobId id = CalculateBlobId(kBlobdata, 10);
  EXPECT_EQ(crypto::kSHA256Length, id.length());
  for (size_t i = 0; i < crypto::kSHA256Length; i++)
    EXPECT_EQ(kBlobId[i], static_cast<int>(id[i]));
  EXPECT_EQ(kBlobIdString, BlobIdToString(id));
}

TEST(IdUtilTest, BlobIdsAreCorrectLength) {
  BlobId id1 = CalculateBlobId(kBlobdata, 10);
  EXPECT_EQ(crypto::kSHA256Length, id1.length());
  EXPECT_TRUE(IsValidBlobId(id1));

  BlobId id2 = CalculateBlobId(kBlobdata, 8);
  EXPECT_EQ(crypto::kSHA256Length, id2.length());
  EXPECT_TRUE(IsValidBlobId(id2));
}

TEST(IdUtilTest, BlobIdLengthCheck) {
  EXPECT_FALSE(IsValidBlobId(BlobId("")));
  EXPECT_FALSE(IsValidBlobId(BlobId("123")));
  EXPECT_TRUE(IsValidBlobId(BlobId("12345678901234567890123456789012")));
  EXPECT_FALSE(IsValidBlobId(BlobId("12345678901234567890123456789012fff")));
}

}  // namespace
}  // namespace blimp
