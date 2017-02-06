// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/shell_connection_ref.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"

namespace shell {

class ShellConnectionRefImpl : public ShellConnectionRef {
 public:
  ShellConnectionRefImpl(
      base::WeakPtr<ShellConnectionRefFactory> factory,
      scoped_refptr<base::SingleThreadTaskRunner> shell_client_task_runner)
      : factory_(factory),
        shell_client_task_runner_(shell_client_task_runner) {
    // This object is not thread-safe but may be used exclusively on a different
    // thread from the one which constructed it.
    thread_checker_.DetachFromThread();
  }

  ~ShellConnectionRefImpl() override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (shell_client_task_runner_->BelongsToCurrentThread() && factory_) {
      factory_->Release();
    } else {
      shell_client_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ShellConnectionRefFactory::Release, factory_));
    }
  }

 private:
  // ShellConnectionRef:
  std::unique_ptr<ShellConnectionRef> Clone() override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (shell_client_task_runner_->BelongsToCurrentThread() && factory_) {
      factory_->AddRef();
    } else {
      shell_client_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ShellConnectionRefFactory::AddRef, factory_));
    }

    return base::WrapUnique(
        new ShellConnectionRefImpl(factory_, shell_client_task_runner_));
  }

  base::WeakPtr<ShellConnectionRefFactory> factory_;
  scoped_refptr<base::SingleThreadTaskRunner> shell_client_task_runner_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ShellConnectionRefImpl);
};

ShellConnectionRefFactory::ShellConnectionRefFactory(
    const base::Closure& quit_closure)
    : quit_closure_(quit_closure), weak_factory_(this) {
  DCHECK(!quit_closure_.is_null());
}

ShellConnectionRefFactory::~ShellConnectionRefFactory() {}

std::unique_ptr<ShellConnectionRef> ShellConnectionRefFactory::CreateRef() {
  AddRef();
  return base::WrapUnique(
      new ShellConnectionRefImpl(weak_factory_.GetWeakPtr(),
                                 base::ThreadTaskRunnerHandle::Get()));
}

void ShellConnectionRefFactory::AddRef() {
  ++ref_count_;
}

void ShellConnectionRefFactory::Release() {
  if (!--ref_count_)
    quit_closure_.Run();
}

}  // namespace shell
