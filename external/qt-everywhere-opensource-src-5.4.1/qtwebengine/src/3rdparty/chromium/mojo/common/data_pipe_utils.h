// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DATA_PIPE_UTILS_H_
#define MOJO_SHELL_DATA_PIPE_UTILS_H_

#include "base/callback_forward.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/cpp/system/core.h"

namespace base {
class FilePath;
class TaskRunner;
}

namespace mojo {
namespace common {

// Asynchronously copies data from source to the destination file. The given
// |callback| is run upon completion. File writes will be scheduled to the
// given |task_runner|.
void MOJO_COMMON_EXPORT CopyToFile(
    ScopedDataPipeConsumerHandle source,
    const base::FilePath& destination,
    base::TaskRunner* task_runner,
    const base::Callback<void(bool /*success*/)>& callback);

}  // namespace common
}  // namespace mojo

#endif  // MOJO_SHELL_DATA_PIPE_UTILS_H_
