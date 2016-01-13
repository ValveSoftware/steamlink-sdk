// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/data_pipe_utils.h"

#include <stdio.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner_util.h"
#include "mojo/common/handle_watcher.h"

namespace mojo {
namespace common {

bool BlockingCopyToFile(ScopedDataPipeConsumerHandle source,
                        const base::FilePath& destination) {
  base::ScopedFILE fp(base::OpenFile(destination, "wb"));
  if (!fp)
    return false;

  for (;;) {
    const void* buffer;
    uint32_t num_bytes;
    MojoResult result = BeginReadDataRaw(source.get(), &buffer, &num_bytes,
                                         MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      size_t bytes_written = fwrite(buffer, 1, num_bytes, fp.get());
      result = EndReadDataRaw(source.get(), num_bytes);
      if (bytes_written < num_bytes || result != MOJO_RESULT_OK)
        return false;
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      result = Wait(source.get(),
                    MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE);
      if (result != MOJO_RESULT_OK) {
        // If the producer handle was closed, then treat as EOF.
        return result == MOJO_RESULT_FAILED_PRECONDITION;
      }
    } else if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      // If the producer handle was closed, then treat as EOF.
      return true;
    } else {
      // Some other error occurred.
      break;
    }
  }

  return false;
}

void CompleteBlockingCopyToFile(const base::Callback<void(bool)>& callback,
                                bool result) {
  callback.Run(result);
}

void CopyToFile(ScopedDataPipeConsumerHandle source,
                const base::FilePath& destination,
                base::TaskRunner* task_runner,
                const base::Callback<void(bool)>& callback) {
  base::PostTaskAndReplyWithResult(
      task_runner,
      FROM_HERE,
      base::Bind(&BlockingCopyToFile, base::Passed(&source), destination),
      callback);
}

}  // namespace common
}  // namespace mojo
