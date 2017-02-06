// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the V4L2Device interface which is used by the
// V4L2DecodeAccelerator class to delegate/pass the device specific
// handling of any of the functionalities.

#ifndef MEDIA_GPU_V4L2_DEVICE_H_
#define MEDIA_GPU_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <linux/videodev2.h>

#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

// TODO(posciak): remove this once V4L2 headers are updated.
#define V4L2_PIX_FMT_VP9 v4l2_fourcc('V', 'P', '9', '0')
#define V4L2_PIX_FMT_H264_SLICE v4l2_fourcc('S', '2', '6', '4')
#define V4L2_PIX_FMT_VP8_FRAME v4l2_fourcc('V', 'P', '8', 'F')
#define V4L2_PIX_FMT_MT21 v4l2_fourcc('M', 'T', '2', '1')

namespace media {

class MEDIA_GPU_EXPORT V4L2Device
    : public base::RefCountedThreadSafe<V4L2Device> {
 public:
  // Utility format conversion functions
  static VideoPixelFormat V4L2PixFmtToVideoPixelFormat(uint32_t format);
  static uint32_t VideoPixelFormatToV4L2PixFmt(VideoPixelFormat format);
  static uint32_t VideoCodecProfileToV4L2PixFmt(VideoCodecProfile profile,
                                                bool slice_based);
  static uint32_t V4L2PixFmtToDrmFormat(uint32_t format);
  // Convert format requirements requested by a V4L2 device to gfx::Size.
  static gfx::Size CodedSizeFromV4L2Format(struct v4l2_format format);

  enum Type {
    kDecoder,
    kEncoder,
    kImageProcessor,
    kJpegDecoder,
  };

  // Creates and initializes an appropriate V4L2Device of |type| for the
  // current platform and returns a scoped_refptr<V4L2Device> on success, or
  // NULL.
  static scoped_refptr<V4L2Device> Create(Type type);

  // Parameters and return value are the same as for the standard ioctl() system
  // call.
  virtual int Ioctl(int request, void* arg) = 0;

  // This method sleeps until either:
  // - SetDevicePollInterrupt() is called (on another thread),
  // - |poll_device| is true, and there is new data to be read from the device,
  //   or an event from the device has arrived; in the latter case
  //   |*event_pending| will be set to true.
  // Returns false on error, true otherwise.
  // This method should be called from a separate thread.
  virtual bool Poll(bool poll_device, bool* event_pending) = 0;

  // These methods are used to interrupt the thread sleeping on Poll() and force
  // it to return regardless of device state, which is usually when the client
  // is no longer interested in what happens with the device (on cleanup,
  // client state change, etc.). When SetDevicePollInterrupt() is called, Poll()
  // will return immediately, and any subsequent calls to it will also do so
  // until ClearDevicePollInterrupt() is called.
  virtual bool SetDevicePollInterrupt() = 0;
  virtual bool ClearDevicePollInterrupt() = 0;

  // Wrappers for standard mmap/munmap system calls.
  virtual void* Mmap(void* addr,
                     unsigned int len,
                     int prot,
                     int flags,
                     unsigned int offset) = 0;
  virtual void Munmap(void* addr, unsigned int len) = 0;

  // Initializes the V4L2Device to operate as a device of |type|.
  // Returns true on success.
  virtual bool Initialize() = 0;

  // Return a vector of dmabuf file descriptors, exported for V4L2 buffer with
  // |index|, assuming the buffer contains |num_planes| V4L2 planes and is of
  // |type|. Return an empty vector on failure.
  // The caller is responsible for closing the file descriptors after use.
  virtual std::vector<base::ScopedFD> GetDmabufsForV4L2Buffer(
      int index,
      size_t num_planes,
      enum v4l2_buf_type type) = 0;

  // Return true if the given V4L2 pixfmt can be used in CreateEGLImage()
  // for the current platform.
  virtual bool CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) = 0;

  // Create an EGLImage from provided |dmabuf_fds| and bind |texture_id| to it.
  // Some implementations may also require the V4L2 |buffer_index| of the buffer
  // for which |dmabuf_fds| have been exported.
  // The caller may choose to close the file descriptors after this method
  // returns, and may expect the buffers to remain valid for the lifetime of
  // the created EGLImage.
  // Return EGL_NO_IMAGE_KHR on failure.
  virtual EGLImageKHR CreateEGLImage(
      EGLDisplay egl_display,
      EGLContext egl_context,
      GLuint texture_id,
      const gfx::Size& size,
      unsigned int buffer_index,
      uint32_t v4l2_pixfmt,
      const std::vector<base::ScopedFD>& dmabuf_fds) = 0;

  // Destroys the EGLImageKHR.
  virtual EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                                     EGLImageKHR egl_image) = 0;

  // Returns the supported texture target for the V4L2Device.
  virtual GLenum GetTextureTarget() = 0;

  // Returns the preferred V4L2 input format or 0 if don't care.
  virtual uint32_t PreferredInputFormat() = 0;

  // Get minimum and maximum resolution for fourcc |pixelformat| and store to
  // |min_resolution| and |max_resolution|.
  void GetSupportedResolution(uint32_t pixelformat,
                              gfx::Size* min_resolution,
                              gfx::Size* max_resolution);

  // Return supported profiles for decoder, including only profiles for given
  // fourcc |pixelformats|.
  VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
      const size_t num_formats,
      const uint32_t pixelformats[]);

  // Return true if the device supports |profile|, taking into account only
  // fourccs from the given array of |pixelformats| of size |num_formats|.
  bool SupportsDecodeProfileForV4L2PixelFormats(VideoCodecProfile profile,
                                                const size_t num_formats,
                                                const uint32_t pixelformats[]);

 protected:
  friend class base::RefCountedThreadSafe<V4L2Device>;
  explicit V4L2Device(Type type);
  virtual ~V4L2Device();

  const Type type_;
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_DEVICE_H_
