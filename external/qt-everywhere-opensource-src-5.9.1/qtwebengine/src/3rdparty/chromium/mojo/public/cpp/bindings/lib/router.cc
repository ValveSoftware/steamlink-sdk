// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/router.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

namespace mojo {
namespace internal {

// ----------------------------------------------------------------------------

namespace {

void DCheckIfInvalid(const base::WeakPtr<Router>& router,
                   const std::string& message) {
  bool is_valid = router && !router->encountered_error() && router->is_valid();
  DCHECK(!is_valid) << message;
}

class ResponderThunk : public MessageReceiverWithStatus {
 public:
  explicit ResponderThunk(const base::WeakPtr<Router>& router,
                          scoped_refptr<base::SingleThreadTaskRunner> runner)
      : router_(router),
        accept_was_invoked_(false),
        task_runner_(std::move(runner)) {}
  ~ResponderThunk() override {
    if (!accept_was_invoked_) {
      // The Service handled a message that was expecting a response
      // but did not send a response.
      // We raise an error to signal the calling application that an error
      // condition occurred. Without this the calling application would have no
      // way of knowing it should stop waiting for a response.
      if (task_runner_->RunsTasksOnCurrentThread()) {
        // Please note that even if this code is run from a different task
        // runner on the same thread as |task_runner_|, it is okay to directly
        // call Router::RaiseError(), because it will raise error from the
        // correct task runner asynchronously.
        if (router_)
          router_->RaiseError();
      } else {
        task_runner_->PostTask(FROM_HERE,
                               base::Bind(&Router::RaiseError, router_));
      }
    }
  }

  // MessageReceiver implementation:
  bool Accept(Message* message) override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    accept_was_invoked_ = true;
    DCHECK(message->has_flag(Message::kFlagIsResponse));

    bool result = false;

    if (router_)
      result = router_->Accept(message);

    return result;
  }

  // MessageReceiverWithStatus implementation:
  bool IsValid() override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    return router_ && !router_->encountered_error() && router_->is_valid();
  }

  void DCheckInvalid(const std::string& message) override {
    if (task_runner_->RunsTasksOnCurrentThread()) {
      DCheckIfInvalid(router_, message);
    } else {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&DCheckIfInvalid, router_, message));
    }
  }

 private:
  base::WeakPtr<Router> router_;
  bool accept_was_invoked_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

// ----------------------------------------------------------------------------

Router::SyncResponseInfo::SyncResponseInfo(bool* in_response_received)
    : response_received(in_response_received) {}

Router::SyncResponseInfo::~SyncResponseInfo() {}

// ----------------------------------------------------------------------------

Router::HandleIncomingMessageThunk::HandleIncomingMessageThunk(Router* router)
    : router_(router) {
}

Router::HandleIncomingMessageThunk::~HandleIncomingMessageThunk() {
}

bool Router::HandleIncomingMessageThunk::Accept(Message* message) {
  return router_->HandleIncomingMessage(message);
}

// ----------------------------------------------------------------------------

Router::Router(ScopedMessagePipeHandle message_pipe,
               FilterChain filters,
               bool expects_sync_requests,
               scoped_refptr<base::SingleThreadTaskRunner> runner,
               int interface_version)
    : thunk_(this),
      filters_(std::move(filters)),
      connector_(std::move(message_pipe),
                 Connector::SINGLE_THREADED_SEND,
                 std::move(runner)),
      incoming_receiver_(nullptr),
      next_request_id_(0),
      testing_mode_(false),
      pending_task_for_messages_(false),
      encountered_error_(false),
      control_message_proxy_(this),
      control_message_handler_(interface_version),
      weak_factory_(this) {
  filters_.SetSink(&thunk_);
  if (expects_sync_requests)
    connector_.AllowWokenUpBySyncWatchOnSameThread();
  connector_.set_incoming_receiver(&filters_);
  connector_.set_connection_error_handler(
      base::Bind(&Router::OnConnectionError, base::Unretained(this)));
}

Router::~Router() {}

void Router::AddFilter(std::unique_ptr<MessageReceiver> filter) {
  filters_.Append(std::move(filter));
}

bool Router::Accept(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!message->has_flag(Message::kFlagExpectsResponse));
  return connector_.Accept(message);
}

bool Router::AcceptWithResponder(Message* message, MessageReceiver* responder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message->has_flag(Message::kFlagExpectsResponse));

  // Reserve 0 in case we want it to convey special meaning in the future.
  uint64_t request_id = next_request_id_++;
  if (request_id == 0)
    request_id = next_request_id_++;

  bool is_sync = message->has_flag(Message::kFlagIsSync);
  message->set_request_id(request_id);
  if (!connector_.Accept(message))
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
      request_id, base::MakeUnique<SyncResponseInfo>(&response_received)));

  base::WeakPtr<Router> weak_self = weak_factory_.GetWeakPtr();
  connector_.SyncWatch(&response_received);
  // Make sure that this instance hasn't been destroyed.
  if (weak_self) {
    DCHECK(base::ContainsKey(sync_responses_, request_id));
    auto iter = sync_responses_.find(request_id);
    DCHECK_EQ(&response_received, iter->second->response_received);
    if (response_received)
      ignore_result(sync_responder->Accept(&iter->second->response));
    sync_responses_.erase(iter);
  }

  // Return true means that we take ownership of |responder|.
  return true;
}

void Router::EnableTestingMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  testing_mode_ = true;
  connector_.set_enforce_errors_from_incoming_receiver(false);
}

bool Router::HandleIncomingMessage(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const bool during_sync_call =
      connector_.during_sync_handle_watcher_callback();
  if (!message->has_flag(Message::kFlagIsSync) &&
      (during_sync_call || !pending_messages_.empty())) {
    pending_messages_.emplace(std::move(*message));

    if (!pending_task_for_messages_) {
      pending_task_for_messages_ = true;
      connector_.task_runner()->PostTask(
          FROM_HERE, base::Bind(&Router::HandleQueuedMessages,
                                weak_factory_.GetWeakPtr()));
    }

    return true;
  }

  return HandleMessageInternal(message);
}

void Router::HandleQueuedMessages() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(pending_task_for_messages_);

  base::WeakPtr<Router> weak_self = weak_factory_.GetWeakPtr();
  while (!pending_messages_.empty()) {
    Message message(std::move(pending_messages_.front()));
    pending_messages_.pop();

    bool result = HandleMessageInternal(&message);
    if (!weak_self)
      return;

    if (!result && !testing_mode_) {
      connector_.RaiseError();
      break;
    }
  }

  pending_task_for_messages_ = false;

  // We may have already seen a connection error from the connector, but
  // haven't notified the user because we want to process all the queued
  // messages first. We should do it now.
  if (connector_.encountered_error() && !encountered_error_)
    OnConnectionError();
}

bool Router::HandleMessageInternal(Message* message) {
  DCHECK(!encountered_error_);

  if (message->has_flag(Message::kFlagExpectsResponse)) {
    MessageReceiverWithStatus* responder = new ResponderThunk(
        weak_factory_.GetWeakPtr(), connector_.task_runner());
    bool ok = false;
    if (mojo::internal::ControlMessageHandler::IsControlMessage(message)) {
      ok = control_message_handler_.AcceptWithResponder(message, responder);
    } else {
      ok = incoming_receiver_->AcceptWithResponder(message, responder);
    }
    if (!ok)
      delete responder;
    return ok;

  } else if (message->has_flag(Message::kFlagIsResponse)) {
    uint64_t request_id = message->request_id();

    if (message->has_flag(Message::kFlagIsSync)) {
      auto it = sync_responses_.find(request_id);
      if (it == sync_responses_.end()) {
        DCHECK(testing_mode_);
        return false;
      }
      it->second->response = std::move(*message);
      *it->second->response_received = true;
      return true;
    }

    auto it = async_responders_.find(request_id);
    if (it == async_responders_.end()) {
      DCHECK(testing_mode_);
      return false;
    }
    std::unique_ptr<MessageReceiver> responder = std::move(it->second);
    async_responders_.erase(it);
    return responder->Accept(message);
  } else {
    if (mojo::internal::ControlMessageHandler::IsControlMessage(message))
      return control_message_handler_.Accept(message);

    return incoming_receiver_->Accept(message);
  }
}

void Router::OnConnectionError() {
  if (encountered_error_)
    return;

  if (!pending_messages_.empty()) {
    // After all the pending messages are processed, we will check whether an
    // error has been encountered and run the user's connection error handler
    // if necessary.
    DCHECK(pending_task_for_messages_);
    return;
  }

  if (connector_.during_sync_handle_watcher_callback()) {
    // We don't want the error handler to reenter an ongoing sync call.
    connector_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&Router::OnConnectionError, weak_factory_.GetWeakPtr()));
    return;
  }

  control_message_proxy_.OnConnectionError();

  encountered_error_ = true;

  // Response callbacks may hold on to resource, and there's no need to keep
  // them alive any longer. Note that it's allowed that a pending response
  // callback may own this endpoint, so we simply move the responders onto the
  // stack here and let them be destroyed when the stack unwinds.
  AsyncResponderMap responders = std::move(async_responders_);

  if (!error_handler_.is_null()) {
    error_handler_.Run();
  } else if (!error_with_reason_handler_.is_null()) {
    // Make a copy on the stack. If we directly pass a reference to a member of
    // |control_message_handler_|, that reference will be invalidated as soon as
    // the user destroys the interface endpoint.
    std::string description = control_message_handler_.disconnect_description();
    error_with_reason_handler_.Run(
        control_message_handler_.disconnect_custom_reason(), description);
  }
}

// ----------------------------------------------------------------------------

}  // namespace internal
}  // namespace mojo
