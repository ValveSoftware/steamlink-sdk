// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/control_message_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {
namespace {

bool ValidateControlRequestWithResponse(Message* message) {
  ValidationContext validation_context(
      message->data(), message->data_num_bytes(), message->handles()->size(),
      message, "ControlRequestValidator");
  if (!ValidateMessageIsRequestExpectingResponse(message, &validation_context))
    return false;

  switch (message->header()->name) {
    case interface_control::kRunMessageId:
      return ValidateMessagePayload<
          interface_control::internal::RunMessageParams_Data>(
          message, &validation_context);
  }
  return false;
}

bool ValidateControlRequestWithoutResponse(Message* message) {
  ValidationContext validation_context(
      message->data(), message->data_num_bytes(), message->handles()->size(),
      message, "ControlRequestValidator");
  if (!ValidateMessageIsRequestWithoutResponse(message, &validation_context))
    return false;

  switch (message->header()->name) {
    case interface_control::kRunOrClosePipeMessageId:
      return ValidateMessageIsRequestWithoutResponse(message,
                                                     &validation_context) &&
             ValidateMessagePayload<
                 interface_control::internal::RunOrClosePipeMessageParams_Data>(
                 message, &validation_context);
  }
  return false;
}

}  // namespace

// static
bool ControlMessageHandler::IsControlMessage(const Message* message) {
  return message->header()->name == interface_control::kRunMessageId ||
         message->header()->name == interface_control::kRunOrClosePipeMessageId;
}

ControlMessageHandler::ControlMessageHandler(uint32_t interface_version)
    : interface_version_(interface_version) {
}

ControlMessageHandler::~ControlMessageHandler() {
}

bool ControlMessageHandler::Accept(Message* message) {
  if (!ValidateControlRequestWithoutResponse(message))
    return false;

  if (message->header()->name == interface_control::kRunOrClosePipeMessageId)
    return RunOrClosePipe(message);

  NOTREACHED();
  return false;
}

bool ControlMessageHandler::AcceptWithResponder(
    Message* message,
    MessageReceiverWithStatus* responder) {
  if (!ValidateControlRequestWithResponse(message))
    return false;

  if (message->header()->name == interface_control::kRunMessageId)
    return Run(message, responder);

  NOTREACHED();
  return false;
}

bool ControlMessageHandler::Run(Message* message,
                                MessageReceiverWithStatus* responder) {
  interface_control::internal::RunMessageParams_Data* params =
      reinterpret_cast<interface_control::internal::RunMessageParams_Data*>(
          message->mutable_payload());
  interface_control::RunMessageParamsPtr params_ptr;
  Deserialize<interface_control::RunMessageParamsDataView>(params, &params_ptr,
                                                           &context_);
  auto& input = *params_ptr->input;
  interface_control::RunOutputPtr output = interface_control::RunOutput::New();
  if (input.is_query_version()) {
    output->set_query_version_result(
        interface_control::QueryVersionResult::New());
    output->get_query_version_result()->version = interface_version_;
  } else if (input.is_flush_for_testing()) {
    output.reset();
  } else {
    output.reset();
  }

  auto response_params_ptr = interface_control::RunResponseMessageParams::New();
  response_params_ptr->output = std::move(output);
  size_t size =
      PrepareToSerialize<interface_control::RunResponseMessageParamsDataView>(
          response_params_ptr, &context_);
  ResponseMessageBuilder builder(interface_control::kRunMessageId, size,
                                 message->request_id());

  interface_control::internal::RunResponseMessageParams_Data* response_params =
      nullptr;
  Serialize<interface_control::RunResponseMessageParamsDataView>(
      response_params_ptr, builder.buffer(), &response_params, &context_);
  bool ok = responder->Accept(builder.message());
  ALLOW_UNUSED_LOCAL(ok);
  delete responder;

  return true;
}

bool ControlMessageHandler::RunOrClosePipe(Message* message) {
  interface_control::internal::RunOrClosePipeMessageParams_Data* params =
      reinterpret_cast<
          interface_control::internal::RunOrClosePipeMessageParams_Data*>(
          message->mutable_payload());
  interface_control::RunOrClosePipeMessageParamsPtr params_ptr;
  Deserialize<interface_control::RunOrClosePipeMessageParamsDataView>(
      params, &params_ptr, &context_);
  auto& input = *params_ptr->input;
  if (input.is_require_version())
    return interface_version_ >= input.get_require_version()->version;
  else if (input.is_send_disconnect_reason()) {
    disconnect_custom_reason_ =
        input.get_send_disconnect_reason()->custom_reason;
    disconnect_description_ =
        std::move(input.get_send_disconnect_reason()->description);
    return true;
  }

  return false;
}

}  // namespace internal
}  // namespace mojo
