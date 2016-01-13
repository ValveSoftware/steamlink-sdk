// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_QUOTA_TASK_H_
#define WEBKIT_BROWSER_QUOTA_QUOTA_TASK_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace quota {

class QuotaTaskObserver;

// A base class for quota tasks.
// TODO(kinuko): Revise this using base::Callback.
class QuotaTask {
 public:
  void Start();

 protected:
  explicit QuotaTask(QuotaTaskObserver* observer);
  virtual ~QuotaTask();

  // The task body.
  virtual void Run() = 0;

  // Called upon completion, on the original message loop.
  virtual void Completed() = 0;

  // Called when the task is aborted.
  virtual void Aborted() {}

  void CallCompleted();

  // Call this to delete itself.
  void DeleteSoon();

  QuotaTaskObserver* observer() const { return observer_; }
  base::SingleThreadTaskRunner* original_task_runner() const {
    return original_task_runner_.get();
  }

 private:
  friend class base::DeleteHelper<QuotaTask>;
  friend class QuotaTaskObserver;

  void Abort();
  QuotaTaskObserver* observer_;
  scoped_refptr<base::SingleThreadTaskRunner> original_task_runner_;
  bool delete_scheduled_;
};

class WEBKIT_STORAGE_BROWSER_EXPORT QuotaTaskObserver {
 protected:
  friend class QuotaTask;

  QuotaTaskObserver();
  virtual ~QuotaTaskObserver();

  void RegisterTask(QuotaTask* task);
  void UnregisterTask(QuotaTask* task);

  typedef std::set<QuotaTask*> TaskSet;
  TaskSet running_quota_tasks_;
};
}

#endif  // WEBKIT_BROWSER_QUOTA_QUOTA_TASK_H_
