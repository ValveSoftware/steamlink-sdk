// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_TASK_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_TASK_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/common/content_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

// Used to sequence ServiceWorkerDatabase tasks. Vends two task runners: one
// that blocks shutdown and one that doesn't. The task runners use the same
// sequence token, so no matter which runner you post to, the tasks run in the
// order they are posted. This lets you post sequenced tasks with mixed shutdown
// behaviors.
class ServiceWorkerDatabaseTaskManager {
 public:
  virtual ~ServiceWorkerDatabaseTaskManager() {}
  virtual std::unique_ptr<ServiceWorkerDatabaseTaskManager> Clone() = 0;
  virtual base::SequencedTaskRunner* GetTaskRunner() = 0;
  virtual base::SequencedTaskRunner* GetShutdownBlockingTaskRunner() = 0;
};

class ServiceWorkerDatabaseTaskManagerImpl
    : public ServiceWorkerDatabaseTaskManager {
 public:
  explicit ServiceWorkerDatabaseTaskManagerImpl(
      base::SequencedWorkerPool* pool);
  ~ServiceWorkerDatabaseTaskManagerImpl() override;

 protected:
  std::unique_ptr<ServiceWorkerDatabaseTaskManager> Clone() override;
  base::SequencedTaskRunner* GetTaskRunner() override;
  base::SequencedTaskRunner* GetShutdownBlockingTaskRunner() override;

 private:
  ServiceWorkerDatabaseTaskManagerImpl(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<base::SequencedTaskRunner>&
          shutdown_blocking_task_runner);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> shutdown_blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDatabaseTaskManagerImpl);
};

// Dummy implementation for testing. It vends whatever you give it in the ctor.
class CONTENT_EXPORT MockServiceWorkerDatabaseTaskManager
    : public NON_EXPORTED_BASE(ServiceWorkerDatabaseTaskManager) {
 public:
  explicit MockServiceWorkerDatabaseTaskManager(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~MockServiceWorkerDatabaseTaskManager() override;

 protected:
  std::unique_ptr<ServiceWorkerDatabaseTaskManager> Clone() override;
  base::SequencedTaskRunner* GetTaskRunner() override;
  base::SequencedTaskRunner* GetShutdownBlockingTaskRunner() override;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerDatabaseTaskManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_TASK_MANAGER_H_
