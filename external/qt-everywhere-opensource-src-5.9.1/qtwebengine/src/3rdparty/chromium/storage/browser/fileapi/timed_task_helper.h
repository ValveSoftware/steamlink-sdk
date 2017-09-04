// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_TIMED_TASK_HELPER_H_
#define STORAGE_BROWSER_FILEAPI_TIMED_TASK_HELPER_H_

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {

// Works similarly as base::Timer, but takes SequencedTaskRunner and
// runs tasks on it (instead of implicitly bound to a thread).
// TODO(kinuko): This has nothing to do with fileapi. Move somewhere
// more common place.
class STORAGE_EXPORT TimedTaskHelper {
 public:
  explicit TimedTaskHelper(base::SequencedTaskRunner* task_runner);
  ~TimedTaskHelper();

  bool IsRunning() const;
  void Start(const tracked_objects::Location& posted_from,
             base::TimeDelta delay,
             const base::Closure& user_task);
  void Reset();

 private:
  struct Tracker;
  static void Fired(std::unique_ptr<Tracker> tracker);

  void OnFired(std::unique_ptr<Tracker> tracker);
  void PostDelayedTask(std::unique_ptr<Tracker> tracker, base::TimeDelta delay);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  tracked_objects::Location posted_from_;
  base::TimeDelta delay_;
  base::Closure user_task_;

  base::TimeTicks desired_run_time_;

  // This is set to non-null and owned by a timer task while timer is running.
  Tracker* tracker_;

  DISALLOW_COPY_AND_ASSIGN(TimedTaskHelper);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_TIMED_TASK_HELPER_H_
