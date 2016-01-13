// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/task_util.h"

#include "base/location.h"

namespace google_apis {

void RunTaskOnThread(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     const base::Closure& task) {
  if (task_runner->RunsTasksOnCurrentThread()) {
    task.Run();
  } else {
    const bool posted = task_runner->PostTask(FROM_HERE, task);
    DCHECK(posted);
  }
}

}  // namespace google_apis
