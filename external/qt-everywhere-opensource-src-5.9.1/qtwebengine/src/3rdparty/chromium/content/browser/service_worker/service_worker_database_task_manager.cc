// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database_task_manager.h"

#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"

namespace content {

ServiceWorkerDatabaseTaskManagerImpl::ServiceWorkerDatabaseTaskManagerImpl(
    base::SequencedWorkerPool* pool) {
  base::SequencedWorkerPool::SequenceToken token = pool->GetSequenceToken();
  task_runner_ = pool->GetSequencedTaskRunner(token);
  shutdown_blocking_task_runner_ =
      pool->GetSequencedTaskRunnerWithShutdownBehavior(
          token, base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

ServiceWorkerDatabaseTaskManagerImpl::~ServiceWorkerDatabaseTaskManagerImpl() {
}

std::unique_ptr<ServiceWorkerDatabaseTaskManager>
ServiceWorkerDatabaseTaskManagerImpl::Clone() {
  return base::WrapUnique(new ServiceWorkerDatabaseTaskManagerImpl(
      task_runner_, shutdown_blocking_task_runner_));
}

base::SequencedTaskRunner*
ServiceWorkerDatabaseTaskManagerImpl::GetTaskRunner() {
  return task_runner_.get();
}

base::SequencedTaskRunner*
ServiceWorkerDatabaseTaskManagerImpl::GetShutdownBlockingTaskRunner() {
  return shutdown_blocking_task_runner_.get();
}

ServiceWorkerDatabaseTaskManagerImpl::ServiceWorkerDatabaseTaskManagerImpl(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>&
        shutdown_blocking_task_runner)
    : task_runner_(task_runner),
      shutdown_blocking_task_runner_(shutdown_blocking_task_runner) {
}

MockServiceWorkerDatabaseTaskManager::MockServiceWorkerDatabaseTaskManager(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {
}

MockServiceWorkerDatabaseTaskManager::~MockServiceWorkerDatabaseTaskManager() {
}

std::unique_ptr<ServiceWorkerDatabaseTaskManager>
MockServiceWorkerDatabaseTaskManager::Clone() {
  return base::WrapUnique(
      new MockServiceWorkerDatabaseTaskManager(task_runner_));
}

base::SequencedTaskRunner*
MockServiceWorkerDatabaseTaskManager::GetTaskRunner() {
  return task_runner_.get();
}

base::SequencedTaskRunner*
MockServiceWorkerDatabaseTaskManager::GetShutdownBlockingTaskRunner() {
  return task_runner_.get();
}

}  // namespace content
