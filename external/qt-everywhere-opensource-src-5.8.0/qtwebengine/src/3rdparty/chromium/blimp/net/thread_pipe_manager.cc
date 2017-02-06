// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/thread_pipe_manager.h"

#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_thread_pipe.h"
#include "blimp/net/browser_connection_handler.h"

namespace blimp {

// ConnectionThreadPipeManager manages ThreadPipeManager resources used on the
// connection thread. It is created on the caller thread, and then has pipe
// and proxy pairs passed to it on the connection thread, to register with
// the supplied |connection_handler|. It is finally deleted on the connection
// thread.
class ConnectionThreadPipeManager {
 public:
  explicit ConnectionThreadPipeManager(
      BrowserConnectionHandler* connection_handler);
  virtual ~ConnectionThreadPipeManager();

  // Connects message pipes between the specified feature and the network layer,
  // using |incoming_proxy| as the incoming message processor, and connecting
  // |outgoing_pipe| to the actual message sender.
  void RegisterFeature(BlimpMessage::FeatureCase feature_case,
                       std::unique_ptr<BlimpMessageThreadPipe> outgoing_pipe,
                       std::unique_ptr<BlimpMessageProcessor> incoming_proxy);

 private:
  BrowserConnectionHandler* connection_handler_;

  // Container for the feature-specific MessageProcessors.
  // connection-side proxy for sending messages to caller thread.
  std::vector<std::unique_ptr<BlimpMessageProcessor>> incoming_proxies_;

  // Containers for the MessageProcessors used to write feature-specific
  // messages to the network, and the thread-pipe endpoints through which
  // they are used from the caller thread.
  std::vector<std::unique_ptr<BlimpMessageProcessor>>
      outgoing_message_processors_;
  std::vector<std::unique_ptr<BlimpMessageThreadPipe>> outgoing_pipes_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionThreadPipeManager);
};

ConnectionThreadPipeManager::ConnectionThreadPipeManager(
    BrowserConnectionHandler* connection_handler)
    : connection_handler_(connection_handler) {
  DCHECK(connection_handler_);
}

ConnectionThreadPipeManager::~ConnectionThreadPipeManager() {}

void ConnectionThreadPipeManager::RegisterFeature(
    BlimpMessage::FeatureCase feature_case,
    std::unique_ptr<BlimpMessageThreadPipe> outgoing_pipe,
    std::unique_ptr<BlimpMessageProcessor> incoming_proxy) {
  // Registers |incoming_proxy| as the message processor for incoming
  // messages with |feature_case|. Sets the returned outgoing message processor
  // as the target of the |outgoing_pipe|.
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor =
      connection_handler_->RegisterFeature(feature_case, incoming_proxy.get());
  outgoing_pipe->set_target_processor(outgoing_message_processor.get());

  // This object manages the lifetimes of the pipe, proxy and target processor.
  incoming_proxies_.push_back(std::move(incoming_proxy));
  outgoing_pipes_.push_back(std::move(outgoing_pipe));
  outgoing_message_processors_.push_back(std::move(outgoing_message_processor));
}

ThreadPipeManager::ThreadPipeManager(
    const scoped_refptr<base::SequencedTaskRunner>& connection_task_runner,
    BrowserConnectionHandler* connection_handler)
    : connection_task_runner_(connection_task_runner),
      connection_pipe_manager_(
          new ConnectionThreadPipeManager(connection_handler)) {}

ThreadPipeManager::~ThreadPipeManager() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  connection_task_runner_->DeleteSoon(FROM_HERE,
                                      connection_pipe_manager_.release());
}

std::unique_ptr<BlimpMessageProcessor> ThreadPipeManager::RegisterFeature(
    BlimpMessage::FeatureCase feature_case,
    BlimpMessageProcessor* incoming_processor) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  // Creates an outgoing pipe and a proxy for forwarding messages
  // from features on the caller thread to network components on the
  // connection thread.
  std::unique_ptr<BlimpMessageThreadPipe> outgoing_pipe(
      new BlimpMessageThreadPipe(connection_task_runner_));
  std::unique_ptr<BlimpMessageProcessor> outgoing_proxy =
      outgoing_pipe->CreateProxy();

  // Creates an incoming pipe and a proxy for receiving messages
  // from network components on the connection thread.
  std::unique_ptr<BlimpMessageThreadPipe> incoming_pipe(
      new BlimpMessageThreadPipe(base::SequencedTaskRunnerHandle::Get()));
  incoming_pipe->set_target_processor(incoming_processor);
  std::unique_ptr<BlimpMessageProcessor> incoming_proxy =
      incoming_pipe->CreateProxy();

  // Finishes registration on connection thread.
  connection_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ConnectionThreadPipeManager::RegisterFeature,
                 base::Unretained(connection_pipe_manager_.get()), feature_case,
                 base::Passed(std::move(outgoing_pipe)),
                 base::Passed(std::move(incoming_proxy))));

  incoming_pipes_.push_back(std::move(incoming_pipe));
  return outgoing_proxy;
}

}  // namespace blimp
