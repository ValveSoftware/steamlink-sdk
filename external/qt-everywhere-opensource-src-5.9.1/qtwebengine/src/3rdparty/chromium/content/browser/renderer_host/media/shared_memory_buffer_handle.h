// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SHARED_MEMORY_BUFFER_HANDLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SHARED_MEMORY_BUFFER_HANDLE_H_

#include "media/capture/video/video_capture_buffer_handle.h"

namespace content {

class SharedMemoryBufferTracker;

// A simple proxy that implements the BufferHandle interface, providing
// accessors to SharedMemTracker's memory and properties.
class SharedMemoryBufferHandle : public media::VideoCaptureBufferHandle {
 public:
  // |tracker| must outlive SimpleBufferHandle. This is ensured since a
  // tracker is pinned until ownership of this SimpleBufferHandle is returned
  // to VideoCaptureBufferPool.
  explicit SharedMemoryBufferHandle(SharedMemoryBufferTracker* tracker);
  ~SharedMemoryBufferHandle() override;

  gfx::Size dimensions() const override;
  size_t mapped_size() const override;
  void* data(int plane) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  base::FileDescriptor AsPlatformFile() override;
#endif
  bool IsBackedByVideoFrame() const override;
  scoped_refptr<media::VideoFrame> GetVideoFrame() override;

 private:
  SharedMemoryBufferTracker* const tracker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SHARED_MEMORY_BUFFER_HANDLE_H_
