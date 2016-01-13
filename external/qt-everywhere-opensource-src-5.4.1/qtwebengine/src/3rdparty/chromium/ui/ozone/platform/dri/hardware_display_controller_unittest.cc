// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/test/mock_dri_surface.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::Size kDefaultModeSize(kDefaultMode.hdisplay, kDefaultMode.vdisplay);

}  // namespace

class HardwareDisplayControllerTest : public testing::Test {
 public:
  HardwareDisplayControllerTest() {}
  virtual ~HardwareDisplayControllerTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<ui::HardwareDisplayController> controller_;
  scoped_ptr<ui::MockDriWrapper> drm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerTest);
};

void HardwareDisplayControllerTest::SetUp() {
  drm_.reset(new ui::MockDriWrapper(3));
  controller_.reset(new ui::HardwareDisplayController(drm_.get(), 1, 1));
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_.reset();
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterSurfaceIsBound) {
  scoped_ptr<ui::ScanoutSurface> surface(
      new ui::MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));
  EXPECT_TRUE(controller_->surface() != NULL);
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  scoped_ptr<ui::ScanoutSurface> surface(
      new ui::MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));
  EXPECT_TRUE(controller_->SchedulePageFlip());
  EXPECT_TRUE(controller_->surface() != NULL);
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfModesetFails) {
  drm_->set_set_crtc_expectation(false);

  scoped_ptr<ui::ScanoutSurface> surface(
      new ui::MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_FALSE(controller_->BindSurfaceToController(surface.Pass(),
                                                    kDefaultMode));
  EXPECT_EQ(NULL, controller_->surface());
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfPageFlipFails) {
  drm_->set_page_flip_expectation(false);

  scoped_ptr<ui::ScanoutSurface> surface(
      new ui::MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));
  EXPECT_FALSE(controller_->SchedulePageFlip());
}

TEST_F(HardwareDisplayControllerTest, VerifyNoDRMCallsWhenDisabled) {
  scoped_ptr<ui::ScanoutSurface> surface(
      new ui::MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));
  controller_->Disable();
  EXPECT_TRUE(controller_->SchedulePageFlip());
  EXPECT_EQ(0, drm_->get_page_flip_call_count());

  surface.reset(new ui::MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));
  EXPECT_TRUE(controller_->SchedulePageFlip());
  EXPECT_EQ(1, drm_->get_page_flip_call_count());
}
