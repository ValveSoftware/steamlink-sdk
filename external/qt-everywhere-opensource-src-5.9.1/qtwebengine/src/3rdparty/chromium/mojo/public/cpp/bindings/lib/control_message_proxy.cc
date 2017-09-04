// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {

namespace {

bool ValidateControlResponse(Message* message) {
  ValidationContext validation_context(
      message->data(), message->data_num_bytes(), message->handles()->size(),
      message, "ControlResponseValidator");
  if (!ValidateMessageIsResponse(message, &validation_context))
    return false;

  switch (message->header()->name) {
    case interface_control::kRunMessageId:
      return ValidateMessagePayload<
          interface_control::internal::RunResponseMessageParams_Data>(
          message, &validation_context);
  }
  return false;
}

using RunCallback =
    base::Callback<void(interface_control::RunResponseMessageParamsPtr)>;

class RunResponseForwardToCallback : public MessageReceiver {
 public:
  explicit RunResponseForwardToCallback(const RunCallback& callback)
      : callback_(callback) {}
  bool Accept(Message* message) override;

 private:
  RunCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(RunResponseForwardToCallback);
};

bool RunResponseForwardToCallback::Accept(Message* message) {
  if (!ValidateControlResponse(message))
    return false;

  interface_control::internal::RunResponseMessageParams_Data* params =
      reinterpret_cast<
          interface_control::internal::RunResponseMessageParams_Data*>(
          message->mutable_payload());
  interface_control::RunResponseMessageParamsPtr params_ptr;
  SerializationContext context;
  Deserialize<interface_control::RunResponseMessageParamsDataView>(
      params, &params_ptr, &context);

  callback_.Run(std::move(params_ptr));
  return true;
}

void SendRunMessage(MessageReceiverWithResponder* receiver,
                    interface_control::RunInputPtr input_ptr,
                    const RunCallback& callback) {
  SerializationContext context;

  auto params_ptr = interface_control::RunMessageParams::New();
  params_ptr->input = std::move(input_ptr);
  size_t size = PrepareToSerialize<interface_control::RunMessageParamsDataView>(
      params_ptr, &context);
  RequestMessageBuilder builder(interface_control::kRunMessageId, size);

  interface_control::internal::RunMessageParams_Data* params = nullptr;
  Serialize<interface_control::RunMessageParamsDataView>(
      params_ptr, builder.buffer(), &params, &context);
  MessageReceiver* responder = new RunResponseForwardToCallback(callback);
  if (!receiver->AcceptWithResponder(builder.message(), responder))
    delete responder;
}

Message ConstructRunOrClosePipeMessage(
    interface_control::RunOrClosePipeInputPtr input_ptr) {
  SerializationContext context;

  auto params_ptr = interface_control::RunOrClosePipeMessageParams::New();
  params_ptr->input = std::move(input_ptr);

  size_t size = PrepareToSerialize<
      interface_control::RunOrClosePipeMessageParamsDataView>(params_ptr,
                                                              &context);
  MessageBuilder builder(interface_control::kRunOrClosePipeMessageId, size);

  interface_control::internal::RunOrClosePipeMessageParams_Data* params =
      nullptr;
  Serialize<interface_control::RunOrClosePipeMessageParamsDataView>(
      params_ptr, builder.buffer(), &params, &context);
  return std::move(*builder.message());
}

void SendRunOrClosePipeMessage(
    MessageReceiverWithResponder* receiver,
    interface_control::RunOrClosePipeInputPtr input_ptr) {
  Message message(ConstructRunOrClosePipeMessage(std::move(input_ptr)));

  bool ok = receiver->Accept(&message);
  ALLOW_UNUSED_LOCAL(ok);
}

void RunVersionCallback(
    const base::Callback<void(uint32_t)>& callback,
    interface_control::RunResponseMessageParamsPtr run_response) {
  uint32_t version = 0u;
  if (run_response->output && run_response->output->is_query_version_result())
    version = run_response->output->get_query_version_result()->version;
  callback.Run(version);
}

void RunClosure(const base::Closure& callback,
                interface_control::RunResponseMessageParamsPtr run_response) {
  callback.Run();
}

}  // namespace

ControlMessageProxy::ControlMessageProxy(MessageReceiverWithResponder* receiver)
    : receiver_(receiver) {
}

ControlMessageProxy::~ControlMessageProxy() = default;

void ControlMessageProxy::QueryVersion(
    const base::Callback<void(uint32_t)>& callback) {
  auto input_ptr = interface_control::RunInput::New();
  input_ptr->set_query_version(interface_control::QueryVersion::New());
  SendRunMessage(receiver_, std::move(input_ptr),
                 base::Bind(&RunVersionCallback, callback));
}

void ControlMessageProxy::RequireVersion(uint32_t version) {
  auto require_version = interface_control::RequireVersion::New();
  require_version->version = version;
  auto input_ptr = interface_control::RunOrClosePipeInput::New();
  input_ptr->set_require_version(std::move(require_version));
  SendRunOrClosePipeMessage(receiver_, std::move(input_ptr));
}

void ControlMessageProxy::FlushForTesting() {
  if (encountered_error_)
    return;

  auto input_ptr = interface_control::RunInput::New();
  input_ptr->set_flush_for_testing(interface_control::FlushForTesting::New());
  base::RunLoop run_loop;
  run_loop_quit_closure_ = run_loop.QuitClosure();
  SendRunMessage(
      receiver_, std::move(input_ptr),
      base::Bind(&RunClosure,
                 base::Bind(&ControlMessageProxy::RunFlushForTestingClosure,
                            base::Unretained(this))));
  run_loop.Run();
}

void ControlMessageProxy::SendDisconnectReason(uint32_t custom_reason,
                                               const std::string& description) {
  Message message =
      ConstructDisconnectReasonMessage(custom_reason, description);
  bool ok = receiver_->Accept(&message);
  ALLOW_UNUSED_LOCAL(ok);
}

void ControlMessageProxy::RunFlushForTestingClosure() {
  DCHECK(!run_loop_quit_closure_.is_null());
  base::ResetAndReturn(&run_loop_quit_closure_).Run();
}

void ControlMessageProxy::OnConnectionError() {
  encountered_error_ = true;
  if (!run_loop_quit_closure_.is_null())
    RunFlushForTestingClosure();
}

// static
Message ControlMessageProxy::ConstructDisconnectReasonMessage(
    uint32_t custom_reason,
    const std::string& description) {
  auto send_disconnect_reason = interface_control::SendDisconnectReason::New();
  send_disconnect_reason->custom_reason = custom_reason;
  send_disconnect_reason->description = description;
  auto input_ptr = interface_control::RunOrClosePipeInput::New();
  input_ptr->set_send_disconnect_reason(std::move(send_disconnect_reason));
  return ConstructRunOrClosePipeMessage(std::move(input_ptr));
}

}  // namespace internal
}  // namespace mojo
