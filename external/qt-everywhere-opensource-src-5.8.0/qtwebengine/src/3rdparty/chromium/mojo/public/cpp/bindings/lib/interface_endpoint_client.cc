// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/interface_endpoint_client.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

namespace mojo {

// ----------------------------------------------------------------------------

namespace {

void DCheckIfInvalid(const base::WeakPtr<InterfaceEndpointClient>& client,
                   const std::string& message) {
  bool is_valid = client && !client->encountered_error();
  DCHECK(!is_valid) << message;
}

// When receiving an incoming message which expects a repsonse,
// InterfaceEndpointClient creates a ResponderThunk object and passes it to the
// incoming message receiver. When the receiver finishes processing the message,
// it can provide a response using this object.
class ResponderThunk : public MessageReceiverWithStatus {
 public:
  explicit ResponderThunk(
      const base::WeakPtr<InterfaceEndpointClient>& endpoint_client,
      scoped_refptr<base::SingleThreadTaskRunner> runner)
      : endpoint_client_(endpoint_client),
        accept_was_invoked_(false),
        task_runner_(std::move(runner)) {}
  ~ResponderThunk() override {
    if (!accept_was_invoked_) {
      // The Mojo application handled a message that was expecting a response
      // but did not send a response.
      // We raise an error to signal the calling application that an error
      // condition occurred. Without this the calling application would have no
      // way of knowing it should stop waiting for a response.
      if (task_runner_->RunsTasksOnCurrentThread()) {
        // Please note that even if this code is run from a different task
        // runner on the same thread as |task_runner_|, it is okay to directly
        // call InterfaceEndpointClient::RaiseError(), because it will raise
        // error from the correct task runner asynchronously.
        if (endpoint_client_) {
          endpoint_client_->RaiseError();
        }
      } else {
        task_runner_->PostTask(
            FROM_HERE,
            base::Bind(&InterfaceEndpointClient::RaiseError, endpoint_client_));
      }
    }
  }

  // MessageReceiver implementation:
  bool Accept(Message* message) override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    accept_was_invoked_ = true;
    DCHECK(message->has_flag(Message::kFlagIsResponse));

    bool result = false;

    if (endpoint_client_)
      result = endpoint_client_->Accept(message);

    return result;
  }

  // MessageReceiverWithStatus implementation:
  bool IsValid() override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    return endpoint_client_ && !endpoint_client_->encountered_error();
  }

  void DCheckInvalid(const std::string& message) override {
    if (task_runner_->RunsTasksOnCurrentThread()) {
      DCheckIfInvalid(endpoint_client_, message);
    } else {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&DCheckIfInvalid, endpoint_client_, message));
    }
 }

 private:
  base::WeakPtr<InterfaceEndpointClient> endpoint_client_;
  bool accept_was_invoked_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ResponderThunk);
};

}  // namespace

// ----------------------------------------------------------------------------

InterfaceEndpointClient::SyncResponseInfo::SyncResponseInfo(
    bool* in_response_received)
    : response_received(in_response_received) {}

InterfaceEndpointClient::SyncResponseInfo::~SyncResponseInfo() {}

// ----------------------------------------------------------------------------

InterfaceEndpointClient::HandleIncomingMessageThunk::HandleIncomingMessageThunk(
    InterfaceEndpointClient* owner)
    : owner_(owner) {}

InterfaceEndpointClient::HandleIncomingMessageThunk::
    ~HandleIncomingMessageThunk() {}

bool InterfaceEndpointClient::HandleIncomingMessageThunk::Accept(
    Message* message) {
  return owner_->HandleValidatedMessage(message);
}

// ----------------------------------------------------------------------------

InterfaceEndpointClient::InterfaceEndpointClient(
    ScopedInterfaceEndpointHandle handle,
    MessageReceiverWithResponderStatus* receiver,
    std::unique_ptr<MessageFilter> payload_validator,
    bool expect_sync_requests,
    scoped_refptr<base::SingleThreadTaskRunner> runner)
    : handle_(std::move(handle)),
      incoming_receiver_(receiver),
      payload_validator_(std::move(payload_validator)),
      thunk_(this),
      next_request_id_(1),
      encountered_error_(false),
      task_runner_(std::move(runner)),
      weak_ptr_factory_(this) {
  DCHECK(handle_.is_valid());
  DCHECK(handle_.is_local());

  // TODO(yzshen): the way to use validator (or message filter in general)
  // directly is a little awkward.
  payload_validator_->set_sink(&thunk_);

  controller_ = handle_.group_controller()->AttachEndpointClient(
      handle_, this, task_runner_);
  if (expect_sync_requests)
    controller_->AllowWokenUpBySyncWatchOnSameThread();
}

InterfaceEndpointClient::~InterfaceEndpointClient() {
  DCHECK(thread_checker_.CalledOnValidThread());

  handle_.group_controller()->DetachEndpointClient(handle_);
}

AssociatedGroup* InterfaceEndpointClient::associated_group() {
  if (!associated_group_)
    associated_group_ = handle_.group_controller()->CreateAssociatedGroup();
  return associated_group_.get();
}

uint32_t InterfaceEndpointClient::interface_id() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return handle_.id();
}

ScopedInterfaceEndpointHandle InterfaceEndpointClient::PassHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!has_pending_responders());

  if (!handle_.is_valid())
    return ScopedInterfaceEndpointHandle();

  controller_ = nullptr;
  handle_.group_controller()->DetachEndpointClient(handle_);

  return std::move(handle_);
}

void InterfaceEndpointClient::RaiseError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  handle_.group_controller()->RaiseError();
}

bool InterfaceEndpointClient::Accept(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(controller_);
  DCHECK(!message->has_flag(Message::kFlagExpectsResponse));

  if (encountered_error_)
    return false;

  return controller_->SendMessage(message);
}

bool InterfaceEndpointClient::AcceptWithResponder(Message* message,
                                                  MessageReceiver* responder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(controller_);
  DCHECK(message->has_flag(Message::kFlagExpectsResponse));

  if (encountered_error_)
    return false;

  // Reserve 0 in case we want it to convey special meaning in the future.
  uint64_t request_id = next_request_id_++;
  if (request_id == 0)
    request_id = next_request_id_++;

  message->set_request_id(request_id);

  bool is_sync = message->has_flag(Message::kFlagIsSync);
  if (!controller_->SendMessage(message))
    return false;

  if (!is_sync) {
    // We assume ownership of |responder|.
    async_responders_[request_id] = base::WrapUnique(responder);
    return true;
  }

  SyncCallRestrictions::AssertSyncCallAllowed();

  bool response_received = false;
  std::unique_ptr<MessageReceiver> sync_responder(responder);
  sync_responses_.insert(std::make_pair(
      request_id, base::WrapUnique(new SyncResponseInfo(&response_received))));

  base::WeakPtr<InterfaceEndpointClient> weak_self =
      weak_ptr_factory_.GetWeakPtr();
  controller_->SyncWatch(&response_received);
  // Make sure that this instance hasn't been destroyed.
  if (weak_self) {
    DCHECK(ContainsKey(sync_responses_, request_id));
    auto iter = sync_responses_.find(request_id);
    DCHECK_EQ(&response_received, iter->second->response_received);
    if (response_received) {
      std::unique_ptr<Message> response = std::move(iter->second->response);
      ignore_result(sync_responder->Accept(response.get()));
    }
    sync_responses_.erase(iter);
  }

  // Return true means that we take ownership of |responder|.
  return true;
}

bool InterfaceEndpointClient::HandleIncomingMessage(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return payload_validator_->Accept(message);
}

void InterfaceEndpointClient::NotifyError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (encountered_error_)
    return;
  encountered_error_ = true;
  if (!error_handler_.is_null())
    error_handler_.Run();
}

bool InterfaceEndpointClient::HandleValidatedMessage(Message* message) {
  DCHECK_EQ(handle_.id(), message->interface_id());

  if (message->has_flag(Message::kFlagExpectsResponse)) {
    if (!incoming_receiver_)
      return false;

    MessageReceiverWithStatus* responder =
        new ResponderThunk(weak_ptr_factory_.GetWeakPtr(), task_runner_);
    bool ok = incoming_receiver_->AcceptWithResponder(message, responder);
    if (!ok)
      delete responder;
    return ok;
  } else if (message->has_flag(Message::kFlagIsResponse)) {
    uint64_t request_id = message->request_id();

    if (message->has_flag(Message::kFlagIsSync)) {
      auto it = sync_responses_.find(request_id);
      if (it == sync_responses_.end())
        return false;
      it->second->response.reset(new Message());
      message->MoveTo(it->second->response.get());
      *it->second->response_received = true;
      return true;
    }

    auto it = async_responders_.find(request_id);
    if (it == async_responders_.end())
      return false;
    std::unique_ptr<MessageReceiver> responder = std::move(it->second);
    async_responders_.erase(it);
    return responder->Accept(message);
  } else {
    if (!incoming_receiver_)
      return false;

    return incoming_receiver_->Accept(message);
  }
}

}  // namespace mojo
