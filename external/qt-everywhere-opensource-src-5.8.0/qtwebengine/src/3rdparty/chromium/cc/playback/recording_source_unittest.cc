// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/base/region.h"
#include "cc/playback/raster_source.h"
#include "cc/proto/recording_source.pb.h"
#include "cc/test/fake_client_picture_cache.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_engine_picture_cache.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
namespace {

std::unique_ptr<FakeRecordingSource> CreateRecordingSource(
    const gfx::Rect& viewport) {
  gfx::Rect layer_rect(viewport.right(), viewport.bottom());
  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateRecordingSource(viewport, layer_rect.size());
  return recording_source;
}

scoped_refptr<RasterSource> CreateRasterSource(
    FakeRecordingSource* recording_source) {
  bool can_use_lcd_text = true;
  return RasterSource::CreateFromRecordingSource(recording_source,
                                                 can_use_lcd_text);
}

void ValidateRecordingSourceSerialization(FakeRecordingSource* source) {
  std::unique_ptr<FakeImageSerializationProcessor>
      fake_image_serialization_processor =
          base::WrapUnique(new FakeImageSerializationProcessor);
  std::unique_ptr<EnginePictureCache> fake_engine_picture_cache =
      fake_image_serialization_processor->CreateEnginePictureCache();
  FakeEnginePictureCache* fake_engine_picture_cache_ptr =
      static_cast<FakeEnginePictureCache*>(fake_engine_picture_cache.get());
  std::unique_ptr<ClientPictureCache> fake_client_picture_cache =
      fake_image_serialization_processor->CreateClientPictureCache();

  fake_engine_picture_cache_ptr->MarkAllSkPicturesAsUsed(
      source->GetDisplayItemList());

  proto::RecordingSource proto;
  source->ToProtobuf(&proto);

  std::vector<uint32_t> actual_picture_ids;
  FakeRecordingSource new_source;
  new_source.FromProtobuf(proto, fake_client_picture_cache.get(),
                          &actual_picture_ids);

  EXPECT_THAT(actual_picture_ids,
              testing::UnorderedElementsAreArray(
                  fake_engine_picture_cache_ptr->GetAllUsedPictureIds()));

  EXPECT_TRUE(source->EqualsTo(new_source));
}

TEST(RecordingSourceTest, TestNullDisplayListSerialization) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  recording_source->SetDisplayListUsesCachedPicture(false);
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();
  recording_source->SetEmptyBounds();

  ValidateRecordingSourceSerialization(recording_source.get());
}

TEST(RecordingSourceTest, TestEmptySerializationDeserialization) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  recording_source->SetDisplayListUsesCachedPicture(false);
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  ValidateRecordingSourceSerialization(recording_source.get());
}

TEST(RecordingSourceTest, TestPopulatedSerializationDeserialization) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  recording_source->SetDisplayListUsesCachedPicture(false);

  SkPaint simple_paint;
  simple_paint.setColor(SkColorSetARGB(255, 12, 23, 34));
  recording_source->add_draw_rect_with_paint(gfx::Rect(0, 0, 256, 256),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(128, 128, 512, 512),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(512, 0, 256, 256),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(0, 512, 256, 256),
                                             simple_paint);

  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  ValidateRecordingSourceSerialization(recording_source.get());
}

TEST(RecordingSourceTest, DiscardableImagesWithTransform) {
  gfx::Rect recorded_viewport(256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(
          recorded_viewport.size());
  sk_sp<SkImage> discardable_image[2][2];
  gfx::Transform identity_transform;
  discardable_image[0][0] = CreateDiscardableImage(gfx::Size(32, 32));
  // Translate transform is equivalent to moving using point.
  gfx::Transform translate_transform;
  translate_transform.Translate(0, 130);
  discardable_image[1][0] = CreateDiscardableImage(gfx::Size(32, 32));
  // This moves the bitmap to center of viewport and rotate, this would make
  // this bitmap in all four tile grids.
  gfx::Transform rotate_transform;
  rotate_transform.Translate(112, 112);
  rotate_transform.Rotate(45);
  discardable_image[1][1] = CreateDiscardableImage(gfx::Size(32, 32));

  gfx::RectF rect(0, 0, 32, 32);
  gfx::RectF translate_rect = rect;
  translate_transform.TransformRect(&translate_rect);
  gfx::RectF rotate_rect = rect;
  rotate_transform.TransformRect(&rotate_rect);

  recording_source->add_draw_image_with_transform(discardable_image[0][0],
                                                  identity_transform);
  recording_source->add_draw_image_with_transform(discardable_image[1][0],
                                                  translate_transform);
  recording_source->add_draw_image_with_transform(discardable_image[1][1],
                                                  rotate_transform);
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  bool can_use_lcd_text = true;
  scoped_refptr<RasterSource> raster_source =
      RasterSource::CreateFromRecordingSource(recording_source.get(),
                                              can_use_lcd_text);

  // Tile sized iterators. These should find only one pixel ref.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128), 1.f,
                                              &images);
    EXPECT_EQ(2u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1].image() == discardable_image[1][1]);
  }

  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(130, 140, 128, 128),
                                              1.f, &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[1][1]);
  }

  // The rotated bitmap would still be in the top right tile.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(130, 0, 128, 128), 1.f,
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[1][1]);
  }

  // Layer sized iterators. These should find all pixel refs.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256), 1.f,
                                              &images);
    EXPECT_EQ(3u, images.size());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(images[0].image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1].image() == discardable_image[1][0]);
    EXPECT_TRUE(images[2].image() == discardable_image[1][1]);
  }

  // Verify different raster scales
  for (float scale = 1.f; scale <= 5.f; scale += 0.5f) {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(130, 0, 128, 128),
                                              scale, &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_FLOAT_EQ(scale, images[0].scale().width());
    EXPECT_FLOAT_EQ(scale, images[0].scale().height());
  }
}

TEST(RecordingSourceTest, NoGatherImageEmptyImages) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  recording_source->SetGenerateDiscardableImagesMetadata(false);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource(recording_source.get());

  // If recording source do not gather images, raster source is not going to
  // get images.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(recorded_viewport, 1.f, &images);
    EXPECT_TRUE(images.empty());
  }
}

TEST(RecordingSourceTest, EmptyImages) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource(recording_source.get());

  // Tile sized iterators.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128), 1.f,
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Shifted tile sized iterators.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 140, 128, 128),
                                              1.f, &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256), 1.f,
                                              &images);
    EXPECT_TRUE(images.empty());
  }
}

TEST(RecordingSourceTest, NoDiscardableImages) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);

  SkPaint simple_paint;
  simple_paint.setColor(SkColorSetARGB(255, 12, 23, 34));

  SkBitmap non_discardable_bitmap;
  non_discardable_bitmap.allocN32Pixels(128, 128);
  non_discardable_bitmap.setImmutable();
  sk_sp<SkImage> non_discardable_image =
      SkImage::MakeFromBitmap(non_discardable_bitmap);

  recording_source->add_draw_rect_with_paint(gfx::Rect(0, 0, 256, 256),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(128, 128, 512, 512),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(512, 0, 256, 256),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(0, 512, 256, 256),
                                             simple_paint);
  recording_source->add_draw_image(non_discardable_image, gfx::Point(128, 0));
  recording_source->add_draw_image(non_discardable_image, gfx::Point(0, 128));
  recording_source->add_draw_image(non_discardable_image, gfx::Point(150, 150));
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource(recording_source.get());

  // Tile sized iterators.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128), 1.f,
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Shifted tile sized iterators.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 140, 128, 128),
                                              1.f, &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256), 1.f,
                                              &images);
    EXPECT_TRUE(images.empty());
  }
}

TEST(RecordingSourceTest, DiscardableImages) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);

  sk_sp<SkImage> discardable_image[2][2];
  discardable_image[0][0] = CreateDiscardableImage(gfx::Size(32, 32));
  discardable_image[1][0] = CreateDiscardableImage(gfx::Size(32, 32));
  discardable_image[1][1] = CreateDiscardableImage(gfx::Size(32, 32));

  // Discardable images are found in the following cells:
  // |---|---|
  // | x |   |
  // |---|---|
  // | x | x |
  // |---|---|
  recording_source->add_draw_image(discardable_image[0][0], gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[1][0], gfx::Point(0, 130));
  recording_source->add_draw_image(discardable_image[1][1],
                                   gfx::Point(140, 140));
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource(recording_source.get());

  // Tile sized iterators. These should find only one image.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128), 1.f,
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[0][0]);
  }

  // Shifted tile sized iterators. These should find only one image.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 140, 128, 128),
                                              1.f, &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[1][1]);
  }

  // Ensure there's no discardable images in the empty cell
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 0, 128, 128), 1.f,
                                              &images);
    EXPECT_TRUE(images.empty());
  }

  // Layer sized iterators. These should find all 3 images.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256), 1.f,
                                              &images);
    EXPECT_EQ(3u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1].image() == discardable_image[1][0]);
    EXPECT_TRUE(images[2].image() == discardable_image[1][1]);
  }
}

TEST(RecordingSourceTest, DiscardableImagesBaseNonDiscardable) {
  gfx::Rect recorded_viewport(0, 0, 512, 512);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);

  SkBitmap non_discardable_bitmap;
  non_discardable_bitmap.allocN32Pixels(512, 512);
  non_discardable_bitmap.setImmutable();
  sk_sp<SkImage> non_discardable_image =
      SkImage::MakeFromBitmap(non_discardable_bitmap);

  sk_sp<SkImage> discardable_image[2][2];
  discardable_image[0][0] = CreateDiscardableImage(gfx::Size(128, 128));
  discardable_image[0][1] = CreateDiscardableImage(gfx::Size(128, 128));
  discardable_image[1][1] = CreateDiscardableImage(gfx::Size(128, 128));

  // One large non-discardable image covers the whole grid.
  // Discardable images are found in the following cells:
  // |---|---|
  // | x | x |
  // |---|---|
  // |   | x |
  // |---|---|
  recording_source->add_draw_image(non_discardable_image, gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[0][0], gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[0][1], gfx::Point(260, 0));
  recording_source->add_draw_image(discardable_image[1][1],
                                   gfx::Point(260, 260));
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource(recording_source.get());

  // Tile sized iterators. These should find only one image.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256), 1.f,
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[0][0]);
  }
  // Shifted tile sized iterators. These should find only one image.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(260, 260, 256, 256),
                                              1.f, &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[1][1]);
  }
  // Ensure there's no discardable images in the empty cell
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 256, 256, 256), 1.f,
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators. These should find three images.
  {
    std::vector<DrawImage> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 512, 512), 1.f,
                                              &images);
    EXPECT_EQ(3u, images.size());
    EXPECT_TRUE(images[0].image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1].image() == discardable_image[0][1]);
    EXPECT_TRUE(images[2].image() == discardable_image[1][1]);
  }
}

}  // namespace
}  // namespace cc
