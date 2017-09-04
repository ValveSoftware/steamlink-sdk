// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostScrollbarsPixelTest : public LayerTreePixelTest {
 protected:
  LayerTreeHostScrollbarsPixelTest() = default;

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->layer_transforms_should_scale_layer_contents = true;
  }

  void SetupTree() override {
    layer_tree_host()->GetLayerTree()->SetDeviceScaleFactor(
        device_scale_factor_);
    LayerTreePixelTest::SetupTree();
  }

  float device_scale_factor_ = 1.f;
};

class PaintedScrollbar : public Scrollbar {
 public:
  ~PaintedScrollbar() override = default;

  ScrollbarOrientation Orientation() const override { return VERTICAL; }
  bool IsLeftSideVerticalScrollbar() const override { return false; }
  gfx::Point Location() const override { return gfx::Point(); }
  bool IsOverlay() const override { return false; }
  bool HasThumb() const override { return thumb_; }
  int ThumbThickness() const override { return rect_.width(); }
  int ThumbLength() const override { return rect_.height(); }
  gfx::Rect TrackRect() const override { return rect_; }
  float ThumbOpacity() const override { return 1.f; }
  bool NeedsPaintPart(ScrollbarPart part) const override { return true; }
  void PaintPart(SkCanvas* canvas,
                 ScrollbarPart part,
                 const gfx::Rect& content_rect) override {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(paint_scale_));
    paint.setColor(color_);

    gfx::Rect inset_rect = content_rect;
    while (!inset_rect.IsEmpty()) {
      int big = paint_scale_ + 2;
      int small = paint_scale_;
      inset_rect.Inset(big, big, small, small);
      canvas->drawRect(RectToSkRect(inset_rect), paint);
      inset_rect.Inset(big, big, small, small);
    }
  }

  void set_paint_scale(int scale) { paint_scale_ = scale; }

 private:
  int paint_scale_ = 4;
  bool thumb_ = false;
  SkColor color_ = SK_ColorGREEN;
  gfx::Rect rect_;
};

TEST_F(LayerTreeHostScrollbarsPixelTest, NoScale) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  auto scrollbar = base::MakeUnique<PaintedScrollbar>();
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar), Layer::INVALID_ID);
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(200, 200));
  background->AddChild(layer);

  RunPixelTest(PIXEL_TEST_GL, background,
               base::FilePath(FILE_PATH_LITERAL("spiral.png")));
}

TEST_F(LayerTreeHostScrollbarsPixelTest, DeviceScaleFactor) {
  // With a device scale of 2, the scrollbar should still be rendered
  // pixel-perfect, not show scaling artifacts
  device_scale_factor_ = 2.f;

  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  auto scrollbar = base::MakeUnique<PaintedScrollbar>();
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar), Layer::INVALID_ID);
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(100, 100));
  background->AddChild(layer);

  RunPixelTest(PIXEL_TEST_GL, background,
               base::FilePath(FILE_PATH_LITERAL("spiral_double_scale.png")));
}

TEST_F(LayerTreeHostScrollbarsPixelTest, TransformScale) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  auto scrollbar = base::MakeUnique<PaintedScrollbar>();
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar), Layer::INVALID_ID);
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(100, 100));
  background->AddChild(layer);

  // This has a scale of 2, it should still be rendered pixel-perfect, not show
  // scaling artifacts
  gfx::Transform scale_transform;
  scale_transform.Scale(2.0, 2.0);
  layer->SetTransform(scale_transform);

  RunPixelTest(PIXEL_TEST_GL, background,
               base::FilePath(FILE_PATH_LITERAL("spiral_double_scale.png")));
}

TEST_F(LayerTreeHostScrollbarsPixelTest, HugeTransformScale) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(400, 400), SK_ColorWHITE);

  auto scrollbar = base::MakeUnique<PaintedScrollbar>();
  scrollbar->set_paint_scale(1);
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar), Layer::INVALID_ID);
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(10, 400));
  background->AddChild(layer);

  scoped_refptr<TestInProcessContextProvider> context(
      new TestInProcessContextProvider(nullptr));
  context->BindToCurrentThread();
  int max_texture_size = 0;
  context->ContextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

  // We want a scale that creates a texture taller than the max texture size. If
  // there's no clamping, the texture will be invalid and we'll just get black.
  double scale = 64.0;
  ASSERT_GT(scale * layer->bounds().height(), max_texture_size);

  // Let's show the bottom right of the layer, so we know the texture wasn't
  // just cut off.
  layer->SetPosition(
      gfx::PointF(-10.f * scale + 400.f, -400.f * scale + 400.f));

  gfx::Transform scale_transform;
  scale_transform.Scale(scale, scale);
  layer->SetTransform(scale_transform);

  RunPixelTest(PIXEL_TEST_GL, background,
               base::FilePath(FILE_PATH_LITERAL("spiral_64_scale.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
