// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blimp_engine_picture_cache.h"

#include "blimp/test/support/compositor/picture_cache_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blimp {
namespace engine {
namespace {

class BlimpEnginePictureCacheTest : public testing::Test {
 public:
  BlimpEnginePictureCacheTest() : cache_(nullptr) {}
  ~BlimpEnginePictureCacheTest() override = default;

 protected:
  BlimpEnginePictureCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpEnginePictureCacheTest);
};

TEST_F(BlimpEnginePictureCacheTest, TwoCachedPicturesCanBeRetrieved) {
  EXPECT_TRUE(cache_.CalculateCacheUpdateAndFlush().empty());

  sk_sp<const SkPicture> picture1 = CreateSkPicture(SK_ColorBLUE);
  cache_.MarkUsed(picture1.get());
  sk_sp<const SkPicture> picture2 = CreateSkPicture(SK_ColorRED);
  cache_.MarkUsed(picture2.get());

  std::vector<cc::PictureData> update = cache_.CalculateCacheUpdateAndFlush();
  ASSERT_EQ(2U, update.size());
  EXPECT_THAT(update, testing::UnorderedElementsAre(PictureMatches(picture1),
                                                    PictureMatches(picture2)));

  EXPECT_TRUE(cache_.CalculateCacheUpdateAndFlush().empty());
}

TEST_F(BlimpEnginePictureCacheTest, SamePictureOnlyReturnedOnce) {
  EXPECT_TRUE(cache_.CalculateCacheUpdateAndFlush().empty());

  sk_sp<const SkPicture> picture = CreateSkPicture(SK_ColorBLUE);
  cache_.MarkUsed(picture.get());
  cache_.MarkUsed(picture.get());

  std::vector<cc::PictureData> update = cache_.CalculateCacheUpdateAndFlush();
  ASSERT_EQ(1U, update.size());
  EXPECT_THAT(update, testing::UnorderedElementsAre(PictureMatches(picture)));
}

}  // namespace
}  // namespace engine
}  // namespace blimp
