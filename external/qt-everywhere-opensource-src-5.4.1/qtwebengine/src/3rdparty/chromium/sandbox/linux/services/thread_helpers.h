// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_THREAD_HELPERS_H_
#define SANDBOX_LINUX_SERVICES_THREAD_HELPERS_H_

#include "base/basictypes.h"
#include "sandbox/sandbox_export.h"

namespace base { class Thread; }

namespace sandbox {

class SANDBOX_EXPORT ThreadHelpers {
 public:
  // Check whether the current process is single threaded. |proc_self_tasks|
  // can be a file descriptor to /proc/self/task/ and remains owned by the
  // caller or -1.
  // If |proc_self_tasks| is -1, this method will open /proc/self/task/ and
  // crash if it cannot.
  static bool IsSingleThreaded(int proc_self_task);

  // Stop |thread| and ensure that it does not have an entry in
  // /proc/self/task/ from the point of view of the current thread. This is
  // the way to stop threads before calling IsSingleThreaded().
  static bool StopThreadAndWatchProcFS(int proc_self_tasks,
                                       base::Thread* thread);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ThreadHelpers);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_THREAD_HELPERS_H_
