// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/videodev2.h>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/posix/eintr_wrapper.h"
#include "content/common/gpu/media/tegra_v4l2_video_device.h"
#include "ui/gl/gl_bindings.h"

namespace content {

namespace {
const char kDecoderDevice[] = "/dev/tegra_avpchannel";
const char kEncoderDevice[] = "/dev/nvhost-msenc";
}

typedef int32 (*TegraV4L2Open)(const char* name, int32 flags);
typedef int32 (*TegraV4L2Close)(int32 fd);
typedef int32 (*TegraV4L2Ioctl)(int32 fd, unsigned long cmd, ...);
typedef int32 (*TegraV4L2Poll)(int32 fd, bool poll_device, bool* event_pending);
typedef int32 (*TegraV4L2SetDevicePollInterrupt)(int32 fd);
typedef int32 (*TegraV4L2ClearDevicePollInterrupt)(int32 fd);
typedef void* (*TegraV4L2Mmap)(void* addr,
                               size_t length,
                               int prot,
                               int flags,
                               int fd,
                               unsigned int offset);
typedef int32 (*TegraV4L2Munmap)(void* addr, size_t length);
typedef int32 (*TegraV4L2UseEglImage)(int fd,
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
      DLOG(ERROR) << "Failed to load libtegrav4l2.so ";
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
    : type_(type),
      device_fd_(-1) {
}

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
    DLOG(ERROR) << "TegraV4L2Poll returned -1 ";
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
    DLOG(ERROR) << "Error in calling TegraV4L2SetDevicePollInterrupt";
    return false;
  }
  return true;
}

bool TegraV4L2Device::ClearDevicePollInterrupt() {
  if (HANDLE_EINTR(TegraV4L2_ClearDevicePollInterrupt(device_fd_)) == -1) {
    DLOG(ERROR) << "Error in calling TegraV4L2ClearDevicePollInterrupt";
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
    case kImageProcessor:
      DVLOG(1) << "Device type " << type_ << " not supported on this platform";
      return false;
  }

  if (!g_tegra_function_symbol_finder_.Get().initialized()) {
    DLOG(ERROR) << "Unable to initialize functions ";
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

EGLImageKHR TegraV4L2Device::CreateEGLImage(EGLDisplay egl_display,
                                            EGLContext egl_context,
                                            GLuint texture_id,
                                            gfx::Size /* frame_buffer_size */,
                                            unsigned int buffer_index,
                                            size_t /* planes_count */) {
  DVLOG(3) << "CreateEGLImage()";
  EGLint attr = EGL_NONE;
  EGLImageKHR egl_image =
      eglCreateImageKHR(egl_display,
                        egl_context,
                        EGL_GL_TEXTURE_2D_KHR,
                        reinterpret_cast<EGLClientBuffer>(texture_id),
                        &attr);
  if (egl_image == EGL_NO_IMAGE_KHR) {
    return egl_image;
  }
  if (TegraV4L2_UseEglImage(device_fd_, buffer_index, egl_image) != 0) {
    eglDestroyImageKHR(egl_display, egl_image);
    egl_image = EGL_NO_IMAGE_KHR;
  }
  return egl_image;
}

EGLBoolean TegraV4L2Device::DestroyEGLImage(EGLDisplay egl_display,
                                            EGLImageKHR egl_image) {
  return eglDestroyImageKHR(egl_display, egl_image);
}

GLenum TegraV4L2Device::GetTextureTarget() { return GL_TEXTURE_2D; }

uint32 TegraV4L2Device::PreferredInputFormat() {
  // TODO(posciak): We should support "dontcare" returns here once we
  // implement proper handling (fallback, negotiation) for this in users.
  CHECK_EQ(type_, kEncoder);
  return V4L2_PIX_FMT_YUV420M;
}

uint32 TegraV4L2Device::PreferredOutputFormat() {
  // TODO(posciak): We should support "dontcare" returns here once we
  // implement proper handling (fallback, negotiation) for this in users.
  CHECK_EQ(type_, kDecoder);
  return V4L2_PIX_FMT_NV12M;
}

}  //  namespace content
