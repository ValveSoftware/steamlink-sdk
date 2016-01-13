// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <vector>

#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/test/mock_render_process.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/rect.h"

namespace content {

class RenderWidgetUnittest : public testing::Test {
 public:
  RenderWidgetUnittest() {}
  virtual ~RenderWidgetUnittest() {}

 private:
  MockRenderProcess render_process_;
  MockRenderThread render_thread_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetUnittest);
};

class TouchableRenderWidget : public RenderWidget {
 public:
  TouchableRenderWidget()
      : RenderWidget(blink::WebPopupTypeNone,
                     blink::WebScreenInfo(),
                     false,
                     false,
                     false) {}

  void SetTouchRegion(const std::vector<gfx::Rect>& rects) {
    rects_ = rects;
  }

  void SendInputEvent(const blink::WebInputEvent& event) {
    OnHandleInputEvent(&event, ui::LatencyInfo(), false);
  }

  IPC::TestSink* sink() { return &sink_; }

 protected:
  virtual ~TouchableRenderWidget() {}

  // Overridden from RenderWidget:
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const OVERRIDE {
    for (std::vector<gfx::Rect>::const_iterator iter = rects_.begin();
         iter != rects_.end(); ++iter) {
      if ((*iter).Contains(point))
        return true;
    }
    return false;
  }

  virtual bool Send(IPC::Message* msg) OVERRIDE {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  std::vector<gfx::Rect> rects_;
  IPC::TestSink sink_;

  DISALLOW_COPY_AND_ASSIGN(TouchableRenderWidget);
};

TEST_F(RenderWidgetUnittest, TouchHitTestSinglePoint) {
  scoped_refptr<TouchableRenderWidget> widget = new TouchableRenderWidget();

  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);

  widget->SendInputEvent(touch);
  ASSERT_EQ(1u, widget->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = params.a.state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget->sink()->ClearMessages();

  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget->SetTouchRegion(rects);

  widget->SendInputEvent(touch);
  ASSERT_EQ(1u, widget->sink()->message_count());
  message = widget->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = params.a.state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, TouchHitTestMultiplePoints) {
  scoped_refptr<TouchableRenderWidget> widget = new TouchableRenderWidget();
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget->SetTouchRegion(rects);

  SyntheticWebTouchEvent touch;
  touch.PressPoint(25, 25);

  widget->SendInputEvent(touch);
  ASSERT_EQ(1u, widget->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = params.a.state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget->sink()->ClearMessages();

  // Press a second touch point. This time, on a touch-handling region.
  touch.PressPoint(10, 10);
  widget->SendInputEvent(touch);
  ASSERT_EQ(1u, widget->sink()->message_count());
  message = widget->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = params.a.state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget->sink()->ClearMessages();
}

}  // namespace content
