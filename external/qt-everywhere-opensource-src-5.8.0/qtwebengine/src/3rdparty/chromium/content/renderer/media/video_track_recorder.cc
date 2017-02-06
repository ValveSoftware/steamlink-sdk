// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_track_recorder.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(RTC_USE_H264)
#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"
#endif  // #if BUILDFLAG(RTC_USE_H264)

extern "C" {
// VPX_CODEC_DISABLE_COMPAT excludes parts of the libvpx API that provide
// backwards compatibility for legacy applications using the library.
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
}

using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

namespace {

const int kVEAEncoderMinResolutionWidth = 640;
const int kVEAEncoderMinResolutionHeight = 480;
const int kVEAEncoderOutputBufferCount = 4;

static struct {
  VideoTrackRecorder::CodecId codec_id;
  media::VideoCodecProfile min_profile;
  media::VideoCodecProfile max_profile;
} const kSupportedVideoCodecIdToProfile[] = {
    {VideoTrackRecorder::CodecId::VP8,
     media::VP8PROFILE_MIN,
     media::VP8PROFILE_MAX},
    {VideoTrackRecorder::CodecId::VP9,
     media::VP9PROFILE_MIN,
     media::VP9PROFILE_MAX},
    {VideoTrackRecorder::CodecId::H264,
     media::H264PROFILE_MIN,
     media::H264PROFILE_MAX}};

// Returns the corresponding codec profile from VEA supported codecs. If no
// profile is found, returns VIDEO_CODEC_PROFILE_UNKNOWN.
media::VideoCodecProfile CodecIdToVEAProfile(
    content::VideoTrackRecorder::CodecId codec) {
  // See https://crbug.com/616659.
#if defined(OS_CHROMEOS)
  return media::VIDEO_CODEC_PROFILE_UNKNOWN;
#endif  // defined(OS_CHROMEOS)
  content::RenderThreadImpl* render_thread_impl =
      content::RenderThreadImpl::current();
  if (!render_thread_impl)
    return media::VIDEO_CODEC_PROFILE_UNKNOWN;

  media::GpuVideoAcceleratorFactories* gpu_factories =
      content::RenderThreadImpl::current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    DVLOG(3) << "Couldn't initialize GpuVideoAcceleratorFactories";
    return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }

  const media::VideoEncodeAccelerator::SupportedProfiles& vea_profiles =
      gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles();
  for (const auto& vea_profile : vea_profiles) {
    for (const auto& supported_profile : kSupportedVideoCodecIdToProfile) {
      if (codec == supported_profile.codec_id &&
          vea_profile.profile >= supported_profile.min_profile &&
          vea_profile.profile <= supported_profile.max_profile)
        return vea_profile.profile;
    }
  }
  return media::VIDEO_CODEC_PROFILE_UNKNOWN;
}

}  // anonymous namespace

// Base class to describe a generic Encoder, encapsulating all actual encoder
// (re)configurations, encoding and delivery of received frames. This class is
// ref-counted to allow the MediaStreamVideoTrack to hold a reference to it (via
// the callback that MediaStreamVideoSink passes along) and to jump back and
// forth to an internal encoder thread. Moreover, this class:
// - is created and destroyed on its parent's thread (usually the main Render
// thread), |main_task_runner_|.
// - receives VideoFrames on |origin_task_runner_| and runs OnEncodedVideoCB on
// that thread as well. This task runner is cached on first frame arrival, and
// is supposed to be the render IO thread (but this is not enforced);
// - uses an internal |encoding_task_runner_| for actual encoder interactions,
// namely configuration, encoding (which might take some time) and destruction.
// This task runner can be passed on the creation. If nothing is passed, a new
// encoding thread is created and used.
class VideoTrackRecorder::Encoder : public base::RefCountedThreadSafe<Encoder> {
 public:
  Encoder(const OnEncodedVideoCB& on_encoded_video_callback,
          int32_t bits_per_second,
          scoped_refptr<base::SingleThreadTaskRunner> encoding_task_runner =
              nullptr)
      : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        encoding_task_runner_(encoding_task_runner),
        paused_(false),
        on_encoded_video_callback_(on_encoded_video_callback),
        bits_per_second_(bits_per_second) {
    DCHECK(!on_encoded_video_callback_.is_null());
    if (encoding_task_runner_)
      return;
    encoding_thread_.reset(new base::Thread("EncodingThread"));
    encoding_thread_->Start();
    encoding_task_runner_ = encoding_thread_->task_runner();
  }

  // Start encoding |frame|, returning via |on_encoded_video_callback_|. This
  // call will also trigger a ConfigureEncoderOnEncodingTaskRunner() upon first
  // frame arrival or parameter change, and an EncodeOnEncodingTaskRunner() to
  // actually encode the frame.
  void StartFrameEncode(const scoped_refptr<VideoFrame>& frame,
                        base::TimeTicks capture_timestamp);

  void SetPaused(bool paused);

 protected:
  friend class base::RefCountedThreadSafe<Encoder>;
  virtual ~Encoder() {}

  virtual void EncodeOnEncodingTaskRunner(
      const scoped_refptr<VideoFrame>& frame,
      base::TimeTicks capture_timestamp) = 0;
  virtual void ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) = 0;

  // Used to shutdown properly on the same thread we were created.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Task runner where frames to encode and reply callbacks must happen.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  // Task runner where encoding interactions happen.
  scoped_refptr<base::SingleThreadTaskRunner> encoding_task_runner_;

  // Optional thread for encoding. Active for the lifetime of VpxEncoder.
  std::unique_ptr<base::Thread> encoding_thread_;

  // While |paused_|, frames are not encoded. Used only from |encoding_thread_|.
  bool paused_;

  // This callback should be exercised on IO thread.
  const OnEncodedVideoCB on_encoded_video_callback_;

  // Target bitrate for video encoding. If 0, a standard bitrate is used.
  const int32_t bits_per_second_;

  DISALLOW_COPY_AND_ASSIGN(Encoder);
};

void VideoTrackRecorder::Encoder::StartFrameEncode(
    const scoped_refptr<VideoFrame>& video_frame,
    base::TimeTicks capture_timestamp) {
  // Cache the thread sending frames on first frame arrival.
  if (!origin_task_runner_.get())
    origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(origin_task_runner_->BelongsToCurrentThread());
  if (paused_)
    return;

  if (!(video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_YV12 ||
        video_frame->format() == media::PIXEL_FORMAT_YV12A)) {
    NOTREACHED();
    return;
  }
  scoped_refptr<media::VideoFrame> frame = video_frame;
  // Drop alpha channel since we do not support it yet.
  if (frame->format() == media::PIXEL_FORMAT_YV12A)
    frame = media::WrapAsI420VideoFrame(video_frame);

  encoding_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Encoder::EncodeOnEncodingTaskRunner, this, frame,
                            capture_timestamp));
}

void VideoTrackRecorder::Encoder::SetPaused(bool paused) {
  if (!encoding_task_runner_->BelongsToCurrentThread()) {
    encoding_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Encoder::SetPaused, this, paused));
    return;
  }
  paused_ = paused;
}

namespace {

// Originally from remoting/codec/scoped_vpx_codec.h.
// TODO(mcasas): Refactor into a common location.
struct VpxCodecDeleter {
  void operator()(vpx_codec_ctx_t* codec) {
    if (!codec)
      return;
    vpx_codec_err_t ret = vpx_codec_destroy(codec);
    CHECK_EQ(ret, VPX_CODEC_OK);
    delete codec;
  }
};
typedef std::unique_ptr<vpx_codec_ctx_t, VpxCodecDeleter> ScopedVpxCodecCtxPtr;

static void OnFrameEncodeCompleted(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    const scoped_refptr<VideoFrame>& frame,
    std::unique_ptr<std::string> data,
    base::TimeTicks capture_timestamp,
    bool keyframe) {
  DVLOG(1) << (keyframe ? "" : "non ") << "keyframe "<< data->length() << "B, "
           << capture_timestamp << " ms";
  on_encoded_video_cb.Run(frame, std::move(data), capture_timestamp, keyframe);
}

static int GetNumberOfThreadsForEncoding() {
  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  return std::min(8, (base::SysInfo::NumberOfProcessors() + 1) / 2);
}

// Class encapsulating VideoEncodeAccelerator interactions.
// This class is created and destroyed in its owner thread. All other methods
// operate on the task runner pointed by GpuFactories.
class VEAEncoder final : public VideoTrackRecorder::Encoder,
                         public media::VideoEncodeAccelerator::Client {
 public:
  VEAEncoder(
      const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
      int32_t bits_per_second,
      media::VideoCodecProfile codec);

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
  using VideoFrameAndTimestamp =
      std::pair<scoped_refptr<VideoFrame>, base::TimeTicks>;

  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);
  void FrameFinished(std::unique_ptr<base::SharedMemory> shm);

  // VideoTrackRecorder::Encoder implementation.
  ~VEAEncoder() override;
  void EncodeOnEncodingTaskRunner(const scoped_refptr<VideoFrame>& frame,
                                  base::TimeTicks capture_timestamp) override;
  void ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) override;

  media::GpuVideoAcceleratorFactories* const gpu_factories_;

  const media::VideoCodecProfile codec_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Shared memory buffers for output with the VEA.
  std::vector<std::unique_ptr<base::SharedMemory>> output_buffers_;

  // Shared memory buffers for output with the VEA as FIFO.
  std::queue<std::unique_ptr<base::SharedMemory>> input_buffers_;

  // Tracks error status.
  bool error_notified_;

  // Tracks the first frame to encode.
  std::unique_ptr<VideoFrameAndTimestamp> first_frame_;

  // Size used to initialize encoder.
  gfx::Size input_size_;

  // Coded size that encoder requests as input.
  gfx::Size vea_requested_input_size_;

  // Frames and corresponding timestamps in encode as FIFO.
  std::queue<VideoFrameAndTimestamp> frames_in_encode_;
};

// Class encapsulating all libvpx interactions for VP8/VP9 encoding.
class VpxEncoder final : public VideoTrackRecorder::Encoder {
 public:
  static void ShutdownEncoder(std::unique_ptr<base::Thread> encoding_thread,
                              ScopedVpxCodecCtxPtr encoder);

  VpxEncoder(
      bool use_vp9,
      const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
      int32_t bits_per_second);

 private:
  // VideoTrackRecorder::Encoder implementation.
  ~VpxEncoder() override;
  void EncodeOnEncodingTaskRunner(const scoped_refptr<VideoFrame>& frame,
                                  base::TimeTicks capture_timestamp) override;
  void ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) override;

  // Returns true if |codec_config_| has been filled in at least once.
  bool IsInitialized() const;

  // Estimate the frame duration from |frame| and |last_frame_timestamp_|.
  base::TimeDelta EstimateFrameDuration(const scoped_refptr<VideoFrame>& frame);

  // Force usage of VP9 for encoding, instead of VP8 which is the default.
  const bool use_vp9_;

  // VPx internal objects: configuration and encoder. |encoder_| is a special
  // scoped pointer to guarantee proper destruction, particularly when
  // reconfiguring due to parameters change. Only used on |encoding_thread_|.
  vpx_codec_enc_cfg_t codec_config_;
  ScopedVpxCodecCtxPtr encoder_;

  // The |VideoFrame::timestamp()| of the last encoded frame.  This is used to
  // predict the duration of the next frame. Only used on |encoding_thread_|.
  base::TimeDelta last_frame_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VpxEncoder);
};

#if BUILDFLAG(RTC_USE_H264)

struct ISVCEncoderDeleter {
  void operator()(ISVCEncoder* codec) {
    if (!codec)
      return;
    const int uninit_ret = codec->Uninitialize();
    CHECK_EQ(cmResultSuccess, uninit_ret);
    WelsDestroySVCEncoder(codec);
  }
};
typedef std::unique_ptr<ISVCEncoder, ISVCEncoderDeleter> ScopedISVCEncoderPtr;

// Class encapsulating all openh264 interactions for H264 encoding.
class H264Encoder final : public VideoTrackRecorder::Encoder {
 public:
  static void ShutdownEncoder(std::unique_ptr<base::Thread> encoding_thread,
                              ScopedISVCEncoderPtr encoder);

  H264Encoder(
      const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
      int32_t bits_per_second);

 private:
  // VideoTrackRecorder::Encoder implementation.
  ~H264Encoder() override;
  void EncodeOnEncodingTaskRunner(const scoped_refptr<VideoFrame>& frame,
                              base::TimeTicks capture_timestamp) override;
  void ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) override;

  // |openh264_encoder_| is a special scoped pointer to guarantee proper
  // destruction, also when reconfiguring due to parameters change. Only used on
  // |encoding_thread_|.
  gfx::Size configured_size_;
  ScopedISVCEncoderPtr openh264_encoder_;

  // The |VideoFrame::timestamp()| of the first received frame. Only used on
  // |encoding_thread_|.
  base::TimeTicks first_frame_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(H264Encoder);
};

#endif  // #if BUILDFLAG(RTC_USE_H264)

VEAEncoder::VEAEncoder(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second,
    media::VideoCodecProfile codec)
    : Encoder(on_encoded_video_callback,
              bits_per_second,
              RenderThreadImpl::current()->GetGpuFactories()->GetTaskRunner()),
      gpu_factories_(RenderThreadImpl::current()->GetGpuFactories()),
      codec_(codec),
      error_notified_(false) {
  DCHECK(gpu_factories_);
}

VEAEncoder::~VEAEncoder() {
  encoding_task_runner_->PostTask(
      FROM_HERE, base::Bind(&media::VideoEncodeAccelerator::Destroy,
                            base::Unretained(video_encoder_.release())));
}

void VEAEncoder::RequireBitstreamBuffers(unsigned int /*input_count*/,
                                         const gfx::Size& input_coded_size,
                                         size_t output_buffer_size) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  vea_requested_input_size_ = input_coded_size;
  output_buffers_.clear();
  std::queue<std::unique_ptr<base::SharedMemory>>().swap(input_buffers_);

  for (int i = 0; i < kVEAEncoderOutputBufferCount; ++i) {
    std::unique_ptr<base::SharedMemory> shm =
        gpu_factories_->CreateSharedMemory(output_buffer_size);
    if (shm)
      output_buffers_.push_back(base::WrapUnique(shm.release()));
  }

  for (size_t i = 0; i < output_buffers_.size(); ++i)
    UseOutputBitstreamBufferId(i);
}

void VEAEncoder::BitstreamBufferReady(int32_t bitstream_buffer_id,
                                      size_t payload_size,
                                      bool keyframe,
                                      base::TimeDelta timestamp) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  base::SharedMemory* output_buffer =
      output_buffers_[bitstream_buffer_id].get();

  std::unique_ptr<std::string> data(new std::string);
  data->append(reinterpret_cast<char*>(output_buffer->memory()), payload_size);

  const auto front_frame = frames_in_encode_.front();
  frames_in_encode_.pop();
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(OnFrameEncodeCompleted, on_encoded_video_callback_,
                            front_frame.first, base::Passed(&data),
                            front_frame.second, keyframe));
  UseOutputBitstreamBufferId(bitstream_buffer_id);
}

void VEAEncoder::NotifyError(media::VideoEncodeAccelerator::Error error) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  // TODO(emircan): Notify the owner via a callback.
  error_notified_ = true;
}

void VEAEncoder::UseOutputBitstreamBufferId(int32_t bitstream_buffer_id) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_id, output_buffers_[bitstream_buffer_id]->handle(),
      output_buffers_[bitstream_buffer_id]->mapped_size()));
}

void VEAEncoder::FrameFinished(std::unique_ptr<base::SharedMemory> shm) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  input_buffers_.push(std::move(shm));
}

void VEAEncoder::EncodeOnEncodingTaskRunner(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks capture_timestamp) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  if (input_size_ != frame->visible_rect().size() && video_encoder_) {
    video_encoder_->Destroy();
    video_encoder_.reset();
  }

  if (!video_encoder_) {
    ConfigureEncoderOnEncodingTaskRunner(frame->visible_rect().size());
    first_frame_.reset(
        new std::pair<scoped_refptr<VideoFrame>, base::TimeTicks>(
            frame, capture_timestamp));
  }

  if (error_notified_) {
    DVLOG(3) << "An error occurred in VEA encoder";
    return;
  }

  // Drop frames if there is no output buffers available.
  if (output_buffers_.empty()) {
    // TODO(emircan): Investigate if resetting encoder would help.
    DVLOG(3) << "Dropped frame.";
    return;
  }

  // If first frame hasn't been encoded, do it first.
  if (first_frame_) {
    std::unique_ptr<VideoFrameAndTimestamp> first_frame(first_frame_.release());
    EncodeOnEncodingTaskRunner(first_frame->first, first_frame->second);
  }

  // Lower resolutions may fall back to SW encoder in some platforms, i.e. Mac.
  // In that case, the encoder expects more frames before returning result.
  // Therefore, a copy is necessary to release the current frame.
  scoped_refptr<media::VideoFrame> video_frame = frame;
  if (vea_requested_input_size_ != input_size_ ||
      input_size_.width() < kVEAEncoderMinResolutionWidth ||
      input_size_.height() < kVEAEncoderMinResolutionHeight) {
    // Create SharedMemory backed input buffers as necessary. These SharedMemory
    // instances will be shared with GPU process.
    std::unique_ptr<base::SharedMemory> input_buffer;
    const size_t desired_mapped_size = media::VideoFrame::AllocationSize(
        media::PIXEL_FORMAT_I420, vea_requested_input_size_);
    if (input_buffers_.empty()) {
      input_buffer = gpu_factories_->CreateSharedMemory(desired_mapped_size);
    } else {
      do {
        input_buffer = std::move(input_buffers_.front());
        input_buffers_.pop();
      } while (!input_buffers_.empty() &&
               input_buffer->mapped_size() < desired_mapped_size);
      if (!input_buffer || input_buffer->mapped_size() < desired_mapped_size)
        return;
    }

    video_frame = media::VideoFrame::WrapExternalSharedMemory(
        media::PIXEL_FORMAT_I420, vea_requested_input_size_,
        gfx::Rect(input_size_), input_size_,
        reinterpret_cast<uint8_t*>(input_buffer->memory()),
        input_buffer->mapped_size(), input_buffer->handle(), 0,
        frame->timestamp());
    video_frame->AddDestructionObserver(media::BindToCurrentLoop(
        base::Bind(&VEAEncoder::FrameFinished, this,
                   base::Passed(std::move(input_buffer)))));
    libyuv::I420Copy(frame->visible_data(media::VideoFrame::kYPlane),
                     frame->stride(media::VideoFrame::kYPlane),
                     frame->visible_data(media::VideoFrame::kUPlane),
                     frame->stride(media::VideoFrame::kUPlane),
                     frame->visible_data(media::VideoFrame::kVPlane),
                     frame->stride(media::VideoFrame::kVPlane),
                     video_frame->visible_data(media::VideoFrame::kYPlane),
                     video_frame->stride(media::VideoFrame::kYPlane),
                     video_frame->visible_data(media::VideoFrame::kUPlane),
                     video_frame->stride(media::VideoFrame::kUPlane),
                     video_frame->visible_data(media::VideoFrame::kVPlane),
                     video_frame->stride(media::VideoFrame::kVPlane),
                     input_size_.width(), input_size_.height());
  }
  frames_in_encode_.push(std::make_pair(video_frame, capture_timestamp));

  encoding_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&media::VideoEncodeAccelerator::Encode,
                 base::Unretained(video_encoder_.get()), video_frame, false));
}

void VEAEncoder::ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());

  input_size_ = size;
  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!video_encoder_ ||
      !video_encoder_->Initialize(media::PIXEL_FORMAT_I420, input_size_, codec_,
                                  bits_per_second_, this)) {
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
  }
}

// static
void VpxEncoder::ShutdownEncoder(std::unique_ptr<base::Thread> encoding_thread,
                                 ScopedVpxCodecCtxPtr encoder) {
  DCHECK(encoding_thread->IsRunning());
  encoding_thread->Stop();
  // Both |encoding_thread| and |encoder| will be destroyed at end-of-scope.
}

VpxEncoder::VpxEncoder(
    bool use_vp9,
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second)
    : Encoder(on_encoded_video_callback, bits_per_second),
      use_vp9_(use_vp9) {
  codec_config_.g_timebase.den = 0;  // Not initialized.
  DCHECK(encoding_thread_->IsRunning());
}

VpxEncoder::~VpxEncoder() {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&VpxEncoder::ShutdownEncoder,
                                         base::Passed(&encoding_thread_),
                                         base::Passed(&encoder_)));
}

void VpxEncoder::EncodeOnEncodingTaskRunner(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks capture_timestamp) {
  TRACE_EVENT0("video", "VpxEncoder::EncodeOnEncodingTaskRunner");
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  const gfx::Size frame_size = frame->visible_rect().size();
  if (!IsInitialized() ||
      gfx::Size(codec_config_.g_w, codec_config_.g_h) != frame_size) {
    ConfigureEncoderOnEncodingTaskRunner(frame_size);
  }

  vpx_image_t vpx_image;
  vpx_image_t* const result = vpx_img_wrap(&vpx_image,
                                           VPX_IMG_FMT_I420,
                                           frame_size.width(),
                                           frame_size.height(),
                                           1  /* align */,
                                           frame->data(VideoFrame::kYPlane));
  DCHECK_EQ(result, &vpx_image);
  vpx_image.planes[VPX_PLANE_Y] = frame->visible_data(VideoFrame::kYPlane);
  vpx_image.planes[VPX_PLANE_U] = frame->visible_data(VideoFrame::kUPlane);
  vpx_image.planes[VPX_PLANE_V] = frame->visible_data(VideoFrame::kVPlane);
  vpx_image.stride[VPX_PLANE_Y] = frame->stride(VideoFrame::kYPlane);
  vpx_image.stride[VPX_PLANE_U] = frame->stride(VideoFrame::kUPlane);
  vpx_image.stride[VPX_PLANE_V] = frame->stride(VideoFrame::kVPlane);

  const base::TimeDelta duration = EstimateFrameDuration(frame);
  // Encode the frame.  The presentation time stamp argument here is fixed to
  // zero to force the encoder to base its single-frame bandwidth calculations
  // entirely on |predicted_frame_duration|.
  const vpx_codec_err_t ret = vpx_codec_encode(encoder_.get(),
                                               &vpx_image,
                                               0  /* pts */,
                                               duration.InMicroseconds(),
                                               0 /* flags */,
                                               VPX_DL_REALTIME);
  DCHECK_EQ(ret, VPX_CODEC_OK) << vpx_codec_err_to_string(ret) << ", #"
                               << vpx_codec_error(encoder_.get()) << " -"
                               << vpx_codec_error_detail(encoder_.get());

  std::unique_ptr<std::string> data(new std::string);
  bool keyframe = false;
  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t* pkt = NULL;
  while ((pkt = vpx_codec_get_cx_data(encoder_.get(), &iter)) != NULL) {
    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
      continue;
    data->assign(static_cast<char*>(pkt->data.frame.buf), pkt->data.frame.sz);
    keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
    break;
  }
  origin_task_runner_->PostTask(FROM_HERE,
      base::Bind(OnFrameEncodeCompleted,
                 on_encoded_video_callback_,
                 frame,
                 base::Passed(&data),
                 capture_timestamp,
                 keyframe));
}

void VpxEncoder::ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  if (IsInitialized()) {
    // TODO(mcasas) VP8 quirk/optimisation: If the new |size| is strictly less-
    // than-or-equal than the old size, in terms of area, the existing encoder
    // instance could be reused after changing |codec_config_.{g_w,g_h}|.
    DVLOG(1) << "Destroying/Re-Creating encoder for new frame size: "
             << gfx::Size(codec_config_.g_w, codec_config_.g_h).ToString()
             << " --> " << size.ToString() << (use_vp9_ ? " vp9" : " vp8");
    encoder_.reset();
  }

  const vpx_codec_iface_t* interface =
      use_vp9_ ? vpx_codec_vp9_cx() : vpx_codec_vp8_cx();
  vpx_codec_err_t result =
      vpx_codec_enc_config_default(interface, &codec_config_, 0 /* reserved */);
  DCHECK_EQ(VPX_CODEC_OK, result);

  DCHECK_EQ(320u, codec_config_.g_w);
  DCHECK_EQ(240u, codec_config_.g_h);
  DCHECK_EQ(256u, codec_config_.rc_target_bitrate);
  // Use the selected bitrate or adjust default bit rate to account for the
  // actual size.  Note: |rc_target_bitrate| units are kbit per second.
  if (bits_per_second_ > 0) {
    codec_config_.rc_target_bitrate = bits_per_second_ / 1000;
  } else {
    codec_config_.rc_target_bitrate = size.GetArea() *
                                      codec_config_.rc_target_bitrate /
                                      codec_config_.g_w / codec_config_.g_h;
  }
  // Both VP8/VP9 configuration should be Variable BitRate by default.
  DCHECK_EQ(VPX_VBR, codec_config_.rc_end_usage);
  if (use_vp9_) {
    // Number of frames to consume before producing output.
    codec_config_.g_lag_in_frames = 0;

    // DCHECK that the profile selected by default is I420 (magic number 0).
    DCHECK_EQ(0u, codec_config_.g_profile);
  } else {
    // VP8 always produces frames instantaneously.
    DCHECK_EQ(0u, codec_config_.g_lag_in_frames);
  }

  DCHECK(size.width());
  DCHECK(size.height());
  codec_config_.g_w = size.width();
  codec_config_.g_h = size.height();
  codec_config_.g_pass = VPX_RC_ONE_PASS;

  // Timebase is the smallest interval used by the stream, can be set to the
  // frame rate or to e.g. microseconds.
  codec_config_.g_timebase.num = 1;
  codec_config_.g_timebase.den = base::Time::kMicrosecondsPerSecond;

  // Let the encoder decide where to place the Keyframes, between min and max.
  // In VPX_KF_AUTO mode libvpx will sometimes emit keyframes regardless of min/
  // max distance out of necessity.
  // Note that due to http://crbug.com/440223, it might be necessary to force a
  // key frame after 10,000frames since decoding fails after 30,000 non-key
  // frames.
  // Forcing a keyframe in regular intervals also allows seeking in the
  // resulting recording with decent performance.
  codec_config_.kf_mode = VPX_KF_AUTO;
  codec_config_.kf_min_dist = 0;
  codec_config_.kf_max_dist = 100;

  codec_config_.g_threads = GetNumberOfThreadsForEncoding();

  // Number of frames to consume before producing output.
  codec_config_.g_lag_in_frames = 0;

  DCHECK(!encoder_);
  encoder_.reset(new vpx_codec_ctx_t);
  const vpx_codec_err_t ret = vpx_codec_enc_init(encoder_.get(), interface,
                                                 &codec_config_, 0 /* flags */);
  DCHECK_EQ(VPX_CODEC_OK, ret);

  if (use_vp9_) {
    // Values of VP8E_SET_CPUUSED greater than 0 will increase encoder speed at
    // the expense of quality up to a maximum value of 8 for VP9, by tuning the
    // target time spent encoding the frame. Go from 8 to 5 (values for real
    // time encoding) depending on the amount of cores available in the system.
    const int kCpuUsed =
        std::max(5, 8 - base::SysInfo::NumberOfProcessors() / 2);
    result = vpx_codec_control(encoder_.get(), VP8E_SET_CPUUSED, kCpuUsed);
    DLOG_IF(WARNING, VPX_CODEC_OK != result) << "VP8E_SET_CPUUSED failed";
  }
}

bool VpxEncoder::IsInitialized() const {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  return codec_config_.g_timebase.den != 0;
}

base::TimeDelta VpxEncoder::EstimateFrameDuration(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  using base::TimeDelta;
  TimeDelta predicted_frame_duration;
  if (!frame->metadata()->GetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                                       &predicted_frame_duration) ||
      predicted_frame_duration <= TimeDelta()) {
    // The source of the video frame did not provide the frame duration.  Use
    // the actual amount of time between the current and previous frame as a
    // prediction for the next frame's duration.
    // TODO(mcasas): This duration estimation could lead to artifacts if the
    // cadence of the received stream is compromised (e.g. camera freeze, pause,
    // remote packet loss).  Investigate using GetFrameRate() in this case.
    predicted_frame_duration = frame->timestamp() - last_frame_timestamp_;
  }
  last_frame_timestamp_ = frame->timestamp();
  // Make sure |predicted_frame_duration| is in a safe range of values.
  const TimeDelta kMaxFrameDuration = TimeDelta::FromSecondsD(1.0 / 8);
  const TimeDelta kMinFrameDuration = TimeDelta::FromMilliseconds(1);
  return std::min(kMaxFrameDuration, std::max(predicted_frame_duration,
                                              kMinFrameDuration));
}

#if BUILDFLAG(RTC_USE_H264)

// static
void H264Encoder::ShutdownEncoder(std::unique_ptr<base::Thread> encoding_thread,
                                  ScopedISVCEncoderPtr encoder) {
  DCHECK(encoding_thread->IsRunning());
  encoding_thread->Stop();
  // Both |encoding_thread| and |encoder| will be destroyed at end-of-scope.
}

H264Encoder::H264Encoder(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second)
    : Encoder(on_encoded_video_callback, bits_per_second) {
  DCHECK(encoding_thread_->IsRunning());
}

H264Encoder::~H264Encoder() {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&H264Encoder::ShutdownEncoder,
                                         base::Passed(&encoding_thread_),
                                         base::Passed(&openh264_encoder_)));
}

void H264Encoder::EncodeOnEncodingTaskRunner(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks capture_timestamp) {
  TRACE_EVENT0("video", "H264Encoder::EncodeOnEncodingTaskRunner");
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  const gfx::Size frame_size = frame->visible_rect().size();
  if (!openh264_encoder_ || configured_size_ != frame_size) {
    ConfigureEncoderOnEncodingTaskRunner(frame_size);
    first_frame_timestamp_ = capture_timestamp;
  }

  SSourcePicture picture = {};
  picture.iPicWidth = frame_size.width();
  picture.iPicHeight = frame_size.height();
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.uiTimeStamp =
      (capture_timestamp - first_frame_timestamp_).InMilliseconds();
  picture.iStride[0] = frame->stride(VideoFrame::kYPlane);
  picture.iStride[1] = frame->stride(VideoFrame::kUPlane);
  picture.iStride[2] = frame->stride(VideoFrame::kVPlane);
  picture.pData[0] = frame->visible_data(VideoFrame::kYPlane);
  picture.pData[1] = frame->visible_data(VideoFrame::kUPlane);
  picture.pData[2] = frame->visible_data(VideoFrame::kVPlane);

  SFrameBSInfo info = {};
  if (openh264_encoder_->EncodeFrame(&picture, &info) != cmResultSuccess) {
    NOTREACHED() << "OpenH264 encoding failed";
    return;
  }

  std::unique_ptr<std::string> data(new std::string);
  const uint8_t kNALStartCode[4] = {0, 0, 0, 1};
  for (int layer = 0; layer < info.iLayerNum; ++layer) {
    const SLayerBSInfo& layerInfo = info.sLayerInfo[layer];
    // Iterate NAL units making up this layer, noting fragments.
    size_t layer_len = 0;
    for (int nal = 0; nal < layerInfo.iNalCount; ++nal) {
      // The following DCHECKs make sure that the header of each NAL unit is OK.
      DCHECK_GE(layerInfo.pNalLengthInByte[nal], 4);
      DCHECK_EQ(kNALStartCode[0], layerInfo.pBsBuf[layer_len+0]);
      DCHECK_EQ(kNALStartCode[1], layerInfo.pBsBuf[layer_len+1]);
      DCHECK_EQ(kNALStartCode[2], layerInfo.pBsBuf[layer_len+2]);
      DCHECK_EQ(kNALStartCode[3], layerInfo.pBsBuf[layer_len+3]);

      layer_len += layerInfo.pNalLengthInByte[nal];
    }
    // Copy the entire layer's data (including NAL start codes).
    data->append(reinterpret_cast<char*>(layerInfo.pBsBuf), layer_len);
  }

  const bool is_key_frame = info.eFrameType == videoFrameTypeIDR;
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(OnFrameEncodeCompleted, on_encoded_video_callback_, frame,
                 base::Passed(&data), capture_timestamp, is_key_frame));
}

void H264Encoder::ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  ISVCEncoder* temp_encoder = nullptr;
  if (WelsCreateSVCEncoder(&temp_encoder) != 0) {
    NOTREACHED() << "Failed to create OpenH264 encoder";
    return;
  }
  openh264_encoder_.reset(temp_encoder);
  configured_size_ = size;

#if DCHECK_IS_ON()
  int trace_level = WELS_LOG_INFO;
  openh264_encoder_->SetOption(ENCODER_OPTION_TRACE_LEVEL, &trace_level);
#endif

  SEncParamExt init_params;
  openh264_encoder_->GetDefaultParams(&init_params);
  init_params.iUsageType = CAMERA_VIDEO_REAL_TIME;

  DCHECK_EQ(AUTO_REF_PIC_COUNT, init_params.iNumRefFrame);
  DCHECK(!init_params.bSimulcastAVC);

  init_params.uiIntraPeriod = 100;  // Same as for VpxEncoder.
  init_params.iPicWidth = size.width();
  init_params.iPicHeight = size.height();

  DCHECK_EQ(RC_QUALITY_MODE, init_params.iRCMode);
  DCHECK_EQ(0, init_params.iPaddingFlag);
  DCHECK_EQ(UNSPECIFIED_BIT_RATE, init_params.iTargetBitrate);
  DCHECK_EQ(UNSPECIFIED_BIT_RATE, init_params.iMaxBitrate);
  if (bits_per_second_ > 0) {
    init_params.iRCMode = RC_BITRATE_MODE;
    init_params.iTargetBitrate = bits_per_second_;
  } else {
    init_params.iRCMode = RC_OFF_MODE;
  }

  // Threading model: Set to 1 due to https://crbug.com/583348.
  init_params.iMultipleThreadIdc = 1;

  // TODO(mcasas): consider reducing complexity if there are few CPUs available.
  DCHECK_EQ(MEDIUM_COMPLEXITY, init_params.iComplexityMode);
  DCHECK(!init_params.bEnableDenoise);
  DCHECK(init_params.bEnableFrameSkip);

  // The base spatial layer 0 is the only one we use.
  DCHECK_EQ(1, init_params.iSpatialLayerNum);
  init_params.sSpatialLayers[0].iVideoWidth        = init_params.iPicWidth;
  init_params.sSpatialLayers[0].iVideoHeight       = init_params.iPicHeight;
  init_params.sSpatialLayers[0].iSpatialBitrate    = init_params.iTargetBitrate;
  // Slice num according to number of threads.
  init_params.sSpatialLayers[0].sSliceCfg.uiSliceMode = SM_AUTO_SLICE;

  if (openh264_encoder_->InitializeExt(&init_params) != cmResultSuccess) {
    NOTREACHED() << "Failed to initialize OpenH264 encoder";
    return;
  }

  int pixel_format = EVideoFormatType::videoFormatI420;
  openh264_encoder_->SetOption(ENCODER_OPTION_DATAFORMAT, &pixel_format);
}
#endif //#if BUILDFLAG(RTC_USE_H264)

}  // anonymous namespace

VideoTrackRecorder::VideoTrackRecorder(
    CodecId codec,
    const blink::WebMediaStreamTrack& track,
    const OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second)
    : track_(track) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!track_.isNull());
  DCHECK(track_.getTrackData());

  const auto& vea_supported_profile = CodecIdToVEAProfile(codec);
  // TODO(emircan): Prioritize software based encoders in lower resolutions.
  if (vea_supported_profile != media::VIDEO_CODEC_PROFILE_UNKNOWN) {
    encoder_ = new VEAEncoder(on_encoded_video_callback, bits_per_second,
                              vea_supported_profile);
  } else {
    switch (codec) {
#if BUILDFLAG(RTC_USE_H264)
      case CodecId::H264:
        encoder_ = new H264Encoder(on_encoded_video_callback, bits_per_second);
        break;
#endif
      case CodecId::VP8:
      case CodecId::VP9:
        encoder_ = new VpxEncoder(codec == CodecId::VP9,
                                  on_encoded_video_callback, bits_per_second);
        break;
      default:
        NOTREACHED() << "Unsupported codec";
    }
  }

  // StartFrameEncode() will be called on Render IO thread.
  MediaStreamVideoSink::ConnectToTrack(
      track_,
      base::Bind(&VideoTrackRecorder::Encoder::StartFrameEncode, encoder_),
      false);
}

VideoTrackRecorder::~VideoTrackRecorder() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  MediaStreamVideoSink::DisconnectFromTrack();
  track_.reset();
}

void VideoTrackRecorder::Pause() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(encoder_);
  encoder_->SetPaused(true);
}

void VideoTrackRecorder::Resume() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(encoder_);
  encoder_->SetPaused(false);
}

void VideoTrackRecorder::OnVideoFrameForTesting(
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks timestamp) {
  encoder_->StartFrameEncode(frame, timestamp);
}

}  // namespace content
