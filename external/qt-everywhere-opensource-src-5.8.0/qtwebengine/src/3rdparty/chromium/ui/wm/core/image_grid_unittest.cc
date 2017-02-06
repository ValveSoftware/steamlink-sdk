// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/image_grid.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace wm {

namespace {

// Creates a gfx::Image with the requested dimensions.
gfx::Image* CreateImage(const gfx::Size& size) {
  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::MakeN32Premul(size.width(), size.height()));
  return new gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

}  // namespace

typedef aura::test::AuraTestBase ImageGridTest;

// Test that an ImageGrid's layers are transformed correctly when SetSize() is
// called.
TEST_F(ImageGridTest, Basic) {
  // Size of the images around the grid's border.
  const int kBorder = 2;

  std::unique_ptr<gfx::Image> image_1x1(CreateImage(gfx::Size(1, 1)));
  std::unique_ptr<gfx::Image> image_1xB(CreateImage(gfx::Size(1, kBorder)));
  std::unique_ptr<gfx::Image> image_Bx1(CreateImage(gfx::Size(kBorder, 1)));
  std::unique_ptr<gfx::Image> image_BxB(
      CreateImage(gfx::Size(kBorder, kBorder)));

  ImageGrid grid;
  grid.SetImages(image_BxB.get(), image_1xB.get(), image_BxB.get(),
                 image_Bx1.get(), image_1x1.get(), image_Bx1.get(),
                 image_BxB.get(), image_1xB.get(), image_BxB.get());

  ImageGrid::TestAPI test_api(&grid);
  ASSERT_TRUE(grid.top_left_layer() != NULL);
  ASSERT_TRUE(grid.top_layer() != NULL);
  ASSERT_TRUE(grid.top_right_layer() != NULL);
  ASSERT_TRUE(grid.left_layer() != NULL);
  ASSERT_TRUE(grid.center_layer() != NULL);
  ASSERT_TRUE(grid.right_layer() != NULL);
  ASSERT_TRUE(grid.bottom_left_layer() != NULL);
  ASSERT_TRUE(grid.bottom_layer() != NULL);
  ASSERT_TRUE(grid.bottom_right_layer() != NULL);

  const gfx::Size size(20, 30);
  grid.SetSize(size);

  // The top-left layer should be flush with the top-left corner and unscaled.
  EXPECT_EQ(gfx::RectF(0, 0, kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.top_left_layer()).ToString());

  // The top layer should be flush with the top edge and stretched horizontally
  // between the two top corners.
  EXPECT_EQ(gfx::RectF(
                kBorder, 0, size.width() - 2 * kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.top_layer()).ToString());

  // The top-right layer should be flush with the top-right corner and unscaled.
  EXPECT_EQ(gfx::RectF(size.width() - kBorder, 0, kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.top_right_layer()).ToString());

  // The left layer should be flush with the left edge and stretched vertically
  // between the two left corners.
  EXPECT_EQ(gfx::RectF(
                0, kBorder, kBorder, size.height() - 2 * kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.left_layer()).ToString());

  // The center layer should fill the space in the middle of the grid.
  EXPECT_EQ(gfx::RectF(
                kBorder, kBorder, size.width() - 2 * kBorder,
                size.height() - 2 * kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.center_layer()).ToString());

  // The right layer should be flush with the right edge and stretched
  // vertically between the two right corners.
  EXPECT_EQ(gfx::RectF(
                size.width() - kBorder, kBorder,
                kBorder, size.height() - 2 * kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.right_layer()).ToString());

  // The bottom-left layer should be flush with the bottom-left corner and
  // unscaled.
  EXPECT_EQ(gfx::RectF(0, size.height() - kBorder, kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.bottom_left_layer()).ToString());

  // The bottom layer should be flush with the bottom edge and stretched
  // horizontally between the two bottom corners.
  EXPECT_EQ(gfx::RectF(
                kBorder, size.height() - kBorder,
                size.width() - 2 * kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.bottom_layer()).ToString());

  // The bottom-right layer should be flush with the bottom-right corner and
  // unscaled.
  EXPECT_EQ(gfx::RectF(
                size.width() - kBorder, size.height() - kBorder,
                kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.bottom_right_layer()).ToString());
}

// Test that an ImageGrid's layers are transformed correctly when
// SetContentBounds() is called.
TEST_F(ImageGridTest, SetContentBounds) {
  // Size of the images around the grid's border.
  const int kBorder = 2;

  std::unique_ptr<gfx::Image> image_1x1(CreateImage(gfx::Size(1, 1)));
  std::unique_ptr<gfx::Image> image_1xB(CreateImage(gfx::Size(1, kBorder)));
  std::unique_ptr<gfx::Image> image_Bx1(CreateImage(gfx::Size(kBorder, 1)));
  std::unique_ptr<gfx::Image> image_BxB(
      CreateImage(gfx::Size(kBorder, kBorder)));

  ImageGrid grid;
  grid.SetImages(image_BxB.get(), image_1xB.get(), image_BxB.get(),
                 image_Bx1.get(), image_1x1.get(), image_Bx1.get(),
                 image_BxB.get(), image_1xB.get(), image_BxB.get());

  ImageGrid::TestAPI test_api(&grid);

  const gfx::Point origin(5, 10);
  const gfx::Size size(20, 30);
  grid.SetContentBounds(gfx::Rect(origin, size));

  // The master layer is positioned above the top-left corner of the content
  // bounds and has height/width that contain the grid and bounds.
  EXPECT_EQ(gfx::RectF(origin.x() - kBorder,
                      origin.y() - kBorder,
                      size.width() + 2 * kBorder,
                      size.height() + 2 * kBorder).ToString(),
            test_api.GetTransformedLayerBounds(*grid.layer()).ToString());
}

// Check that we don't crash if only a single image is supplied.
TEST_F(ImageGridTest, SingleImage) {
  const int kBorder = 1;
  std::unique_ptr<gfx::Image> image(CreateImage(gfx::Size(kBorder, kBorder)));

  ImageGrid grid;
  grid.SetImages(NULL, image.get(), NULL,
                 NULL, NULL, NULL,
                 NULL, NULL, NULL);

  ImageGrid::TestAPI test_api(&grid);
  EXPECT_TRUE(grid.top_left_layer() == NULL);
  ASSERT_TRUE(grid.top_layer() != NULL);
  EXPECT_TRUE(grid.top_right_layer() == NULL);
  EXPECT_TRUE(grid.left_layer() == NULL);
  EXPECT_TRUE(grid.center_layer() == NULL);
  EXPECT_TRUE(grid.right_layer() == NULL);
  EXPECT_TRUE(grid.bottom_left_layer() == NULL);
  EXPECT_TRUE(grid.bottom_layer() == NULL);
  EXPECT_TRUE(grid.bottom_right_layer() == NULL);

  const gfx::Size kSize(10, 10);
  grid.SetSize(kSize);

  // The top layer should be scaled horizontally across the entire width, but it
  // shouldn't be scaled vertically.
  EXPECT_EQ(gfx::RectF(0, 0, kSize.width(), kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.top_layer()).ToString());
}

// Check that we don't crash when we reset existing images to NULL and
// reset NULL images to new ones.
TEST_F(ImageGridTest, ResetImages) {
  const int kBorder = 1;
  std::unique_ptr<gfx::Image> image(CreateImage(gfx::Size(kBorder, kBorder)));

  ImageGrid grid;
  grid.SetImages(NULL, image.get(), NULL,
                 NULL, NULL, NULL,
                 NULL, NULL, NULL);

  // Only the top edge has a layer.
  ImageGrid::TestAPI test_api(&grid);
  ASSERT_TRUE(grid.top_left_layer() == NULL);
  ASSERT_FALSE(grid.top_layer() == NULL);
  ASSERT_TRUE(grid.top_right_layer() == NULL);
  ASSERT_TRUE(grid.left_layer() == NULL);
  ASSERT_TRUE(grid.center_layer() == NULL);
  ASSERT_TRUE(grid.right_layer() == NULL);
  ASSERT_TRUE(grid.bottom_left_layer() == NULL);
  ASSERT_TRUE(grid.bottom_layer() == NULL);
  ASSERT_TRUE(grid.bottom_right_layer() == NULL);

  grid.SetImages(NULL, NULL, NULL,
                 NULL, NULL, NULL,
                 NULL, image.get(), NULL);

  // Now only the bottom edge has a layer.
  ASSERT_TRUE(grid.top_left_layer() == NULL);
  ASSERT_TRUE(grid.top_layer() == NULL);
  ASSERT_TRUE(grid.top_right_layer() == NULL);
  ASSERT_TRUE(grid.left_layer() == NULL);
  ASSERT_TRUE(grid.center_layer() == NULL);
  ASSERT_TRUE(grid.right_layer() == NULL);
  ASSERT_TRUE(grid.bottom_left_layer() == NULL);
  ASSERT_FALSE(grid.bottom_layer() == NULL);
  ASSERT_TRUE(grid.bottom_right_layer() == NULL);
}

// Test that side (top, left, right, bottom) layers that are narrower than their
// adjacent corner layers stay pinned to the outside edges instead of getting
// moved inwards or scaled.  This exercises the scenario used for shadows.
TEST_F(ImageGridTest, SmallerSides) {
  const int kCorner = 2;
  const int kEdge = 1;

  std::unique_ptr<gfx::Image> top_left_image(
      CreateImage(gfx::Size(kCorner, kCorner)));
  std::unique_ptr<gfx::Image> top_image(CreateImage(gfx::Size(kEdge, kEdge)));
  std::unique_ptr<gfx::Image> top_right_image(
      CreateImage(gfx::Size(kCorner, kCorner)));
  std::unique_ptr<gfx::Image> left_image(CreateImage(gfx::Size(kEdge, kEdge)));
  std::unique_ptr<gfx::Image> right_image(CreateImage(gfx::Size(kEdge, kEdge)));

  ImageGrid grid;
  grid.SetImages(top_left_image.get(), top_image.get(), top_right_image.get(),
                 left_image.get(), NULL, right_image.get(),
                 NULL, NULL, NULL);
  ImageGrid::TestAPI test_api(&grid);

  const gfx::Size kSize(20, 30);
  grid.SetSize(kSize);

  // The top layer should be flush with the top edge and stretched horizontally
  // between the two top corners.
  EXPECT_EQ(gfx::RectF(
                kCorner, 0, kSize.width() - 2 * kCorner, kEdge).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.top_layer()).ToString());

  // The left layer should be flush with the left edge and stretched vertically
  // between the top left corner and the bottom.
  EXPECT_EQ(gfx::RectF(
                0, kCorner, kEdge, kSize.height() - kCorner).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.left_layer()).ToString());

  // The right layer should be flush with the right edge and stretched
  // vertically between the top right corner and the bottom.
  EXPECT_EQ(gfx::RectF(
                kSize.width() - kEdge, kCorner,
                kEdge, kSize.height() - kCorner).ToString(),
            test_api.GetTransformedLayerBounds(
                *grid.right_layer()).ToString());
}

// Test that we hide or clip layers as needed when the grid is assigned a small
// size.
TEST_F(ImageGridTest, TooSmall) {
  const int kCorner = 5;
  const int kCenter = 3;
  const int kEdge = 3;

  std::unique_ptr<gfx::Image> top_left_image(
      CreateImage(gfx::Size(kCorner, kCorner)));
  std::unique_ptr<gfx::Image> top_image(CreateImage(gfx::Size(kEdge, kEdge)));
  std::unique_ptr<gfx::Image> top_right_image(
      CreateImage(gfx::Size(kCorner, kCorner)));
  std::unique_ptr<gfx::Image> left_image(CreateImage(gfx::Size(kEdge, kEdge)));
  std::unique_ptr<gfx::Image> center_image(
      CreateImage(gfx::Size(kCenter, kCenter)));
  std::unique_ptr<gfx::Image> right_image(CreateImage(gfx::Size(kEdge, kEdge)));
  std::unique_ptr<gfx::Image> bottom_left_image(
      CreateImage(gfx::Size(kCorner, kCorner)));
  std::unique_ptr<gfx::Image> bottom_image(
      CreateImage(gfx::Size(kEdge, kEdge)));
  std::unique_ptr<gfx::Image> bottom_right_image(
      CreateImage(gfx::Size(kCorner, kCorner)));

  ImageGrid grid;
  grid.SetImages(
      top_left_image.get(), top_image.get(), top_right_image.get(),
      left_image.get(), center_image.get(), right_image.get(),
      bottom_left_image.get(), bottom_image.get(), bottom_right_image.get());
  ImageGrid::TestAPI test_api(&grid);

  // Set a size that's smaller than the combined (unscaled) corner images.
  const gfx::Size kSmallSize(kCorner + kCorner - 3, kCorner + kCorner - 5);
  grid.SetSize(kSmallSize);

  // The scalable images around the sides and in the center should be hidden.
  EXPECT_FALSE(grid.top_layer()->visible());
  EXPECT_FALSE(grid.bottom_layer()->visible());
  EXPECT_FALSE(grid.left_layer()->visible());
  EXPECT_FALSE(grid.right_layer()->visible());
  EXPECT_FALSE(grid.center_layer()->visible());

  // The corner images' clip rects should sum to the expected size.
  EXPECT_EQ(kSmallSize.width(),
            test_api.top_left_clip_rect().width() +
            test_api.top_right_clip_rect().width());
  EXPECT_EQ(kSmallSize.width(),
            test_api.bottom_left_clip_rect().width() +
            test_api.bottom_right_clip_rect().width());
  EXPECT_EQ(kSmallSize.height(),
            test_api.top_left_clip_rect().height() +
            test_api.bottom_left_clip_rect().height());
  EXPECT_EQ(kSmallSize.height(),
            test_api.top_right_clip_rect().height() +
            test_api.bottom_right_clip_rect().height());

  // Resize the grid to be large enough to show all images.
  const gfx::Size kLargeSize(kCorner + kCorner + kCenter,
                             kCorner + kCorner + kCenter);
  grid.SetSize(kLargeSize);

  // The scalable images should be visible now.
  EXPECT_TRUE(grid.top_layer()->visible());
  EXPECT_TRUE(grid.bottom_layer()->visible());
  EXPECT_TRUE(grid.left_layer()->visible());
  EXPECT_TRUE(grid.right_layer()->visible());
  EXPECT_TRUE(grid.center_layer()->visible());

  // We shouldn't be clipping the corner images anymore.
  EXPECT_TRUE(test_api.top_left_clip_rect().IsEmpty());
  EXPECT_TRUE(test_api.top_right_clip_rect().IsEmpty());
  EXPECT_TRUE(test_api.bottom_left_clip_rect().IsEmpty());
  EXPECT_TRUE(test_api.bottom_right_clip_rect().IsEmpty());
}

}  // namespace wm
