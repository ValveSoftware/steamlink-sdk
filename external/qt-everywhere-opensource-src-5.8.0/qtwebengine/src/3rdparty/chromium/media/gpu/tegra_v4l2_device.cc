// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/videodev2.h>

#include "base/lazy_instance.h"
#include "base/posix/eintr_wrapper.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/tegra_v4l2_device.h"
#include "ui/gl/gl_bindings.h"

namespace media {

namespace {
const char kDecoderDevice[] = "/dev/tegra_avpchannel";
const char kEncoderDevice[] = "/dev/nvhost-msenc";
}

typedef int32_t (*TegraV4L2Open)(const char* name, int32_t flags);
typedef int32_t (*TegraV4L2Close)(int32_t fd);
typedef int32_t (*TegraV4L2Ioctl)(int32_t fd, unsigned long cmd, ...);
typedef int32_t (*TegraV4L2Poll)(int32_t fd,
                                 bool poll_device,
                                 bool* event_pending);
typedef int32_t (*TegraV4L2SetDevicePollInterrupt)(int32_t fd);
typedef int32_t (*TegraV4L2ClearDevicePollInterrupt)(int32_t fd);
typedef void* (*TegraV4L2Mmap)(void* addr,
                               size_t length,
                               int prot,
                               int flags,
                               int fd,
                               unsigned int offset);
typedef int32_t (*TegraV4L2Munmap)(void* addr, size_t length);
typedef int32_t (*TegraV4L2UseEglImage)(int fd,
                                        unsigned int buffer_index,
                                        void* egl_image);

#define TEGRAV4L2_SYM(name) TegraV4L2##name TegraV4L2_##name = NULL

TEGRAV4L2_SYM(Open);
TEGRAV4L2_SYM(Close);
TEGRAV4L2_SYM(Ioctl);
TEGRAV4L2_SYM(Poll);
TEGRAV4L2_SYM(SetDevicePollInterrupt);
TEGRAV4L2_SYM(ClearDevicePollInterrupt);
TEGRAV4L2_SYM(Mmap);
TEGRAV4L2_SYM(Munmap);
TEGRAV4L2_SYM(UseEglImage);

#undef TEGRAV4L2_SYM

class TegraFunctionSymbolFinder {
 public:
  TegraFunctionSymbolFinder() : initialized_(false) {
    if (!dlopen("/usr/lib/libtegrav4l2.so",
                RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE)) {
      DLOG(ERROR) << "Failed to load libtegrav4l2.so";
      return;
    }
#define TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(name)          \
  do {                                                    \
    TegraV4L2_##name = reinterpret_cast<TegraV4L2##name>( \
        dlsym(RTLD_DEFAULT, "TegraV4L2_" #name));         \
    if (TegraV4L2_##name == NULL) {                       \
      LOG(ERROR) << "Failed to dlsym TegraV4L2_" #name;   \
      return;                                             \
    }                                                     \
  } while (0)

    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(Open);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(Close);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(Ioctl);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(Poll);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(SetDevicePollInterrupt);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(ClearDevicePollInterrupt);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(Mmap);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(Munmap);
    TEGRAV4L2_DLSYM_OR_RETURN_ON_ERROR(UseEglImage);
#undef TEGRAV4L2_DLSYM
    initialized_ = true;
  }

  bool initialized() { return initialized_; }

 private:
  bool initialized_;
};

base::LazyInstance<TegraFunctionSymbolFinder> g_tegra_function_symbol_finder_ =
    LAZY_INSTANCE_INITIALIZER;

TegraV4L2Device::TegraV4L2Device(Type type)
    : V4L2Device(type), device_fd_(-1) {}

TegraV4L2Device::~TegraV4L2Device() {
  if (device_fd_ != -1) {
    TegraV4L2_Close(device_fd_);
    device_fd_ = -1;
  }
}

int TegraV4L2Device::Ioctl(int flags, void* arg) {
  return HANDLE_EINTR(TegraV4L2_Ioctl(device_fd_, flags, arg));
}

bool TegraV4L2Device::Poll(bool poll_device, bool* event_pending) {
  if (HANDLE_EINTR(TegraV4L2_Poll(device_fd_, poll_device, event_pending)) ==
      -1) {
    LOG(ERROR) << "TegraV4L2Poll returned -1 ";
    return false;
  }
  return true;
}

void* TegraV4L2Device::Mmap(void* addr,
                            unsigned int len,
                            int prot,
                            int flags,
                            unsigned int offset) {
  return TegraV4L2_Mmap(addr, len, prot, flags, device_fd_, offset);
}

void TegraV4L2Device::Munmap(void* addr, unsigned int len) {
  TegraV4L2_Munmap(addr, len);
}

bool TegraV4L2Device::SetDevicePollInterrupt() {
  if (HANDLE_EINTR(TegraV4L2_SetDevicePollInterrupt(device_fd_)) == -1) {
    LOG(ERROR) << "Error in calling TegraV4L2SetDevicePollInterrupt";
    return false;
  }
  return true;
}

bool TegraV4L2Device::ClearDevicePollInterrupt() {
  if (HANDLE_EINTR(TegraV4L2_ClearDevicePollInterrupt(device_fd_)) == -1) {
    LOG(ERROR) << "Error in calling TegraV4L2ClearDevicePollInterrupt";
    return false;
  }
  return true;
}

bool TegraV4L2Device::Initialize() {
  const char* device_path = NULL;
  switch (type_) {
    case kDecoder:
      device_path = kDecoderDevice;
      break;
    case kEncoder:
      device_path = kEncoderDevice;
      break;
    default:
      DVLOG(1) << "Device type " << type_ << " not supported on this platform";
      return false;
  }

  if (!g_tegra_function_symbol_finder_.Get().initialized()) {
    DLOG(ERROR) << "Unable to initialize functions";
    return false;
  }
  device_fd_ = HANDLE_EINTR(
      TegraV4L2_Open(device_path, O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (device_fd_ == -1) {
    DLOG(ERROR) << "Unable to open device " << device_path;
    return false;
  }
  return true;
}

std::vector<base::ScopedFD> TegraV4L2Device::GetDmabufsForV4L2Buffer(
    int /* index */,
    size_t num_planes,
    enum v4l2_buf_type /* type */) {
  std::vector<base::ScopedFD> dmabuf_fds;
  // Tegra does not actually provide dmabuf fds currently. Fill the vector with
  // invalid descriptors to prevent the caller from failing on an empty vector
  // being returned. TegraV4L2Device::CreateEGLImage() will ignore the invalid
  // descriptors and create images based on V4L2 index passed to it.
  dmabuf_fds.resize(num_planes);
  return dmabuf_fds;
}

bool TegraV4L2Device::CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) {
  return v4l2_pixfmt == V4L2_PIX_FMT_NV12M;
}

EGLImageKHR TegraV4L2Device::CreateEGLImage(
    EGLDisplay egl_display,
    EGLContext egl_context,
    GLuint texture_id,
    const gfx::Size& /* size */,
    unsigned int buffer_index,
    uint32_t v4l2_pixfmt,
    const std::vector<base::ScopedFD>& /* dmabuf_fds */) {
  DVLOG(3) << "CreateEGLImage()";
  if (!CanCreateEGLImageFrom(v4l2_pixfmt)) {
    LOG(ERROR) << "Unsupported V4L2 pixel format";
    return EGL_NO_IMAGE_KHR;
  }

  EGLint attr = EGL_NONE;
  EGLImageKHR egl_image =
      eglCreateImageKHR(egl_display, egl_context, EGL_GL_TEXTURE_2D_KHR,
                        reinterpret_cast<EGLClientBuffer>(texture_id), &attr);
  if (egl_image == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "Unable to create EGL image";
    return egl_image;
  }
  if (TegraV4L2_UseEglImage(device_fd_, buffer_index, egl_image) != 0) {
    LOG(ERROR) << "Unable to use EGL image";
    eglDestroyImageKHR(egl_display, egl_image);
    egl_image = EGL_NO_IMAGE_KHR;
  }
  return egl_image;
}

EGLBoolean TegraV4L2Device::DestroyEGLImage(EGLDisplay egl_display,
                                            EGLImageKHR egl_image) {
  return eglDestroyImageKHR(egl_display, egl_image);
}

GLenum TegraV4L2Device::GetTextureTarget() {
  return GL_TEXTURE_2D;
}

uint32_t TegraV4L2Device::PreferredInputFormat() {
  // TODO(posciak): We should support "dontcare" returns here once we
  // implement proper handling (fallback, negotiation) for this in users.
  CHECK_EQ(type_, kEncoder);
  return V4L2_PIX_FMT_YUV420M;
}

}  //  namespace media
