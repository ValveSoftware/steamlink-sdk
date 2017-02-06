// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ROUTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ROUTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/lib/filter_chain.h"

namespace mojo {
namespace internal {

// TODO(yzshen): Consider removing this class and use MultiplexRouter in all
// cases. crbug.com/594244
class Router : public MessageReceiverWithResponder {
 public:
  Router(ScopedMessagePipeHandle message_pipe,
         FilterChain filters,
         bool expects_sync_requests,
         scoped_refptr<base::SingleThreadTaskRunner> runner);
  ~Router() override;

  // Sets the receiver to handle messages read from the message pipe that do
  // not have the Message::kFlagIsResponse flag set.
  void set_incoming_receiver(MessageReceiverWithResponderStatus* receiver) {
    incoming_receiver_ = receiver;
  }

  // Sets the error handler to receive notifications when an error is
  // encountered while reading from the pipe or waiting to read from the pipe.
  void set_connection_error_handler(const base::Closure& error_handler) {
    error_handler_ = error_handler;
  }

  // Returns true if an error was encountered while reading from the pipe or
  // waiting to read from the pipe.
  bool encountered_error() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return encountered_error_;
  }

  // Is the router bound to a MessagePipe handle?
  bool is_valid() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return connector_.is_valid();
  }

  // Please note that this method shouldn't be called unless it results from an
  // explicit request of the user of bindings (e.g., the user sets an
  // InterfacePtr to null or closes a Binding).
  void CloseMessagePipe() {
    DCHECK(thread_checker_.CalledOnValidThread());
    connector_.CloseMessagePipe();
  }

  ScopedMessagePipeHandle PassMessagePipe() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return connector_.PassMessagePipe();
  }

  void RaiseError() {
    DCHECK(thread_checker_.CalledOnValidThread());
    connector_.RaiseError();
  }

  // MessageReceiver implementation:
  bool Accept(Message* message) override;
  bool AcceptWithResponder(Message* message,
                           MessageReceiver* responder) override;

  // Blocks the current thread until the first incoming method call, i.e.,
  // either a call to a client method or a callback method, or |deadline|.
  bool WaitForIncomingMessage(MojoDeadline deadline) {
    DCHECK(thread_checker_.CalledOnValidThread());
    return connector_.WaitForIncomingMessage(deadline);
  }

  // See Binding for details of pause/resume.
  void PauseIncomingMethodCallProcessing() {
    DCHECK(thread_checker_.CalledOnValidThread());
    connector_.PauseIncomingMethodCallProcessing();
  }
  void ResumeIncomingMethodCallProcessing() {
    DCHECK(thread_checker_.CalledOnValidThread());
    connector_.ResumeIncomingMethodCallProcessing();
  }

  // Sets this object to testing mode.
  // In testing mode:
  // - the object is more tolerant of unrecognized response messages;
  // - the connector continues working after seeing errors from its incoming
  //   receiver.
  void EnableTestingMode();

  MessagePipeHandle handle() const { return connector_.handle(); }

  // Returns true if this Router has any pending callbacks.
  bool has_pending_responders() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return !async_responders_.empty() || !sync_responses_.empty();
  }

 private:
  // Maps from the id of a response to the MessageReceiver that handles the
  // response.
  using AsyncResponderMap =
      std::map<uint64_t, std::unique_ptr<MessageReceiver>>;

  struct SyncResponseInfo {
   public:
    explicit SyncResponseInfo(bool* in_response_received);
    ~SyncResponseInfo();

    std::unique_ptr<Message> response;

    // Points to a stack-allocated variable.
    bool* response_received;

   private:
    DISALLOW_COPY_AND_ASSIGN(SyncResponseInfo);
  };

  using SyncResponseMap = std::map<uint64_t, std::unique_ptr<SyncResponseInfo>>;

  class HandleIncomingMessageThunk : public MessageReceiver {
   public:
    HandleIncomingMessageThunk(Router* router);
    ~HandleIncomingMessageThunk() override;

    // MessageReceiver implementation:
    bool Accept(Message* message) override;

   private:
    Router* router_;
  };

  bool HandleIncomingMessage(Message* message);
  void HandleQueuedMessages();
  bool HandleMessageInternal(Message* message);

  void OnConnectionError();

  HandleIncomingMessageThunk thunk_;
  FilterChain filters_;
  Connector connector_;
  MessageReceiverWithResponderStatus* incoming_receiver_;
  AsyncResponderMap async_responders_;
  SyncResponseMap sync_responses_;
  uint64_t next_request_id_;
  bool testing_mode_;
  std::queue<std::unique_ptr<Message>> pending_messages_;
  // Whether a task has been posted to trigger processing of
  // |pending_messages_|.
  bool pending_task_for_messages_;
  bool encountered_error_;
  base::Closure error_handler_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Router> weak_factory_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ROUTER_H_
