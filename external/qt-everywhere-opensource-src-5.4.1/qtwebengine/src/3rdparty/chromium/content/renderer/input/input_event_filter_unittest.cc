// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
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

namespace content {
namespace {

const int kTestRoutingID = 13;

class InputEventRecorder {
 public:
  InputEventRecorder()
      : filter_(NULL),
        handle_events_(false),
        send_to_widget_(false) {
  }

  void set_filter(InputEventFilter* filter) { filter_ = filter; }
  void set_handle_events(bool value) { handle_events_ = value; }
  void set_send_to_widget(bool value) { send_to_widget_ = value; }

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
    } else {
      return send_to_widget_ ? INPUT_EVENT_ACK_STATE_NOT_CONSUMED
                             : INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
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
  std::vector<Record> records_;
};

class IPCMessageRecorder : public IPC::Listener {
 public:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
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
  for (size_t i = 0; i < events.size(); ++i) {
    message_filter->OnMessageReceived(events[i]);
  }

  base::MessageLoop::current()->RunUntilIdle();
}

void AddEventsToFilter(IPC::MessageFilter* message_filter,
                       const WebMouseEvent events[],
                       size_t count) {
  std::vector<IPC::Message> messages;
  for (size_t i = 0; i < count; ++i) {
    messages.push_back(
        InputMsg_HandleInputEvent(
            kTestRoutingID, &events[i], ui::LatencyInfo(), false));
  }

  AddMessagesToFilter(message_filter, messages);
}

}  // namespace

class InputEventFilterTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    filter_ = new InputEventFilter(
        &message_recorder_,
        message_loop_.message_loop_proxy());
    filter_->SetBoundHandler(
        base::Bind(&InputEventRecorder::HandleInputEvent,
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

  filter_->DidAddInputHandler(kTestRoutingID, NULL);

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
    WebInputEvent::Type event_type = params.a.type;
    InputEventAckState ack_result = params.a.state;

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
    const WebInputEvent* event = params.a;

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
    WebInputEvent::Type event_type = params.a.type;
    InputEventAckState ack_result = params.a.state;
    EXPECT_EQ(kEvents[i].type, event_type);
    EXPECT_EQ(ack_result, INPUT_EVENT_ACK_STATE_CONSUMED);
  }

  filter_->OnFilterRemoved();
}

TEST_F(InputEventFilterTest, PreserveRelativeOrder) {
  filter_->DidAddInputHandler(kTestRoutingID, NULL);
  event_recorder_.set_send_to_widget(true);


  WebMouseEvent mouse_down =
      SyntheticWebMouseEventBuilder::Build(WebMouseEvent::MouseDown);
  WebMouseEvent mouse_up =
      SyntheticWebMouseEventBuilder::Build(WebMouseEvent::MouseUp);

  std::vector<IPC::Message> messages;
  messages.push_back(InputMsg_HandleInputEvent(kTestRoutingID,
                                              &mouse_down,
                                              ui::LatencyInfo(),
                                              false));
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

  messages.push_back(InputMsg_HandleInputEvent(kTestRoutingID,
                                              &mouse_up,
                                              ui::LatencyInfo(),
                                              false));
  AddMessagesToFilter(filter_.get(), messages);

  // We should have sent all messages back to the main thread and preserved
  // their relative order.
  ASSERT_EQ(message_recorder_.message_count(), messages.size());
  for (size_t i = 0; i < messages.size(); ++i) {
    EXPECT_EQ(message_recorder_.message_at(i).type(), messages[i].type()) << i;
  }
}

}  // namespace content
