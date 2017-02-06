// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_client_picture_cache.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "blimp/test/support/compositor/picture_cache_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blimp {
namespace client {
namespace {

bool FakeImageDecoder(const void* input, size_t input_size, SkBitmap* bitmap) {
  return true;
}

class BlimpClientPictureCacheTest : public testing::Test {
 public:
  BlimpClientPictureCacheTest() : cache_(&FakeImageDecoder) {}
  ~BlimpClientPictureCacheTest() override = default;

 protected:
  BlimpClientPictureCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpClientPictureCacheTest);
};

TEST_F(BlimpClientPictureCacheTest, TwoSkPicturesInCache) {
  sk_sp<const SkPicture> picture1 = CreateSkPicture(SK_ColorBLUE);
  sk_sp<const SkPicture> picture2 = CreateSkPicture(SK_ColorRED);
  cc::PictureData picture1_data = CreatePictureData(picture1);
  cc::PictureData picture2_data = CreatePictureData(picture2);

  std::vector<cc::PictureData> cache_update;
  cache_update.push_back(picture1_data);
  cache_update.push_back(picture2_data);

  cache_.ApplyCacheUpdate(cache_update);

  cache_.MarkUsed(picture1->uniqueID());
  cache_.MarkUsed(picture2->uniqueID());

  sk_sp<const SkPicture> cached_picture1 =
      cache_.GetPicture(picture1->uniqueID());
  EXPECT_NE(nullptr, cached_picture1);
  EXPECT_EQ(GetBlobId(picture1), GetBlobId(cached_picture1));
  sk_sp<const SkPicture> cached_picture2 =
      cache_.GetPicture(picture2->uniqueID());
  EXPECT_NE(nullptr, cached_picture2);
  EXPECT_EQ(GetBlobId(picture2), GetBlobId(cached_picture2));

  cache_.Flush();
}

}  // namespace
}  // namespace client
}  // namespace blimp
