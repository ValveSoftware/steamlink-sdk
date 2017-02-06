// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_THREAD_PIPE_MANAGER_H_
#define BLIMP_NET_THREAD_PIPE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_net_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blimp {

class BlimpMessageProcessor;
class BlimpMessageThreadPipe;
class BrowserConnectionHandler;
class ConnectionThreadPipeManager;

// Manages routing of messages between a |connection_manager| operating on one
// SequencedTaskRunner, and per-feature message processors running on the same
// TaskRunner as the caller. This is used to allow Blimp feature implementations
// to operate on the UI thread, with network I/O delegated to an IO thread.
class BLIMP_NET_EXPORT ThreadPipeManager {
 public:
  // Caller is responsible for ensuring that |connection_handler| outlives
  // |this|.
  explicit ThreadPipeManager(
      const scoped_refptr<base::SequencedTaskRunner>& connection_task_runner,
      BrowserConnectionHandler* connection_handler);

  ~ThreadPipeManager();

  // Registers a message processor |incoming_processor| which will receive all
  // messages of the |feature_case| specified. Returns a BlimpMessageProcessor
  // object for sending messages of the given feature.
  std::unique_ptr<BlimpMessageProcessor> RegisterFeature(
      BlimpMessage::FeatureCase feature_case,
      BlimpMessageProcessor* incoming_processor);

 private:
  scoped_refptr<base::SequencedTaskRunner> connection_task_runner_;

  // Container for BlimpMessageThreadPipes that are destroyed on IO thread.
  std::unique_ptr<ConnectionThreadPipeManager> connection_pipe_manager_;

  // Pipes for routing incoming messages from the connection's task runner
  // to handlers on the caller's task runner.
  std::vector<std::unique_ptr<BlimpMessageThreadPipe>> incoming_pipes_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPipeManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_THREAD_PIPE_MANAGER_H_
