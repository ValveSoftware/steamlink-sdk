// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/gpu/ipc/client/gpu_jpeg_decode_accelerator_host.h"

namespace content {

VideoCaptureGpuJpegDecoder::VideoCaptureGpuJpegDecoder(
    const DecodeDoneCB& decode_done_cb)
    : decode_done_cb_(decode_done_cb),
      next_bitstream_buffer_id_(0),
      in_buffer_id_(media::JpegDecodeAccelerator::kInvalidBitstreamBufferId),
      decoder_status_(INIT_PENDING) {}

VideoCaptureGpuJpegDecoder::~VideoCaptureGpuJpegDecoder() {
  DCHECK(CalledOnValidThread());

  // |decoder_| guarantees no more JpegDecodeAccelerator::Client callbacks
  // on IO thread after deletion.
  decoder_.reset();

  // |gpu_channel_host_| should outlive |decoder_|, so |gpu_channel_host_|
  // must be released after |decoder_| has been destroyed.
  gpu_channel_host_ = nullptr;
}

void VideoCaptureGpuJpegDecoder::Initialize() {
  DCHECK(CalledOnValidThread());

  base::AutoLock lock(lock_);
  bool is_platform_supported = false;
#if defined(OS_CHROMEOS)
  // Non-ChromeOS platforms do not support HW JPEG decode now. Do not establish
  // gpu channel to avoid introducing overhead.
  is_platform_supported = true;
#endif

  if (!is_platform_supported ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAcceleratedMjpegDecode)) {
    decoder_status_ = FAILED;
    RecordInitDecodeUMA_Locked();
    return;
  }

  const scoped_refptr<base::SingleThreadTaskRunner> current_task_runner(
      base::ThreadTaskRunnerHandle::Get());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&EstablishGpuChannelOnUIThread,
                                     current_task_runner, AsWeakPtr()));
}

VideoCaptureGpuJpegDecoder::STATUS VideoCaptureGpuJpegDecoder::GetStatus()
    const {
  DCHECK(CalledOnValidThread());
  base::AutoLock lock(lock_);
  return decoder_status_;
}

void VideoCaptureGpuJpegDecoder::DecodeCapturedData(
    const uint8_t* data,
    size_t in_buffer_size,
    const media::VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::unique_ptr<media::VideoCaptureDevice::Client::Buffer> out_buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(decoder_);

  TRACE_EVENT_ASYNC_BEGIN0("jpeg", "VideoCaptureGpuJpegDecoder decoding",
                           next_bitstream_buffer_id_);
  TRACE_EVENT0("jpeg", "VideoCaptureGpuJpegDecoder::DecodeCapturedData");

  // TODO(kcwu): enqueue decode requests in case decoding is not fast enough
  // (say, if decoding time is longer than 16ms for 60fps 4k image)
  {
    base::AutoLock lock(lock_);
    if (IsDecoding_Locked()) {
      DVLOG(1) << "Drop captured frame. Previous jpeg frame is still decoding";
      return;
    }
  }

  // Enlarge input buffer if necessary.
  if (!in_shared_memory_.get() ||
      in_buffer_size > in_shared_memory_->mapped_size()) {
    // Reserve 2x space to avoid frequent reallocations for initial frames.
    const size_t reserved_size = 2 * in_buffer_size;
    in_shared_memory_.reset(new base::SharedMemory);
    if (!in_shared_memory_->CreateAndMapAnonymous(reserved_size)) {
      base::AutoLock lock(lock_);
      decoder_status_ = FAILED;
      LOG(WARNING) << "CreateAndMapAnonymous failed, size=" << reserved_size;
      return;
    }
  }
  memcpy(in_shared_memory_->memory(), data, in_buffer_size);

  // No need to lock for |in_buffer_id_| since IsDecoding_Locked() is false.
  in_buffer_id_ = next_bitstream_buffer_id_;
  media::BitstreamBuffer in_buffer(in_buffer_id_, in_shared_memory_->handle(),
                                   in_buffer_size);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  const gfx::Size dimensions = frame_format.frame_size;
  base::SharedMemoryHandle out_handle = out_buffer->AsPlatformFile();
  scoped_refptr<media::VideoFrame> out_frame =
      media::VideoFrame::WrapExternalSharedMemory(
          media::PIXEL_FORMAT_I420,                   // format
          dimensions,                                 // coded_size
          gfx::Rect(dimensions),                      // visible_rect
          dimensions,                                 // natural_size
          static_cast<uint8_t*>(out_buffer->data()),  // data
          out_buffer->mapped_size(),                  // data_size
          out_handle,                                 // handle
          0,                                          // shared_memory_offset
          timestamp);                                 // timestamp
  if (!out_frame) {
    base::AutoLock lock(lock_);
    decoder_status_ = FAILED;
    LOG(ERROR) << "DecodeCapturedData: WrapExternalSharedMemory failed";
    return;
  }
  out_frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                   frame_format.frame_rate);

  out_frame->metadata()->SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                                      reference_time);

  {
    base::AutoLock lock(lock_);
    decode_done_closure_ =
        base::Bind(decode_done_cb_, base::Passed(&out_buffer), out_frame);
  }
  decoder_->Decode(in_buffer, out_frame);
#else
  NOTREACHED();
#endif
}

void VideoCaptureGpuJpegDecoder::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0("jpeg", "VideoCaptureGpuJpegDecoder::VideoFrameReady");
  base::AutoLock lock(lock_);

  if (!IsDecoding_Locked()) {
    LOG(ERROR) << "Got decode response while not decoding";
    return;
  }

  if (bitstream_buffer_id != in_buffer_id_) {
    LOG(ERROR) << "Unexpected bitstream_buffer_id " << bitstream_buffer_id
               << ", expected " << in_buffer_id_;
    return;
  }
  in_buffer_id_ = media::JpegDecodeAccelerator::kInvalidBitstreamBufferId;

  decode_done_closure_.Run();
  decode_done_closure_.Reset();

  TRACE_EVENT_ASYNC_END0("jpeg", "VideoCaptureGpuJpegDecoder decoding",
                         bitstream_buffer_id);
}

void VideoCaptureGpuJpegDecoder::NotifyError(
    int32_t bitstream_buffer_id,
    media::JpegDecodeAccelerator::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LOG(ERROR) << "Decode error, bitstream_buffer_id=" << bitstream_buffer_id
             << ", error=" << error;

  base::AutoLock lock(lock_);
  decode_done_closure_.Reset();
  decoder_status_ = FAILED;
}

// static
void VideoCaptureGpuJpegDecoder::EstablishGpuChannelOnUIThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<VideoCaptureGpuJpegDecoder> weak_this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(BrowserGpuChannelHostFactory::instance());

  BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
      CAUSE_FOR_GPU_LAUNCH_JPEGDECODEACCELERATOR_INITIALIZE,
      base::Bind(&VideoCaptureGpuJpegDecoder::GpuChannelEstablishedOnUIThread,
                 task_runner, weak_this));
}

// static
void VideoCaptureGpuJpegDecoder::GpuChannelEstablishedOnUIThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<VideoCaptureGpuJpegDecoder> weak_this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      BrowserGpuChannelHostFactory::instance()->GetGpuChannel());
  task_runner->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureGpuJpegDecoder::FinishInitialization,
                            weak_this, std::move(gpu_channel_host)));
}

void VideoCaptureGpuJpegDecoder::FinishInitialization(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  TRACE_EVENT0("gpu", "VideoCaptureGpuJpegDecoder::FinishInitialization");
  DCHECK(CalledOnValidThread());
  base::AutoLock lock(lock_);
  if (!gpu_channel_host) {
    LOG(ERROR) << "Failed to establish GPU channel for JPEG decoder";
  } else if (gpu_channel_host->gpu_info().jpeg_decode_accelerator_supported) {
    gpu_channel_host_ = std::move(gpu_channel_host);
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
        BrowserGpuChannelHostFactory::instance()->GetIOThreadTaskRunner();

    int32_t route_id = gpu_channel_host_->GenerateRouteID();
    std::unique_ptr<media::GpuJpegDecodeAcceleratorHost> decoder(
        new media::GpuJpegDecodeAcceleratorHost(gpu_channel_host_.get(),
                                                route_id, io_task_runner));
    if (decoder->Initialize(this)) {
      gpu_channel_host_->AddRouteWithTaskRunner(
          route_id, decoder->GetReceiver(), io_task_runner);
      decoder_ = std::move(decoder);
    } else {
      DLOG(ERROR) << "Failed to initialize JPEG decoder";
    }
  }
  decoder_status_ = decoder_ ? INIT_PASSED : FAILED;
  RecordInitDecodeUMA_Locked();
}

bool VideoCaptureGpuJpegDecoder::IsDecoding_Locked() const {
  lock_.AssertAcquired();
  return !decode_done_closure_.is_null();
}

void VideoCaptureGpuJpegDecoder::RecordInitDecodeUMA_Locked() {
  UMA_HISTOGRAM_BOOLEAN("Media.VideoCaptureGpuJpegDecoder.InitDecodeSuccess",
                        decoder_status_ == INIT_PASSED);
}

}  // namespace content
