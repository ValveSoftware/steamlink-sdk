// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {

namespace {

using RunCallback = base::Callback<void(QueryVersionResultPtr)>;

class RunResponseForwardToCallback : public MessageReceiver {
 public:
  RunResponseForwardToCallback(const RunCallback& callback)
      : callback_(callback) {}
  bool Accept(Message* message) override;

 private:
  RunCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(RunResponseForwardToCallback);
};

bool RunResponseForwardToCallback::Accept(Message* message) {
  RunResponseMessageParams_Data* params =
      reinterpret_cast<RunResponseMessageParams_Data*>(
          message->mutable_payload());
  params->DecodePointers();

  RunResponseMessageParamsPtr params_ptr;
  SerializationContext context;
  Deserialize<RunResponseMessageParamsPtr>(params, &params_ptr, &context);

  callback_.Run(std::move(params_ptr->query_version_result));
  return true;
}

void SendRunMessage(MessageReceiverWithResponder* receiver,
                    QueryVersionPtr query_version,
                    const RunCallback& callback,
                    SerializationContext* context) {
  RunMessageParamsPtr params_ptr(RunMessageParams::New());
  params_ptr->reserved0 = 16u;
  params_ptr->reserved1 = 0u;
  params_ptr->query_version = std::move(query_version);

  size_t size = PrepareToSerialize<RunMessageParamsPtr>(params_ptr, context);
  RequestMessageBuilder builder(kRunMessageId, size);

  RunMessageParams_Data* params = nullptr;
  Serialize<RunMessageParamsPtr>(params_ptr, builder.buffer(), &params,
                                 context);
  params->EncodePointers();
  MessageReceiver* responder = new RunResponseForwardToCallback(callback);
  if (!receiver->AcceptWithResponder(builder.message(), responder))
    delete responder;
}

void SendRunOrClosePipeMessage(MessageReceiverWithResponder* receiver,
                               RequireVersionPtr require_version,
                               SerializationContext* context) {
  RunOrClosePipeMessageParamsPtr params_ptr(RunOrClosePipeMessageParams::New());
  params_ptr->reserved0 = 16u;
  params_ptr->reserved1 = 0u;
  params_ptr->require_version = std::move(require_version);

  size_t size =
      PrepareToSerialize<RunOrClosePipeMessageParamsPtr>(params_ptr, context);
  MessageBuilder builder(kRunOrClosePipeMessageId, size);

  RunOrClosePipeMessageParams_Data* params = nullptr;
  Serialize<RunOrClosePipeMessageParamsPtr>(params_ptr, builder.buffer(),
                                            &params, context);
  params->EncodePointers();
  bool ok = receiver->Accept(builder.message());
  ALLOW_UNUSED_LOCAL(ok);
}

void RunVersionCallback(const base::Callback<void(uint32_t)>& callback,
                        QueryVersionResultPtr query_version_result) {
  callback.Run(query_version_result->version);
}

}  // namespace

ControlMessageProxy::ControlMessageProxy(MessageReceiverWithResponder* receiver)
    : receiver_(receiver) {
}

void ControlMessageProxy::QueryVersion(
    const base::Callback<void(uint32_t)>& callback) {
  SendRunMessage(receiver_, QueryVersion::New(),
                 base::Bind(&RunVersionCallback, callback), &context_);
}

void ControlMessageProxy::RequireVersion(uint32_t version) {
  RequireVersionPtr require_version(RequireVersion::New());
  require_version->version = version;
  SendRunOrClosePipeMessage(receiver_, std::move(require_version), &context_);
}

}  // namespace internal
}  // namespace mojo
