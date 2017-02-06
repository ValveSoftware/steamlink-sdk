// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <new>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/input/input_event_filter.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "ipc/message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace content {
namespace {

const int kTestRoutingID = 13;

class InputEventRecorder {
 public:
  InputEventRecorder()
      : filter_(NULL),
        handle_events_(false),
        send_to_widget_(false),
        passive_(false) {}

  void set_filter(InputEventFilter* filter) { filter_ = filter; }
  void set_handle_events(bool value) { handle_events_ = value; }
  void set_send_to_widget(bool value) { send_to_widget_ = value; }
  void set_passive(bool value) { passive_ = value; }

  size_t record_count() const { return records_.size(); }

  const WebInputEvent* record_at(size_t i) const {
    const Record& record = records_[i];
    return reinterpret_cast<const WebInputEvent*>(&record.event_data[0]);
  }

  void Clear() {
    records_.clear();
  }

  InputEventAckState HandleInputEvent(int routing_id,
                                      const WebInputEvent* event,
                                      ui::LatencyInfo* latency_info) {
    DCHECK_EQ(kTestRoutingID, routing_id);
    records_.push_back(Record(event));
    if (handle_events_) {
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    } else if (send_to_widget_) {
      if (passive_)
        return INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;
      else
        return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    } else {
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    }
  }

 private:
  struct Record {
    Record(const WebInputEvent* event) {
      const char* ptr = reinterpret_cast<const char*>(event);
      event_data.assign(ptr, ptr + event->size);
    }
    std::vector<char> event_data;
  };

  InputEventFilter* filter_;
  bool handle_events_;
  bool send_to_widget_;
  bool passive_;
  std::vector<Record> records_;
};

class IPCMessageRecorder : public IPC::Listener {
 public:
  bool OnMessageReceived(const IPC::Message& message) override {
    messages_.push_back(message);
    return true;
  }

  size_t message_count() const { return messages_.size(); }

  const IPC::Message& message_at(size_t i) const {
    return messages_[i];
  }

  void Clear() {
    messages_.clear();
  }

 private:
  std::vector<IPC::Message> messages_;
};

void AddMessagesToFilter(IPC::MessageFilter* message_filter,
                         const std::vector<IPC::Message>& events) {
  for (size_t i = 0; i < events.size(); ++i)
    message_filter->OnMessageReceived(events[i]);

  base::RunLoop().RunUntilIdle();
}

template <typename T>
void AddEventsToFilter(IPC::MessageFilter* message_filter,
                       const T events[],
                       size_t count) {
  std::vector<IPC::Message> messages;
  for (size_t i = 0; i < count; ++i) {
    messages.push_back(InputMsg_HandleInputEvent(
        kTestRoutingID, &events[i], ui::LatencyInfo(),
        WebInputEventTraits::ShouldBlockEventStream(events[i])
            ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
            : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING));
  }

  AddMessagesToFilter(message_filter, messages);
}

}  // namespace

class InputEventFilterTest : public testing::Test {
 public:
  void SetUp() override {
    filter_ = new InputEventFilter(
        base::Bind(base::IgnoreResult(&IPCMessageRecorder::OnMessageReceived),
                   base::Unretained(&message_recorder_)),
        base::ThreadTaskRunnerHandle::Get(), message_loop_.task_runner());
    filter_->SetBoundHandler(base::Bind(&InputEventRecorder::HandleInputEvent,
                                        base::Unretained(&event_recorder_)));

    event_recorder_.set_filter(filter_.get());

    filter_->OnFilterAdded(&ipc_sink_);
  }

 protected:
  base::MessageLoop message_loop_;

  // Used to record IPCs sent by the filter to the RenderWidgetHost.
  IPC::TestSink ipc_sink_;

  // Used to record IPCs forwarded by the filter to the main thread.
  IPCMessageRecorder message_recorder_;

  // Used to record WebInputEvents delivered to the handler.
  InputEventRecorder event_recorder_;

  scoped_refptr<InputEventFilter> filter_;
};

TEST_F(InputEventFilterTest, Basic) {
  WebMouseEvent kEvents[3] = {
    SyntheticWebMouseEventBuilder::Build(WebMouseEvent::MouseMove, 10, 10, 0),
    SyntheticWebMouseEventBuilder::Build(WebMouseEvent::MouseMove, 20, 20, 0),
    SyntheticWebMouseEventBuilder::Build(WebMouseEvent::MouseMove, 30, 30, 0)
  };

  AddEventsToFilter(filter_.get(), kEvents, arraysize(kEvents));
  EXPECT_EQ(0U, ipc_sink_.message_count());
  EXPECT_EQ(0U, event_recorder_.record_count());
  EXPECT_EQ(0U, message_recorder_.message_count());

  filter_->RegisterRoutingID(kTestRoutingID);

  AddEventsToFilter(filter_.get(), kEvents, arraysize(kEvents));
  ASSERT_EQ(arraysize(kEvents), ipc_sink_.message_count());
  ASSERT_EQ(arraysize(kEvents), event_recorder_.record_count());
  EXPECT_EQ(0U, message_recorder_.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message* message = ipc_sink_.GetMessageAt(i);
    EXPECT_EQ(kTestRoutingID, message->routing_id());
    EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());

    InputHostMsg_HandleInputEvent_ACK::Param params;
    EXPECT_TRUE(InputHostMsg_HandleInputEvent_ACK::Read(message, &params));
    WebInputEvent::Type event_type = std::get<0>(params).type;
    InputEventAckState ack_result = std::get<0>(params).state;

    EXPECT_EQ(kEvents[i].type, event_type);
    EXPECT_EQ(ack_result, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

    const WebInputEvent* event = event_recorder_.record_at(i);
    ASSERT_TRUE(event);

    EXPECT_EQ(kEvents[i].size, event->size);
    EXPECT_TRUE(memcmp(&kEvents[i], event, event->size) == 0);
  }

  event_recorder_.set_send_to_widget(true);

  AddEventsToFilter(filter_.get(), kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), ipc_sink_.message_count());
  EXPECT_EQ(2 * arraysize(kEvents), event_recorder_.record_count());
  EXPECT_EQ(arraysize(kEvents), message_recorder_.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message& message = message_recorder_.message_at(i);

    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebInputEvent* event = std::get<0>(params);

    EXPECT_EQ(kEvents[i].size, event->size);
    EXPECT_TRUE(memcmp(&kEvents[i], event, event->size) == 0);
  }

  // Now reset everything, and test that DidHandleInputEvent is called.

  ipc_sink_.ClearMessages();
  event_recorder_.Clear();
  message_recorder_.Clear();

  event_recorder_.set_handle_events(true);

  AddEventsToFilter(filter_.get(), kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), ipc_sink_.message_count());
  EXPECT_EQ(arraysize(kEvents), event_recorder_.record_count());
  EXPECT_EQ(0U, message_recorder_.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message* message = ipc_sink_.GetMessageAt(i);
    EXPECT_EQ(kTestRoutingID, message->routing_id());
    EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());

    InputHostMsg_HandleInputEvent_ACK::Param params;
    EXPECT_TRUE(InputHostMsg_HandleInputEvent_ACK::Read(message, &params));
    WebInputEvent::Type event_type = std::get<0>(params).type;
    InputEventAckState ack_result = std::get<0>(params).state;
    EXPECT_EQ(kEvents[i].type, event_type);
    EXPECT_EQ(ack_result, INPUT_EVENT_ACK_STATE_CONSUMED);
  }

  filter_->OnFilterRemoved();
}

TEST_F(InputEventFilterTest, PreserveRelativeOrder) {
  filter_->RegisterRoutingID(kTestRoutingID);
  event_recorder_.set_send_to_widget(true);


  WebMouseEvent mouse_down =
      SyntheticWebMouseEventBuilder::Build(WebMouseEvent::MouseDown);
  WebMouseEvent mouse_up =
      SyntheticWebMouseEventBuilder::Build(WebMouseEvent::MouseUp);

  std::vector<IPC::Message> messages;
  messages.push_back(InputMsg_HandleInputEvent(
      kTestRoutingID, &mouse_down, ui::LatencyInfo(),
      WebInputEventTraits::ShouldBlockEventStream(mouse_down)
          ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
          : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING));
  // Control where input events are delivered.
  messages.push_back(InputMsg_MouseCaptureLost(kTestRoutingID));
  messages.push_back(InputMsg_SetFocus(kTestRoutingID, true));

  // Editing operations
  messages.push_back(InputMsg_Undo(kTestRoutingID));
  messages.push_back(InputMsg_Redo(kTestRoutingID));
  messages.push_back(InputMsg_Cut(kTestRoutingID));
  messages.push_back(InputMsg_Copy(kTestRoutingID));
#if defined(OS_MACOSX)
  messages.push_back(InputMsg_CopyToFindPboard(kTestRoutingID));
#endif
  messages.push_back(InputMsg_Paste(kTestRoutingID));
  messages.push_back(InputMsg_PasteAndMatchStyle(kTestRoutingID));
  messages.push_back(InputMsg_Delete(kTestRoutingID));
  messages.push_back(InputMsg_Replace(kTestRoutingID, base::string16()));
  messages.push_back(InputMsg_ReplaceMisspelling(kTestRoutingID,
                                                     base::string16()));
  messages.push_back(InputMsg_Delete(kTestRoutingID));
  messages.push_back(InputMsg_SelectAll(kTestRoutingID));
  messages.push_back(InputMsg_Unselect(kTestRoutingID));
  messages.push_back(InputMsg_SelectRange(kTestRoutingID,
                                         gfx::Point(), gfx::Point()));
  messages.push_back(InputMsg_MoveCaret(kTestRoutingID, gfx::Point()));

  messages.push_back(InputMsg_HandleInputEvent(
      kTestRoutingID, &mouse_up, ui::LatencyInfo(),
      WebInputEventTraits::ShouldBlockEventStream(mouse_up)
          ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
          : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING));
  AddMessagesToFilter(filter_.get(), messages);

  // We should have sent all messages back to the main thread and preserved
  // their relative order.
  ASSERT_EQ(message_recorder_.message_count(), messages.size());
  for (size_t i = 0; i < messages.size(); ++i) {
    EXPECT_EQ(message_recorder_.message_at(i).type(), messages[i].type()) << i;
  }
}

TEST_F(InputEventFilterTest, NonBlockingWheel) {
  WebMouseWheelEvent kEvents[4] = {
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
  };

  filter_->RegisterRoutingID(kTestRoutingID);
  event_recorder_.set_send_to_widget(true);
  event_recorder_.set_passive(true);

  AddEventsToFilter(filter_.get(), kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), event_recorder_.record_count());
  ASSERT_EQ(4u, ipc_sink_.message_count());

  // First event is sent right away.
  EXPECT_EQ(1u, message_recorder_.message_count());

  // Second event was queued; ack the first.
  filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::MouseWheel,
                                   INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(4u, ipc_sink_.message_count());
  EXPECT_EQ(2u, message_recorder_.message_count());

  // Third event won't be coalesced into the second because modifiers are
  // different.
  filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::MouseWheel,
                                   INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, message_recorder_.message_count());

  // The last events will be coalesced.
  filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::MouseWheel,
                                   INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, message_recorder_.message_count());

  // First two messages should be identical.
  for (size_t i = 0; i < 2; ++i) {
    const IPC::Message& message = message_recorder_.message_at(i);

    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebInputEvent* event = std::get<0>(params);
    InputEventDispatchType dispatch_type = std::get<2>(params);

    EXPECT_EQ(kEvents[i].size, event->size);
    kEvents[i].dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[i], event, event->size) == 0);
    EXPECT_EQ(InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN,
              dispatch_type);
  }

  // Third message is coalesced.
  {
    const IPC::Message& message = message_recorder_.message_at(2);

    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebMouseWheelEvent* event =
        static_cast<const WebMouseWheelEvent*>(std::get<0>(params));
    InputEventDispatchType dispatch_type = std::get<2>(params);

    kEvents[2].dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(kEvents[2].size, event->size);
    EXPECT_EQ(kEvents[2].deltaX + kEvents[3].deltaX, event->deltaX);
    EXPECT_EQ(kEvents[2].deltaY + kEvents[3].deltaY, event->deltaY);
    EXPECT_EQ(InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN,
              dispatch_type);
  }
}

TEST_F(InputEventFilterTest, NonBlockingTouch) {
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].modifiers = 1;
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  filter_->RegisterRoutingID(kTestRoutingID);
  event_recorder_.set_send_to_widget(true);
  event_recorder_.set_passive(true);

  AddEventsToFilter(filter_.get(), kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), event_recorder_.record_count());
  ASSERT_EQ(4u, ipc_sink_.message_count());

  // First event is sent right away.
  EXPECT_EQ(1u, message_recorder_.message_count());

  // Second event was queued; ack the first.
  filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::TouchStart,
                                   INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(4u, ipc_sink_.message_count());
  EXPECT_EQ(2u, message_recorder_.message_count());

  // Third event won't be coalesced into the second because modifiers are
  // different.
  filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::TouchMove,
                                   INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, message_recorder_.message_count());

  // The last events will be coalesced.
  filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::TouchMove,
                                   INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, message_recorder_.message_count());

  // First two messages should be identical.
  for (size_t i = 0; i < 2; ++i) {
    const IPC::Message& message = message_recorder_.message_at(i);

    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebInputEvent* event = std::get<0>(params);
    InputEventDispatchType dispatch_type = std::get<2>(params);

    EXPECT_EQ(kEvents[i].size, event->size);
    kEvents[i].dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[i], event, event->size) == 0);
    EXPECT_EQ(InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN,
              dispatch_type);
  }

  // Third message is coalesced.
  {
    const IPC::Message& message = message_recorder_.message_at(2);

    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebTouchEvent* event =
        static_cast<const WebTouchEvent*>(std::get<0>(params));
    InputEventDispatchType dispatch_type = std::get<2>(params);

    EXPECT_EQ(kEvents[3].size, event->size);
    EXPECT_EQ(1u, kEvents[3].touchesLength);
    EXPECT_EQ(kEvents[3].touches[0].position.x, event->touches[0].position.x);
    EXPECT_EQ(kEvents[3].touches[0].position.y, event->touches[0].position.y);
    EXPECT_EQ(InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN,
              dispatch_type);
  }
}

TEST_F(InputEventFilterTest, IntermingledNonBlockingTouch) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].ReleasePoint(0);
  SyntheticWebTouchEvent kBlockingEvents[1];
  kBlockingEvents[0].PressPoint(10, 10);

  filter_->RegisterRoutingID(kTestRoutingID);
  event_recorder_.set_send_to_widget(true);
  event_recorder_.set_passive(true);
  AddEventsToFilter(filter_.get(), kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), event_recorder_.record_count());

  event_recorder_.set_passive(false);
  AddEventsToFilter(filter_.get(), kBlockingEvents, arraysize(kBlockingEvents));
  EXPECT_EQ(arraysize(kEvents) + arraysize(kBlockingEvents),
            event_recorder_.record_count());
  ASSERT_EQ(3u, event_recorder_.record_count());

  {
    // First event is sent right away.
    EXPECT_EQ(1u, message_recorder_.message_count());

    const IPC::Message& message = message_recorder_.message_at(0);
    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebInputEvent* event = std::get<0>(params);
    InputEventDispatchType dispatch_type = std::get<2>(params);

    EXPECT_EQ(kEvents[0].size, event->size);
    kEvents[0].dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[0], event, event->size) == 0);
    EXPECT_EQ(InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN,
              dispatch_type);
  }

  {
    // Second event was queued; ack the first.
    filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::TouchStart,
                                     INPUT_EVENT_ACK_STATE_CONSUMED);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(2u, message_recorder_.message_count());

    const IPC::Message& message = message_recorder_.message_at(1);
    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebInputEvent* event = std::get<0>(params);
    InputEventDispatchType dispatch_type = std::get<2>(params);

    EXPECT_EQ(kEvents[1].size, event->size);
    kEvents[1].dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[1], event, event->size) == 0);
    EXPECT_EQ(InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN,
              dispatch_type);
  }

  {
    // Third event should be put in the queue.
    filter_->NotifyInputEventHandled(kTestRoutingID, WebInputEvent::TouchEnd,
                                     INPUT_EVENT_ACK_STATE_CONSUMED);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(3u, message_recorder_.message_count());

    const IPC::Message& message = message_recorder_.message_at(2);
    ASSERT_EQ(InputMsg_HandleInputEvent::ID, message.type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(&message, &params));
    const WebInputEvent* event = std::get<0>(params);
    InputEventDispatchType dispatch_type = std::get<2>(params);

    EXPECT_EQ(kBlockingEvents[0].size, event->size);
    EXPECT_TRUE(memcmp(&kBlockingEvents[0], event, event->size) == 0);
    EXPECT_EQ(InputEventDispatchType::DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN,
              dispatch_type);
  }
}

}  // namespace content
