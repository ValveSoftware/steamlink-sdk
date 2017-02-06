// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/rtc_video_encoder.h"

#include <string.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/filters/h264_parser.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/webrtc/base/timeutils.h"

namespace content {

namespace {

// Translate from webrtc::VideoCodecType and webrtc::VideoCodec to
// media::VideoCodecProfile.
media::VideoCodecProfile WebRTCVideoCodecToVideoCodecProfile(
    webrtc::VideoCodecType type,
    const webrtc::VideoCodec* codec_settings) {
  DCHECK_EQ(type, codec_settings->codecType);
  switch (type) {
    case webrtc::kVideoCodecVP8:
      return media::VP8PROFILE_ANY;
    case webrtc::kVideoCodecH264: {
      switch (codec_settings->codecSpecific.H264.profile) {
        case webrtc::kProfileBase:
          return media::H264PROFILE_BASELINE;
        case webrtc::kProfileMain:
          return media::H264PROFILE_MAIN;
      }
    }
    default:
      NOTREACHED() << "Unrecognized video codec type";
      return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

// Populates struct webrtc::RTPFragmentationHeader for H264 codec.
// Each entry specifies the offset and length (excluding start code) of a NALU.
// Returns true if successful.
bool GetRTPFragmentationHeaderH264(webrtc::RTPFragmentationHeader* header,
                                   const uint8_t* data, uint32_t length) {
  media::H264Parser parser;
  parser.SetStream(data, length);

  std::vector<media::H264NALU> nalu_vector;
  while (true) {
    media::H264NALU nalu;
    const media::H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result == media::H264Parser::kOk) {
      nalu_vector.push_back(nalu);
    } else if (result == media::H264Parser::kEOStream) {
      break;
    } else {
      DLOG(ERROR) << "Unexpected H264 parser result";
      return false;
    }
  }

  header->VerifyAndAllocateFragmentationHeader(nalu_vector.size());
  for (size_t i = 0; i < nalu_vector.size(); ++i) {
    header->fragmentationOffset[i] = nalu_vector[i].data - data;
    header->fragmentationLength[i] = nalu_vector[i].size;
    header->fragmentationPlType[i] = 0;
    header->fragmentationTimeDiff[i] = 0;
  }
  return true;
}

}  // namespace

// This private class of RTCVideoEncoder does the actual work of communicating
// with a media::VideoEncodeAccelerator for handling video encoding.  It can
// be created on any thread, but should subsequently be posted to (and Destroy()
// called on) a single thread.
//
// This class separates state related to the thread that RTCVideoEncoder
// operates on from the thread that |gpu_factories_| provides for accelerator
// operations (presently the media thread).
class RTCVideoEncoder::Impl
    : public media::VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<RTCVideoEncoder::Impl> {
 public:
  Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
       webrtc::VideoCodecType video_codec_type);

  // Create the VEA and call Initialize() on it.  Called once per instantiation,
  // and then the instance is bound forevermore to whichever thread made the
  // call.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |async_waiter| and |async_retval| arguments.
  void CreateAndInitializeVEA(const gfx::Size& input_visible_size,
                              uint32_t bitrate,
                              media::VideoCodecProfile profile,
                              base::WaitableEvent* async_waiter,
                              int32_t* async_retval);
  // Enqueue a frame from WebRTC for encoding.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |async_waiter| and |async_retval| arguments.
  void Enqueue(const webrtc::VideoFrame* input_frame,
               bool force_keyframe,
               base::WaitableEvent* async_waiter,
               int32_t* async_retval);

  // RTCVideoEncoder is given a buffer to be passed to WebRTC through the
  // RTCVideoEncoder::ReturnEncodedImage() function.  When that is complete,
  // the buffer is returned to Impl by its index using this function.
  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);

  // Request encoding parameter change for the underlying encoder.
  void RequestEncodingParametersChange(uint32_t bitrate, uint32_t framerate);

  void RegisterEncodeCompleteCallback(base::WaitableEvent* async_waiter,
                                      int32_t* async_retval,
                                      webrtc::EncodedImageCallback* callback);

  // Destroy this Impl's encoder.  The destructor is not explicitly called, as
  // Impl is a base::RefCountedThreadSafe.
  void Destroy(base::WaitableEvent* async_waiter);

  // Return the status of Impl. One of WEBRTC_VIDEO_CODEC_XXX value.
  int32_t GetStatus() const;

  webrtc::VideoCodecType video_codec_type() { return video_codec_type_; }

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame,
                            base::TimeDelta timestamp) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

 private:
  friend class base::RefCountedThreadSafe<Impl>;

  enum {
    kInputBufferExtraCount = 1,  // The number of input buffers allocated, more
                                 // than what is requested by
                                 // VEA::RequireBitstreamBuffers().
    kOutputBufferCount = 3,
  };

  ~Impl() override;

  // Logs the |error| and |str| sent from |location| and NotifyError()s forward.
  void LogAndNotifyError(const tracked_objects::Location& location,
                         const std::string& str,
                         media::VideoEncodeAccelerator::Error error);

  // Perform encoding on an input frame from the input queue.
  void EncodeOneFrame();

  // Notify that an input frame is finished for encoding.  |index| is the index
  // of the completed frame in |input_buffers_|.
  void EncodeFrameFinished(int index);

  // Set up/signal |async_waiter_| and |async_retval_|; see declarations below.
  void RegisterAsyncWaiter(base::WaitableEvent* waiter, int32_t* retval);
  void SignalAsyncWaiter(int32_t retval);

  // Checks if the bitrate would overflow when passing from kbps to bps.
  bool IsBitrateTooHigh(uint32_t bitrate);

  // Checks if the frame size is different than hardware accelerator
  // requirements.
  bool RequiresSizeChange(const scoped_refptr<media::VideoFrame>& frame) const;

  // Return an encoded output buffer to WebRTC.
  void ReturnEncodedImage(const webrtc::EncodedImage& image,
                          int32_t bitstream_buffer_id,
                          uint16_t picture_id);

  void SetStatus(int32_t status);

  // This is attached to |gpu_task_runner_|, not the thread class is constructed
  // on.
  base::ThreadChecker thread_checker_;

  // Factory for creating VEAs, shared memory buffers, etc.
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // webrtc::VideoEncoder expects InitEncode() and Encode() to be synchronous.
  // Do this by waiting on the |async_waiter_| and returning the return value in
  // |async_retval_| when initialization completes, encoding completes, or
  // an error occurs.
  base::WaitableEvent* async_waiter_;
  int32_t* async_retval_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Next input frame.  Since there is at most one next frame, a single-element
  // queue is sufficient.
  const webrtc::VideoFrame* input_next_frame_;

  // Whether to encode a keyframe next.
  bool input_next_frame_keyframe_;

  // Frame sizes.
  gfx::Size input_frame_coded_size_;
  gfx::Size input_visible_size_;

  // Shared memory buffers for input/output with the VEA.
  ScopedVector<base::SharedMemory> input_buffers_;
  ScopedVector<base::SharedMemory> output_buffers_;

  // Input buffers ready to be filled with input from Encode().  As a LIFO since
  // we don't care about ordering.
  std::vector<int> input_buffers_free_;

  // The number of output buffers ready to be filled with output from the
  // encoder.
  int output_buffers_free_count_;

  // 15 bits running index of the VP8 frames. See VP8 RTP spec for details.
  uint16_t picture_id_;

  // webrtc::VideoEncoder encode complete callback.
  webrtc::EncodedImageCallback* encoded_image_callback_;

  // The video codec type, as reported to WebRTC.
  const webrtc::VideoCodecType video_codec_type_;

  // Protect |status_|. |status_| is read or written on |gpu_task_runner_| in
  // Impl. It can be read in RTCVideoEncoder on other threads.
  mutable base::Lock status_lock_;

  // We cannot immediately return error conditions to the WebRTC user of this
  // class, as there is no error callback in the webrtc::VideoEncoder interface.
  // Instead, we cache an error status here and return it the next time an
  // interface entry point is called. This is protected by |status_lock_|.
  int32_t status_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

RTCVideoEncoder::Impl::Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
                            webrtc::VideoCodecType video_codec_type)
    : gpu_factories_(gpu_factories),
      async_waiter_(NULL),
      async_retval_(NULL),
      input_next_frame_(NULL),
      input_next_frame_keyframe_(false),
      output_buffers_free_count_(0),
      encoded_image_callback_(nullptr),
      video_codec_type_(video_codec_type),
      status_(WEBRTC_VIDEO_CODEC_UNINITIALIZED) {
  thread_checker_.DetachFromThread();
  // Picture ID should start on a random number.
  picture_id_ = static_cast<uint16_t>(base::RandInt(0, 0x7FFF));
}

void RTCVideoEncoder::Impl::CreateAndInitializeVEA(
    const gfx::Size& input_visible_size,
    uint32_t bitrate,
    media::VideoCodecProfile profile,
    base::WaitableEvent* async_waiter,
    int32_t* async_retval) {
  DVLOG(3) << "Impl::CreateAndInitializeVEA()";
  DCHECK(thread_checker_.CalledOnValidThread());

  SetStatus(WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  RegisterAsyncWaiter(async_waiter, async_retval);

  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  if (IsBitrateTooHigh(bitrate))
    return;

  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!video_encoder_) {
    LogAndNotifyError(FROM_HERE, "Error creating VideoEncodeAccelerator",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  input_visible_size_ = input_visible_size;
  if (!video_encoder_->Initialize(media::PIXEL_FORMAT_I420, input_visible_size_,
                                  profile, bitrate * 1000, this)) {
    LogAndNotifyError(FROM_HERE, "Error initializing video_encoder",
                      media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }
  // RequireBitstreamBuffers or NotifyError will be called and the waiter will
  // be signaled.
}

void RTCVideoEncoder::Impl::Enqueue(const webrtc::VideoFrame* input_frame,
                                    bool force_keyframe,
                                    base::WaitableEvent* async_waiter,
                                    int32_t* async_retval) {
  DVLOG(3) << "Impl::Enqueue()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!input_next_frame_);

  RegisterAsyncWaiter(async_waiter, async_retval);
  int32_t retval = GetStatus();
  if (retval != WEBRTC_VIDEO_CODEC_OK) {
    SignalAsyncWaiter(retval);
    return;
  }

  // If there are no free input and output buffers, drop the frame to avoid a
  // deadlock. If there is a free input buffer, EncodeOneFrame will run and
  // unblock Encode(). If there are no free input buffers but there is a free
  // output buffer, EncodeFrameFinished will be called later to unblock
  // Encode().
  //
  // The caller of Encode() holds a webrtc lock. The deadlock happens when:
  // (1) Encode() is waiting for the frame to be encoded in EncodeOneFrame().
  // (2) There are no free input buffers and they cannot be freed because
  //     the encoder has no output buffers.
  // (3) Output buffers cannot be freed because ReturnEncodedImage is queued
  //     on libjingle worker thread to be run. But the worker thread is waiting
  //     for the same webrtc lock held by the caller of Encode().
  //
  // Dropping a frame is fine. The encoder has been filled with all input
  // buffers. Returning an error in Encode() is not fatal and WebRTC will just
  // continue. If this is a key frame, WebRTC will request a key frame again.
  // Besides, webrtc will drop a frame if Encode() blocks too long.
  if (input_buffers_free_.empty() && output_buffers_free_count_ == 0) {
    DVLOG(2) << "Run out of input and output buffers. Drop the frame.";
    SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }
  input_next_frame_ = input_frame;
  input_next_frame_keyframe_ = force_keyframe;

  if (!input_buffers_free_.empty())
    EncodeOneFrame();
}

void RTCVideoEncoder::Impl::UseOutputBitstreamBufferId(
    int32_t bitstream_buffer_id) {
  DVLOG(3) << "Impl::UseOutputBitstreamBufferIndex(): "
              "bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (video_encoder_) {
    video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id]->handle(),
        output_buffers_[bitstream_buffer_id]->mapped_size()));
    output_buffers_free_count_++;
  }
}

void RTCVideoEncoder::Impl::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << "Impl::RequestEncodingParametersChange(): bitrate=" << bitrate
           << ", framerate=" << framerate;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  if (IsBitrateTooHigh(bitrate))
    return;

  if (video_encoder_)
    video_encoder_->RequestEncodingParametersChange(bitrate * 1000, framerate);
}

void RTCVideoEncoder::Impl::Destroy(base::WaitableEvent* async_waiter) {
  DVLOG(3) << "Impl::Destroy()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (video_encoder_) {
    video_encoder_.reset();
    SetStatus(WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  }
  async_waiter->Signal();
}

int32_t RTCVideoEncoder::Impl::GetStatus() const {
  base::AutoLock lock(status_lock_);
  return status_;
}

void RTCVideoEncoder::Impl::SetStatus(int32_t status) {
  base::AutoLock lock(status_lock_);
  status_ = status;
}

void RTCVideoEncoder::Impl::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DVLOG(3) << "Impl::RequireBitstreamBuffers(): input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!video_encoder_)
    return;

  input_frame_coded_size_ = input_coded_size;

  for (unsigned int i = 0; i < input_count + kInputBufferExtraCount; ++i) {
    std::unique_ptr<base::SharedMemory> shm =
        gpu_factories_->CreateSharedMemory(media::VideoFrame::AllocationSize(
            media::PIXEL_FORMAT_I420, input_coded_size));
    if (!shm) {
      LogAndNotifyError(FROM_HERE, "failed to create input buffer ",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    input_buffers_.push_back(shm.release());
    input_buffers_free_.push_back(i);
  }

  for (int i = 0; i < kOutputBufferCount; ++i) {
    std::unique_ptr<base::SharedMemory> shm =
        gpu_factories_->CreateSharedMemory(output_buffer_size);
    if (!shm) {
      LogAndNotifyError(FROM_HERE, "failed to create output buffer",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    output_buffers_.push_back(shm.release());
  }

  // Immediately provide all output buffers to the VEA.
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        i, output_buffers_[i]->handle(), output_buffers_[i]->mapped_size()));
    output_buffers_free_count_++;
  }
  DCHECK_EQ(GetStatus(), WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  SetStatus(WEBRTC_VIDEO_CODEC_OK);
  SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::BitstreamBufferReady(int32_t bitstream_buffer_id,
    size_t payload_size,
    bool key_frame,
    base::TimeDelta timestamp) {
  DVLOG(3) << "Impl::BitstreamBufferReady(): bitstream_buffer_id="
           << bitstream_buffer_id << ", payload_size=" << payload_size
           << ", key_frame=" << key_frame
           << ", timestamp ms=" << timestamp.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());

  if (bitstream_buffer_id < 0 ||
      bitstream_buffer_id >= static_cast<int>(output_buffers_.size())) {
    LogAndNotifyError(FROM_HERE, "invalid bitstream_buffer_id",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  base::SharedMemory* output_buffer = output_buffers_[bitstream_buffer_id];
  if (payload_size > output_buffer->mapped_size()) {
    LogAndNotifyError(FROM_HERE, "invalid payload_size",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  output_buffers_free_count_--;

  // CrOS Nyan provides invalid timestamp. Use the current time for now.
  // TODO(wuchengli): use the timestamp in BitstreamBufferReady after Nyan is
  // fixed. http://crbug.com/620565.
  const int64_t capture_time_us = rtc::TimeMicros();

  // Derive the capture time (in ms) and RTP timestamp (in 90KHz ticks).
  const int64_t capture_time_ms =
      capture_time_us / base::Time::kMicrosecondsPerMillisecond;

  const uint32_t rtp_timestamp = static_cast<uint32_t>(
      capture_time_us * 90 / base::Time::kMicrosecondsPerMillisecond);

  webrtc::EncodedImage image(
      reinterpret_cast<uint8_t*>(output_buffer->memory()), payload_size,
      output_buffer->mapped_size());
  image._encodedWidth = input_visible_size_.width();
  image._encodedHeight = input_visible_size_.height();
  image._timeStamp = rtp_timestamp;
  image.capture_time_ms_ = capture_time_ms;
  image._frameType =
      (key_frame ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta);
  image._completeFrame = true;

  ReturnEncodedImage(image, bitstream_buffer_id, picture_id_);
  // Picture ID must wrap after reaching the maximum.
  picture_id_ = (picture_id_ + 1) & 0x7FFF;
}

void RTCVideoEncoder::Impl::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int32_t retval;
  switch (error) {
    case media::VideoEncodeAccelerator::kInvalidArgumentError:
      retval = WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      break;
    default:
      retval = WEBRTC_VIDEO_CODEC_ERROR;
  }

  video_encoder_.reset();

  SetStatus(retval);
  if (async_waiter_)
    SignalAsyncWaiter(retval);
}

RTCVideoEncoder::Impl::~Impl() { DCHECK(!video_encoder_); }

void RTCVideoEncoder::Impl::LogAndNotifyError(
    const tracked_objects::Location& location,
    const std::string& str,
    media::VideoEncodeAccelerator::Error error) {
  static const char* const kErrorNames[] = {
      "kIllegalStateError", "kInvalidArgumentError", "kPlatformFailureError"};
  static_assert(
      arraysize(kErrorNames) == media::VideoEncodeAccelerator::kErrorMax + 1,
      "Different number of errors and textual descriptions");
  DLOG(ERROR) << location.ToString() << kErrorNames[error] << " - " << str;
  NotifyError(error);
}

void RTCVideoEncoder::Impl::EncodeOneFrame() {
  DVLOG(3) << "Impl::EncodeOneFrame()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(input_next_frame_);
  DCHECK(!input_buffers_free_.empty());

  // EncodeOneFrame() may re-enter EncodeFrameFinished() if VEA::Encode() fails,
  // we receive a VEA::NotifyError(), and the media::VideoFrame we pass to
  // Encode() gets destroyed early.  Handle this by resetting our
  // input_next_frame_* state before we hand off the VideoFrame to the VEA.
  const webrtc::VideoFrame* next_frame = input_next_frame_;
  const bool next_frame_keyframe = input_next_frame_keyframe_;
  input_next_frame_ = NULL;
  input_next_frame_keyframe_ = false;

  if (!video_encoder_) {
    SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  const int index = input_buffers_free_.back();
  bool requires_copy = false;
  scoped_refptr<media::VideoFrame> frame;
  if (next_frame->video_frame_buffer()->native_handle()) {
    frame = static_cast<media::VideoFrame*>(
        next_frame->video_frame_buffer()->native_handle());
    requires_copy = RequiresSizeChange(frame);
  } else {
    requires_copy = true;
  }

  if (requires_copy) {
    base::SharedMemory* input_buffer = input_buffers_[index];
    frame = media::VideoFrame::WrapExternalSharedMemory(
        media::PIXEL_FORMAT_I420, input_frame_coded_size_,
        gfx::Rect(input_visible_size_), input_visible_size_,
        reinterpret_cast<uint8_t*>(input_buffer->memory()),
        input_buffer->mapped_size(), input_buffer->handle(), 0,
        base::TimeDelta::FromMilliseconds(next_frame->ntp_time_ms()));
    if (!frame.get()) {
      LogAndNotifyError(FROM_HERE, "failed to create frame",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    // Do a strided copy of the input frame to match the input requirements for
    // the encoder.
    // TODO(sheu): support zero-copy from WebRTC.  http://crbug.com/269312
    if (libyuv::I420Copy(next_frame->video_frame_buffer()->DataY(),
                         next_frame->video_frame_buffer()->StrideY(),
                         next_frame->video_frame_buffer()->DataU(),
                         next_frame->video_frame_buffer()->StrideU(),
                         next_frame->video_frame_buffer()->DataV(),
                         next_frame->video_frame_buffer()->StrideV(),
                         frame->data(media::VideoFrame::kYPlane),
                         frame->stride(media::VideoFrame::kYPlane),
                         frame->data(media::VideoFrame::kUPlane),
                         frame->stride(media::VideoFrame::kUPlane),
                         frame->data(media::VideoFrame::kVPlane),
                         frame->stride(media::VideoFrame::kVPlane),
                         next_frame->width(), next_frame->height())) {
      LogAndNotifyError(FROM_HERE, "Failed to copy buffer",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
  }
  frame->AddDestructionObserver(media::BindToCurrentLoop(
      base::Bind(&RTCVideoEncoder::Impl::EncodeFrameFinished, this, index)));
  video_encoder_->Encode(frame, next_frame_keyframe);
  input_buffers_free_.pop_back();
  SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::EncodeFrameFinished(int index) {
  DVLOG(3) << "Impl::EncodeFrameFinished(): index=" << index;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(input_buffers_.size()));
  input_buffers_free_.push_back(index);
  if (input_next_frame_)
    EncodeOneFrame();
}

void RTCVideoEncoder::Impl::RegisterAsyncWaiter(base::WaitableEvent* waiter,
                                                int32_t* retval) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!async_waiter_);
  DCHECK(!async_retval_);
  async_waiter_ = waiter;
  async_retval_ = retval;
}

void RTCVideoEncoder::Impl::SignalAsyncWaiter(int32_t retval) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *async_retval_ = retval;
  async_waiter_->Signal();
  async_retval_ = NULL;
  async_waiter_ = NULL;
}

bool RTCVideoEncoder::Impl::IsBitrateTooHigh(uint32_t bitrate) {
  if (base::IsValueInRangeForNumericType<uint32_t>(bitrate * UINT64_C(1000)))
    return false;
  LogAndNotifyError(FROM_HERE, "Overflow converting bitrate from kbps to bps",
                    media::VideoEncodeAccelerator::kInvalidArgumentError);
  return true;
}

bool RTCVideoEncoder::Impl::RequiresSizeChange(
    const scoped_refptr<media::VideoFrame>& frame) const {
  return (frame->coded_size() != input_frame_coded_size_ ||
          frame->visible_rect() != gfx::Rect(input_visible_size_));
}

void RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback(
    base::WaitableEvent* async_waiter,
    int32_t* async_retval,
    webrtc::EncodedImageCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "RegisterEncodeCompleteCallback()";
  RegisterAsyncWaiter(async_waiter, async_retval);
  int32_t retval = GetStatus();
  if (retval == WEBRTC_VIDEO_CODEC_OK)
    encoded_image_callback_ = callback;
  SignalAsyncWaiter(retval);
}

void RTCVideoEncoder::Impl::ReturnEncodedImage(
    const webrtc::EncodedImage& image,
    int32_t bitstream_buffer_id,
    uint16_t picture_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "ReturnEncodedImage(): "
           << "bitstream_buffer_id=" << bitstream_buffer_id
           << ", picture_id=" << picture_id;

  if (!encoded_image_callback_)
    return;

  webrtc::RTPFragmentationHeader header;
  memset(&header, 0, sizeof(header));
  switch (video_codec_type_) {
    case webrtc::kVideoCodecVP8:
      // Generate a header describing a single fragment.
      header.VerifyAndAllocateFragmentationHeader(1);
      header.fragmentationOffset[0] = 0;
      header.fragmentationLength[0] = image._length;
      header.fragmentationPlType[0] = 0;
      header.fragmentationTimeDiff[0] = 0;
      break;
    case webrtc::kVideoCodecH264:
      if (!GetRTPFragmentationHeaderH264(&header, image._buffer,
                                         image._length)) {
        DLOG(ERROR) << "Failed to get RTP fragmentation header for H264";
        NotifyError(
            (media::VideoEncodeAccelerator::Error)WEBRTC_VIDEO_CODEC_ERROR);
        return;
      }
      break;
    default:
      NOTREACHED() << "Invalid video codec type";
      return;
  }

  webrtc::CodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.codecType = video_codec_type_;
  if (video_codec_type_ == webrtc::kVideoCodecVP8) {
    info.codecSpecific.VP8.pictureId = picture_id;
    info.codecSpecific.VP8.tl0PicIdx = -1;
    info.codecSpecific.VP8.keyIdx = -1;
  }

  const int32_t retval =
      encoded_image_callback_->Encoded(image, &info, &header);
  if (retval < 0) {
    DVLOG(2) << "ReturnEncodedImage(): encoded_image_callback_ returned "
             << retval;
  }

  UseOutputBitstreamBufferId(bitstream_buffer_id);
}

RTCVideoEncoder::RTCVideoEncoder(
    webrtc::VideoCodecType type,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : video_codec_type_(type),
      gpu_factories_(gpu_factories),
      gpu_task_runner_(gpu_factories->GetTaskRunner()) {
  DVLOG(1) << "RTCVideoEncoder(): codec type=" << type;
}

RTCVideoEncoder::~RTCVideoEncoder() {
  DVLOG(3) << "~RTCVideoEncoder";
  Release();
  DCHECK(!impl_.get());
}

int32_t RTCVideoEncoder::InitEncode(const webrtc::VideoCodec* codec_settings,
                                    int32_t number_of_cores,
                                    size_t max_payload_size) {
  DVLOG(1) << "InitEncode(): codecType=" << codec_settings->codecType
           << ", width=" << codec_settings->width
           << ", height=" << codec_settings->height
           << ", startBitrate=" << codec_settings->startBitrate;
  DCHECK(!impl_.get());

  impl_ = new Impl(gpu_factories_, video_codec_type_);
  const media::VideoCodecProfile profile = WebRTCVideoCodecToVideoCodecProfile(
      impl_->video_codec_type(), codec_settings);

  base::WaitableEvent initialization_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t initialization_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::CreateAndInitializeVEA,
                 impl_,
                 gfx::Size(codec_settings->width, codec_settings->height),
                 codec_settings->startBitrate,
                 profile,
                 &initialization_waiter,
                 &initialization_retval));

  // webrtc::VideoEncoder expects this call to be synchronous.
  initialization_waiter.Wait();
  RecordInitEncodeUMA(initialization_retval, profile);
  return initialization_retval;
}

int32_t RTCVideoEncoder::Encode(
    const webrtc::VideoFrame& input_image,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    const std::vector<webrtc::FrameType>* frame_types) {
  DVLOG(3) << "Encode()";
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  const bool want_key_frame = frame_types && frame_types->size() &&
                              frame_types->front() == webrtc::kVideoFrameKey;
  base::WaitableEvent encode_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t encode_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::Enqueue,
                 impl_,
                 &input_image,
                 want_key_frame,
                 &encode_waiter,
                 &encode_retval));

  // webrtc::VideoEncoder expects this call to be synchronous.
  encode_waiter.Wait();
  DVLOG(3) << "Encode(): returning encode_retval=" << encode_retval;
  return encode_retval;
}

int32_t RTCVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DVLOG(3) << "RegisterEncodeCompleteCallback()";
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  base::WaitableEvent register_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t register_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback, impl_,
                 &register_waiter, &register_retval, callback));
  register_waiter.Wait();
  return register_retval;
}

int32_t RTCVideoEncoder::Release() {
  DVLOG(3) << "Release()";
  if (!impl_.get())
    return WEBRTC_VIDEO_CODEC_OK;

  base::WaitableEvent release_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::Destroy, impl_, &release_waiter));
  release_waiter.Wait();
  impl_ = NULL;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::SetChannelParameters(uint32_t packet_loss,
                                              int64_t rtt) {
  DVLOG(3) << "SetChannelParameters(): packet_loss=" << packet_loss
           << ", rtt=" << rtt;
  // Ignored.
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::SetRates(uint32_t new_bit_rate, uint32_t frame_rate) {
  DVLOG(3) << "SetRates(): new_bit_rate=" << new_bit_rate
           << ", frame_rate=" << frame_rate;
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  const int32_t retval = impl_->GetStatus();
  if (retval != WEBRTC_VIDEO_CODEC_OK) {
    DVLOG(3) << "SetRates(): returning " << retval;
    return retval;
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::RequestEncodingParametersChange,
                 impl_,
                 new_bit_rate,
                 frame_rate));
  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoEncoder::RecordInitEncodeUMA(
    int32_t init_retval, media::VideoCodecProfile profile) {
  UMA_HISTOGRAM_BOOLEAN("Media.RTCVideoEncoderInitEncodeSuccess",
                        init_retval == WEBRTC_VIDEO_CODEC_OK);
  if (init_retval == WEBRTC_VIDEO_CODEC_OK) {
    UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoEncoderProfile",
                              profile,
                              media::VIDEO_CODEC_PROFILE_MAX + 1);
  }
}

}  // namespace content
