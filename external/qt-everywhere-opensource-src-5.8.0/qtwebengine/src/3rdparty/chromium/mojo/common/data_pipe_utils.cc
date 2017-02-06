// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/data_pipe_utils.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner_util.h"

namespace mojo {
namespace common {
namespace {

bool BlockingCopyHelper(ScopedDataPipeConsumerHandle source,
    const base::Callback<size_t(const void*, uint32_t)>& write_bytes) {
  for (;;) {
    const void* buffer;
    uint32_t num_bytes;
    MojoResult result = BeginReadDataRaw(
        source.get(), &buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      size_t bytes_written = write_bytes.Run(buffer, num_bytes);
      result = EndReadDataRaw(source.get(), num_bytes);
      if (bytes_written < num_bytes || result != MOJO_RESULT_OK)
        return false;
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      result = Wait(source.get(),
                    MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE,
                    nullptr);
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

size_t CopyToStringHelper(
    std::string* result, const void* buffer, uint32_t num_bytes) {
  result->append(static_cast<const char*>(buffer), num_bytes);
  return num_bytes;
}

size_t CopyToFileHelper(FILE* fp, const void* buffer, uint32_t num_bytes) {
  return fwrite(buffer, 1, num_bytes, fp);
}

} // namespace


// TODO(hansmuller): Add a max_size parameter.
bool BlockingCopyToString(ScopedDataPipeConsumerHandle source,
                          std::string* result) {
  CHECK(result);
  result->clear();
  return BlockingCopyHelper(std::move(source),
                            base::Bind(&CopyToStringHelper, result));
}

bool MOJO_COMMON_EXPORT BlockingCopyFromString(
    const std::string& source,
    const ScopedDataPipeProducerHandle& destination) {
  auto it = source.begin();
  for (;;) {
    void* buffer = nullptr;
    uint32_t buffer_num_bytes = 0;
    MojoResult result =
        BeginWriteDataRaw(destination.get(), &buffer, &buffer_num_bytes,
                          MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      char* char_buffer = static_cast<char*>(buffer);
      uint32_t byte_index = 0;
      while (it != source.end() && byte_index < buffer_num_bytes) {
        char_buffer[byte_index++] = *it++;
      }
      EndWriteDataRaw(destination.get(), byte_index);
      if (it == source.end())
        return true;
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      result = Wait(destination.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                    MOJO_DEADLINE_INDEFINITE, nullptr);
      if (result != MOJO_RESULT_OK) {
        // If the consumer handle was closed, then treat as EOF.
        return result == MOJO_RESULT_FAILED_PRECONDITION;
      }
    } else {
      // If the consumer handle was closed, then treat as EOF.
      return result == MOJO_RESULT_FAILED_PRECONDITION;
    }
  }
}

bool BlockingCopyToFile(ScopedDataPipeConsumerHandle source,
                        const base::FilePath& destination) {
  base::ScopedFILE fp(base::OpenFile(destination, "wb"));
  if (!fp)
    return false;
  return BlockingCopyHelper(std::move(source),
                            base::Bind(&CopyToFileHelper, fp.get()));
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
