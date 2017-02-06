// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_instance_throttler_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "ui/gfx/canvas.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::Return;

namespace content {

class PluginInstanceThrottlerImplTest
    : public testing::Test,
      public PluginInstanceThrottler::Observer {
 protected:
  const int kMaximumFramesToExamine =
      PluginInstanceThrottlerImpl::kMaximumFramesToExamine;

  PluginInstanceThrottlerImplTest() : change_callback_calls_(0) {}
  ~PluginInstanceThrottlerImplTest() override {
    throttler_->RemoveObserver(this);
  }

  void SetUp() override {
    throttler_.reset(new PluginInstanceThrottlerImpl);
    throttler_->Initialize(nullptr, url::Origin(GURL("http://example.com")),
                           "Shockwave Flash", gfx::Size(100, 100));
    throttler_->AddObserver(this);
  }

  PluginInstanceThrottlerImpl* throttler() {
    DCHECK(throttler_.get());
    return throttler_.get();
  }

  void DisablePowerSaverByRetroactiveWhitelist() {
    throttler()->MarkPluginEssential(
        PluginInstanceThrottlerImpl::UNTHROTTLE_METHOD_BY_WHITELIST);
  }

  int change_callback_calls() { return change_callback_calls_; }

  void EngageThrottle() { throttler_->EngageThrottle(); }

  void SendEventAndTest(blink::WebInputEvent::Type event_type,
                        bool expect_consumed,
                        bool expect_throttled,
                        int expect_change_callback_count) {
    blink::WebMouseEvent event;
    event.type = event_type;
    event.modifiers = blink::WebInputEvent::Modifiers::LeftButtonDown;
    EXPECT_EQ(expect_consumed, throttler()->ConsumeInputEvent(event));
    EXPECT_EQ(expect_throttled, throttler()->IsThrottled());
    EXPECT_EQ(expect_change_callback_count, change_callback_calls());
  }

 private:
  // PluginInstanceThrottlerImpl::Observer
  void OnThrottleStateChange() override { ++change_callback_calls_; }

  std::unique_ptr<PluginInstanceThrottlerImpl> throttler_;

  int change_callback_calls_;

  base::MessageLoop loop_;
};

TEST_F(PluginInstanceThrottlerImplTest, ThrottleAndUnthrottleByClick) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  // MouseUp while throttled should be consumed and disengage throttling.
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, true, false, 2);
}

TEST_F(PluginInstanceThrottlerImplTest, ThrottleByKeyframe) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  SkBitmap boring_bitmap;
  gfx::Canvas canvas(gfx::Size(20, 10), 1.0f, true);
  canvas.FillRect(gfx::Rect(20, 10), SK_ColorBLACK);
  canvas.FillRect(gfx::Rect(10, 10), SK_ColorWHITE);
  SkBitmap interesting_bitmap = skia::ReadPixels(canvas.sk_canvas());

  // Don't throttle for a boring frame.
  throttler()->OnImageFlush(&boring_bitmap);
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  // Throttle after an interesting frame.
  throttler()->OnImageFlush(&interesting_bitmap);
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, MaximumKeyframesAnalyzed) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  SkBitmap boring_bitmap;

  // Throttle after tons of boring bitmaps.
  for (int i = 0; i < kMaximumFramesToExamine; ++i) {
    throttler()->OnImageFlush(&boring_bitmap);
  }
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());
}
TEST_F(PluginInstanceThrottlerImplTest, IgnoreThrottlingAfterMouseUp) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  // MouseUp before throttling engaged should not be consumed, but should
  // prevent subsequent throttling from engaging.
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, false, false, 0);

  EngageThrottle();
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, FastWhitelisting) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  DisablePowerSaverByRetroactiveWhitelist();

  EngageThrottle();
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, SlowWhitelisting) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  DisablePowerSaverByRetroactiveWhitelist();
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(2, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, EventConsumption) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  // Consume but don't unthrottle on a variety of other events.
  SendEventAndTest(blink::WebInputEvent::Type::MouseDown, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::MouseWheel, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::MouseMove, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::KeyDown, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::KeyUp, true, true, 1);

  // Consume and unthrottle on MouseUp
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, true, false, 2);

  // Don't consume events after unthrottle.
  SendEventAndTest(blink::WebInputEvent::Type::MouseDown, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::MouseWheel, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::MouseMove, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::KeyDown, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::KeyUp, false, false, 2);

  // Subsequent MouseUps should also not be consumed.
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, false, false, 2);
}

TEST_F(PluginInstanceThrottlerImplTest, ThrottleOnLeftClickOnly) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  blink::WebMouseEvent event;
  event.type = blink::WebInputEvent::Type::MouseUp;

  event.modifiers = blink::WebInputEvent::Modifiers::RightButtonDown;
  EXPECT_FALSE(throttler()->ConsumeInputEvent(event));
  EXPECT_TRUE(throttler()->IsThrottled());

  event.modifiers = blink::WebInputEvent::Modifiers::MiddleButtonDown;
  EXPECT_TRUE(throttler()->ConsumeInputEvent(event));
  EXPECT_TRUE(throttler()->IsThrottled());

  event.modifiers = blink::WebInputEvent::Modifiers::LeftButtonDown;
  EXPECT_TRUE(throttler()->ConsumeInputEvent(event));
  EXPECT_FALSE(throttler()->IsThrottled());
}

}  // namespace content
