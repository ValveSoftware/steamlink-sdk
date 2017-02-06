// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_device_client.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv.h"

using media::VideoCaptureFormat;
using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

// Class combining a Client::Buffer interface implementation and a pool buffer
// implementation to guarantee proper cleanup on destruction on our side.
class AutoReleaseBuffer : public media::VideoCaptureDevice::Client::Buffer {
 public:
  AutoReleaseBuffer(const scoped_refptr<VideoCaptureBufferPool>& pool,
                    int buffer_id)
      : id_(buffer_id),
        pool_(pool),
        buffer_handle_(pool_->GetBufferHandle(buffer_id)) {
    DCHECK(pool_.get());
  }
  int id() const override { return id_; }
  gfx::Size dimensions() const override { return buffer_handle_->dimensions(); }
  size_t mapped_size() const override { return buffer_handle_->mapped_size(); }
  void* data(int plane) override { return buffer_handle_->data(plane); }
  ClientBuffer AsClientBuffer(int plane) override {
    return buffer_handle_->AsClientBuffer(plane);
  }
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  base::FileDescriptor AsPlatformFile() override {
    return buffer_handle_->AsPlatformFile();
  }
#endif

 private:
  ~AutoReleaseBuffer() override { pool_->RelinquishProducerReservation(id_); }

  const int id_;
  const scoped_refptr<VideoCaptureBufferPool> pool_;
  const std::unique_ptr<VideoCaptureBufferPool::BufferHandle> buffer_handle_;
};

VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool)
    : controller_(controller),
      external_jpeg_decoder_initialized_(false),
      buffer_pool_(buffer_pool),
      use_gpu_memory_buffers_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuMemoryBuffersForCapture)),
      last_captured_pixel_format_(media::PIXEL_FORMAT_UNKNOWN) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {
  // This should be on the platform auxiliary thread since
  // |external_jpeg_decoder_| need to be destructed on the same thread as
  // OnIncomingCapturedData.
}

void VideoCaptureDeviceClient::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& frame_format,
    int rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  TRACE_EVENT0("video", "VideoCaptureDeviceClient::OnIncomingCapturedData");
  DCHECK_EQ(media::PIXEL_STORAGE_CPU, frame_format.pixel_storage);

  if (last_captured_pixel_format_ != frame_format.pixel_format) {
    OnLog("Pixel format: " +
          media::VideoPixelFormatToString(frame_format.pixel_format));
    last_captured_pixel_format_ = frame_format.pixel_format;

    if (frame_format.pixel_format == media::PIXEL_FORMAT_MJPEG &&
        !external_jpeg_decoder_initialized_) {
      external_jpeg_decoder_initialized_ = true;
      external_jpeg_decoder_.reset(new VideoCaptureGpuJpegDecoder(base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_)));
      external_jpeg_decoder_->Initialize();
    }
  }

  if (!frame_format.IsValid())
    return;

  // |chopped_{width,height} and |new_unrotated_{width,height}| are the lowest
  // bit decomposition of {width, height}, grabbing the odd and even parts.
  const int chopped_width = frame_format.frame_size.width() & 1;
  const int chopped_height = frame_format.frame_size.height() & 1;
  const int new_unrotated_width = frame_format.frame_size.width() & ~1;
  const int new_unrotated_height = frame_format.frame_size.height() & ~1;

  int destination_width = new_unrotated_width;
  int destination_height = new_unrotated_height;
  if (rotation == 90 || rotation == 270)
    std::swap(destination_width, destination_height);

  DCHECK_EQ(0, rotation % 90)
      << " Rotation must be a multiple of 90, now: " << rotation;
  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (rotation == 90)
    rotation_mode = libyuv::kRotate90;
  else if (rotation == 180)
    rotation_mode = libyuv::kRotate180;
  else if (rotation == 270)
    rotation_mode = libyuv::kRotate270;

  const gfx::Size dimensions(destination_width, destination_height);
  const media::VideoPixelStorage output_pixel_storage =
      use_gpu_memory_buffers_ ? media::PIXEL_STORAGE_GPUMEMORYBUFFER
                              : media::PIXEL_STORAGE_CPU;
  uint8_t *y_plane_data, *u_plane_data, *v_plane_data;
  std::unique_ptr<Buffer> buffer(
      ReserveI420OutputBuffer(dimensions, output_pixel_storage, &y_plane_data,
                              &u_plane_data, &v_plane_data));
  if (!buffer.get()) {
    DLOG(WARNING) << "Failed to reserve I420 output buffer.";
    return;
  }

  const int yplane_stride = dimensions.width();
  const int uv_plane_stride = yplane_stride / 2;
  int crop_x = 0;
  int crop_y = 0;
  libyuv::FourCC origin_colorspace = libyuv::FOURCC_ANY;

  bool flip = false;
  switch (frame_format.pixel_format) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_I420:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_I420;
      break;
    case media::PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YV12;
      break;
    case media::PIXEL_FORMAT_NV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV12;
      break;
    case media::PIXEL_FORMAT_NV21:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV21;
      break;
    case media::PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YUY2;
      break;
    case media::PIXEL_FORMAT_UYVY:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_UYVY;
      break;
    case media::PIXEL_FORMAT_RGB24:
      // Linux RGB24 defines red at lowest byte address,
      // see http://linuxtv.org/downloads/v4l-dvb-apis/packed-rgb.html.
      // Windows RGB24 defines blue at lowest byte,
      // see https://msdn.microsoft.com/en-us/library/windows/desktop/dd407253
#if defined(OS_LINUX)
      origin_colorspace = libyuv::FOURCC_RAW;
#elif defined(OS_WIN)
      origin_colorspace = libyuv::FOURCC_24BG;
#else
      NOTREACHED() << "RGB24 is only available in Linux and Windows platforms";
#endif
#if defined(OS_WIN)
      // TODO(wjia): Currently, for RGB24 on WIN, capture device always passes
      // in positive src_width and src_height. Remove this hardcoded value when
      // negative src_height is supported. The negative src_height indicates
      // that vertical flipping is needed.
      flip = true;
#endif
      break;
    case media::PIXEL_FORMAT_RGB32:
// Fallback to PIXEL_FORMAT_ARGB setting |flip| in Windows
// platforms.
#if defined(OS_WIN)
      flip = true;
#endif
    case media::PIXEL_FORMAT_ARGB:
      origin_colorspace = libyuv::FOURCC_ARGB;
      break;
    case media::PIXEL_FORMAT_MJPEG:
      origin_colorspace = libyuv::FOURCC_MJPG;
      break;
    default:
      NOTREACHED();
  }

  // The input |length| can be greater than the required buffer size because of
  // paddings and/or alignments, but it cannot be smaller.
  DCHECK_GE(static_cast<size_t>(length), frame_format.ImageAllocationSize());

  if (external_jpeg_decoder_) {
    const VideoCaptureGpuJpegDecoder::STATUS status =
        external_jpeg_decoder_->GetStatus();
    if (status == VideoCaptureGpuJpegDecoder::FAILED) {
      external_jpeg_decoder_.reset();
    } else if (status == VideoCaptureGpuJpegDecoder::INIT_PASSED &&
        frame_format.pixel_format == media::PIXEL_FORMAT_MJPEG &&
        rotation == 0 && !flip) {
      external_jpeg_decoder_->DecodeCapturedData(data, length, frame_format,
                                                 reference_time, timestamp,
                                                 std::move(buffer));
      return;
    }
  }

  if (libyuv::ConvertToI420(data,
                            length,
                            y_plane_data,
                            yplane_stride,
                            u_plane_data,
                            uv_plane_stride,
                            v_plane_data,
                            uv_plane_stride,
                            crop_x,
                            crop_y,
                            frame_format.frame_size.width(),
                            (flip ? -1 : 1) * frame_format.frame_size.height(),
                            new_unrotated_width,
                            new_unrotated_height,
                            rotation_mode,
                            origin_colorspace) != 0) {
    DLOG(WARNING) << "Failed to convert buffer's pixel format to I420 from "
                  << media::VideoPixelFormatToString(frame_format.pixel_format);
    return;
  }

  const VideoCaptureFormat output_format = VideoCaptureFormat(
      dimensions, frame_format.frame_rate,
      media::PIXEL_FORMAT_I420, output_pixel_storage);
  OnIncomingCapturedBuffer(std::move(buffer), output_format, reference_time,
                           timestamp);
}

std::unique_ptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureDeviceClient::ReserveOutputBuffer(
    const gfx::Size& frame_size,
    media::VideoPixelFormat pixel_format,
    media::VideoPixelStorage pixel_storage) {
  DCHECK_GT(frame_size.width(), 0);
  DCHECK_GT(frame_size.height(), 0);
  // Currently, only I420 pixel format is supported.
  DCHECK_EQ(media::PIXEL_FORMAT_I420, pixel_format);

  // TODO(mcasas): For PIXEL_STORAGE_GPUMEMORYBUFFER, find a way to indicate if
  // it's a ShMem GMB or a DmaBuf GMB.
  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  const int buffer_id = buffer_pool_->ReserveForProducer(
      frame_size, pixel_format, pixel_storage, &buffer_id_to_drop);
  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoBufferDestroyedOnIOThread,
                   controller_, buffer_id_to_drop));
  }
  if (buffer_id == VideoCaptureBufferPool::kInvalidId)
    return nullptr;
  return base::WrapUnique<Buffer>(
      new AutoReleaseBuffer(buffer_pool_, buffer_id));
}

void VideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    std::unique_ptr<Buffer> buffer,
    const VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  // Currently, only I420 pixel format is supported.
  DCHECK_EQ(media::PIXEL_FORMAT_I420, frame_format.pixel_format);

  scoped_refptr<VideoFrame> frame;
  switch (frame_format.pixel_storage) {
    case media::PIXEL_STORAGE_GPUMEMORYBUFFER: {
      // Create a VideoFrame to set the correct storage_type and pixel_format.
      gfx::GpuMemoryBufferHandle handle;
      frame = VideoFrame::WrapExternalYuvGpuMemoryBuffers(
          media::PIXEL_FORMAT_I420, frame_format.frame_size,
          gfx::Rect(frame_format.frame_size), frame_format.frame_size, 0, 0, 0,
          reinterpret_cast<uint8_t*>(buffer->data(media::VideoFrame::kYPlane)),
          reinterpret_cast<uint8_t*>(buffer->data(media::VideoFrame::kUPlane)),
          reinterpret_cast<uint8_t*>(buffer->data(media::VideoFrame::kVPlane)),
          handle, handle, handle, timestamp);
      break;
    }
    case media::PIXEL_STORAGE_CPU:
      frame = VideoFrame::WrapExternalSharedMemory(
          media::PIXEL_FORMAT_I420, frame_format.frame_size,
          gfx::Rect(frame_format.frame_size), frame_format.frame_size,
          reinterpret_cast<uint8_t*>(buffer->data()),
          VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420,
                                     frame_format.frame_size),
          base::SharedMemory::NULLHandle(), 0u, timestamp);
      break;
  }
  if (!frame)
    return;
  frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                               frame_format.frame_rate);
  frame->metadata()->SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                                  reference_time);
  OnIncomingCapturedVideoFrame(std::move(buffer), frame);
}

void VideoCaptureDeviceClient::OnIncomingCapturedVideoFrame(
    std::unique_ptr<Buffer> buffer,
    const scoped_refptr<VideoFrame>& frame) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_, base::Passed(&buffer), frame));
}

std::unique_ptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureDeviceClient::ResurrectLastOutputBuffer(
    const gfx::Size& dimensions,
    media::VideoPixelFormat format,
    media::VideoPixelStorage storage) {
  const int buffer_id =
      buffer_pool_->ResurrectLastForProducer(dimensions, format, storage);
  if (buffer_id == VideoCaptureBufferPool::kInvalidId)
    return nullptr;
  return base::WrapUnique<Buffer>(
      new AutoReleaseBuffer(buffer_pool_, buffer_id));
}

void VideoCaptureDeviceClient::OnError(
    const tracked_objects::Location& from_here,
    const std::string& reason) {
  const std::string log_message = base::StringPrintf(
      "error@ %s, %s, OS message: %s", from_here.ToString().c_str(),
      reason.c_str(),
      logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode())
          .c_str());
  DLOG(ERROR) << log_message;
  OnLog(log_message);
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

void VideoCaptureDeviceClient::OnLog(
    const std::string& message) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&VideoCaptureController::DoLogOnIOThread,
                                     controller_, message));
}

double VideoCaptureDeviceClient::GetBufferPoolUtilization() const {
  // VideoCaptureBufferPool::GetBufferPoolUtilization() is thread-safe.
  return buffer_pool_->GetBufferPoolUtilization();
}

std::unique_ptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureDeviceClient::ReserveI420OutputBuffer(
    const gfx::Size& dimensions,
    media::VideoPixelStorage storage,
    uint8_t** y_plane_data,
    uint8_t** u_plane_data,
    uint8_t** v_plane_data) {
  DCHECK(storage == media::PIXEL_STORAGE_GPUMEMORYBUFFER ||
         storage == media::PIXEL_STORAGE_CPU);
  DCHECK(dimensions.height());
  DCHECK(dimensions.width());

  const media::VideoPixelFormat format = media::PIXEL_FORMAT_I420;
  std::unique_ptr<Buffer> buffer(
      ReserveOutputBuffer(dimensions, media::PIXEL_FORMAT_I420, storage));
  if (!buffer)
    return std::unique_ptr<Buffer>();

  switch (storage) {
    case media::PIXEL_STORAGE_CPU:
      // TODO(emircan): See http://crbug.com/521068, move this pointer
      // arithmetic inside Buffer::data() when this bug is resolved.
      *y_plane_data = reinterpret_cast<uint8_t*>(buffer->data());
      *u_plane_data =
          *y_plane_data +
          VideoFrame::PlaneSize(format, VideoFrame::kYPlane, dimensions)
              .GetArea();
      *v_plane_data =
          *u_plane_data +
          VideoFrame::PlaneSize(format, VideoFrame::kUPlane, dimensions)
              .GetArea();
      return buffer;
    case media::PIXEL_STORAGE_GPUMEMORYBUFFER:
      *y_plane_data =
          reinterpret_cast<uint8_t*>(buffer->data(VideoFrame::kYPlane));
      *u_plane_data =
          reinterpret_cast<uint8_t*>(buffer->data(VideoFrame::kUPlane));
      *v_plane_data =
          reinterpret_cast<uint8_t*>(buffer->data(VideoFrame::kVPlane));
      return buffer;
  }
  NOTREACHED();
  return std::unique_ptr<Buffer>();
}

}  // namespace content
