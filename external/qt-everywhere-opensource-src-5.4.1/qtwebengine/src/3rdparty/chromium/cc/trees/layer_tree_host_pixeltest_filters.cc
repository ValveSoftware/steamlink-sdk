// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostFiltersPixelTest : public LayerTreePixelTest {};

TEST_F(LayerTreeHostFiltersPixelTest, BackgroundFilterBlur) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // The green box is entirely behind a layer with background blur, so it
  // should appear blurred on its edges.
  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(30, 30, 140, 140), SK_ColorTRANSPARENT);
  background->AddChild(green);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));
  blur->SetBackgroundFilters(filters);

#if defined(OS_WIN)
  // Windows has 436 pixels off by 1: crbug.com/259915
  float percentage_pixels_large_error = 1.09f;  // 436px / (200*200)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 1;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("background_filter_blur.png")));
}

TEST_F(LayerTreeHostFiltersPixelTest, BackgroundFilterBlurOutsets) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // The green border is outside the layer with background blur, but the
  // background blur should use pixels from outside its layer borders, up to the
  // radius of the blur effect. So the border should be blurred underneath the
  // top layer causing the green to bleed under the transparent layer, but not
  // in the 1px region between the transparent layer and the green border.
  scoped_refptr<SolidColorLayer> green_border = CreateSolidColorLayerWithBorder(
      gfx::Rect(1, 1, 198, 198), SK_ColorWHITE, 10, kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(12, 12, 176, 176), SK_ColorTRANSPARENT);
  background->AddChild(green_border);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));
  blur->SetBackgroundFilters(filters);

#if defined(OS_WIN)
  // Windows has 2596 pixels off by at most 2: crbug.com/259922
  float percentage_pixels_large_error = 6.5f;  // 2596px / (200*200), rounded up
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 2;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "background_filter_blur_outsets.png")));
}

TEST_F(LayerTreeHostFiltersPixelTest, BackgroundFilterBlurOffAxis) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // This verifies that the perspective of the clear layer (with black border)
  // does not influence the blending of the green box behind it. Also verifies
  // that the blur is correctly clipped inside the transformed clear layer.
  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayerWithBorder(
      gfx::Rect(30, 30, 120, 120), SK_ColorTRANSPARENT, 1, SK_ColorBLACK);
  background->AddChild(green);
  background->AddChild(blur);

  background->SetShouldFlattenTransform(false);
  background->Set3dSortingContextId(1);
  green->SetShouldFlattenTransform(false);
  green->Set3dSortingContextId(1);
  gfx::Transform background_transform;
  background_transform.ApplyPerspectiveDepth(200.0);
  background->SetTransform(background_transform);

  blur->SetShouldFlattenTransform(false);
  blur->Set3dSortingContextId(1);
  for (size_t i = 0; i < blur->children().size(); ++i)
    blur->children()[i]->Set3dSortingContextId(1);

  gfx::Transform blur_transform;
  blur_transform.Translate(55.0, 65.0);
  blur_transform.RotateAboutXAxis(85.0);
  blur_transform.RotateAboutYAxis(180.0);
  blur_transform.RotateAboutZAxis(20.0);
  blur_transform.Translate(-60.0, -60.0);
  blur->SetTransform(blur_transform);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));
  blur->SetBackgroundFilters(filters);

#if defined(OS_WIN)
  // Windows has 153 pixels off by at most 2: crbug.com/225027
  float percentage_pixels_large_error = 0.3825f;  // 153px / (200*200)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 2;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "background_filter_blur_off_axis.png")));
}

class ImageFilterClippedPixelTest : public LayerTreeHostFiltersPixelTest {
 protected:
  void RunPixelTestType(PixelTestType test_type) {
    scoped_refptr<SolidColorLayer> root =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorYELLOW);
    root->AddChild(background);

    scoped_refptr<SolidColorLayer> foreground =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorRED);
    background->AddChild(foreground);

    SkScalar matrix[20];
    memset(matrix, 0, 20 * sizeof(matrix[0]));
    // This filter does a red-blue swap, so the foreground becomes blue.
    matrix[2] = matrix[6] = matrix[10] = matrix[18] = SK_Scalar1;
    skia::RefPtr<SkColorFilter> colorFilter(
        skia::AdoptRef(SkColorMatrixFilter::Create(matrix)));
    // We filter only the bottom 200x100 pixels of the foreground.
    SkImageFilter::CropRect crop_rect(SkRect::MakeXYWH(0, 100, 200, 100));
    skia::RefPtr<SkImageFilter> filter = skia::AdoptRef(
        SkColorFilterImageFilter::Create(colorFilter.get(), NULL, &crop_rect));
    FilterOperations filters;
    filters.Append(FilterOperation::CreateReferenceFilter(filter));

    // Make the foreground layer's render surface be clipped by the background
    // layer.
    background->SetMasksToBounds(true);
    foreground->SetFilters(filters);

    // Then we translate the foreground up by 100 pixels in Y, so the cropped
    // region is moved to to the top. This ensures that the crop rect is being
    // correctly transformed in skia by the amount of clipping that the
    // compositor performs.
    gfx::Transform transform;
    transform.Translate(0.0, -100.0);
    foreground->SetTransform(transform);

    RunPixelTest(test_type,
                 background,
                 base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")));
  }
};

TEST_F(ImageFilterClippedPixelTest, ImageFilterClipped_GL) {
  RunPixelTestType(GL_WITH_BITMAP);
}

TEST_F(ImageFilterClippedPixelTest, ImageFilterClipped_Software) {
  RunPixelTestType(SOFTWARE_WITH_BITMAP);
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
