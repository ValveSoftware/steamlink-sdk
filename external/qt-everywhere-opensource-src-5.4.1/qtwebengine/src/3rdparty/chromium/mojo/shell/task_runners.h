// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_TASK_RUNNERS_H_
#define MOJO_SHELL_TASK_RUNNERS_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"

namespace base {
class SequencedWorkerPool;
}

namespace mojo {
namespace shell {

// A context object that contains the common task runners for the shell's main
// process.
class TaskRunners {
 public:
  explicit TaskRunners(base::SingleThreadTaskRunner* ui_runner);
  ~TaskRunners();

  base::SingleThreadTaskRunner* ui_runner() const {
    return ui_runner_.get();
  }

  base::SingleThreadTaskRunner* io_runner() const {
    return io_thread_->message_loop_proxy();
  }

  base::SequencedWorkerPool* blocking_pool() const {
    return blocking_pool_.get();
  }

 private:
  // TODO(beng): should this be named shell_runner_?
  scoped_refptr<base::SingleThreadTaskRunner> ui_runner_;
  scoped_ptr<base::Thread> io_thread_;

  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunners);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_TASK_RUNNERS_H_
