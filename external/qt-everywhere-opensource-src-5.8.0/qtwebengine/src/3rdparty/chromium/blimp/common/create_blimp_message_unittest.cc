// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/navigation.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/common/proto/tab_control.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

const int kTabId = 1234;

TEST(CreateBlimpMessageTest, CompositorMessage) {
  CompositorMessage* details = nullptr;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details, kTabId);
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(details, message->mutable_compositor());
  EXPECT_EQ(kTabId, message->target_tab_id());
}

TEST(CreateBlimpMessageTest, TabControlMessage) {
  TabControlMessage* details = nullptr;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(details, message->mutable_tab_control());
}

TEST(CreateBlimpMessageTest, InputMessage) {
  InputMessage* details = nullptr;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(details, message->mutable_input());
}

TEST(CreateBlimpMessageTest, NavigationMessage) {
  NavigationMessage* details = nullptr;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details, kTabId);
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(details, message->mutable_navigation());
  EXPECT_EQ(kTabId, message->target_tab_id());
}

TEST(CreateBlimpMessageTest, RenderWidgetMessage) {
  RenderWidgetMessage* details = nullptr;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details, kTabId);
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(details, message->mutable_render_widget());
  EXPECT_EQ(kTabId, message->target_tab_id());
}

TEST(CreateBlimpMessageTest, SizeMessage) {
  SizeMessage* details = nullptr;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(TabControlMessage::kSize,
            message->mutable_tab_control()->tab_control_case());
  EXPECT_EQ(details, message->mutable_tab_control()->mutable_size());
}

TEST(CreateBlimpMessageTest, EngineSettingsMessage) {
  EngineSettingsMessage* details;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(details, message->mutable_settings()->mutable_engine_settings());
}

TEST(CreateBlimpMessageTest, StartConnectionMessage) {
  const char* client_token = "token";
  const int protocol_version = 1;
  std::unique_ptr<BlimpMessage> message =
      CreateStartConnectionMessage(client_token, protocol_version);
  EXPECT_EQ(BlimpMessage::kProtocolControl, message->feature_case());
  EXPECT_EQ(ProtocolControlMessage::kStartConnection,
            message->protocol_control().connection_message_case());
  EXPECT_EQ(client_token,
            message->protocol_control().start_connection().client_token());
  EXPECT_EQ(protocol_version,
            message->protocol_control().start_connection().protocol_version());
}

TEST(CreateBlimpMessageTest, EndConnectionMessage) {
  std::unique_ptr<BlimpMessage> message =
      CreateEndConnectionMessage(EndConnectionMessage::PROTOCOL_MISMATCH);
  EXPECT_EQ(BlimpMessage::kProtocolControl, message->feature_case());
  EXPECT_EQ(ProtocolControlMessage::kEndConnection,
            message->protocol_control().connection_message_case());
  EXPECT_EQ(EndConnectionMessage::PROTOCOL_MISMATCH,
            message->protocol_control().end_connection().reason());
}

TEST(CreateBlimpMessageTest, CheckpointAckMessage) {
  const int64_t kTestCheckpointId = 1;

  std::unique_ptr<BlimpMessage> message =
      CreateCheckpointAckMessage(kTestCheckpointId);
  EXPECT_EQ(BlimpMessage::kProtocolControl, message->feature_case());
  EXPECT_EQ(ProtocolControlMessage::kCheckpointAck,
            message->protocol_control().connection_message_case());
  EXPECT_EQ(kTestCheckpointId,
            message->protocol_control().checkpoint_ack().checkpoint_id());
}

TEST(CreateBlimpMessageTest, BlobChannelMessage) {
  BlobChannelMessage* details;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  ASSERT_TRUE(message);
  EXPECT_EQ(details, &message->blob_channel());
  EXPECT_EQ(BlimpMessage::kBlobChannel, message->feature_case());
}

}  // namespace
}  // namespace blimp
