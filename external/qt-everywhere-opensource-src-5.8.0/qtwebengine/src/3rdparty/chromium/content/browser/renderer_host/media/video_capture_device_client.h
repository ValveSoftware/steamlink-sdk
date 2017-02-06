// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "media/capture/video/video_capture_device.h"

namespace content {
class VideoCaptureBufferPool;
class VideoCaptureController;
class VideoCaptureGpuJpegDecoder;

// Receives events from the VideoCaptureDevice and posts them to a |controller_|
// on the IO thread. An instance of this class may safely outlive its target
// VideoCaptureController. This is a shallow class meant to convert incoming
// frames and holds no significant state.
//
// Methods of this class may be called from any thread, and in practice will
// often be called on some auxiliary thread depending on the platform and the
// device type; including, for example, the DirectShow thread on Windows, the
// v4l2_thread on Linux, and the UI thread for tab capture.
//
// It has an internal ref counted TextureWrapHelper class used to wrap incoming
// GpuMemoryBuffers into Texture backed VideoFrames. This class creates and
// manages the necessary entities to interact with the GPU process, notably an
// offscreen Context to avoid janking the UI thread.
class CONTENT_EXPORT VideoCaptureDeviceClient
    : public media::VideoCaptureDevice::Client,
      public base::SupportsWeakPtr<VideoCaptureDeviceClient> {
 public:
  VideoCaptureDeviceClient(
      const base::WeakPtr<VideoCaptureController>& controller,
      const scoped_refptr<VideoCaptureBufferPool>& buffer_pool);
  ~VideoCaptureDeviceClient() override;

  // VideoCaptureDevice::Client implementation.
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const media::VideoCaptureFormat& frame_format,
                              int rotation,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp) override;
  std::unique_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override;
  void OnIncomingCapturedBuffer(std::unique_ptr<Buffer> buffer,
                                const media::VideoCaptureFormat& frame_format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override;
  void OnIncomingCapturedVideoFrame(
      std::unique_ptr<Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame) override;
  std::unique_ptr<Buffer> ResurrectLastOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override;
  void OnError(const tracked_objects::Location& from_here,
               const std::string& reason) override;
  void OnLog(const std::string& message) override;
  double GetBufferPoolUtilization() const override;

 private:
  // Reserve output buffer into which I420 contents can be copied directly.
  // The dimensions of the frame is described by |dimensions|, and requested
  // output buffer format is specified by |storage|. The reserved output buffer
  // is returned; and the pointer to Y plane is stored at [y_plane_data], U
  // plane is stored at [u_plane_data], V plane is stored at [v_plane_data].
  // Returns an nullptr if allocation fails.
  //
  // When requested |storage| is PIXEL_STORAGE_CPU, a single shared memory
  // chunk is reserved; whereas for PIXEL_STORAGE_GPUMEMORYBUFFER, three
  // GpuMemoryBuffers in R_8 format representing I420 planes are reserved. The
  // output buffers stay reserved and mapped for use until the Buffer objects
  // are destroyed or returned.
  std::unique_ptr<Buffer> ReserveI420OutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelStorage storage,
      uint8_t** y_plane_data,
      uint8_t** u_plane_data,
      uint8_t** v_plane_data);

  // The controller to which we post events.
  const base::WeakPtr<VideoCaptureController> controller_;

  // Hardware JPEG decoder.
  std::unique_ptr<VideoCaptureGpuJpegDecoder> external_jpeg_decoder_;

  // Whether |external_jpeg_decoder_| has been initialized.
  bool external_jpeg_decoder_initialized_;

  // The pool of shared-memory buffers used for capturing.
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  // Indication to the Client to copy-transform the incoming data into
  // GpuMemoryBuffers.
  const bool use_gpu_memory_buffers_;

  media::VideoPixelFormat last_captured_pixel_format_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceClient);
};


}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_CLIENT_H_
