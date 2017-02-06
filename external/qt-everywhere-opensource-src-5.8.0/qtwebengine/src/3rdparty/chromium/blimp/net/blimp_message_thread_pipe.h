// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_THREAD_PIPE_H_
#define BLIMP_NET_BLIMP_MESSAGE_THREAD_PIPE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_net_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace blimp {

class BlimpMessage;
class BlimpMessageProcessor;

// Uni-directional MessageProcessor "pipe" that accepts messages on
// one thread and dispatches them to a MessageProcessor on a different
// thread.
//
// Typical usage involves:
// 1. Create the pipe on the "main" thread, specifying the target thread's
//    task runner.
// 2. Take one or more MessageProcessor proxies from it.
// 3. Post a task to the target thread to set the target MessageProcessor.
// 4. Start using the MessageProcessor proxy(/ies) on the main thread.
// 5. When the target MessageProcessor is about to be destroyed on the
//    target thread, destroy the pipe instance immediately beforehand.
//    Any messages that are subsequently passed to a proxy, or are already
//    in-flight to the pipe, will be silently dropped.
class BLIMP_NET_EXPORT BlimpMessageThreadPipe {
 public:
  explicit BlimpMessageThreadPipe(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~BlimpMessageThreadPipe();

  // Creates a proxy MessageProcessor that routes messages to
  // the outgoing MessageProcessor on |task_runner|.
  // Proxies are safe to create before the outgoing MessageProcessor
  // has been set, but cannot be used until it has after been set -
  // see the class-level comment on usage.
  // Proxies must be deleted on the thread on which they are used.
  std::unique_ptr<BlimpMessageProcessor> CreateProxy();

  // Sets/gets the target MessageProcessor on the target thread.
  void set_target_processor(BlimpMessageProcessor* processor);
  BlimpMessageProcessor* target_processor() const;

 private:
  // Target MessageProcessor & TaskRunner to process messages with.
  BlimpMessageProcessor* target_processor_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> target_task_runner_;

  // Allows |this| to be safely detached from existing proxies on deletion.
  base::WeakPtrFactory<BlimpMessageThreadPipe> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageThreadPipe);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_THREAD_PIPE_H_
