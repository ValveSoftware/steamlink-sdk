// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/test/mock_dri_surface.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

}  // namespace

class DriSurfaceTest : public testing::Test {
 public:
  DriSurfaceTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<ui::MockDriWrapper> drm_;
  scoped_ptr<ui::HardwareDisplayController> controller_;
  scoped_ptr<ui::MockDriSurface> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceTest);
};

void DriSurfaceTest::SetUp() {
  drm_.reset(new ui::MockDriWrapper(3));
  controller_.reset(new ui::HardwareDisplayController(drm_.get(), 1, 1));

  surface_.reset(new ui::MockDriSurface(
      drm_.get(), gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));
}

void DriSurfaceTest::TearDown() {
  surface_.reset();
  controller_.reset();
  drm_.reset();
}

TEST_F(DriSurfaceTest, FailInitialization) {
  surface_->set_initialize_expectation(false);
  EXPECT_FALSE(surface_->Initialize());
}

TEST_F(DriSurfaceTest, SuccessfulInitialization) {
  EXPECT_TRUE(surface_->Initialize());
}

TEST_F(DriSurfaceTest, CheckFBIDOnSwap) {
  EXPECT_TRUE(surface_->Initialize());
  controller_->BindSurfaceToController(surface_.PassAs<ui::ScanoutSurface>(),
                                       kDefaultMode);

  // Check that the framebuffer ID is correct.
  EXPECT_EQ(2u, controller_->surface()->GetFramebufferId());

  controller_->surface()->SwapBuffers();

  EXPECT_EQ(1u, controller_->surface()->GetFramebufferId());
}

TEST_F(DriSurfaceTest, CheckPixelPointerOnSwap) {
  EXPECT_TRUE(surface_->Initialize());

  void* bitmap_pixels1 = surface_->GetDrawableForWidget()->getDevice()
      ->accessBitmap(false).getPixels();

  surface_->SwapBuffers();

  void* bitmap_pixels2 = surface_->GetDrawableForWidget()->getDevice()
      ->accessBitmap(false).getPixels();

  // Check that once the buffers have been swapped the drawable's underlying
  // pixels have been changed.
  EXPECT_NE(bitmap_pixels1, bitmap_pixels2);
}

TEST_F(DriSurfaceTest, CheckCorrectBufferSync) {
  EXPECT_TRUE(surface_->Initialize());

  SkCanvas* canvas = surface_->GetDrawableForWidget();
  SkRect clip;
  // Modify part of the canvas.
  clip.set(0, 0,
           canvas->getDeviceSize().width() / 2,
           canvas->getDeviceSize().height() / 2);
  canvas->clipRect(clip, SkRegion::kReplace_Op);

  canvas->drawColor(SK_ColorWHITE);

  surface_->SwapBuffers();

  // Verify that the modified contents have been copied over on swap (make sure
  // the 2 buffers have the same content).
  for (int i = 0; i < canvas->getDeviceSize().height(); ++i) {
    for (int j = 0; j < canvas->getDeviceSize().width(); ++j) {
      if (i < clip.height() && j < clip.width())
        EXPECT_EQ(SK_ColorWHITE,
                  canvas->getDevice()->accessBitmap(false).getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK,
                  canvas->getDevice()->accessBitmap(false).getColor(j, i));
    }
  }
}
