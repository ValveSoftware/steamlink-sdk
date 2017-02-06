// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_MEDIA_SHARED_MEMORY_CHUNK_H_
#define CHROMECAST_COMMON_MEDIA_SHARED_MEMORY_CHUNK_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"

namespace base {
class SharedMemory;
}

namespace chromecast {
namespace media {

class SharedMemoryChunk : public MediaMemoryChunk {
 public:
  SharedMemoryChunk(std::unique_ptr<base::SharedMemory> shared_mem,
                    size_t size);
  ~SharedMemoryChunk() override;

  // MediaMemoryChunk implementation.
  void* data() const override;
  size_t size() const override;
  bool valid() const override;

 private:
  std::unique_ptr<base::SharedMemory> shared_mem_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryChunk);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_MEDIA_SHARED_MEMORY_CHUNK_H_
