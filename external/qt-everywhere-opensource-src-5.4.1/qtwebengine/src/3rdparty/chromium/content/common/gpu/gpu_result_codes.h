// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_RESULT_CODES_H_
#define CONTENT_COMMON_GPU_GPU_RESULT_CODES_H_

namespace content {

enum CreateCommandBufferResult {
  CREATE_COMMAND_BUFFER_SUCCEEDED,
  CREATE_COMMAND_BUFFER_FAILED,
  CREATE_COMMAND_BUFFER_FAILED_AND_CHANNEL_LOST,
  CREATE_COMMAND_BUFFER_RESULT_LAST =
      CREATE_COMMAND_BUFFER_FAILED_AND_CHANNEL_LOST
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_RESULT_CODES_H_
