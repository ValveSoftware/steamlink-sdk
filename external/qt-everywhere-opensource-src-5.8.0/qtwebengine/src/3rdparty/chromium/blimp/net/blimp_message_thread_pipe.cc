// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_thread_pipe.h"

#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"

namespace blimp {

namespace {

class BlimpMessageThreadProxy : public BlimpMessageProcessor {
 public:
  BlimpMessageThreadProxy(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::WeakPtr<BlimpMessageThreadPipe>& pipe);
  ~BlimpMessageThreadProxy() override;

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  // Thread & pipe instance to route messages to/through.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtr<BlimpMessageThreadPipe> pipe_;

  // Used to correctly drop ProcessMessage callbacks if |this| is deleted.
  base::WeakPtrFactory<BlimpMessageThreadProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageThreadProxy);
};

BlimpMessageThreadProxy::BlimpMessageThreadProxy(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<BlimpMessageThreadPipe>& pipe)
    : task_runner_(task_runner), pipe_(pipe), weak_factory_(this) {}

BlimpMessageThreadProxy::~BlimpMessageThreadProxy() {}

void DispatchProcessMessage(const base::WeakPtr<BlimpMessageThreadPipe> pipe,
                            std::unique_ptr<BlimpMessage> message,
                            const net::CompletionCallback& callback) {
  // Process the message only if the pipe is still active.
  if (pipe) {
    pipe->target_processor()->ProcessMessage(std::move(message), callback);
  }
}

void DispatchProcessMessageCallback(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<BlimpMessageThreadProxy> proxy,
    const net::CompletionCallback& callback,
    int result) {
  if (!task_runner->RunsTasksOnCurrentThread()) {
    // Bounce the completion to the thread from which ProcessMessage was called.
    task_runner->PostTask(
        FROM_HERE, base::Bind(&DispatchProcessMessageCallback, task_runner,
                              proxy, callback, result));
    return;
  }

  // Only dispatch the completion callback if the |proxy| is still live.
  if (proxy) {
    callback.Run(result);
  }
}

void BlimpMessageThreadProxy::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  // If |callback| is non-null then wrap it to be called on this thread, iff
  // this proxy instance is still alive at the time.
  net::CompletionCallback wrapped_callback;
  if (!callback.is_null()) {
    wrapped_callback = base::Bind(&DispatchProcessMessageCallback,
                                  base::SequencedTaskRunnerHandle::Get(),
                                  weak_factory_.GetWeakPtr(), callback);
  }

  // Post |message| to be processed via |pipe_| on |task_runner_|.
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&DispatchProcessMessage, pipe_,
                                    base::Passed(&message), wrapped_callback));
}

}  // namespace

BlimpMessageThreadPipe::BlimpMessageThreadPipe(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : target_task_runner_(task_runner), weak_factory_(this) {}

BlimpMessageThreadPipe::~BlimpMessageThreadPipe() {
  DCHECK(target_task_runner_->RunsTasksOnCurrentThread());
}

std::unique_ptr<BlimpMessageProcessor> BlimpMessageThreadPipe::CreateProxy() {
  return base::WrapUnique(new BlimpMessageThreadProxy(
      target_task_runner_, weak_factory_.GetWeakPtr()));
}

void BlimpMessageThreadPipe::set_target_processor(
    BlimpMessageProcessor* processor) {
  DCHECK(target_task_runner_->RunsTasksOnCurrentThread());
  target_processor_ = processor;
}

BlimpMessageProcessor* BlimpMessageThreadPipe::target_processor() const {
  DCHECK(target_task_runner_->RunsTasksOnCurrentThread());
  return target_processor_;
}

}  // namespace blimp
