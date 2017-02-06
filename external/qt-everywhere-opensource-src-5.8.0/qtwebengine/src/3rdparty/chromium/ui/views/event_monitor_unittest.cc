// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/views/event_monitor.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {

class EventMonitorTest : public WidgetTest {
 public:
  EventMonitorTest() : widget_(nullptr) {}

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();
    widget_ = CreateTopLevelNativeWidget();
    widget_->SetSize(gfx::Size(100, 100));
    widget_->Show();
    if (IsMus()) {
      generator_.reset(
          new ui::test::EventGenerator(widget_->GetNativeWindow()));
    } else {
      generator_.reset(new ui::test::EventGenerator(
          GetContext(), widget_->GetNativeWindow()));
    }
    generator_->set_targeting_application(true);
  }
  void TearDown() override {
    widget_->CloseNow();
    WidgetTest::TearDown();
  }

 protected:
  Widget* widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  ui::test::TestEventHandler handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventMonitorTest);
};

TEST_F(EventMonitorTest, ShouldReceiveAppEventsWhileInstalled) {
  std::unique_ptr<EventMonitor> monitor(
      EventMonitor::CreateApplicationMonitor(&handler_));

  generator_->ClickLeftButton();
  EXPECT_EQ(2, handler_.num_mouse_events());

  monitor.reset();
  generator_->ClickLeftButton();
  EXPECT_EQ(2, handler_.num_mouse_events());
}

TEST_F(EventMonitorTest, ShouldReceiveWindowEventsWhileInstalled) {
  std::unique_ptr<EventMonitor> monitor(
      EventMonitor::CreateWindowMonitor(&handler_, widget_->GetNativeWindow()));

  generator_->ClickLeftButton();
  EXPECT_EQ(2, handler_.num_mouse_events());

  monitor.reset();
  generator_->ClickLeftButton();
  EXPECT_EQ(2, handler_.num_mouse_events());
}

TEST_F(EventMonitorTest, ShouldNotReceiveEventsFromOtherWindow) {
  Widget* widget2 = CreateTopLevelNativeWidget();
  std::unique_ptr<EventMonitor> monitor(
      EventMonitor::CreateWindowMonitor(&handler_, widget2->GetNativeWindow()));

  generator_->ClickLeftButton();
  EXPECT_EQ(0, handler_.num_mouse_events());

  monitor.reset();
  widget2->CloseNow();
}

}  // namespace test
}  // namespace views
