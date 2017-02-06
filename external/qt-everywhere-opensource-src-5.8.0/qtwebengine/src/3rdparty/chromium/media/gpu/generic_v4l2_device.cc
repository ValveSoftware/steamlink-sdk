// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "media/gpu/generic_v4l2_device.h"

#include <errno.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/gpu/generic_v4l2_device.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"

#if defined(USE_LIBV4L2)
// Auto-generated for dlopen libv4l2 libraries
#include "media/gpu/v4l2_stubs.h"
#include "third_party/v4l-utils/lib/include/libv4l2.h"

using media_gpu::kModuleV4l2;
using media_gpu::InitializeStubs;
using media_gpu::StubPathMap;

static const base::FilePath::CharType kV4l2Lib[] =
    FILE_PATH_LITERAL("/usr/lib/libv4l2.so");
#endif

namespace media {

namespace {
const char kDecoderDevice[] = "/dev/video-dec";
const char kEncoderDevice[] = "/dev/video-enc";
const char kImageProcessorDevice[] = "/dev/image-proc0";
const char kJpegDecoderDevice[] = "/dev/jpeg-dec";
}

GenericV4L2Device::GenericV4L2Device(Type type)
    : V4L2Device(type), use_libv4l2_(false) {}

GenericV4L2Device::~GenericV4L2Device() {
#if defined(USE_LIBV4L2)
  if (use_libv4l2_ && device_fd_.is_valid())
    v4l2_close(device_fd_.release());
#endif
}

int GenericV4L2Device::Ioctl(int request, void* arg) {
#if defined(USE_LIBV4L2)
  if (use_libv4l2_)
    return HANDLE_EINTR(v4l2_ioctl(device_fd_.get(), request, arg));
#endif
  return HANDLE_EINTR(ioctl(device_fd_.get(), request, arg));
}

bool GenericV4L2Device::Poll(bool poll_device, bool* event_pending) {
  struct pollfd pollfds[2];
  nfds_t nfds;
  int pollfd = -1;

  pollfds[0].fd = device_poll_interrupt_fd_.get();
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  if (poll_device) {
    DVLOG(3) << "Poll(): adding device fd to poll() set";
    pollfds[nfds].fd = device_fd_.get();
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
    pollfd = nfds;
    nfds++;
  }

  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    DPLOG(ERROR) << "poll() failed";
    return false;
  }
  *event_pending = (pollfd != -1 && pollfds[pollfd].revents & POLLPRI);
  return true;
}

void* GenericV4L2Device::Mmap(void* addr,
                              unsigned int len,
                              int prot,
                              int flags,
                              unsigned int offset) {
  return mmap(addr, len, prot, flags, device_fd_.get(), offset);
}

void GenericV4L2Device::Munmap(void* addr, unsigned int len) {
  munmap(addr, len);
}

bool GenericV4L2Device::SetDevicePollInterrupt() {
  DVLOG(3) << "SetDevicePollInterrupt()";

  const uint64_t buf = 1;
  if (HANDLE_EINTR(write(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    DPLOG(ERROR) << "SetDevicePollInterrupt(): write() failed";
    return false;
  }
  return true;
}

bool GenericV4L2Device::ClearDevicePollInterrupt() {
  DVLOG(3) << "ClearDevicePollInterrupt()";

  uint64_t buf;
  if (HANDLE_EINTR(read(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    if (errno == EAGAIN) {
      // No interrupt flag set, and we're reading nonblocking.  Not an error.
      return true;
    } else {
      DPLOG(ERROR) << "ClearDevicePollInterrupt(): read() failed";
      return false;
    }
  }
  return true;
}

bool GenericV4L2Device::Initialize() {
  const char* device_path = NULL;
  static bool v4l2_functions_initialized = PostSandboxInitialization();
  if (!v4l2_functions_initialized) {
    LOG(ERROR) << "Failed to initialize LIBV4L2 libs";
    return false;
  }

  switch (type_) {
    case kDecoder:
      device_path = kDecoderDevice;
      break;
    case kEncoder:
      device_path = kEncoderDevice;
      break;
    case kImageProcessor:
      device_path = kImageProcessorDevice;
      break;
    case kJpegDecoder:
      device_path = kJpegDecoderDevice;
      break;
  }

  DVLOG(2) << "Initialize(): opening device: " << device_path;
  // Open the video device.
  device_fd_.reset(
      HANDLE_EINTR(open(device_path, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd_.is_valid()) {
    return false;
  }
#if defined(USE_LIBV4L2)
  if (type_ == kEncoder &&
      HANDLE_EINTR(v4l2_fd_open(device_fd_.get(), V4L2_DISABLE_CONVERSION)) !=
          -1) {
    DVLOG(2) << "Using libv4l2 for " << device_path;
    use_libv4l2_ = true;
  }
#endif

  device_poll_interrupt_fd_.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
  if (!device_poll_interrupt_fd_.is_valid()) {
    return false;
  }
  return true;
}

std::vector<base::ScopedFD> GenericV4L2Device::GetDmabufsForV4L2Buffer(
    int index,
    size_t num_planes,
    enum v4l2_buf_type type) {
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(type));

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; ++i) {
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = type;
    expbuf.index = index;
    expbuf.plane = i;
    expbuf.flags = O_CLOEXEC;
    if (Ioctl(VIDIOC_EXPBUF, &expbuf) != 0) {
      dmabuf_fds.clear();
      break;
    }

    dmabuf_fds.push_back(base::ScopedFD(expbuf.fd));
  }

  return dmabuf_fds;
}

bool GenericV4L2Device::CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) {
  static uint32_t kEGLImageDrmFmtsSupported[] = {
    DRM_FORMAT_ARGB8888,
#if defined(ARCH_CPU_ARMEL)
    DRM_FORMAT_NV12,
#endif
  };

  return std::find(
             kEGLImageDrmFmtsSupported,
             kEGLImageDrmFmtsSupported + arraysize(kEGLImageDrmFmtsSupported),
             V4L2PixFmtToDrmFormat(v4l2_pixfmt)) !=
         kEGLImageDrmFmtsSupported + arraysize(kEGLImageDrmFmtsSupported);
}

EGLImageKHR GenericV4L2Device::CreateEGLImage(
    EGLDisplay egl_display,
    EGLContext /* egl_context */,
    GLuint texture_id,
    const gfx::Size& size,
    unsigned int buffer_index,
    uint32_t v4l2_pixfmt,
    const std::vector<base::ScopedFD>& dmabuf_fds) {
  DVLOG(3) << "CreateEGLImage()";
  if (!CanCreateEGLImageFrom(v4l2_pixfmt)) {
    LOG(ERROR) << "Unsupported V4L2 pixel format";
    return EGL_NO_IMAGE_KHR;
  }

  VideoPixelFormat vf_format = V4L2PixFmtToVideoPixelFormat(v4l2_pixfmt);
  // Number of components, as opposed to the number of V4L2 planes, which is
  // just a buffer count.
  size_t num_planes = VideoFrame::NumPlanes(vf_format);
  DCHECK_LE(num_planes, 3u);
  if (num_planes < dmabuf_fds.size()) {
    // It's possible for more than one DRM plane to reside in one V4L2 plane,
    // but not the other way around. We must use all V4L2 planes.
    LOG(ERROR) << "Invalid plane count";
    return EGL_NO_IMAGE_KHR;
  }

  std::vector<EGLint> attrs;
  attrs.push_back(EGL_WIDTH);
  attrs.push_back(size.width());
  attrs.push_back(EGL_HEIGHT);
  attrs.push_back(size.height());
  attrs.push_back(EGL_LINUX_DRM_FOURCC_EXT);
  attrs.push_back(V4L2PixFmtToDrmFormat(v4l2_pixfmt));

  // For existing formats, if we have less buffers (V4L2 planes) than
  // components (planes), the remaining planes are stored in the last
  // V4L2 plane. Use one V4L2 plane per each component until we run out of V4L2
  // planes, and use the last V4L2 plane for all remaining components, each
  // with an offset equal to the size of the preceding planes in the same
  // V4L2 plane.
  size_t v4l2_plane = 0;
  size_t plane_offset = 0;
  for (size_t plane = 0; plane < num_planes; ++plane) {
    attrs.push_back(EGL_DMA_BUF_PLANE0_FD_EXT + plane * 3);
    attrs.push_back(dmabuf_fds[v4l2_plane].get());
    attrs.push_back(EGL_DMA_BUF_PLANE0_OFFSET_EXT + plane * 3);
    attrs.push_back(plane_offset);
    attrs.push_back(EGL_DMA_BUF_PLANE0_PITCH_EXT + plane * 3);
    attrs.push_back(VideoFrame::RowBytes(plane, vf_format, size.width()));

    if (v4l2_plane + 1 < dmabuf_fds.size()) {
      ++v4l2_plane;
      plane_offset = 0;
    } else {
      plane_offset += VideoFrame::PlaneSize(vf_format, plane, size).GetArea();
    }
  }

  attrs.push_back(EGL_NONE);

  EGLImageKHR egl_image = eglCreateImageKHR(
      egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, &attrs[0]);
  if (egl_image == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "Failed creating EGL image: " << ui::GetLastEGLErrorString();
    return egl_image;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);

  return egl_image;
}

EGLBoolean GenericV4L2Device::DestroyEGLImage(EGLDisplay egl_display,
                                              EGLImageKHR egl_image) {
  return eglDestroyImageKHR(egl_display, egl_image);
}

GLenum GenericV4L2Device::GetTextureTarget() {
  return GL_TEXTURE_EXTERNAL_OES;
}

uint32_t GenericV4L2Device::PreferredInputFormat() {
  // TODO(posciak): We should support "dontcare" returns here once we
  // implement proper handling (fallback, negotiation) for this in users.
  CHECK_EQ(type_, kEncoder);
  return V4L2_PIX_FMT_NV12M;
}

// static
bool GenericV4L2Device::PostSandboxInitialization() {
#if defined(USE_LIBV4L2)
  StubPathMap paths;
  paths[kModuleV4l2].push_back(kV4l2Lib);

  return InitializeStubs(paths);
#else
  return true;
#endif
}

}  //  namespace media
