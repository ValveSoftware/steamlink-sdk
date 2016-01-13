// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_encoder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/webrtc/system_wrappers/interface/tick_util.h"

#define NOTIFY_ERROR(x)                             \
  do {                                              \
    DLOG(ERROR) << "calling NotifyError(): " << x;  \
    NotifyError(x);                                 \
  } while (0)

namespace content {

// This private class of RTCVideoEncoder does the actual work of communicating
// with a media::VideoEncodeAccelerator for handling video encoding.  It can
// be created on any thread, but should subsequently be posted to (and Destroy()
// called on) a single thread.  Callbacks to RTCVideoEncoder are posted to the
// thread on which the instance was constructed.
//
// This class separates state related to the thread that RTCVideoEncoder
// operates on (presently the libjingle worker thread) from the thread that
// |gpu_factories_| provides for accelerator operations (presently the media
// thread).  The RTCVideoEncoder class can be deleted directly by WebRTC, while
// RTCVideoEncoder::Impl stays around long enough to properly shut down the VEA.
class RTCVideoEncoder::Impl
    : public media::VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<RTCVideoEncoder::Impl> {
 public:
  Impl(const base::WeakPtr<RTCVideoEncoder>& weak_encoder,
       const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories);

  // Create the VEA and call Initialize() on it.  Called once per instantiation,
  // and then the instance is bound forevermore to whichever thread made the
  // call.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |async_waiter| and |async_retval| arguments.
  void CreateAndInitializeVEA(const gfx::Size& input_visible_size,
                              uint32 bitrate,
                              media::VideoCodecProfile profile,
                              base::WaitableEvent* async_waiter,
                              int32_t* async_retval);
  // Enqueue a frame from WebRTC for encoding.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |async_waiter| and |async_retval| arguments.
  void Enqueue(const webrtc::I420VideoFrame* input_frame,
               bool force_keyframe,
               base::WaitableEvent* async_waiter,
               int32_t* async_retval);

  // RTCVideoEncoder is given a buffer to be passed to WebRTC through the
  // RTCVideoEncoder::ReturnEncodedImage() function.  When that is complete,
  // the buffer is returned to Impl by its index using this function.
  void UseOutputBitstreamBufferId(int32 bitstream_buffer_id);

  // Request encoding parameter change for the underlying encoder.
  void RequestEncodingParametersChange(uint32 bitrate, uint32 framerate);

  // Destroy this Impl's encoder.  The destructor is not explicitly called, as
  // Impl is a base::RefCountedThreadSafe.
  void Destroy();

  // media::VideoEncodeAccelerator::Client implementation.
  virtual void RequireBitstreamBuffers(unsigned int input_count,
                                       const gfx::Size& input_coded_size,
                                       size_t output_buffer_size) OVERRIDE;
  virtual void BitstreamBufferReady(int32 bitstream_buffer_id,
                                    size_t payload_size,
                                    bool key_frame) OVERRIDE;
  virtual void NotifyError(media::VideoEncodeAccelerator::Error error) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Impl>;

  enum {
    kInputBufferExtraCount = 1,  // The number of input buffers allocated, more
                                 // than what is requested by
                                 // VEA::RequireBitstreamBuffers().
    kOutputBufferCount = 3,
  };

  virtual ~Impl();

  // Perform encoding on an input frame from the input queue.
  void EncodeOneFrame();

  // Notify that an input frame is finished for encoding.  |index| is the index
  // of the completed frame in |input_buffers_|.
  void EncodeFrameFinished(int index);

  // Set up/signal |async_waiter_| and |async_retval_|; see declarations below.
  void RegisterAsyncWaiter(base::WaitableEvent* waiter, int32_t* retval);
  void SignalAsyncWaiter(int32_t retval);

  base::ThreadChecker thread_checker_;

  // Weak pointer to the parent RTCVideoEncoder, for posting back VEA::Client
  // notifications.
  const base::WeakPtr<RTCVideoEncoder> weak_encoder_;

  // The message loop on which to post callbacks to |weak_encoder_|.
  const scoped_refptr<base::MessageLoopProxy> encoder_message_loop_proxy_;

  // Factory for creating VEAs, shared memory buffers, etc.
  const scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories_;

  // webrtc::VideoEncoder expects InitEncode() and Encode() to be synchronous.
  // Do this by waiting on the |async_waiter_| and returning the return value in
  // |async_retval_| when initialization completes, encoding completes, or
  // an error occurs.
  base::WaitableEvent* async_waiter_;
  int32_t* async_retval_;

  // The underlying VEA to perform encoding on.
  scoped_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Next input frame.  Since there is at most one next frame, a single-element
  // queue is sufficient.
  const webrtc::I420VideoFrame* input_next_frame_;

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
  uint16 picture_id_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

RTCVideoEncoder::Impl::Impl(
    const base::WeakPtr<RTCVideoEncoder>& weak_encoder,
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories)
    : weak_encoder_(weak_encoder),
      encoder_message_loop_proxy_(base::MessageLoopProxy::current()),
      gpu_factories_(gpu_factories),
      async_waiter_(NULL),
      async_retval_(NULL),
      input_next_frame_(NULL),
      input_next_frame_keyframe_(false),
      output_buffers_free_count_(0) {
  thread_checker_.DetachFromThread();
  // Picture ID should start on a random number.
  picture_id_ = static_cast<uint16_t>(base::RandInt(0, 0x7FFF));
}

void RTCVideoEncoder::Impl::CreateAndInitializeVEA(
    const gfx::Size& input_visible_size,
    uint32 bitrate,
    media::VideoCodecProfile profile,
    base::WaitableEvent* async_waiter,
    int32_t* async_retval) {
  DVLOG(3) << "Impl::CreateAndInitializeVEA()";
  DCHECK(thread_checker_.CalledOnValidThread());

  RegisterAsyncWaiter(async_waiter, async_retval);

  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  if (bitrate > kuint32max / 1000) {
    NOTIFY_ERROR(media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }

  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator().Pass();
  if (!video_encoder_) {
    NOTIFY_ERROR(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  input_visible_size_ = input_visible_size;
  if (!video_encoder_->Initialize(media::VideoFrame::I420,
                                  input_visible_size_,
                                  profile,
                                  bitrate * 1000,
                                  this)) {
    NOTIFY_ERROR(media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }
}

void RTCVideoEncoder::Impl::Enqueue(const webrtc::I420VideoFrame* input_frame,
                                    bool force_keyframe,
                                    base::WaitableEvent* async_waiter,
                                    int32_t* async_retval) {
  DVLOG(3) << "Impl::Enqueue()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!input_next_frame_);

  RegisterAsyncWaiter(async_waiter, async_retval);
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
    int32 bitstream_buffer_id) {
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

void RTCVideoEncoder::Impl::RequestEncodingParametersChange(uint32 bitrate,
                                                            uint32 framerate) {
  DVLOG(3) << "Impl::RequestEncodingParametersChange(): bitrate=" << bitrate
           << ", framerate=" << framerate;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  if (bitrate > kuint32max / 1000) {
    NOTIFY_ERROR(media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }

  if (video_encoder_)
    video_encoder_->RequestEncodingParametersChange(bitrate * 1000, framerate);
}

void RTCVideoEncoder::Impl::Destroy() {
  DVLOG(3) << "Impl::Destroy()";
  DCHECK(thread_checker_.CalledOnValidThread());
  video_encoder_.reset();
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
    base::SharedMemory* shm =
        gpu_factories_->CreateSharedMemory(media::VideoFrame::AllocationSize(
            media::VideoFrame::I420, input_coded_size));
    if (!shm) {
      DLOG(ERROR) << "Impl::RequireBitstreamBuffers(): "
                     "failed to create input buffer " << i;
      NOTIFY_ERROR(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    input_buffers_.push_back(shm);
    input_buffers_free_.push_back(i);
  }

  for (int i = 0; i < kOutputBufferCount; ++i) {
    base::SharedMemory* shm =
        gpu_factories_->CreateSharedMemory(output_buffer_size);
    if (!shm) {
      DLOG(ERROR) << "Impl::RequireBitstreamBuffers(): "
                     "failed to create output buffer " << i;
      NOTIFY_ERROR(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    output_buffers_.push_back(shm);
  }

  // Immediately provide all output buffers to the VEA.
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        i, output_buffers_[i]->handle(), output_buffers_[i]->mapped_size()));
    output_buffers_free_count_++;
  }
  SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::BitstreamBufferReady(int32 bitstream_buffer_id,
                                                 size_t payload_size,
                                                 bool key_frame) {
  DVLOG(3) << "Impl::BitstreamBufferReady(): "
              "bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << payload_size
           << ", key_frame=" << key_frame;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (bitstream_buffer_id < 0 ||
      bitstream_buffer_id >= static_cast<int>(output_buffers_.size())) {
    DLOG(ERROR) << "Impl::BitstreamBufferReady(): invalid bitstream_buffer_id="
                << bitstream_buffer_id;
    NOTIFY_ERROR(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  base::SharedMemory* output_buffer = output_buffers_[bitstream_buffer_id];
  if (payload_size > output_buffer->mapped_size()) {
    DLOG(ERROR) << "Impl::BitstreamBufferReady(): invalid payload_size="
                << payload_size;
    NOTIFY_ERROR(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  output_buffers_free_count_--;

  // Use webrtc timestamps to ensure correct RTP sender behavior.
  // TODO(hshi): obtain timestamp from the capturer, see crbug.com/350106.
  const int64 capture_time_us = webrtc::TickTime::MicrosecondTimestamp();

  // Derive the capture time (in ms) and RTP timestamp (in 90KHz ticks).
  int64 capture_time_ms = capture_time_us / 1000;
  uint32_t rtp_timestamp = static_cast<uint32_t>(capture_time_us * 90 / 1000);

  scoped_ptr<webrtc::EncodedImage> image(new webrtc::EncodedImage(
      reinterpret_cast<uint8_t*>(output_buffer->memory()),
      payload_size,
      output_buffer->mapped_size()));
  image->_encodedWidth = input_visible_size_.width();
  image->_encodedHeight = input_visible_size_.height();
  image->_timeStamp = rtp_timestamp;
  image->capture_time_ms_ = capture_time_ms;
  image->_frameType = (key_frame ? webrtc::kKeyFrame : webrtc::kDeltaFrame);
  image->_completeFrame = true;

  encoder_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::ReturnEncodedImage,
                 weak_encoder_,
                 base::Passed(&image),
                 bitstream_buffer_id,
                 picture_id_));
  // Picture ID must wrap after reaching the maximum.
  picture_id_ = (picture_id_ + 1) & 0x7FFF;
}

void RTCVideoEncoder::Impl::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DVLOG(3) << "Impl::NotifyError(): error=" << error;
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

  if (async_waiter_) {
    SignalAsyncWaiter(retval);
  } else {
    encoder_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoEncoder::NotifyError, weak_encoder_, retval));
  }
}

RTCVideoEncoder::Impl::~Impl() { DCHECK(!video_encoder_); }

void RTCVideoEncoder::Impl::EncodeOneFrame() {
  DVLOG(3) << "Impl::EncodeOneFrame()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(input_next_frame_);
  DCHECK(!input_buffers_free_.empty());

  // EncodeOneFrame() may re-enter EncodeFrameFinished() if VEA::Encode() fails,
  // we receive a VEA::NotifyError(), and the media::VideoFrame we pass to
  // Encode() gets destroyed early.  Handle this by resetting our
  // input_next_frame_* state before we hand off the VideoFrame to the VEA.
  const webrtc::I420VideoFrame* next_frame = input_next_frame_;
  bool next_frame_keyframe = input_next_frame_keyframe_;
  input_next_frame_ = NULL;
  input_next_frame_keyframe_ = false;

  if (!video_encoder_) {
    SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  const int index = input_buffers_free_.back();
  base::SharedMemory* input_buffer = input_buffers_[index];
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalPackedMemory(
          media::VideoFrame::I420,
          input_frame_coded_size_,
          gfx::Rect(input_visible_size_),
          input_visible_size_,
          reinterpret_cast<uint8*>(input_buffer->memory()),
          input_buffer->mapped_size(),
          input_buffer->handle(),
          base::TimeDelta(),
          base::Bind(&RTCVideoEncoder::Impl::EncodeFrameFinished, this, index));
  if (!frame) {
    DLOG(ERROR) << "Impl::EncodeOneFrame(): failed to create frame";
    NOTIFY_ERROR(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  // Do a strided copy of the input frame to match the input requirements for
  // the encoder.
  // TODO(sheu): support zero-copy from WebRTC.  http://crbug.com/269312
  media::CopyYPlane(next_frame->buffer(webrtc::kYPlane),
                    next_frame->stride(webrtc::kYPlane),
                    next_frame->height(),
                    frame.get());
  media::CopyUPlane(next_frame->buffer(webrtc::kUPlane),
                    next_frame->stride(webrtc::kUPlane),
                    next_frame->height(),
                    frame.get());
  media::CopyVPlane(next_frame->buffer(webrtc::kVPlane),
                    next_frame->stride(webrtc::kVPlane),
                    next_frame->height(),
                    frame.get());

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

#undef NOTIFY_ERROR

////////////////////////////////////////////////////////////////////////////////
//
// RTCVideoEncoder
//
////////////////////////////////////////////////////////////////////////////////

RTCVideoEncoder::RTCVideoEncoder(
    webrtc::VideoCodecType type,
    media::VideoCodecProfile profile,
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories)
    : video_codec_type_(type),
      video_codec_profile_(profile),
      gpu_factories_(gpu_factories),
      encoded_image_callback_(NULL),
      impl_status_(WEBRTC_VIDEO_CODEC_UNINITIALIZED),
      weak_factory_(this) {
  DVLOG(1) << "RTCVideoEncoder(): profile=" << profile;
}

RTCVideoEncoder::~RTCVideoEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Release();
  DCHECK(!impl_);
}

int32_t RTCVideoEncoder::InitEncode(const webrtc::VideoCodec* codec_settings,
                                    int32_t number_of_cores,
                                    uint32_t max_payload_size) {
  DVLOG(1) << "InitEncode(): codecType=" << codec_settings->codecType
           << ", width=" << codec_settings->width
           << ", height=" << codec_settings->height
           << ", startBitrate=" << codec_settings->startBitrate;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!impl_);

  weak_factory_.InvalidateWeakPtrs();
  impl_ = new Impl(weak_factory_.GetWeakPtr(), gpu_factories_);
  base::WaitableEvent initialization_waiter(true, false);
  int32_t initialization_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  gpu_factories_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::CreateAndInitializeVEA,
                 impl_,
                 gfx::Size(codec_settings->width, codec_settings->height),
                 codec_settings->startBitrate,
                 video_codec_profile_,
                 &initialization_waiter,
                 &initialization_retval));

  // webrtc::VideoEncoder expects this call to be synchronous.
  initialization_waiter.Wait();
  RecordInitEncodeUMA(initialization_retval);
  return initialization_retval;
}

int32_t RTCVideoEncoder::Encode(
    const webrtc::I420VideoFrame& input_image,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DVLOG(3) << "Encode()";
  // TODO(sheu): figure out why this check fails.
  // DCHECK(thread_checker_.CalledOnValidThread());
  if (!impl_) {
    DVLOG(3) << "Encode(): returning impl_status_=" << impl_status_;
    return impl_status_;
  }

  bool want_key_frame = frame_types && frame_types->size() &&
                        frame_types->front() == webrtc::kKeyFrame;
  base::WaitableEvent encode_waiter(true, false);
  int32_t encode_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  gpu_factories_->GetTaskRunner()->PostTask(
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
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!impl_) {
    DVLOG(3) << "RegisterEncodeCompleteCallback(): returning " << impl_status_;
    return impl_status_;
  }

  encoded_image_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::Release() {
  DVLOG(3) << "Release()";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (impl_) {
    gpu_factories_->GetTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&RTCVideoEncoder::Impl::Destroy, impl_));
    impl_ = NULL;
    weak_factory_.InvalidateWeakPtrs();
    impl_status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::SetChannelParameters(uint32_t packet_loss, int rtt) {
  DVLOG(3) << "SetChannelParameters(): packet_loss=" << packet_loss
           << ", rtt=" << rtt;
  DCHECK(thread_checker_.CalledOnValidThread());
  // Ignored.
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::SetRates(uint32_t new_bit_rate, uint32_t frame_rate) {
  DVLOG(3) << "SetRates(): new_bit_rate=" << new_bit_rate
           << ", frame_rate=" << frame_rate;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!impl_) {
    DVLOG(3) << "SetRates(): returning " << impl_status_;
    return impl_status_;
  }

  gpu_factories_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::RequestEncodingParametersChange,
                 impl_,
                 new_bit_rate,
                 frame_rate));
  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoEncoder::ReturnEncodedImage(scoped_ptr<webrtc::EncodedImage> image,
                                         int32 bitstream_buffer_id,
                                         uint16 picture_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "ReturnEncodedImage(): "
           << "bitstream_buffer_id=" << bitstream_buffer_id
           << ", picture_id=" << picture_id;

  if (!encoded_image_callback_)
    return;

  webrtc::CodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.codecType = video_codec_type_;
  if (video_codec_type_ == webrtc::kVideoCodecVP8) {
    info.codecSpecific.VP8.pictureId = picture_id;
    info.codecSpecific.VP8.tl0PicIdx = -1;
    info.codecSpecific.VP8.keyIdx = -1;
  }

  // Generate a header describing a single fragment.
  webrtc::RTPFragmentationHeader header;
  memset(&header, 0, sizeof(header));
  header.VerifyAndAllocateFragmentationHeader(1);
  header.fragmentationOffset[0] = 0;
  header.fragmentationLength[0] = image->_length;
  header.fragmentationPlType[0] = 0;
  header.fragmentationTimeDiff[0] = 0;

  int32_t retval = encoded_image_callback_->Encoded(*image, &info, &header);
  if (retval < 0) {
    DVLOG(2) << "ReturnEncodedImage(): encoded_image_callback_ returned "
             << retval;
  }

  // The call through webrtc::EncodedImageCallback is synchronous, so we can
  // immediately recycle the output buffer back to the Impl.
  gpu_factories_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoEncoder::Impl::UseOutputBitstreamBufferId,
                 impl_,
                 bitstream_buffer_id));
}

void RTCVideoEncoder::NotifyError(int32_t error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "NotifyError(): error=" << error;

  impl_status_ = error;
  gpu_factories_->GetTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RTCVideoEncoder::Impl::Destroy, impl_));
  impl_ = NULL;
}

void RTCVideoEncoder::RecordInitEncodeUMA(int32_t init_retval) {
  UMA_HISTOGRAM_BOOLEAN("Media.RTCVideoEncoderInitEncodeSuccess",
                        init_retval == WEBRTC_VIDEO_CODEC_OK);
  if (init_retval == WEBRTC_VIDEO_CODEC_OK) {
    UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoEncoderProfile",
                              video_codec_profile_,
                              media::VIDEO_CODEC_PROFILE_MAX + 1);
  }
}

}  // namespace content
