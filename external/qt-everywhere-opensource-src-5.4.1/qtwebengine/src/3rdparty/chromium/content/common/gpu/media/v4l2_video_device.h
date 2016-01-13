// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the V4L2Device interface which is used by the
// V4L2DecodeAccelerator class to delegate/pass the device specific
// handling of any of the functionalities.

#ifndef CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_DEVICE_H_
#define CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_DEVICE_H_

#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

namespace content {

class V4L2Device {
 public:
  // Utility format conversion functions
  static media::VideoFrame::Format V4L2PixFmtToVideoFrameFormat(uint32 format);
  static uint32 VideoFrameFormatToV4L2PixFmt(media::VideoFrame::Format format);
  static uint32 VideoCodecProfileToV4L2PixFmt(media::VideoCodecProfile profile);
  // Convert format requirements requested by a V4L2 device to gfx::Size.
  static gfx::Size CodedSizeFromV4L2Format(struct v4l2_format format);

  virtual ~V4L2Device();

  enum Type {
    kDecoder,
    kEncoder,
    kImageProcessor,
  };

  // Creates and initializes an appropriate V4L2Device of |type| for the
  // current platform and returns a scoped_ptr<V4L2Device> on success, or NULL.
  static scoped_ptr<V4L2Device> Create(Type type);

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

  // Creates an EGLImageKHR since each V4L2Device may use a different method of
  // acquiring one and associating it to the given texture. The texture_id is
  // used to bind the texture to the returned EGLImageKHR. buffer_index can be
  // used to associate the returned EGLImageKHR by the underlying V4L2Device
  // implementation.
  virtual EGLImageKHR CreateEGLImage(EGLDisplay egl_display,
                                     EGLContext egl_context,
                                     GLuint texture_id,
                                     gfx::Size frame_buffer_size,
                                     unsigned int buffer_index,
                                     size_t planes_count) = 0;

  // Destroys the EGLImageKHR.
  virtual EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                                     EGLImageKHR egl_image) = 0;

  // Returns the supported texture target for the V4L2Device.
  virtual GLenum GetTextureTarget() = 0;

  // Returns the preferred V4L2 input format or 0 if don't care.
  virtual uint32 PreferredInputFormat() = 0;

  // Returns the preferred V4L2 output format or 0 if don't care.
  virtual uint32 PreferredOutputFormat() = 0;
};

}  //  namespace content

#endif  //  CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_DEVICE_H_
