// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_MEDIA_MEMORY_CHUNK_H_
#define CHROMECAST_MEDIA_CMA_IPC_MEDIA_MEMORY_CHUNK_H_

#include <stddef.h>

namespace chromecast {
namespace media {

// MediaMemoryChunk represents a block of memory without doing any assumption
// about the type of memory (e.g. shared memory) nor about the underlying
// memory ownership.
// The block of memory can be invalidated under the cover (e.g. if the derived
// class does not own the underlying memory),
// in that case, MediaMemoryChunk::valid() will return false.
class MediaMemoryChunk {
 public:
  virtual ~MediaMemoryChunk();

  // Returns the start of the block of memory.
  virtual void* data() const = 0;

  // Returns the size of the block of memory.
  virtual size_t size() const = 0;

  // Returns whether the underlying block of memory is valid.
  // Since MediaMemoryChunk does not specify a memory ownership model,
  // the underlying block of memory might be invalidated by a third party.
  // In that case, valid() will return false.
  virtual bool valid() const = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_MEDIA_MEMORY_CHUNK_H_
