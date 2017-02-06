// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/create_blimp_message.h"

#include <memory>

#include "base/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/blob_channel.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/common/proto/settings.pb.h"
#include "blimp/common/proto/tab_control.pb.h"

namespace blimp {

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    CompositorMessage** compositor_message,
    int target_tab_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_target_tab_id(target_tab_id);
  *compositor_message = output->mutable_compositor();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    TabControlMessage** control_message) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  *control_message = output->mutable_tab_control();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(InputMessage** input_message) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  *input_message = output->mutable_input();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    NavigationMessage** navigation_message,
    int target_tab_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_target_tab_id(target_tab_id);
  *navigation_message = output->mutable_navigation();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(ImeMessage** ime_message,
                                                 int target_tab_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_target_tab_id(target_tab_id);
  *ime_message = output->mutable_ime();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    RenderWidgetMessage** render_widget_message,
    int target_tab_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_target_tab_id(target_tab_id);
  *render_widget_message = output->mutable_render_widget();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(SizeMessage** size_message) {
  TabControlMessage* control_message;
  std::unique_ptr<BlimpMessage> output = CreateBlimpMessage(&control_message);
  control_message->mutable_size();
  *size_message = control_message->mutable_size();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    EngineSettingsMessage** engine_settings) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  *engine_settings = output->mutable_settings()->mutable_engine_settings();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    BlobChannelMessage** blob_channel_message) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  *blob_channel_message = output->mutable_blob_channel();
  return output;
}

std::unique_ptr<BlimpMessage> CreateStartConnectionMessage(
    const std::string& client_token,
    int protocol_version) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);

  ProtocolControlMessage* control_message = output->mutable_protocol_control();
  control_message->mutable_start_connection();

  StartConnectionMessage* start_connection_message =
      control_message->mutable_start_connection();
  start_connection_message->set_client_token(client_token);
  start_connection_message->set_protocol_version(protocol_version);

  return output;
}

std::unique_ptr<BlimpMessage> CreateCheckpointAckMessage(
    int64_t checkpoint_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);

  ProtocolControlMessage* control_message = output->mutable_protocol_control();
  CheckpointAckMessage* checkpoint_ack_message =
      control_message->mutable_checkpoint_ack();
  checkpoint_ack_message->set_checkpoint_id(checkpoint_id);

  return output;
}

std::unique_ptr<BlimpMessage> CreateEndConnectionMessage(
    EndConnectionMessage::Reason reason) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);

  ProtocolControlMessage* control_message = output->mutable_protocol_control();
  EndConnectionMessage* end_connection_message =
      control_message->mutable_end_connection();
  end_connection_message->set_reason(reason);

  return output;
}

}  // namespace blimp
