// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_pipe_reader.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/public/cpp/bindings/message.h"

namespace IPC {
namespace internal {

namespace {

// Used by Send() to capture a serialized Channel::Receive message.
class MessageSerializer : public mojo::MessageReceiverWithResponder {
 public:
  MessageSerializer() {}
  ~MessageSerializer() override {}

  mojo::Message* message() { return &message_; }

 private:
  // mojo::MessageReceiverWithResponder
  bool Accept(mojo::Message* message) override {
    message->MoveTo(&message_);
    return true;
  }

  bool AcceptWithResponder(mojo::Message* message,
                           mojo::MessageReceiver* responder) override {
    NOTREACHED();
    return false;
  }

  mojo::Message message_;

  DISALLOW_COPY_AND_ASSIGN(MessageSerializer);
};

}  // namespace

MessagePipeReader::MessagePipeReader(
    mojo::MessagePipeHandle pipe,
    mojom::ChannelAssociatedPtr sender,
    mojo::AssociatedInterfaceRequest<mojom::Channel> receiver,
    base::ProcessId peer_pid,
    MessagePipeReader::Delegate* delegate)
    : delegate_(delegate),
      peer_pid_(peer_pid),
      sender_(std::move(sender)),
      binding_(this, std::move(receiver)),
      sender_interface_id_(sender_.interface_id()),
      sender_pipe_(pipe) {
  sender_.set_connection_error_handler(
      base::Bind(&MessagePipeReader::OnPipeError, base::Unretained(this),
                 MOJO_RESULT_FAILED_PRECONDITION));
  binding_.set_connection_error_handler(
      base::Bind(&MessagePipeReader::OnPipeError, base::Unretained(this),
                 MOJO_RESULT_FAILED_PRECONDITION));
}

MessagePipeReader::~MessagePipeReader() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // The pipe should be closed before deletion.
}

void MessagePipeReader::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_.reset();
  if (binding_.is_bound())
    binding_.Close();
}

bool MessagePipeReader::Send(std::unique_ptr<Message> message) {
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "MessagePipeReader::Send",
                         message->flags(),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  mojo::Array<mojom::SerializedHandlePtr> handles(nullptr);
  MojoResult result = MOJO_RESULT_OK;
  result = ChannelMojo::ReadFromMessageAttachmentSet(message.get(), &handles);
  if (result != MOJO_RESULT_OK)
    return false;

  mojo::Array<uint8_t> data(message->size());
  std::copy(reinterpret_cast<const uint8_t*>(message->data()),
            reinterpret_cast<const uint8_t*>(message->data()) + message->size(),
            &data[0]);

  MessageSerializer serializer;
  mojom::ChannelProxy proxy(&serializer);
  proxy.Receive(std::move(data), std::move(handles));
  mojo::Message* mojo_message = serializer.message();

  size_t num_handles = mojo_message->handles()->size();
  DCHECK_LE(num_handles, std::numeric_limits<uint32_t>::max());

  mojo_message->set_interface_id(sender_interface_id_);
  result = mojo::WriteMessageNew(sender_pipe_, mojo_message->TakeMojoMessage(),
                                 MOJO_WRITE_MESSAGE_FLAG_NONE);

  DVLOG(4) << "Send " << message->type() << ": " << message->size();
  return result == MOJO_RESULT_OK;
}

void MessagePipeReader::Receive(
    mojo::Array<uint8_t> data,
    mojo::Array<mojom::SerializedHandlePtr> handles) {
  Message message(
      data.size() == 0 ? "" : reinterpret_cast<const char*>(&data[0]),
      static_cast<uint32_t>(data.size()));
  message.set_sender_pid(peer_pid_);

  DVLOG(4) << "Receive " << message.type() << ": " << message.size();
  MojoResult write_result =
      ChannelMojo::WriteToMessageAttachmentSet(std::move(handles), &message);
  if (write_result != MOJO_RESULT_OK) {
    OnPipeError(write_result);
    return;
  }

  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "MessagePipeReader::Receive",
                         message.flags(),
                         TRACE_EVENT_FLAG_FLOW_IN);
  delegate_->OnMessageReceived(message);
}

void MessagePipeReader::OnPipeError(MojoResult error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_)
    delegate_->OnPipeError();
  Close();
}

void MessagePipeReader::DelayedDeleter::operator()(
    MessagePipeReader* ptr) const {
  ptr->Close();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&DeleteNow, ptr));
}

}  // namespace internal
}  // namespace IPC
