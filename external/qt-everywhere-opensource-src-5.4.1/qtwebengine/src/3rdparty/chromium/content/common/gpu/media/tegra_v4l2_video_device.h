// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of TegraV4L2Device used on
// Tegra platform.

#ifndef CONTENT_COMMON_GPU_MEDIA_TEGRA_V4L2_VIDEO_DEVICE_H_
#define CONTENT_COMMON_GPU_MEDIA_TEGRA_V4L2_VIDEO_DEVICE_H_

#include "content/common/gpu/media/v4l2_video_device.h"
#include "ui/gl/gl_bindings.h"

namespace content {

// This class implements the V4L2Device interface for Tegra platform.
// It interfaces with libtegrav4l2 library which provides API that exhibit the
// V4L2 specification via the library API instead of system calls.
class TegraV4L2Device : public V4L2Device {
 public:
  explicit TegraV4L2Device(Type type);
  virtual ~TegraV4L2Device();

  virtual int Ioctl(int flags, void* arg) OVERRIDE;
  virtual bool Poll(bool poll_device, bool* event_pending) OVERRIDE;
  virtual bool SetDevicePollInterrupt() OVERRIDE;
  virtual bool ClearDevicePollInterrupt() OVERRIDE;
  virtual void* Mmap(void* addr,
                     unsigned int len,
                     int prot,
                     int flags,
                     unsigned int offset) OVERRIDE;
  virtual void Munmap(void* addr, unsigned int len) OVERRIDE;
  virtual bool Initialize() OVERRIDE;
  virtual EGLImageKHR CreateEGLImage(EGLDisplay egl_display,
                                     EGLContext egl_context,
                                     GLuint texture_id,
                                     gfx::Size frame_buffer_size,
                                     unsigned int buffer_index,
                                     size_t planes_count) OVERRIDE;
  virtual EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                                     EGLImageKHR egl_image) OVERRIDE;
  virtual GLenum GetTextureTarget() OVERRIDE;
  virtual uint32 PreferredInputFormat() OVERRIDE;
  virtual uint32 PreferredOutputFormat() OVERRIDE;

 private:
  const Type type_;

  // The actual device fd.
  int device_fd_;

  DISALLOW_COPY_AND_ASSIGN(TegraV4L2Device);
};

}  //  namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_TEGRA_V4L2_VIDEO_DEVICE_H_
