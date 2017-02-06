// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include <memory>
#include <string>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/slider_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// A views::SliderListener that tracks simple event call history.
class TestSliderListener : public views::SliderListener {
 public:
  TestSliderListener();
  ~TestSliderListener() override;

  int last_event_epoch() {
    return last_event_epoch_;
  }

  int last_drag_started_epoch() {
    return last_drag_started_epoch_;
  }

  int last_drag_ended_epoch() {
    return last_drag_ended_epoch_;
  }

  views::Slider* last_drag_started_sender() {
    return last_drag_started_sender_;
  }

  views::Slider* last_drag_ended_sender() {
    return last_drag_ended_sender_;
  }

  // Resets the state of this as if it were newly created.
  virtual void ResetCallHistory();

  // views::SliderListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;
  void SliderDragStarted(views::Slider* sender) override;
  void SliderDragEnded(views::Slider* sender) override;

 private:
  // The epoch of the last event.
  int last_event_epoch_;
  // The epoch of the last time SliderDragStarted was called.
  int last_drag_started_epoch_;
  // The epoch of the last time SliderDragEnded was called.
  int last_drag_ended_epoch_;
  // The sender from the last SliderDragStarted call.
  views::Slider* last_drag_started_sender_;
  // The sender from the last SliderDragEnded call.
  views::Slider* last_drag_ended_sender_;

  DISALLOW_COPY_AND_ASSIGN(TestSliderListener);
};

TestSliderListener::TestSliderListener()
  : last_event_epoch_(0),
    last_drag_started_epoch_(-1),
    last_drag_ended_epoch_(-1),
    last_drag_started_sender_(NULL),
    last_drag_ended_sender_(NULL) {
}

TestSliderListener::~TestSliderListener() {
  last_drag_started_sender_ = NULL;
  last_drag_ended_sender_ = NULL;
}

void TestSliderListener::ResetCallHistory() {
  last_event_epoch_ = 0;
  last_drag_started_epoch_ = -1;
  last_drag_ended_epoch_ = -1;
  last_drag_started_sender_ = NULL;
  last_drag_ended_sender_ = NULL;
}

void TestSliderListener::SliderValueChanged(views::Slider* sender,
                                            float value,
                                            float old_value,
                                            views::SliderChangeReason reason) {
  ++last_event_epoch_;
}

void TestSliderListener::SliderDragStarted(views::Slider* sender) {
  last_drag_started_sender_ = sender;
  last_drag_started_epoch_ = ++last_event_epoch_;
}

void TestSliderListener::SliderDragEnded(views::Slider* sender) {
  last_drag_ended_sender_ = sender;
  last_drag_ended_epoch_ = ++last_event_epoch_;
}

}  // namespace

namespace views {

// Base test fixture for Slider tests.
class SliderTest : public views::ViewsTestBase {
 public:
  explicit SliderTest(Slider::Orientation orientation);
  ~SliderTest() override;

 protected:
  Slider* slider() {
    return slider_;
  }

  TestSliderListener& slider_listener() {
    return slider_listener_;
  }

  int max_x() {
    return max_x_;
  }

  int max_y() {
    return max_y_;
  }

  virtual void ClickAt(int x, int y);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::test::EventGenerator* event_generator() {
    return event_generator_.get();
  }

 private:
  // The Slider's orientation
  Slider::Orientation orientation_;
  // The Slider to be tested.
  Slider* slider_;
  // A simple SliderListener test double.
  TestSliderListener slider_listener_;
  // Stores the default locale at test setup so it can be restored
  // during test teardown.
  std::string default_locale_;
  // The maximum x value within the bounds of the slider.
  int max_x_;
  // The maximum y value within the bounds of the slider.
  int max_y_;
  // The widget container for the slider being tested.
  views::Widget* widget_;
  // An event generator.
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  DISALLOW_COPY_AND_ASSIGN(SliderTest);
};

SliderTest::SliderTest(Slider::Orientation orientation)
  : orientation_(orientation),
    slider_(NULL),
    default_locale_(""),
    max_x_(0),
    max_y_(0) {
}

SliderTest::~SliderTest() {
}

void SliderTest::SetUp() {
  views::ViewsTestBase::SetUp();

  slider_ = new Slider(NULL, orientation_);
  View* view = slider_;
  gfx::Size size = view->GetPreferredSize();
  view->SetSize(size);
  max_x_ = size.width() - 1;
  max_y_ = size.height() - 1;
  default_locale_ = base::i18n::GetConfiguredLocale();

  views::Widget::InitParams init_params(CreateParams(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  init_params.bounds = gfx::Rect(size);

  widget_ = new views::Widget();
  widget_->Init(init_params);
  widget_->SetContentsView(slider_);
  widget_->Show();

  event_generator_.reset(
      new ui::test::EventGenerator(widget_->GetNativeWindow()));
}

void SliderTest::TearDown() {
  if (widget_ && !widget_->IsClosed())
    widget_->Close();

  base::i18n::SetICUDefaultLocale(default_locale_);

  views::ViewsTestBase::TearDown();
}

void SliderTest::ClickAt(int x, int y) {
  gfx::Point point(x, y);
  event_generator_->MoveMouseTo(point);
  event_generator_->ClickLeftButton();
}

// Test fixture for horizontally oriented slider tests.
class HorizontalSliderTest : public SliderTest {
 public:
  HorizontalSliderTest();
  ~HorizontalSliderTest() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HorizontalSliderTest);
};

HorizontalSliderTest::HorizontalSliderTest()
  : SliderTest(Slider::HORIZONTAL) {
}

HorizontalSliderTest::~HorizontalSliderTest() {
}

// Test fixture for vertically oriented slider tests.
class VerticalSliderTest : public SliderTest {
 public:
  VerticalSliderTest();
  ~VerticalSliderTest() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VerticalSliderTest);
};

VerticalSliderTest::VerticalSliderTest()
  : SliderTest(Slider::VERTICAL) {
}

VerticalSliderTest::~VerticalSliderTest() {
}

TEST_F(HorizontalSliderTest, UpdateFromClickHorizontal) {
  ClickAt(0, 0);
  EXPECT_EQ(0.0f, slider()->value());

  ClickAt(max_x(), 0);
  EXPECT_EQ(1.0f, slider()->value());
}

TEST_F(VerticalSliderTest, UpdateFromClickVertical) {
  ClickAt(0, 0);
  EXPECT_EQ(1.0f, slider()->value());

  ClickAt(0, max_y());
  EXPECT_EQ(0.0f, slider()->value());
}

TEST_F(HorizontalSliderTest, UpdateFromClickRTLHorizontal) {
  base::i18n::SetICUDefaultLocale("he");

  ClickAt(0, 0);
  EXPECT_EQ(1.0f, slider()->value());

  ClickAt(max_x(), 0);
  EXPECT_EQ(0.0f, slider()->value());
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)

// Test the slider location after a tap gesture.
TEST_F(HorizontalSliderTest, SliderValueForTapGesture) {
  // Tap below the minimum.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FLOAT_EQ(0, slider()->value());

  // Tap above the maximum.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(max_x(), max_y()));
  EXPECT_FLOAT_EQ(1, slider()->value());

  // Tap somwhere in the middle.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(0.75 * max_x(), 0.75 * max_y()));
  EXPECT_NEAR(0.75, slider()->value(), 0.03);
}

// Test the slider location after a scroll gesture.
TEST_F(HorizontalSliderTest, SliderValueForScrollGesture) {
  // Scroll below the minimum.
  slider()->SetValue(0.5);
  event_generator()->GestureScrollSequence(
    gfx::Point(0.5 * max_x(), 0.5 * max_y()),
    gfx::Point(0, 0),
    base::TimeDelta::FromMilliseconds(10),
    5 /* steps */);
  EXPECT_EQ(0, slider()->value());

  // Scroll above the maximum.
  slider()->SetValue(0.5);
  event_generator()->GestureScrollSequence(
    gfx::Point(0.5 * max_x(), 0.5 * max_y()),
    gfx::Point(max_x(), max_y()),
    base::TimeDelta::FromMilliseconds(10),
    5 /* steps */);
  EXPECT_EQ(1, slider()->value());

  // Scroll somewhere in the middle.
  slider()->SetValue(0.25);
  event_generator()->GestureScrollSequence(
    gfx::Point(0.25 * max_x(), 0.25 * max_y()),
    gfx::Point(0.75 * max_x(), 0.75 * max_y()),
    base::TimeDelta::FromMilliseconds(10),
    5 /* steps */);
  EXPECT_NEAR(0.75, slider()->value(), 0.03);
}

// Verifies the correct SliderListener events are raised for a tap gesture.
TEST_F(HorizontalSliderTest, SliderListenerEventsForTapGesture) {
  test::SliderTestApi slider_test_api(slider());
  slider_test_api.SetListener(&slider_listener());

  event_generator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_EQ(1, slider_listener().last_drag_started_epoch());
  EXPECT_EQ(2, slider_listener().last_drag_ended_epoch());
  EXPECT_EQ(slider(), slider_listener().last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener().last_drag_ended_sender());
}

// Verifies the correct SliderListener events are raised for a scroll gesture.
TEST_F(HorizontalSliderTest, SliderListenerEventsForScrollGesture) {
  test::SliderTestApi slider_test_api(slider());
  slider_test_api.SetListener(&slider_listener());

  event_generator()->GestureScrollSequence(
    gfx::Point(0.25 * max_x(), 0.25 * max_y()),
    gfx::Point(0.75 * max_x(), 0.75 * max_y()),
    base::TimeDelta::FromMilliseconds(0),
    5 /* steps */);

  EXPECT_EQ(1, slider_listener().last_drag_started_epoch());
  EXPECT_GT(slider_listener().last_drag_ended_epoch(),
            slider_listener().last_drag_started_epoch());
  EXPECT_EQ(slider(), slider_listener().last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener().last_drag_ended_sender());
}

// Verifies the correct SliderListener events are raised for a multi
// finger scroll gesture.
TEST_F(HorizontalSliderTest, SliderListenerEventsForMultiFingerScrollGesture) {
  test::SliderTestApi slider_test_api(slider());
  slider_test_api.SetListener(&slider_listener());

  gfx::Point points[] = {gfx::Point(0, 0.1 * max_y()),
                         gfx::Point(0, 0.2 * max_y())};
  event_generator()->GestureMultiFingerScroll(2 /* count */, points,
      0 /* event_separation_time_ms */, 5 /* steps */,
      2 /* move_x */, 0 /* move_y */);

  EXPECT_EQ(1, slider_listener().last_drag_started_epoch());
  EXPECT_GT(slider_listener().last_drag_ended_epoch(),
            slider_listener().last_drag_started_epoch());
  EXPECT_EQ(slider(), slider_listener().last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener().last_drag_ended_sender());
}

#endif  // !defined(OS_MACOSX) || defined(USE_AURA)

}  // namespace views
