// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_IN_PROCESS_CHILD_THREAD_PARAMS_H_
#define CONTENT_COMMON_IN_PROCESS_CHILD_THREAD_PARAMS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"

namespace content {

// Tells ChildThreadImpl to run in in-process mode. There are a couple of
// parameters to run in the mode: An emulated io task runner used by
// ChnanelMojo, an IPC channel name to open.
class CONTENT_EXPORT InProcessChildThreadParams {
 public:
  InProcessChildThreadParams(
      const std::string& channel_name,
      scoped_refptr<base::SequencedTaskRunner> io_runner,
      const std::string& ipc_token = std::string(),
      const std::string& application_token = std::string());
  InProcessChildThreadParams(const InProcessChildThreadParams& other);
  ~InProcessChildThreadParams();

  const std::string& channel_name() const { return channel_name_; }
  scoped_refptr<base::SequencedTaskRunner> io_runner() const {
    return io_runner_;
  }
  const std::string& ipc_token() const { return ipc_token_; }
  const std::string& application_token() const {
    return application_token_;
  }

 private:
  std::string channel_name_;
  scoped_refptr<base::SequencedTaskRunner> io_runner_;
  std::string ipc_token_;
  std::string application_token_;
};

}  // namespace content

#endif  // CONTENT_COMMON_IN_PROCESS_CHILD_THREAD_PARAMS_H_
