// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of ExynosV4L2Device used on
// Exynos platform.

#ifndef CONTENT_COMMON_GPU_MEDIA_EXYNOS_V4L2_VIDEO_DEVICE_H_
#define CONTENT_COMMON_GPU_MEDIA_EXYNOS_V4L2_VIDEO_DEVICE_H_

#include "content/common/gpu/media/v4l2_video_device.h"

namespace content {

class ExynosV4L2Device : public V4L2Device {
 public:
  explicit ExynosV4L2Device(Type type);
  virtual ~ExynosV4L2Device();

  // V4L2Device implementation.
  virtual int Ioctl(int request, void* arg) OVERRIDE;
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

  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  int device_poll_interrupt_fd_;

  DISALLOW_COPY_AND_ASSIGN(ExynosV4L2Device);
};
}  //  namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_EXYNOS_V4L2_VIDEO_DEVICE_H_
