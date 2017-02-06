// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "net/ssl/ssl_platform_key_task_runner.h"

namespace net {

SSLPlatformKeyTaskRunner::SSLPlatformKeyTaskRunner() {
  worker_pool_ = new base::SequencedWorkerPool(1, "Platform Key Thread");
  task_runner_ = worker_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
      worker_pool_->GetSequenceToken(),
      base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

SSLPlatformKeyTaskRunner::~SSLPlatformKeyTaskRunner() {}

scoped_refptr<base::SequencedTaskRunner>
SSLPlatformKeyTaskRunner::task_runner() {
  return task_runner_;
}

base::LazyInstance<SSLPlatformKeyTaskRunner>::Leaky g_platform_key_task_runner =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<base::SequencedTaskRunner> GetSSLPlatformKeyTaskRunner() {
  return g_platform_key_task_runner.Get().task_runner();
}

}  // namespace net
