// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/test/layer_tree_pixel_test.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

SkXfermode::Mode const kBlendModes[] = {
    SkXfermode::kSrcOver_Mode,   SkXfermode::kScreen_Mode,
    SkXfermode::kOverlay_Mode,   SkXfermode::kDarken_Mode,
    SkXfermode::kLighten_Mode,   SkXfermode::kColorDodge_Mode,
    SkXfermode::kColorBurn_Mode, SkXfermode::kHardLight_Mode,
    SkXfermode::kSoftLight_Mode, SkXfermode::kDifference_Mode,
    SkXfermode::kExclusion_Mode, SkXfermode::kMultiply_Mode,
    SkXfermode::kHue_Mode,       SkXfermode::kSaturation_Mode,
    SkXfermode::kColor_Mode,     SkXfermode::kLuminosity_Mode};

const int kBlendModesCount = arraysize(kBlendModes);

class LayerTreeHostBlendingPixelTest : public LayerTreePixelTest {
 protected:
  void RunBlendingWithRootPixelTestType(PixelTestType type) {
    const int kLaneWidth = 15;
    const int kLaneHeight = kBlendModesCount * kLaneWidth;
    const int kRootSize = (kBlendModesCount + 2) * kLaneWidth;

    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(kRootSize, kRootSize), kCSSOrange);

    // Orange child layers will blend with the green background
    for (int i = 0; i < kBlendModesCount; ++i) {
      gfx::Rect child_rect(
          (i + 1) * kLaneWidth, kLaneWidth, kLaneWidth, kLaneHeight);
      scoped_refptr<SolidColorLayer> green_lane =
          CreateSolidColorLayer(child_rect, kCSSGreen);
      background->AddChild(green_lane);
      green_lane->SetBlendMode(kBlendModes[i]);
    }

    RunPixelTest(type,
                 background,
                 base::FilePath(FILE_PATH_LITERAL("blending_with_root.png")));
  }

  void RunBlendingWithTransparentPixelTestType(PixelTestType type) {
    const int kLaneWidth = 15;
    const int kLaneHeight = kBlendModesCount * kLaneWidth;
    const int kRootSize = (kBlendModesCount + 2) * kLaneWidth;

    scoped_refptr<SolidColorLayer> root =
        CreateSolidColorLayer(gfx::Rect(kRootSize, kRootSize), kCSSBrown);

    scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
        gfx::Rect(0, kLaneWidth * 2, kRootSize, kLaneWidth), kCSSOrange);

    root->AddChild(background);
    background->SetIsRootForIsolatedGroup(true);

    // Orange child layers will blend with the green background
    for (int i = 0; i < kBlendModesCount; ++i) {
      gfx::Rect child_rect(
          (i + 1) * kLaneWidth, -kLaneWidth, kLaneWidth, kLaneHeight);
      scoped_refptr<SolidColorLayer> green_lane =
          CreateSolidColorLayer(child_rect, kCSSGreen);
      background->AddChild(green_lane);
      green_lane->SetBlendMode(kBlendModes[i]);
    }

    RunPixelTest(type,
                 root,
                 base::FilePath(FILE_PATH_LITERAL("blending_transparent.png")));
  }
};

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRoot_GL) {
  RunBlendingWithRootPixelTestType(GL_WITH_BITMAP);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithBackgroundFilter) {
  const int kLaneWidth = 15;
  const int kLaneHeight = kBlendModesCount * kLaneWidth;
  const int kRootSize = (kBlendModesCount + 2) * kLaneWidth;

  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(kRootSize, kRootSize), kCSSOrange);

  // Orange child layers have a background filter set and they will blend with
  // the green background
  for (int i = 0; i < kBlendModesCount; ++i) {
    gfx::Rect child_rect(
        (i + 1) * kLaneWidth, kLaneWidth, kLaneWidth, kLaneHeight);
    scoped_refptr<SolidColorLayer> green_lane =
        CreateSolidColorLayer(child_rect, kCSSGreen);
    background->AddChild(green_lane);

    FilterOperations filters;
    filters.Append(FilterOperation::CreateGrayscaleFilter(.75));
    green_lane->SetBackgroundFilters(filters);
    green_lane->SetBlendMode(kBlendModes[i]);
  }

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("blending_and_filter.png")));
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithTransparent_GL) {
  RunBlendingWithTransparentPixelTestType(GL_WITH_BITMAP);
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
