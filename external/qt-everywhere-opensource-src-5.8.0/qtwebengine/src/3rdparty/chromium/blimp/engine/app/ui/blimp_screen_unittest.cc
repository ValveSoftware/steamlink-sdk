// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_screen.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

using testing::InSequence;

namespace blimp {
namespace engine {
namespace {

// Checks if two display::Displays have the ID.
MATCHER_P(EqualsDisplay, display, "") {
  return display.id() == arg.id();
}

class MockDisplayObserver : public display::DisplayObserver {
 public:
  MockDisplayObserver() {}
  ~MockDisplayObserver() override {}

  MOCK_METHOD1(OnDisplayAdded, void(const display::Display&));
  MOCK_METHOD1(OnDisplayRemoved, void(const display::Display&));
  MOCK_METHOD2(OnDisplayMetricsChanged,
               void(const display::Display& display, uint32_t changed_metrics));
};

class BlimpScreenTest : public testing::Test {
 protected:
  void SetUp() override {
    screen_ = base::WrapUnique(new BlimpScreen);
    screen_->AddObserver(&observer1_);
    screen_->AddObserver(&observer2_);
  }

  std::unique_ptr<BlimpScreen> screen_;
  testing::StrictMock<MockDisplayObserver> observer1_;
  testing::StrictMock<MockDisplayObserver> observer2_;
};

TEST_F(BlimpScreenTest, ObserversAreInfomed) {
  auto display = screen_->GetPrimaryDisplay();
  uint32_t changed_metrics =
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS;

  InSequence s;
  EXPECT_CALL(observer1_,
              OnDisplayMetricsChanged(EqualsDisplay(display), changed_metrics));
  EXPECT_CALL(observer2_,
              OnDisplayMetricsChanged(EqualsDisplay(display), changed_metrics));

  changed_metrics = display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  EXPECT_CALL(observer1_,
              OnDisplayMetricsChanged(EqualsDisplay(display), changed_metrics));
  EXPECT_CALL(observer2_,
              OnDisplayMetricsChanged(EqualsDisplay(display), changed_metrics));

  changed_metrics =
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  EXPECT_CALL(observer1_,
              OnDisplayMetricsChanged(EqualsDisplay(display), changed_metrics));
  EXPECT_CALL(observer2_,
              OnDisplayMetricsChanged(EqualsDisplay(display), changed_metrics));

  gfx::Size size1(100, 200);
  screen_->UpdateDisplayScaleAndSize(2.0f, size1);
  EXPECT_EQ(size1, screen_->GetPrimaryDisplay().GetSizeInPixel());
  EXPECT_EQ(2.0f, screen_->GetPrimaryDisplay().device_scale_factor());

  screen_->UpdateDisplayScaleAndSize(2.0f, size1);

  gfx::Size size2(200, 100);
  screen_->UpdateDisplayScaleAndSize(2.0f, size2);
  EXPECT_EQ(size2, screen_->GetPrimaryDisplay().GetSizeInPixel());
  EXPECT_EQ(2.0f, screen_->GetPrimaryDisplay().device_scale_factor());

  screen_->UpdateDisplayScaleAndSize(3.0f, size2);
  EXPECT_EQ(3.0f, screen_->GetPrimaryDisplay().device_scale_factor());
}

TEST_F(BlimpScreenTest, RemoveObserver) {
  screen_->RemoveObserver(&observer2_);
  auto display = screen_->GetPrimaryDisplay();
  uint32_t changed_metrics =
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  EXPECT_CALL(observer1_,
              OnDisplayMetricsChanged(EqualsDisplay(display), changed_metrics));

  gfx::Size size1(100, 100);
  screen_->UpdateDisplayScaleAndSize(2.0f, size1);
  EXPECT_EQ(size1, screen_->GetPrimaryDisplay().GetSizeInPixel());
}

}  // namespace
}  // namespace engine
}  // namespace blimp
