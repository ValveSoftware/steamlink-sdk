// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/logging.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"

namespace blimp {
namespace {

using LogFields = std::vector<std::pair<std::string, std::string>>;

// The AddField() suite of functions are used to convert KV pairs with
// arbitrarily typed values into string/string KV pairs for logging.

// Specialization for string values, surrounding them with quotes and escaping
// characters as necessary.
void AddField(const std::string& key,
              const std::string& value,
              LogFields* output) {
  std::string escaped_value;
  base::EscapeJSONString(value, true, &escaped_value);
  output->push_back(std::make_pair(key, escaped_value));
}

// Specialization for string literal values.
void AddField(const std::string& key, const char* value, LogFields* output) {
  output->push_back(std::make_pair(key, std::string(value)));
}

// Specialization for boolean values (serialized as "true" or "false").
void AddField(const std::string& key, bool value, LogFields* output) {
  output->push_back(std::make_pair(key, (value ? "true" : "false")));
}

// Specialization for SizeMessage values, serializing them as
// WIDTHxHEIGHT:RATIO. RATIO is rounded to two digits of precision
// (e.g. 2.123 => 2.12).
void AddField(const std::string& key,
              const SizeMessage& value,
              LogFields* output) {
  output->push_back(std::make_pair(
      key, base::StringPrintf("%" PRIu64 "x%" PRIu64 ":%.2lf", value.width(),
                              value.height(), value.device_pixel_ratio())));
}

// Conversion function for all other types.
// Uses std::to_string() to serialize |value|.
template <typename T>
void AddField(const std::string& key, const T& value, LogFields* output) {
  output->push_back(std::make_pair(key, std::to_string(value)));
}

// The following LogExtractor subclasses contain logic for extracting loggable
// fields from BlimpMessages.

// Logs fields from COMPOSITOR messages.
void ExtractCompositorMessageFields(const BlimpMessage& message,
                                    LogFields* output) {
  AddField("render_widget_id", message.compositor().render_widget_id(), output);
}

// Logs fields from IME messages.
void ExtractImeMessageFields(const BlimpMessage& message, LogFields* output) {
  AddField("render_widget_id", message.ime().render_widget_id(), output);
  switch (message.ime().type()) {
    case ImeMessage::SHOW_IME:
      AddField("subtype", "SHOW_IME", output);
      AddField("text_input_type", message.ime().text_input_type(), output);
      break;
    case ImeMessage::HIDE_IME:
      AddField("subtype", "HIDE_IME", output);
      break;
    case ImeMessage::SET_TEXT:
      AddField("subtype", "SET_TEXT", output);
      AddField("ime_text(length)", message.ime().ime_text().size(), output);
      break;
    case ImeMessage::UNKNOWN:
      AddField("subtype", "UNKNOWN", output);
      break;
  }
}

// Logs fields from INPUT messages.
void ExtractInputMessageFields(const BlimpMessage& message, LogFields* output) {
  AddField("render_widget_id", message.input().render_widget_id(), output);
  AddField("timestamp_seconds", message.input().timestamp_seconds(), output);
  switch (message.input().type()) {
    case InputMessage::Type_GestureScrollBegin:
      AddField("subtype", "GestureScrollBegin", output);
      break;
    case InputMessage::Type_GestureScrollEnd:
      AddField("subtype", "GestureScrollEnd", output);
      break;
    case InputMessage::Type_GestureScrollUpdate:
      AddField("subtype", "GestureScrollUpdate", output);
      break;
    case InputMessage::Type_GestureFlingStart:
      AddField("subtype", "GestureFlingStart", output);
      break;
    case InputMessage::Type_GestureFlingCancel:
      AddField("subtype", "GestureFlingCancel", output);
      AddField("prevent_boosting",
               message.input().gesture_fling_cancel().prevent_boosting(),
               output);
      break;
    case InputMessage::Type_GestureTap:
      AddField("subtype", "GestureTap", output);
      break;
    case InputMessage::Type_GesturePinchBegin:
      AddField("subtype", "GesturePinchBegin", output);
      break;
    case InputMessage::Type_GesturePinchEnd:
      AddField("subtype", "GesturePinchEnd", output);
      break;
    case InputMessage::Type_GesturePinchUpdate:
      AddField("subtype", "GesturePinchUpdate", output);
      break;
    case InputMessage::Type_GestureTapDown:
      AddField("subtype", "GestureTapDown", output);
      break;
    case InputMessage::Type_GestureTapCancel:
      AddField("subtype", "GestureTapCancel", output);
      break;
    case InputMessage::Type_GestureTapUnconfirmed:
      AddField("subtype", "GestureTapUnconfirmed", output);
      break;
    case InputMessage::Type_GestureShowPress:
      AddField("subtype", "GestureShowPress", output);
      break;
    case InputMessage::UNKNOWN:
      break;
  }
}

// Logs fields from NAVIGATION messages.
void ExtractNavigationMessageFields(const BlimpMessage& message,
                                    LogFields* output) {
  switch (message.navigation().type()) {
    case NavigationMessage::NAVIGATION_STATE_CHANGED:
      AddField("subtype", "NAVIGATION_STATE_CHANGED", output);
      if (message.navigation().navigation_state_changed().has_url()) {
        AddField("url", message.navigation().navigation_state_changed().url(),
                 output);
      }
      if (message.navigation().navigation_state_changed().has_favicon()) {
        AddField(
            "favicon_size",
            message.navigation().navigation_state_changed().favicon().size(),
            output);
      }
      if (message.navigation().navigation_state_changed().has_title()) {
        AddField("title",
                 message.navigation().navigation_state_changed().title(),
                 output);
      }
      if (message.navigation().navigation_state_changed().has_loading()) {
        AddField("loading",
                 message.navigation().navigation_state_changed().loading(),
                 output);
      }
      break;
    case NavigationMessage::LOAD_URL:
      AddField("subtype", "LOAD_URL", output);
      AddField("url", message.navigation().load_url().url(), output);
      break;
    case NavigationMessage::GO_BACK:
      AddField("subtype", "GO_BACK", output);
      break;
    case NavigationMessage::GO_FORWARD:
      AddField("subtype", "GO_FORWARD", output);
      break;
    case NavigationMessage::RELOAD:
      AddField("subtype", "RELOAD", output);
      break;
    case NavigationMessage::UNKNOWN:
      break;
  }
}

// Logs fields from PROTOCOL_CONTROL messages.
void ExtractProtocolControlMessageFields(const BlimpMessage& message,
                                         LogFields* output) {
  switch (message.protocol_control().connection_message_case()) {
    case ProtocolControlMessage::kStartConnection:
      AddField("subtype", "START_CONNECTION", output);
      AddField("client_token",
               message.protocol_control().start_connection().client_token(),
               output);
      AddField("protocol_version",
               message.protocol_control().start_connection().protocol_version(),
               output);
      break;
    case ProtocolControlMessage::kCheckpointAck:
      AddField("subtype", "CHECKPOINT_ACK", output);
      AddField("checkpoint_id",
               message.protocol_control().checkpoint_ack().checkpoint_id(),
               output);
      break;
    case ProtocolControlMessage::kEndConnection:
      AddField("subtype", "END_CONNECTION", output);
      switch (message.protocol_control().end_connection().reason()) {
        case EndConnectionMessage::AUTHENTICATION_FAILED:
          AddField("reason", "AUTHENTICATION_FAILED", output);
          break;
        case EndConnectionMessage::PROTOCOL_MISMATCH:
          AddField("reason", "PROTOCOL_MISMATCH", output);
          break;
        case EndConnectionMessage::UNKNOWN:
          break;
      }
      break;
    case ProtocolControlMessage::CONNECTION_MESSAGE_NOT_SET:
      break;
  }
}

// Logs fields from RENDER_WIDGET messages.
void ExtractRenderWidgetMessageFields(const BlimpMessage& message,
                                      LogFields* output) {
  switch (message.render_widget().type()) {
    case RenderWidgetMessage::INITIALIZE:
      AddField("subtype", "INITIALIZE", output);
      break;
    case RenderWidgetMessage::CREATED:
      AddField("subtype", "CREATED", output);
      break;
    case RenderWidgetMessage::DELETED:
      AddField("subtype", "DELETED", output);
      break;
    case RenderWidgetMessage::UNKNOWN:
      break;
  }
  AddField("render_widget_id", message.render_widget().render_widget_id(),
           output);
}

// Logs fields from SETTINGS messages.
void ExtractSettingsMessageFields(const BlimpMessage& message,
                                  LogFields* output) {
  if (message.settings().has_engine_settings()) {
    const EngineSettingsMessage& engine_settings =
        message.settings().engine_settings();
    AddField("subtype", "ENGINE_SETTINGS", output);
    AddField("record_whole_document", engine_settings.record_whole_document(),
             output);
    AddField("client_os_info", engine_settings.client_os_info(), output);
  }
}

// Logs fields from TAB_CONTROL messages.
void ExtractTabControlMessageFields(const BlimpMessage& message,
                                    LogFields* output) {
  switch (message.tab_control().tab_control_case()) {
    case TabControlMessage::kCreateTab:
      AddField("subtype", "CREATE_TAB", output);
      break;
    case TabControlMessage::kCloseTab:
      AddField("subtype", "CLOSE_TAB", output);
      break;
    case TabControlMessage::kSize:
      AddField("subtype", "SIZE", output);
      AddField("size", message.tab_control().size(), output);
      break;
    case TabControlMessage::TAB_CONTROL_NOT_SET:
      break;
  }
}

// Logs fields from BLOB_CHANNEL messages.
void ExtractBlobChannelMessageFields(const BlimpMessage& message,
                                     LogFields* output) {
  switch (message.blob_channel().type_case()) {
    case BlobChannelMessage::TypeCase::kTransferBlob:
      AddField("subtype", "TRANSFER_BLOB", output);
      AddField("id",
               base::HexEncode(
                   message.blob_channel().transfer_blob().blob_id().data(),
                   message.blob_channel().transfer_blob().blob_id().size()),
               output);
      AddField("payload_size",
               message.blob_channel().transfer_blob().payload().size(), output);
      break;
    case BlobChannelMessage::TypeCase::TYPE_NOT_SET:  // unknown
      break;
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const BlimpMessage& message) {
  LogFields fields;

  switch (message.feature_case()) {
    case BlimpMessage::kCompositor:
      AddField("type", "COMPOSITOR", &fields);
      ExtractCompositorMessageFields(message, &fields);
      break;
    case BlimpMessage::kInput:
      AddField("type", "INPUT", &fields);
      ExtractInputMessageFields(message, &fields);
      break;
    case BlimpMessage::kNavigation:
      AddField("type", "NAVIGATION", &fields);
      ExtractNavigationMessageFields(message, &fields);
      break;
    case BlimpMessage::kProtocolControl:
      AddField("type", "PROTOCOL_CONTROL", &fields);
      ExtractProtocolControlMessageFields(message, &fields);
      break;
    case BlimpMessage::kRenderWidget:
      AddField("type", "RENDER_WIDGET", &fields);
      ExtractRenderWidgetMessageFields(message, &fields);
      break;
    case BlimpMessage::kSettings:
      AddField("type", "SETTINGS", &fields);
      ExtractSettingsMessageFields(message, &fields);
      break;
    case BlimpMessage::kTabControl:
      AddField("type", "TAB_CONTROL", &fields);
      ExtractTabControlMessageFields(message, &fields);
      break;
    case BlimpMessage::kBlobChannel:
      AddField("type", "BLOB_CHANNEL", &fields);
      ExtractBlobChannelMessageFields(message, &fields);
      break;
    case BlimpMessage::kIme:
      AddField("type", "IME", &fields);
      ExtractImeMessageFields(message, &fields);
      break;
    case BlimpMessage::FEATURE_NOT_SET:
      AddField("type", "<UNKNOWN>", &fields);
      break;
  }

  // Append "target_tab_id" (if present) and "byte_size" to the field set.
  if (message.has_target_tab_id()) {
    AddField("target_tab_id", message.target_tab_id(), &fields);
  }
  AddField("byte_size", message.ByteSize(), &fields);

  // Format message using the syntax:
  // <BlimpMessage field1=value1 field2="value 2">
  out << "<BlimpMessage ";
  for (size_t i = 0; i < fields.size(); ++i) {
    out << fields[i].first << "=" << fields[i].second
        << (i != fields.size() - 1 ? " " : "");
  }
  out << ">";

  return out;
}

}  // namespace blimp
