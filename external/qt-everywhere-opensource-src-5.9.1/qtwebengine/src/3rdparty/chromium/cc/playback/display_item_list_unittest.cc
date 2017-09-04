// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_list.h"

#include <stddef.h>

#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/compositing_display_item.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/float_clip_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "cc/proto/display_item.pb.h"
#include "cc/test/fake_client_picture_cache.h"
#include "cc/test/fake_engine_picture_cache.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkImageSource.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

const gfx::Rect kVisualRect(0, 0, 42, 42);

scoped_refptr<DisplayItemList> CreateDefaultList() {
  return DisplayItemList::Create(DisplayItemListSettings());
}

sk_sp<const SkPicture> CreateRectPicture(const gfx::Rect& bounds) {
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;

  canvas = sk_ref_sp(recorder.beginRecording(bounds.width(), bounds.height()));
  canvas->drawRect(
      SkRect::MakeXYWH(bounds.x(), bounds.y(), bounds.width(), bounds.height()),
      SkPaint());
  return recorder.finishRecordingAsPicture();
}

void AppendFirstSerializationTestPicture(scoped_refptr<DisplayItemList> list,
                                         const gfx::Size& layer_size) {
  gfx::PointF offset(2.f, 3.f);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;

  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);

  canvas = sk_ref_sp(recorder.beginRecording(SkRect::MakeXYWH(
      offset.x(), offset.y(), layer_size.width(), layer_size.height())));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(0.f, 0.f, 4.f, 4.f, red_paint);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, recorder.finishRecordingAsPicture());
}

void AppendSecondSerializationTestPicture(scoped_refptr<DisplayItemList> list,
                                          const gfx::Size& layer_size) {
  gfx::PointF offset(2.f, 2.f);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;

  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);

  canvas = sk_ref_sp(recorder.beginRecording(SkRect::MakeXYWH(
      offset.x(), offset.y(), layer_size.width(), layer_size.height())));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(3.f, 3.f, 7.f, 7.f, blue_paint);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, recorder.finishRecordingAsPicture());
}

void ValidateDisplayItemListSerialization(const gfx::Size& layer_size,
                                          scoped_refptr<DisplayItemList> list) {
  list->Finalize();

  std::unique_ptr<FakeImageSerializationProcessor>
      fake_image_serialization_processor =
          base::WrapUnique(new FakeImageSerializationProcessor);
  std::unique_ptr<EnginePictureCache> fake_engine_picture_cache =
      fake_image_serialization_processor->CreateEnginePictureCache();
  FakeEnginePictureCache* fake_engine_picture_cache_ptr =
      static_cast<FakeEnginePictureCache*>(fake_engine_picture_cache.get());
  std::unique_ptr<ClientPictureCache> fake_client_picture_cache =
      fake_image_serialization_processor->CreateClientPictureCache();

  fake_engine_picture_cache_ptr->MarkAllSkPicturesAsUsed(list.get());

  // Serialize and deserialize the DisplayItemList.
  proto::DisplayItemList proto;
  list->ToProtobuf(&proto);

  std::vector<uint32_t> actual_picture_ids;
  scoped_refptr<DisplayItemList> new_list = DisplayItemList::CreateFromProto(
      proto, fake_client_picture_cache.get(), &actual_picture_ids);

  EXPECT_THAT(actual_picture_ids,
              testing::UnorderedElementsAreArray(
                  fake_engine_picture_cache_ptr->GetAllUsedPictureIds()));

  EXPECT_TRUE(AreDisplayListDrawingResultsSame(gfx::Rect(layer_size),
                                               list.get(), new_list.get()));
}

}  // namespace

TEST(DisplayItemListTest, SerializeDisplayItemListSettings) {
  DisplayItemListSettings settings;
  settings.use_cached_picture = false;

  {
    proto::DisplayItemListSettings proto;
    settings.ToProtobuf(&proto);
    DisplayItemListSettings deserialized(proto);
    EXPECT_EQ(settings.use_cached_picture, deserialized.use_cached_picture);
  }

  settings.use_cached_picture = true;
  {
    proto::DisplayItemListSettings proto;
    settings.ToProtobuf(&proto);
    DisplayItemListSettings deserialized(proto);
    EXPECT_EQ(settings.use_cached_picture, deserialized.use_cached_picture);
  }
}

TEST(DisplayItemListTest, SerializeSingleDrawingItem) {
  gfx::Size layer_size(10, 10);

  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);
  list->SetRetainVisualRectsForTesting(true);

  // Build the DrawingDisplayItem.
  AppendFirstSerializationTestPicture(list, layer_size);

  ValidateDisplayItemListSerialization(layer_size, list);
}

TEST(DisplayItemListTest, SerializeClipItem) {
  gfx::Size layer_size(10, 10);

  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);
  list->SetRetainVisualRectsForTesting(true);

  // Build the DrawingDisplayItem.
  AppendFirstSerializationTestPicture(list, layer_size);

  // Build the ClipDisplayItem.
  gfx::Rect clip_rect(6, 6, 1, 1);
  std::vector<SkRRect> rrects;
  rrects.push_back(SkRRect::MakeOval(SkRect::MakeXYWH(5.f, 5.f, 4.f, 4.f)));
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(clip_rect, rrects,
                                                        true);

  // Build the second DrawingDisplayItem.
  AppendSecondSerializationTestPicture(list, layer_size);

  // Build the EndClipDisplayItem.
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  ValidateDisplayItemListSerialization(layer_size, list);
}

TEST(DisplayItemListTest, SerializeClipPathItem) {
  gfx::Size layer_size(10, 10);

  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);
  list->SetRetainVisualRectsForTesting(true);

  // Build the DrawingDisplayItem.
  AppendFirstSerializationTestPicture(list, layer_size);

  // Build the ClipPathDisplayItem.
  SkPath path;
  path.addCircle(5.f, 5.f, 2.f, SkPath::Direction::kCW_Direction);
  list->CreateAndAppendPairedBeginItem<ClipPathDisplayItem>(
      path, SkRegion::Op::kReplace_Op, false);

  // Build the second DrawingDisplayItem.
  AppendSecondSerializationTestPicture(list, layer_size);

  // Build the EndClipPathDisplayItem.
  list->CreateAndAppendPairedEndItem<EndClipPathDisplayItem>();

  ValidateDisplayItemListSerialization(layer_size, list);
}

TEST(DisplayItemListTest, SerializeCompositingItem) {
  gfx::Size layer_size(10, 10);

  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);
  list->SetRetainVisualRectsForTesting(true);

  // Build the DrawingDisplayItem.
  AppendFirstSerializationTestPicture(list, layer_size);

  // Build the CompositingDisplayItem.
  list->CreateAndAppendPairedBeginItem<CompositingDisplayItem>(
      150, SkXfermode::Mode::kDst_Mode, nullptr,
      SkColorMatrixFilter::MakeLightingFilter(SK_ColorRED, SK_ColorGREEN),
      false);

  // Build the second DrawingDisplayItem.
  AppendSecondSerializationTestPicture(list, layer_size);

  // Build the EndCompositingDisplayItem.
  list->CreateAndAppendPairedEndItem<EndCompositingDisplayItem>();

  ValidateDisplayItemListSerialization(layer_size, list);
}

TEST(DisplayItemListTest, SerializeFloatClipItem) {
  gfx::Size layer_size(10, 10);

  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);
  list->SetRetainVisualRectsForTesting(true);

  // Build the DrawingDisplayItem.
  AppendFirstSerializationTestPicture(list, layer_size);

  // Build the FloatClipDisplayItem.
  gfx::RectF clip_rect(6.f, 6.f, 1.f, 1.f);
  list->CreateAndAppendPairedBeginItem<FloatClipDisplayItem>(clip_rect);

  // Build the second DrawingDisplayItem.
  AppendSecondSerializationTestPicture(list, layer_size);

  // Build the EndFloatClipDisplayItem.
  list->CreateAndAppendPairedEndItem<EndFloatClipDisplayItem>();

  ValidateDisplayItemListSerialization(layer_size, list);
}

TEST(DisplayItemListTest, SerializeTransformItem) {
  gfx::Size layer_size(10, 10);

  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);
  list->SetRetainVisualRectsForTesting(true);

  // Build the DrawingDisplayItem.
  AppendFirstSerializationTestPicture(list, layer_size);

  // Build the TransformDisplayItem.
  gfx::Transform transform;
  transform.Scale(1.25f, 1.25f);
  transform.Translate(-1.f, -1.f);
  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(transform);

  // Build the second DrawingDisplayItem.
  AppendSecondSerializationTestPicture(list, layer_size);

  // Build the EndTransformDisplayItem.
  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();

  ValidateDisplayItemListSerialization(layer_size, list);
}

TEST(DisplayItemListTest, SingleDrawingItem) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);

  gfx::PointF offset(8.f, 9.f);
  gfx::RectF recording_rect(offset, gfx::SizeF(layer_rect.size()));
  canvas =
      sk_ref_sp(recorder.beginRecording(gfx::RectFToSkRect(recording_rect)));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, recorder.finishRecordingAsPicture());
  list->Finalize();
  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRectCoords(0.f + offset.x(), 0.f + offset.y(),
                                 60.f + offset.x(), 60.f + offset.y(),
                                 red_paint);
  expected_canvas.drawRectCoords(50.f + offset.x(), 50.f + offset.y(),
                                 75.f + offset.x(), 75.f + offset.y(),
                                 blue_paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, ClipItem) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);

  gfx::PointF first_offset(8.f, 9.f);
  gfx::RectF first_recording_rect(first_offset, gfx::SizeF(layer_rect.size()));
  canvas = sk_ref_sp(
      recorder.beginRecording(gfx::RectFToSkRect(first_recording_rect)));
  canvas->translate(first_offset.x(), first_offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, recorder.finishRecordingAsPicture());

  gfx::Rect clip_rect(60, 60, 10, 10);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_rect, std::vector<SkRRect>(), true);

  gfx::PointF second_offset(2.f, 3.f);
  gfx::RectF second_recording_rect(second_offset,
                                   gfx::SizeF(layer_rect.size()));
  canvas = sk_ref_sp(
      recorder.beginRecording(gfx::RectFToSkRect(second_recording_rect)));
  canvas->translate(second_offset.x(), second_offset.y());
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, recorder.finishRecordingAsPicture());

  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();
  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRectCoords(0.f + first_offset.x(), 0.f + first_offset.y(),
                                 60.f + first_offset.x(),
                                 60.f + first_offset.y(), red_paint);
  expected_canvas.clipRect(gfx::RectToSkRect(clip_rect));
  expected_canvas.drawRectCoords(
      50.f + second_offset.x(), 50.f + second_offset.y(),
      75.f + second_offset.x(), 75.f + second_offset.y(), blue_paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, TransformItem) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  DisplayItemListSettings settings;
  settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create(settings);

  gfx::PointF first_offset(8.f, 9.f);
  gfx::RectF first_recording_rect(first_offset, gfx::SizeF(layer_rect.size()));
  canvas = sk_ref_sp(
      recorder.beginRecording(gfx::RectFToSkRect(first_recording_rect)));
  canvas->translate(first_offset.x(), first_offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, recorder.finishRecordingAsPicture());

  gfx::Transform transform;
  transform.Rotate(45.0);
  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(transform);

  gfx::PointF second_offset(2.f, 3.f);
  gfx::RectF second_recording_rect(second_offset,
                                   gfx::SizeF(layer_rect.size()));
  canvas = sk_ref_sp(
      recorder.beginRecording(gfx::RectFToSkRect(second_recording_rect)));
  canvas->translate(second_offset.x(), second_offset.y());
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, recorder.finishRecordingAsPicture());

  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();
  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRectCoords(0.f + first_offset.x(), 0.f + first_offset.y(),
                                 60.f + first_offset.x(),
                                 60.f + first_offset.y(), red_paint);
  expected_canvas.setMatrix(transform.matrix());
  expected_canvas.drawRectCoords(
      50.f + second_offset.x(), 50.f + second_offset.y(),
      75.f + second_offset.x(), 75.f + second_offset.y(), blue_paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, FilterItem) {
  gfx::Rect layer_rect(100, 100);
  FilterOperations filters;
  unsigned char pixels[4 * 100 * 100] = {0};
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(DisplayItemListSettings());

  sk_sp<SkSurface> source_surface = SkSurface::MakeRasterN32Premul(50, 50);
  SkCanvas* source_canvas = source_surface->getCanvas();
  source_canvas->clear(SkColorSetRGB(128, 128, 128));
  sk_sp<SkImage> source_image = source_surface->makeImageSnapshot();

  // For most SkImageFilters, the |dst| bounds computed by computeFastBounds are
  // dependent on the provided |src| bounds. This means, for example, that
  // translating |src| results in a corresponding translation of |dst|. But this
  // is not the case for all SkImageFilters; for some of them (e.g.
  // SkImageSource), the computation of |dst| in computeFastBounds doesn't
  // involve |src| at all. Incorrectly assuming such a relationship (e.g. by
  // translating |dst| after it is computed by computeFastBounds, rather than
  // translating |src| before it provided to computedFastBounds) can cause
  // incorrect clipping of filter output. To test for this, we include an
  // SkImageSource filter in |filters|. Here, |src| is |filter_bounds|, defined
  // below.
  sk_sp<SkImageFilter> image_filter = SkImageSource::Make(source_image);
  filters.Append(FilterOperation::CreateReferenceFilter(image_filter));
  filters.Append(FilterOperation::CreateBrightnessFilter(0.5f));
  gfx::RectF filter_bounds(10.f, 10.f, 50.f, 50.f);
  list->CreateAndAppendPairedBeginItem<FilterDisplayItem>(
      filters, filter_bounds, filter_bounds.origin());

  // Include a rect drawing so that filter is actually applied to something.
  {
    SkPictureRecorder recorder;
    sk_sp<SkCanvas> canvas;

    SkPaint red_paint;
    red_paint.setColor(SK_ColorRED);

    canvas = sk_ref_sp(recorder.beginRecording(
        SkRect::MakeXYWH(0, 0, layer_rect.width(), layer_rect.height())));
    canvas->drawRectCoords(filter_bounds.x(), filter_bounds.y(),
                           filter_bounds.right(), filter_bounds.bottom(),
                           red_paint);
    list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
        ToNearestRect(filter_bounds), recorder.finishRecordingAsPicture());
  }

  list->CreateAndAppendPairedEndItem<EndFilterDisplayItem>();
  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkPaint paint;
  paint.setColor(SkColorSetRGB(64, 64, 64));
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.drawRect(RectFToSkRect(filter_bounds), paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, CompactingItems) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};

  gfx::PointF offset(8.f, 9.f);
  gfx::RectF recording_rect(offset, gfx::SizeF(layer_rect.size()));

  DisplayItemListSettings no_caching_settings;
  scoped_refptr<DisplayItemList> list_without_caching =
      DisplayItemList::Create(no_caching_settings);

  canvas =
      sk_ref_sp(recorder.beginRecording(gfx::RectFToSkRect(recording_rect)));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
  list_without_caching->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      kVisualRect, picture);
  list_without_caching->Finalize();
  DrawDisplayList(pixels, layer_rect, list_without_caching);

  unsigned char expected_pixels[4 * 100 * 100] = {0};
  DisplayItemListSettings caching_settings;
  caching_settings.use_cached_picture = true;
  scoped_refptr<DisplayItemList> list_with_caching =
      DisplayItemList::Create(caching_settings);
  list_with_caching->CreateAndAppendDrawingItem<DrawingDisplayItem>(kVisualRect,
                                                                    picture);
  list_with_caching->Finalize();
  DrawDisplayList(expected_pixels, layer_rect, list_with_caching);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, ApproximateMemoryUsage) {
  const int kNumCommandsInTestSkPicture = 1000;
  scoped_refptr<DisplayItemList> list;
  size_t memory_usage;

  // Make an SkPicture whose size is known.
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(layer_rect));
  for (int i = 0; i < kNumCommandsInTestSkPicture; i++)
    canvas->drawPaint(blue_paint);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
  size_t picture_size = SkPictureUtils::ApproximateBytesUsed(picture.get());
  ASSERT_GE(picture_size, kNumCommandsInTestSkPicture * sizeof(blue_paint));

  // Using a cached picture, we should get about the right size.
  DisplayItemListSettings caching_settings;
  caching_settings.use_cached_picture = true;
  list = DisplayItemList::Create(caching_settings);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(kVisualRect, picture);
  list->Finalize();
  memory_usage = list->ApproximateMemoryUsage();
  EXPECT_GE(memory_usage, picture_size);
  EXPECT_LE(memory_usage, 2 * picture_size);

  // Using no cached picture, we should still get the right size.
  DisplayItemListSettings no_caching_settings;
  no_caching_settings.use_cached_picture = false;
  list = DisplayItemList::Create(no_caching_settings);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(kVisualRect, picture);
  list->Finalize();
  memory_usage = list->ApproximateMemoryUsage();
  EXPECT_GE(memory_usage, picture_size);
  EXPECT_LE(memory_usage, 2 * picture_size);
}

TEST(DisplayItemListTest, AsValueWithNoItems) {
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(DisplayItemListSettings());
  list->SetRetainVisualRectsForTesting(true);
  list->Finalize();

  std::string value = list->AsValue(true)->ToString();
  EXPECT_EQ(value.find("\"layer_rect\": [0,0,0,0]"), std::string::npos);
  EXPECT_NE(value.find("\"items\":[]"), std::string::npos);
  EXPECT_EQ(value.find("visualRect: [0,0 42x42]"), std::string::npos);
  EXPECT_NE(value.find("\"skp64\":"), std::string::npos);

  value = list->AsValue(false)->ToString();
  EXPECT_EQ(value.find("\"layer_rect\": [0,0,0,0]"), std::string::npos);
  EXPECT_EQ(value.find("\"items\":"), std::string::npos);
  EXPECT_EQ(value.find("visualRect: [0,0 42x42]"), std::string::npos);
  EXPECT_NE(value.find("\"skp64\":"), std::string::npos);
}

TEST(DisplayItemListTest, AsValueWithItems) {
  gfx::Rect layer_rect = gfx::Rect(1, 2, 8, 9);
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(DisplayItemListSettings());
  list->SetRetainVisualRectsForTesting(true);
  gfx::Transform transform;
  transform.Translate(6.f, 7.f);
  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(transform);
  AppendFirstSerializationTestPicture(list, layer_rect.size());
  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();
  list->Finalize();

  std::string value = list->AsValue(true)->ToString();
  EXPECT_EQ(value.find("\"layer_rect\": [0,0,42,42]"), std::string::npos);
  EXPECT_NE(value.find("{\"items\":[\"TransformDisplayItem"),
            std::string::npos);
  EXPECT_NE(value.find("visualRect: [0,0 42x42]"), std::string::npos);
  EXPECT_NE(value.find("\"skp64\":"), std::string::npos);

  value = list->AsValue(false)->ToString();
  EXPECT_EQ(value.find("\"layer_rect\": [0,0,42,42]"), std::string::npos);
  EXPECT_EQ(value.find("{\"items\":[\"TransformDisplayItem"),
            std::string::npos);
  EXPECT_EQ(value.find("visualRect: [0,0 42x42]"), std::string::npos);
  EXPECT_NE(value.find("\"skp64\":"), std::string::npos);
}

TEST(DisplayItemListTest, SizeEmpty) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();
  EXPECT_EQ(0u, list->size());
}

TEST(DisplayItemListTest, SizeOne) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();
  gfx::Rect drawing_bounds(5, 6, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_bounds, CreateRectPicture(drawing_bounds));
  EXPECT_EQ(1u, list->size());
}

TEST(DisplayItemListTest, SizeMultiple) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();
  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();
  EXPECT_EQ(2u, list->size());
}

TEST(DisplayItemListTest, AppendVisualRectSimple) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // One drawing: D.

  gfx::Rect drawing_bounds(5, 6, 7, 8);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_bounds, CreateRectPicture(drawing_bounds));

  EXPECT_EQ(1u, list->size());
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
}

TEST(DisplayItemListTest, AppendVisualRectEmptyBlock) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // One block: B1, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(2u, list->size());
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(1));
}

TEST(DisplayItemListTest, AppendVisualRectEmptyBlockContainingEmptyBlock) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // Two nested blocks: B1, B2, E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);
  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(gfx::Transform());
  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(4u, list->size());
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(3));
}

TEST(DisplayItemListTest, AppendVisualRectBlockContainingDrawing) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // One block with one drawing: B1, Da, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect drawing_bounds(5, 6, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_bounds, CreateRectPicture(drawing_bounds));

  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(3u, list->size());
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(2));
}

TEST(DisplayItemListTest, AppendVisualRectBlockContainingEscapedDrawing) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // One block with one drawing: B1, Da (escapes), E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect drawing_bounds(1, 2, 3, 4);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_bounds, CreateRectPicture(drawing_bounds));

  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(3u, list->size());
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(2));
}

TEST(DisplayItemListTest,
     AppendVisualRectDrawingFollowedByBlockContainingEscapedDrawing) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // One drawing followed by one block with one drawing: Da, B1, Db (escapes),
  // E1.

  gfx::Rect drawing_a_bounds(1, 2, 3, 4);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_a_bounds, CreateRectPicture(drawing_a_bounds));

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect drawing_b_bounds(13, 14, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_b_bounds, CreateRectPicture(drawing_b_bounds));

  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(4u, list->size());
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
}

TEST(DisplayItemListTest, AppendVisualRectTwoBlocksTwoDrawings) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // Multiple nested blocks with drawings amidst: B1, Da, B2, Db, E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect drawing_a_bounds(5, 6, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_a_bounds, CreateRectPicture(drawing_a_bounds));

  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(gfx::Transform());

  gfx::Rect drawing_b_bounds(7, 8, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_b_bounds, CreateRectPicture(drawing_b_bounds));

  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(6u, list->size());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(5));
}

TEST(DisplayItemListTest,
     AppendVisualRectTwoBlocksTwoDrawingsInnerDrawingEscaped) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // Multiple nested blocks with drawings amidst: B1, Da, B2, Db (escapes), E2,
  // E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect drawing_a_bounds(5, 6, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_a_bounds, CreateRectPicture(drawing_a_bounds));

  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(gfx::Transform());

  gfx::Rect drawing_b_bounds(1, 2, 3, 4);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_b_bounds, CreateRectPicture(drawing_b_bounds));

  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(6u, list->size());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(5));
}

TEST(DisplayItemListTest,
     AppendVisualRectTwoBlocksTwoDrawingsOuterDrawingEscaped) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // Multiple nested blocks with drawings amidst: B1, Da (escapes), B2, Db, E2,
  // E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect drawing_a_bounds(1, 2, 3, 4);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_a_bounds, CreateRectPicture(drawing_a_bounds));

  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(gfx::Transform());

  gfx::Rect drawing_b_bounds(7, 8, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_b_bounds, CreateRectPicture(drawing_b_bounds));

  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(6u, list->size());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(5));
}

TEST(DisplayItemListTest,
     AppendVisualRectTwoBlocksTwoDrawingsBothDrawingsEscaped) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // Multiple nested blocks with drawings amidst:
  // B1, Da (escapes to the right), B2, Db (escapes to the left), E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect drawing_a_bounds(13, 14, 1, 1);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_a_bounds, CreateRectPicture(drawing_a_bounds));

  list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(gfx::Transform());

  gfx::Rect drawing_b_bounds(1, 2, 3, 4);
  list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      drawing_b_bounds, CreateRectPicture(drawing_b_bounds));

  list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>();
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(6u, list->size());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(5));
}

TEST(DisplayItemListTest, AppendVisualRectOneFilterNoDrawings) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // One filter containing no drawings: Bf, Ef

  gfx::Rect filter_bounds(5, 6, 1, 1);
  list->CreateAndAppendPairedBeginItemWithVisualRect<FilterDisplayItem>(
      filter_bounds, FilterOperations(), gfx::RectF(filter_bounds),
      gfx::PointF(filter_bounds.origin()));

  list->CreateAndAppendPairedEndItem<EndFilterDisplayItem>();

  EXPECT_EQ(2u, list->size());
  EXPECT_RECT_EQ(filter_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(filter_bounds, list->VisualRectForTesting(1));
}

TEST(DisplayItemListTest, AppendVisualRectBlockContainingFilterNoDrawings) {
  scoped_refptr<DisplayItemList> list = CreateDefaultList();

  // One block containing one filter and no drawings: B1, Bf, Ef, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(
      clip_bounds, std::vector<SkRRect>(), true);

  gfx::Rect filter_bounds(5, 6, 1, 1);
  list->CreateAndAppendPairedBeginItemWithVisualRect<FilterDisplayItem>(
      filter_bounds, FilterOperations(), gfx::RectF(filter_bounds),
      gfx::PointF(filter_bounds.origin()));

  list->CreateAndAppendPairedEndItem<EndFilterDisplayItem>();
  list->CreateAndAppendPairedEndItem<EndClipDisplayItem>();

  EXPECT_EQ(4u, list->size());
  EXPECT_RECT_EQ(filter_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(filter_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(filter_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(filter_bounds, list->VisualRectForTesting(3));
}

}  // namespace cc
