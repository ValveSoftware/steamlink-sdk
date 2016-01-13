// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BITSTREAM_BUFFER_H_
#define MEDIA_BASE_BITSTREAM_BUFFER_H_

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"

namespace media {

// Class for passing bitstream buffers around.  Does not take ownership of the
// data.  This is the media-namespace equivalent of PP_VideoBitstreamBuffer_Dev.
class BitstreamBuffer {
 public:
  BitstreamBuffer(int32 id, base::SharedMemoryHandle handle, size_t size)
      : id_(id),
        handle_(handle),
        size_(size) {
  }

  int32 id() const { return id_; }
  base::SharedMemoryHandle handle() const { return handle_; }
  size_t size() const { return size_; }

 private:
  int32 id_;
  base::SharedMemoryHandle handle_;
  size_t size_;

  // Allow compiler-generated copy & assign constructors.
};

}  // namespace media

#endif  // MEDIA_BASE_BITSTREAM_BUFFER_H_
