// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of GenericV4L2Device used on
// platforms, which provide generic V4L2 video codec devices.

#ifndef MEDIA_GPU_GENERIC_V4L2_DEVICE_H_
#define MEDIA_GPU_GENERIC_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "media/gpu/v4l2_device.h"

namespace media {

class GenericV4L2Device : public V4L2Device {
 public:
  explicit GenericV4L2Device(Type type);

  // V4L2Device implementation.
  int Ioctl(int request, void* arg) override;
  bool Poll(bool poll_device, bool* event_pending) override;
  bool SetDevicePollInterrupt() override;
  bool ClearDevicePollInterrupt() override;
  void* Mmap(void* addr,
             unsigned int len,
             int prot,
             int flags,
             unsigned int offset) override;
  void Munmap(void* addr, unsigned int len) override;
  bool Initialize() override;

  std::vector<base::ScopedFD> GetDmabufsForV4L2Buffer(
      int index,
      size_t num_planes,
      enum v4l2_buf_type type) override;

  bool CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) override;
  EGLImageKHR CreateEGLImage(
      EGLDisplay egl_display,
      EGLContext egl_context,
      GLuint texture_id,
      const gfx::Size& size,
      unsigned int buffer_index,
      uint32_t v4l2_pixfmt,
      const std::vector<base::ScopedFD>& dmabuf_fds) override;

  EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                             EGLImageKHR egl_image) override;
  GLenum GetTextureTarget() override;
  uint32_t PreferredInputFormat() override;

 private:
  ~GenericV4L2Device() override;

  // The actual device fd.
  base::ScopedFD device_fd_;

  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  base::ScopedFD device_poll_interrupt_fd_;

  // Use libv4l2 when operating |device_fd_|.
  bool use_libv4l2_;

  DISALLOW_COPY_AND_ASSIGN(GenericV4L2Device);

  // Lazily initialize static data after sandbox is enabled.  Return false on
  // init failure.
  static bool PostSandboxInitialization();
};
}  //  namespace media

#endif  // MEDIA_GPU_GENERIC_V4L2_DEVICE_H_
