// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"
#include "ui/ozone/platform/dri/test/mock_surface_generator.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

class MockScreenManager : public ui::ScreenManager {
 public:
  MockScreenManager(ui::DriWrapper* dri,
                    ui::ScanoutSurfaceGenerator* surface_generator)
      : ScreenManager(dri, surface_generator), dri_(dri) {}

  virtual void ForceInitializationOfPrimaryDisplay() OVERRIDE {}

 private:
  ui::DriWrapper* dri_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenManager);
};

}  // namespace

class ScreenManagerTest : public testing::Test {
 public:
  ScreenManagerTest() {}
  virtual ~ScreenManagerTest() {}

  virtual void SetUp() OVERRIDE {
    dri_.reset(new ui::MockDriWrapper(3));
    surface_generator_.reset(new ui::MockSurfaceGenerator(dri_.get()));
    screen_manager_.reset(new MockScreenManager(
        dri_.get(), surface_generator_.get()));
  }
  virtual void TearDown() OVERRIDE {
    screen_manager_.reset();
    dri_.reset();
  }

 protected:
  scoped_ptr<ui::MockDriWrapper> dri_;
  scoped_ptr<ui::MockSurfaceGenerator> surface_generator_;
  scoped_ptr<MockScreenManager> screen_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenManagerTest);
};

TEST_F(ScreenManagerTest, CheckWithNoControllers) {
  EXPECT_FALSE(screen_manager_->GetDisplayController(1));
}

TEST_F(ScreenManagerTest, CheckWithValidController) {
  screen_manager_->ConfigureDisplayController(1, 2, kDefaultMode);
  base::WeakPtr<ui::HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(1);

  EXPECT_TRUE(controller);
  EXPECT_EQ(1u, controller->crtc_id());
  EXPECT_EQ(2u, controller->connector_id());
}

TEST_F(ScreenManagerTest, CheckWithInvalidId) {
  screen_manager_->ConfigureDisplayController(1, 2, kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
}

TEST_F(ScreenManagerTest, CheckForSecondValidController) {
  screen_manager_->ConfigureDisplayController(1, 2, kDefaultMode);
  screen_manager_->ConfigureDisplayController(3, 4, kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_TRUE(screen_manager_->GetDisplayController(2));
}

TEST_F(ScreenManagerTest, CheckControllerAfterItIsRemoved) {
  screen_manager_->ConfigureDisplayController(1, 2, kDefaultMode);
  base::WeakPtr<ui::HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(1);

  EXPECT_TRUE(controller);
  screen_manager_->RemoveDisplayController(1, 2);
  EXPECT_FALSE(controller);
}

TEST_F(ScreenManagerTest, CheckDuplicateConfiguration) {
  screen_manager_->ConfigureDisplayController(1, 2, kDefaultMode);
  screen_manager_->ConfigureDisplayController(1, 2, kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
}

TEST_F(ScreenManagerTest, CheckChangingMode) {
  screen_manager_->ConfigureDisplayController(1, 2, kDefaultMode);
  drmModeModeInfo new_mode = kDefaultMode;
  new_mode.vdisplay = 10;
  screen_manager_->ConfigureDisplayController(1, 2, new_mode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
  drmModeModeInfo mode = screen_manager_->GetDisplayController(1)->get_mode();
  EXPECT_EQ(new_mode.vdisplay, mode.vdisplay);
  EXPECT_EQ(new_mode.hdisplay, mode.hdisplay);
}
