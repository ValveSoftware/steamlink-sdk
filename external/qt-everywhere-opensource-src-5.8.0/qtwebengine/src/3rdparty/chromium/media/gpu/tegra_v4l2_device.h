// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of TegraV4L2Device used on
// Tegra platform.

#ifndef MEDIA_GPU_TEGRA_V4L2_DEVICE_H_
#define MEDIA_GPU_TEGRA_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "media/gpu/v4l2_device.h"
#include "ui/gl/gl_bindings.h"

namespace media {

// This class implements the V4L2Device interface for Tegra platform.
// It interfaces with libtegrav4l2 library which provides API that exhibit the
// V4L2 specification via the library API instead of system calls.
class TegraV4L2Device : public V4L2Device {
 public:
  explicit TegraV4L2Device(Type type);

  int Ioctl(int flags, void* arg) override;
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
  ~TegraV4L2Device() override;

  // The actual device fd.
  int device_fd_;

  DISALLOW_COPY_AND_ASSIGN(TegraV4L2Device);
};

}  //  namespace media

#endif  // MEDIA_GPU_TEGRA_V4L2_DEVICE_H_
