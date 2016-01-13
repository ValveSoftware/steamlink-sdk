// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/content_layer.h"

#include "cc/layers/content_layer_client.h"
#include "cc/resources/bitmap_content_layer_updater.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {
namespace {

class MockContentLayerClient : public ContentLayerClient {
 public:
  explicit MockContentLayerClient(const gfx::Rect& opaque_layer_rect)
      : opaque_layer_rect_(opaque_layer_rect) {}

  virtual void PaintContents(
      SkCanvas* canvas,
      const gfx::Rect& clip,
      gfx::RectF* opaque,
      ContentLayerClient::GraphicsContextStatus gc_status) OVERRIDE {
    *opaque = gfx::RectF(opaque_layer_rect_);
  }
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}
  virtual bool FillsBoundsCompletely() const OVERRIDE { return false; }

 private:
  gfx::Rect opaque_layer_rect_;
};

TEST(ContentLayerTest, ContentLayerPainterWithDeviceScale) {
  float contents_scale = 2.f;
  gfx::Rect content_rect(10, 10, 100, 100);
  gfx::Rect opaque_rect_in_layer_space(5, 5, 20, 20);
  gfx::Rect opaque_rect_in_content_space = gfx::ScaleToEnclosingRect(
      opaque_rect_in_layer_space, contents_scale, contents_scale);
  MockContentLayerClient client(opaque_rect_in_layer_space);
  FakeRenderingStatsInstrumentation stats_instrumentation;
  scoped_refptr<BitmapContentLayerUpdater> updater =
      BitmapContentLayerUpdater::Create(
          ContentLayerPainter::Create(&client).PassAs<LayerPainter>(),
          &stats_instrumentation,
          0);

  gfx::Rect resulting_opaque_rect;
  updater->PrepareToUpdate(content_rect,
                           gfx::Size(256, 256),
                           contents_scale,
                           contents_scale,
                           &resulting_opaque_rect);

  EXPECT_EQ(opaque_rect_in_content_space.ToString(),
            resulting_opaque_rect.ToString());
}

}  // namespace
}  // namespace cc
