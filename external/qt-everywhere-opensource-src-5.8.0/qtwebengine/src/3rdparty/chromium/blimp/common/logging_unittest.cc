// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/at_exit.h"
#include "base/strings/stringprintf.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/blob_channel.pb.h"
#include "blimp/net/test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace blimp {
namespace {

const int kTargetTab = 123;

// Verifies that the logged form of |msg| matches |expected|, modulo prefix
// and suffix.
void VerifyLogOutput(const std::string& expected_fragment,
                     const BlimpMessage& msg) {
  std::string expected = "<BlimpMessage " + expected_fragment + " byte_size=" +
                         std::to_string(msg.ByteSize()) + ">";
  std::stringstream outstream;
  outstream << msg;
  EXPECT_EQ(expected, outstream.str());
}

class LoggingTest : public testing::Test {
 public:
  LoggingTest() {}
  ~LoggingTest() override {}

 private:
  // Deletes the singleton on test termination.
  base::ShadowingAtExitManager at_exit_;
};

TEST_F(LoggingTest, Compositor) {
  BlimpMessage base_msg;
  base_msg.mutable_compositor();
  base_msg.set_target_tab_id(kTargetTab);
  VerifyLogOutput("type=COMPOSITOR render_widget_id=0 target_tab_id=123",
                  base_msg);
}

TEST_F(LoggingTest, Input) {
  const char* fragment_format =
      "type=INPUT render_widget_id=1 timestamp_seconds=2.000000 subtype=%s"
      " target_tab_id=123";

  BlimpMessage base_msg;
  base_msg.set_target_tab_id(kTargetTab);
  base_msg.mutable_input()->set_type(InputMessage::Type_GestureScrollBegin);
  base_msg.mutable_input()->set_render_widget_id(1);
  base_msg.mutable_input()->set_timestamp_seconds(2);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GestureScrollBegin"),
                  base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GestureScrollEnd);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GestureScrollEnd"),
                  base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GestureScrollUpdate);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GestureScrollUpdate"),
                  base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GestureFlingStart);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GestureFlingStart"),
                  base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GestureTap);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GestureTap"), base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GesturePinchBegin);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GesturePinchBegin"),
                  base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GesturePinchEnd);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GesturePinchEnd"),
                  base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GesturePinchUpdate);
  VerifyLogOutput(base::StringPrintf(fragment_format, "GesturePinchUpdate"),
                  base_msg);

  base_msg.mutable_input()->set_type(InputMessage::Type_GestureFlingCancel);
  base_msg.mutable_input()
      ->mutable_gesture_fling_cancel()
      ->set_prevent_boosting(true);
  VerifyLogOutput(
      "type=INPUT render_widget_id=1 timestamp_seconds=2.000000 "
      "subtype=GestureFlingCancel prevent_boosting=true target_tab_id=123",
      base_msg);
}

TEST_F(LoggingTest, Navigation) {
  BlimpMessage base_msg;
  base_msg.set_target_tab_id(kTargetTab);

  BlimpMessage navigation_state_msg = base_msg;
  navigation_state_msg.mutable_navigation()->set_type(
      NavigationMessage::NAVIGATION_STATE_CHANGED);
  navigation_state_msg.mutable_navigation()
      ->mutable_navigation_state_changed()
      ->set_url("http://foo.com");
  navigation_state_msg.mutable_navigation()
      ->mutable_navigation_state_changed()
      ->set_favicon("bytes!");
  navigation_state_msg.mutable_navigation()
      ->mutable_navigation_state_changed()
      ->set_title("FooCo");
  navigation_state_msg.mutable_navigation()
      ->mutable_navigation_state_changed()
      ->set_loading(true);
  VerifyLogOutput(
      "type=NAVIGATION subtype=NAVIGATION_STATE_CHANGED url=\"http://foo.com\" "
      "favicon_size=6 title=\"FooCo\" loading=true target_tab_id=123",
      navigation_state_msg);

  BlimpMessage load_url_msg = base_msg;
  load_url_msg.mutable_navigation()->set_type(NavigationMessage::LOAD_URL);
  load_url_msg.mutable_navigation()->mutable_load_url()->set_url(
      "http://foo.com");
  VerifyLogOutput(
      "type=NAVIGATION subtype=LOAD_URL url=\"http://foo.com\" "
      "target_tab_id=123",
      load_url_msg);

  BlimpMessage go_back_msg = base_msg;
  go_back_msg.mutable_navigation()->set_type(NavigationMessage::GO_BACK);
  VerifyLogOutput("type=NAVIGATION subtype=GO_BACK target_tab_id=123",
                  go_back_msg);

  BlimpMessage go_forward_msg = base_msg;
  go_forward_msg.mutable_navigation()->set_type(NavigationMessage::GO_FORWARD);
  VerifyLogOutput("type=NAVIGATION subtype=GO_FORWARD target_tab_id=123",
                  go_forward_msg);

  BlimpMessage reload_msg = base_msg;
  reload_msg.mutable_navigation()->set_type(NavigationMessage::RELOAD);
  VerifyLogOutput("type=NAVIGATION subtype=RELOAD target_tab_id=123",
                  reload_msg);
}

TEST_F(LoggingTest, TabControl) {
  BlimpMessage base_msg;
  base_msg.set_target_tab_id(kTargetTab);

  BlimpMessage create_tab_msg = base_msg;
  create_tab_msg.mutable_tab_control()->mutable_create_tab();
  VerifyLogOutput("type=TAB_CONTROL subtype=CREATE_TAB target_tab_id=123",
                  create_tab_msg);

  BlimpMessage close_tab_msg = base_msg;
  close_tab_msg.mutable_tab_control()->mutable_close_tab();
  VerifyLogOutput("type=TAB_CONTROL subtype=CLOSE_TAB target_tab_id=123",
                  close_tab_msg);

  BlimpMessage size_msg = base_msg;
  size_msg.mutable_tab_control()->mutable_size();
  size_msg.mutable_tab_control()->mutable_size()->set_width(640);
  size_msg.mutable_tab_control()->mutable_size()->set_height(480);
  size_msg.mutable_tab_control()->mutable_size()->set_device_pixel_ratio(2);
  VerifyLogOutput(
      "type=TAB_CONTROL subtype=SIZE size=640x480:2.00 target_tab_id=123",
      size_msg);
}

TEST_F(LoggingTest, ProtocolControl) {
  BlimpMessage base_msg;

  BlimpMessage start_connection_msg = base_msg;
  start_connection_msg.mutable_protocol_control()->mutable_start_connection();
  start_connection_msg.mutable_protocol_control()
      ->mutable_start_connection()
      ->set_client_token("token");
  start_connection_msg.mutable_protocol_control()
      ->mutable_start_connection()
      ->set_protocol_version(2);
  VerifyLogOutput(
      "type=PROTOCOL_CONTROL subtype=START_CONNECTION "
      "client_token=\"token\" protocol_version=2",
      start_connection_msg);

  start_connection_msg.mutable_protocol_control()->mutable_checkpoint_ack();
  start_connection_msg.mutable_protocol_control()
      ->mutable_checkpoint_ack()
      ->set_checkpoint_id(123);
  VerifyLogOutput(
      "type=PROTOCOL_CONTROL subtype=CHECKPOINT_ACK "
      "checkpoint_id=123",
      start_connection_msg);
}

TEST_F(LoggingTest, RenderWidget) {
  BlimpMessage base_msg;
  base_msg.mutable_render_widget()->set_render_widget_id(123);

  BlimpMessage initialize_msg = base_msg;
  initialize_msg.mutable_render_widget()->set_type(
      RenderWidgetMessage::INITIALIZE);
  VerifyLogOutput("type=RENDER_WIDGET subtype=INITIALIZE render_widget_id=123",
                  initialize_msg);

  BlimpMessage created_msg = base_msg;
  created_msg.mutable_render_widget()->set_type(
      RenderWidgetMessage::CREATED);
  VerifyLogOutput("type=RENDER_WIDGET subtype=CREATED render_widget_id=123",
                  created_msg);

  BlimpMessage deleted_msg = base_msg;
  deleted_msg.mutable_render_widget()->set_type(RenderWidgetMessage::DELETED);
  VerifyLogOutput("type=RENDER_WIDGET subtype=DELETED render_widget_id=123",
                  deleted_msg);
}

TEST_F(LoggingTest, BlobChannel) {
  BlobChannelMessage* blob_message = nullptr;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&blob_message);
  blob_message->mutable_transfer_blob()->set_blob_id("AAA");
  blob_message->mutable_transfer_blob()->set_payload("123");

  VerifyLogOutput(
      "type=BLOB_CHANNEL subtype=TRANSFER_BLOB id=\"414141\" payload_size=3",
      *blimp_message);
}

TEST_F(LoggingTest, Settings) {
  BlimpMessage message;
  message.mutable_settings()
      ->mutable_engine_settings()
      ->set_record_whole_document(true);
  message.mutable_settings()->mutable_engine_settings()->set_client_os_info(
      "wibble");
  VerifyLogOutput(
      "type=SETTINGS subtype=ENGINE_SETTINGS record_whole_document=true "
      "client_os_info=\"wibble\"",
      message);
}

TEST_F(LoggingTest, Ime) {
  BlimpMessage message;
  message.mutable_ime()->set_render_widget_id(1);

  // Test SHOW_IME.
  message.mutable_ime()->set_type(ImeMessage::SHOW_IME);
  message.mutable_ime()->set_text_input_type(ImeMessage::NONE);
  VerifyLogOutput(
      "type=IME render_widget_id=1 subtype=SHOW_IME text_input_type=0",
      message);

  // Test HIDE_IME.
  message.mutable_ime()->set_type(ImeMessage::HIDE_IME);
  VerifyLogOutput(
      "type=IME render_widget_id=1 subtype=HIDE_IME",
      message);

  // Test SET_TEXT.
  message.mutable_ime()->set_type(ImeMessage::SET_TEXT);
  message.mutable_ime()->set_ime_text("1234");
  VerifyLogOutput(
      "type=IME render_widget_id=1 subtype=SET_TEXT ime_text(length)=4",
      message);
}

}  // namespace
}  // namespace blimp
