// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/engine_render_widget_feature.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/net/input_message_generator.h"
#include "blimp/net/test_common.h"
#include "content/public/browser/render_widget_host.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/ime/dummy_text_input_client.h"

using testing::_;
using testing::InSequence;
using testing::Sequence;

namespace blimp {
namespace engine {

namespace {

class MockHostRenderWidgetMessageDelegate
    : public EngineRenderWidgetFeature::RenderWidgetMessageDelegate {
 public:
  // EngineRenderWidgetFeature implementation.
  void OnWebGestureEvent(
      content::RenderWidgetHost* render_widget_host,
      std::unique_ptr<blink::WebGestureEvent> event) override {
    MockableOnWebGestureEvent(render_widget_host);
  }

  void OnCompositorMessageReceived(
      content::RenderWidgetHost* render_widget_host,
      const std::vector<uint8_t>& message) override {
    MockableOnCompositorMessageReceived(render_widget_host, message);
  }

  MOCK_METHOD1(MockableOnWebGestureEvent,
               void(content::RenderWidgetHost* render_widget_host));
  MOCK_METHOD2(MockableOnCompositorMessageReceived,
               void(content::RenderWidgetHost* render_widget_host,
                    const std::vector<uint8_t>& message));
};

class MockRenderWidgetHost
    : public content::RenderWidgetHost {
 public:
  MockRenderWidgetHost() {}
  ~MockRenderWidgetHost() override {}
  void UpdateTextDirection(blink::WebTextDirection direction) override {}
  void NotifyTextDirection() override {}
  void Focus() override {}
  void Blur() override {}
  void SetActive(bool active) override {}
  void CopyFromBackingStore(const gfx::Rect& src_rect,
                            const gfx::Size& accelerated_dst_size,
                            const content::ReadbackRequestCallback& callback,
                            const SkColorType color_type) override {}
  bool CanCopyFromBackingStore() override { return false; }
  void LockBackingStore() override {}
  void UnlockBackingStore() override {}
  void ForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) override {}
  void ForwardWheelEvent(
      const blink::WebMouseWheelEvent& wheel_event) override {}
  void ForwardKeyboardEvent(
      const content::NativeWebKeyboardEvent& key_event) override {}
  void ForwardGestureEvent(
      const blink::WebGestureEvent& gesture_event) override {}
  content::RenderProcessHost* GetProcess() const override { return nullptr; }
  int GetRoutingID() const override { return 0; }
  content::RenderWidgetHostView* GetView() const override { return nullptr; }
  bool IsLoading() const override { return false; }
  void ResizeRectChanged(const gfx::Rect& new_rect) override {}
  void RestartHangMonitorTimeout() override {}
  void DisableHangMonitorForTesting() override {}
  void SetIgnoreInputEvents(bool ignore_input_events) override {}
  void WasResized() override {}
  void AddKeyPressEventCallback(
      const KeyPressEventCallback& callback) override {}
  void RemoveKeyPressEventCallback(
      const KeyPressEventCallback& callback) override {}
  void AddMouseEventCallback(const MouseEventCallback& callback) override {}
  void RemoveMouseEventCallback(const MouseEventCallback& callback) override {}
  void AddInputEventObserver(InputEventObserver* observer) override {}
  void RemoveInputEventObserver(InputEventObserver* observer) override {}
  void GetWebScreenInfo(blink::WebScreenInfo* result) override {}
  void HandleCompositorProto(const std::vector<uint8_t>& proto) override {}

  bool Send(IPC::Message* msg) override { return false; }
};

class MockTextInputClient : public ui::DummyTextInputClient {
 public:
  MockTextInputClient() : DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override {
    *text = base::string16(base::ASCIIToUTF16("green apple"));
    return false;
  }
};

MATCHER_P(CompMsgEquals, contents, "") {
  if (contents.size() != arg.size())
    return false;

  return memcmp(contents.data(), arg.data(), contents.size()) == 0;
}

MATCHER_P3(BlimpCompMsgEquals, tab_id, rw_id, contents, "") {
  if (contents.size() != arg.compositor().payload().size())
    return false;

  if (memcmp(contents.data(),
             arg.compositor().payload().data(),
             contents.size()) != 0) {
    return false;
  }

  return arg.compositor().render_widget_id() == rw_id &&
      arg.target_tab_id() == tab_id;
}

MATCHER_P3(BlimpRWMsgEquals, tab_id, rw_id, message_type, "") {
  return arg.render_widget().render_widget_id() == rw_id &&
      arg.target_tab_id() == tab_id &&
      arg.render_widget().type() == message_type;
}

MATCHER_P2(BlimpImeMsgEquals, tab_id, message_type, "") {
  return arg.target_tab_id() == tab_id && arg.ime().type() == message_type;
}

MATCHER_P5(BlimpImeMsgEquals,
           tab_id,
           rwid,
           message_type,
           text,
           text_input_type,
           "") {
  return arg.target_tab_id() == tab_id &&
         arg.ime().render_widget_id() == rwid &&
         arg.ime().type() == message_type &&
         arg.ime().ime_text().compare(text) == 0 &&
         arg.ime().text_input_type() == text_input_type;
}

void SendInputMessage(BlimpMessageProcessor* processor,
                      int tab_id,
                      int rw_id) {
  blink::WebGestureEvent input_event;
  input_event.type = blink::WebGestureEvent::Type::GestureTap;

  InputMessageGenerator generator;
  std::unique_ptr<BlimpMessage> message =
      generator.GenerateMessage(input_event);
  message->set_target_tab_id(tab_id);
  message->mutable_input()->set_render_widget_id(rw_id);

  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

void SendCompositorMessage(BlimpMessageProcessor* processor,
                           int tab_id,
                           int rw_id,
                           const std::vector<uint8_t>& payload) {
  CompositorMessage* details;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details, tab_id);
  details->set_render_widget_id(rw_id);
  details->set_payload(payload.data(), base::checked_cast<int>(payload.size()));
  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

}  // namespace

class EngineRenderWidgetFeatureTest : public testing::Test {
 public:
  EngineRenderWidgetFeatureTest() : feature_(&settings_) {}

  void SetUp() override {
    render_widget_message_sender_ = new MockBlimpMessageProcessor;
    feature_.set_render_widget_message_sender(
        base::WrapUnique(render_widget_message_sender_));
    compositor_message_sender_ = new MockBlimpMessageProcessor;
    feature_.set_compositor_message_sender(
        base::WrapUnique(compositor_message_sender_));
    ime_message_sender_ = new MockBlimpMessageProcessor;
    feature_.set_ime_message_sender(base::WrapUnique(ime_message_sender_));
    feature_.SetDelegate(1, &delegate1_);
    feature_.SetDelegate(2, &delegate2_);
  }

 protected:
  MockBlimpMessageProcessor* render_widget_message_sender_;
  MockBlimpMessageProcessor* compositor_message_sender_;
  MockBlimpMessageProcessor* ime_message_sender_;
  MockRenderWidgetHost render_widget_host1_;
  MockRenderWidgetHost render_widget_host2_;
  MockHostRenderWidgetMessageDelegate delegate1_;
  MockHostRenderWidgetMessageDelegate delegate2_;
  MockTextInputClient text_input_client_;
  SettingsManager settings_;
  EngineRenderWidgetFeature feature_;
};

TEST_F(EngineRenderWidgetFeatureTest, DelegateCallsOK) {
  std::vector<uint8_t> payload = { 'd', 'a', 'v', 'i', 'd' };

  EXPECT_CALL(*render_widget_message_sender_, MockableProcessMessage(
      BlimpRWMsgEquals(1, 1, RenderWidgetMessage::CREATED), _))
  .Times(1);
  EXPECT_CALL(*render_widget_message_sender_, MockableProcessMessage(
      BlimpRWMsgEquals(1, 1, RenderWidgetMessage::INITIALIZE), _))
  .Times(1);
  EXPECT_CALL(*render_widget_message_sender_, MockableProcessMessage(
      BlimpRWMsgEquals(2, 2, RenderWidgetMessage::CREATED), _))
  .Times(1);

  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(&render_widget_host1_,
                                                  CompMsgEquals(payload)))
  .Times(1);
  EXPECT_CALL(delegate1_, MockableOnWebGestureEvent(&render_widget_host1_))
  .Times(1);
  EXPECT_CALL(delegate2_,
              MockableOnCompositorMessageReceived(&render_widget_host2_,
                                                  CompMsgEquals(payload)))
  .Times(1);

  feature_.OnRenderWidgetCreated(1, &render_widget_host1_);
  feature_.OnRenderWidgetInitialized(1, &render_widget_host1_);
  feature_.OnRenderWidgetCreated(2, &render_widget_host2_);
  SendCompositorMessage(&feature_, 1, 1, payload);
  SendInputMessage(&feature_, 1, 1);
  SendCompositorMessage(&feature_, 2, 2, payload);
}

TEST_F(EngineRenderWidgetFeatureTest, ImeRequestSentCorrectly) {
  EXPECT_CALL(
      *ime_message_sender_,
      MockableProcessMessage(BlimpImeMsgEquals(2, 1, ImeMessage::SHOW_IME,
                                               std::string("green apple"), 1),
                             _));

  EXPECT_CALL(
      *ime_message_sender_,
      MockableProcessMessage(BlimpImeMsgEquals(2, ImeMessage::HIDE_IME), _));

  feature_.OnRenderWidgetCreated(2, &render_widget_host1_);
  feature_.SendShowImeRequest(2, &render_widget_host1_, &text_input_client_);
  feature_.SendHideImeRequest(2, &render_widget_host1_);
}

TEST_F(EngineRenderWidgetFeatureTest, DropsStaleMessages) {
  InSequence sequence;
  std::vector<uint8_t> payload = { 'f', 'u', 'n' };
  std::vector<uint8_t> new_payload = {'n', 'o', ' ', 'f', 'u', 'n'};

  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(
                  BlimpRWMsgEquals(1, 1, RenderWidgetMessage::CREATED), _));
  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(&render_widget_host1_,
                                                  CompMsgEquals(payload)));
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(
                  BlimpRWMsgEquals(1, 1, RenderWidgetMessage::DELETED), _));
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(
                  BlimpRWMsgEquals(1, 2, RenderWidgetMessage::CREATED), _));

  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(&render_widget_host2_,
                                                  CompMsgEquals(new_payload)));
  EXPECT_CALL(delegate1_, MockableOnWebGestureEvent(&render_widget_host2_));

  feature_.OnRenderWidgetCreated(1, &render_widget_host1_);
  SendCompositorMessage(&feature_, 1, 1, payload);
  feature_.OnRenderWidgetDeleted(1, &render_widget_host1_);
  feature_.OnRenderWidgetCreated(1, &render_widget_host2_);

  // These next three calls should be dropped.
  SendCompositorMessage(&feature_, 1, 1, payload);
  SendCompositorMessage(&feature_, 1, 1, payload);
  SendInputMessage(&feature_, 1, 1);

  SendCompositorMessage(&feature_, 1, 2, new_payload);
  SendInputMessage(&feature_, 1, 2);
}

TEST_F(EngineRenderWidgetFeatureTest, RepliesHaveCorrectRenderWidgetId) {
  Sequence delegate1_sequence;
  Sequence delegate2_sequence;
  std::vector<uint8_t> payload = { 'a', 'b', 'c', 'd' };

  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(
                  BlimpRWMsgEquals(1, 1, RenderWidgetMessage::CREATED), _))
  .InSequence(delegate1_sequence);
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(
                  BlimpRWMsgEquals(1, 1, RenderWidgetMessage::INITIALIZE), _))
  .InSequence(delegate1_sequence);
  EXPECT_CALL(*compositor_message_sender_,
              MockableProcessMessage(BlimpCompMsgEquals(1, 1, payload), _))
  .InSequence(delegate1_sequence);

  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(
                  BlimpRWMsgEquals(2, 2, RenderWidgetMessage::CREATED), _))
  .InSequence(delegate2_sequence);
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(
                  BlimpRWMsgEquals(2, 2, RenderWidgetMessage::DELETED), _))
  .InSequence(delegate2_sequence);

  feature_.OnRenderWidgetCreated(1, &render_widget_host1_);
  feature_.OnRenderWidgetCreated(2, &render_widget_host2_);
  feature_.OnRenderWidgetInitialized(1, &render_widget_host1_);
  feature_.OnRenderWidgetDeleted(2, &render_widget_host2_);
  feature_.SendCompositorMessage(1, &render_widget_host1_, payload);
}

}  // namespace engine
}  // namespace blimp
