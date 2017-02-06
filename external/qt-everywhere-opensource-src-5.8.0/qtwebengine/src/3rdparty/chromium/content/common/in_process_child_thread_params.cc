// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/in_process_child_thread_params.h"

namespace content {

InProcessChildThreadParams::InProcessChildThreadParams(
    const std::string& channel_name,
    scoped_refptr<base::SequencedTaskRunner> io_runner,
    const std::string& ipc_token,
    const std::string& application_token)
    : channel_name_(channel_name), io_runner_(io_runner), ipc_token_(ipc_token),
      application_token_(application_token) {}

InProcessChildThreadParams::InProcessChildThreadParams(
    const InProcessChildThreadParams& other) = default;

InProcessChildThreadParams::~InProcessChildThreadParams() {
}

}  // namespace content
