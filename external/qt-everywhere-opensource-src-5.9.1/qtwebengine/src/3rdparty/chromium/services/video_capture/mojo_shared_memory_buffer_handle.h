// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOJO_SHARED_MEMORY_BUFFER_HANDLE_H_
#define SERVICES_VIDEO_CAPTURE_MOJO_SHARED_MEMORY_BUFFER_HANDLE_H_

#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

namespace video_capture {

class MojoSharedMemoryBufferTracker;

class MojoSharedMemoryBufferHandle : public media::VideoCaptureBufferHandle {
 public:
  MojoSharedMemoryBufferHandle(MojoSharedMemoryBufferTracker* tracker);
  ~MojoSharedMemoryBufferHandle() override;

  gfx::Size dimensions() const override;
  size_t mapped_size() const override;
  void* data(int plane) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  base::FileDescriptor AsPlatformFile() override;
#endif
  bool IsBackedByVideoFrame() const override;
  scoped_refptr<media::VideoFrame> GetVideoFrame() override;

 private:
  MojoSharedMemoryBufferTracker* const tracker_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOJO_SHARED_MEMORY_BUFFER_HANDLE_H_
