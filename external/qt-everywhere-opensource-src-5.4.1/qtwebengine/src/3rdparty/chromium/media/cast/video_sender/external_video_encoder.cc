// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/external_video_encoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/cast/cast_defines.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
namespace cast {
class LocalVideoEncodeAcceleratorClient;
}  // namespace cast
}  // namespace media

namespace {
static const size_t kOutputBufferCount = 3;

void LogFrameEncodedEvent(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    base::TimeTicks event_time,
    media::cast::RtpTimestamp rtp_timestamp,
    uint32 frame_id) {
  cast_environment->Logging()->InsertFrameEvent(
      event_time, media::cast::FRAME_ENCODED, media::cast::VIDEO_EVENT,
      rtp_timestamp, frame_id);
}

// Proxy this call to ExternalVideoEncoder on the cast main thread.
void ProxyCreateVideoEncodeAccelerator(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    const base::WeakPtr<media::cast::ExternalVideoEncoder>& weak_ptr,
    const media::cast::CreateVideoEncodeMemoryCallback&
        create_video_encode_mem_cb,
    scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
    scoped_ptr<media::VideoEncodeAccelerator> vea) {
  cast_environment->PostTask(
      media::cast::CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(
          &media::cast::ExternalVideoEncoder::OnCreateVideoEncodeAccelerator,
          weak_ptr,
          create_video_encode_mem_cb,
          encoder_task_runner,
          base::Passed(&vea)));
}
}  // namespace

namespace media {
namespace cast {

// Container for the associated data of a video frame being processed.
struct EncodedFrameReturnData {
  EncodedFrameReturnData(base::TimeTicks c_time,
                         VideoEncoder::FrameEncodedCallback callback) {
    capture_time = c_time;
    frame_encoded_callback = callback;
  }
  base::TimeTicks capture_time;
  VideoEncoder::FrameEncodedCallback frame_encoded_callback;
};

// The ExternalVideoEncoder class can be deleted directly by cast, while
// LocalVideoEncodeAcceleratorClient stays around long enough to properly shut
// down the VideoEncodeAccelerator.
class LocalVideoEncodeAcceleratorClient
    : public VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<LocalVideoEncodeAcceleratorClient> {
 public:
  LocalVideoEncodeAcceleratorClient(
      scoped_refptr<CastEnvironment> cast_environment,
      scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
      scoped_ptr<media::VideoEncodeAccelerator> vea,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
      const base::WeakPtr<ExternalVideoEncoder>& weak_owner)
      : cast_environment_(cast_environment),
        encoder_task_runner_(encoder_task_runner),
        video_encode_accelerator_(vea.Pass()),
        create_video_encode_memory_cb_(create_video_encode_mem_cb),
        weak_owner_(weak_owner),
        last_encoded_frame_id_(kStartFrameId),
        key_frame_encountered_(false) {
    DCHECK(encoder_task_runner_);
  }

  // Initialize the real HW encoder.
  void Initialize(const VideoSenderConfig& video_config) {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    VideoCodecProfile output_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
    switch (video_config.codec) {
      case transport::kVp8:
        output_profile = media::VP8PROFILE_MAIN;
        break;
      case transport::kH264:
        output_profile = media::H264PROFILE_MAIN;
        break;
      case transport::kFakeSoftwareVideo:
        NOTREACHED() << "Fake software video encoder cannot be external";
        break;
      case transport::kUnknownVideoCodec:
        NOTREACHED() << "Video codec not specified";
        break;
    }
    codec_ = video_config.codec;
    max_frame_rate_ = video_config.max_frame_rate;

    if (!video_encode_accelerator_->Initialize(
            media::VideoFrame::I420,
            gfx::Size(video_config.width, video_config.height),
            output_profile,
            video_config.start_bitrate,
            this)) {
      NotifyError(VideoEncodeAccelerator::kInvalidArgumentError);
      return;
    }

    // Wait until shared memory is allocated to indicate that encoder is
    // initialized.
  }

  // Free the HW.
  void Destroy() {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    video_encode_accelerator_.reset();
  }

  void SetBitRate(uint32 bit_rate) {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    video_encode_accelerator_->RequestEncodingParametersChange(bit_rate,
                                                               max_frame_rate_);
  }

  void EncodeVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time,
      bool key_frame_requested,
      const VideoEncoder::FrameEncodedCallback& frame_encoded_callback) {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    encoded_frame_data_storage_.push_back(
        EncodedFrameReturnData(capture_time, frame_encoded_callback));

    // BitstreamBufferReady will be called once the encoder is done.
    video_encode_accelerator_->Encode(video_frame, key_frame_requested);
  }

 protected:
  virtual void NotifyError(VideoEncodeAccelerator::Error error) OVERRIDE {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    VLOG(1) << "ExternalVideoEncoder NotifyError: " << error;

    video_encode_accelerator_.reset();
    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&ExternalVideoEncoder::EncoderError, weak_owner_));
  }

  // Called to allocate the input and output buffers.
  virtual void RequireBitstreamBuffers(unsigned int input_count,
                                       const gfx::Size& input_coded_size,
                                       size_t output_buffer_size) OVERRIDE {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    DCHECK(video_encode_accelerator_);

    for (size_t j = 0; j < kOutputBufferCount; ++j) {
      create_video_encode_memory_cb_.Run(
          output_buffer_size,
          base::Bind(&LocalVideoEncodeAcceleratorClient::OnCreateSharedMemory,
                     this));
    }
  }

  // Encoder has encoded a frame and it's available in one of out output
  // buffers.
  virtual void BitstreamBufferReady(int32 bitstream_buffer_id,
                                    size_t payload_size,
                                    bool key_frame) OVERRIDE {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    if (bitstream_buffer_id < 0 ||
        bitstream_buffer_id >= static_cast<int32>(output_buffers_.size())) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid bitstream_buffer_id="
              << bitstream_buffer_id;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    base::SharedMemory* output_buffer = output_buffers_[bitstream_buffer_id];
    if (payload_size > output_buffer->mapped_size()) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid payload_size = "
              << payload_size;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    if (key_frame)
      key_frame_encountered_ = true;
    if (!key_frame_encountered_) {
      // Do not send video until we have encountered the first key frame.
      // Save the bitstream buffer in |stream_header_| to be sent later along
      // with the first key frame.
      stream_header_.append(static_cast<const char*>(output_buffer->memory()),
                            payload_size);
    } else if (!encoded_frame_data_storage_.empty()) {
      scoped_ptr<transport::EncodedFrame> encoded_frame(
          new transport::EncodedFrame());
      encoded_frame->dependency = key_frame ? transport::EncodedFrame::KEY :
          transport::EncodedFrame::DEPENDENT;
      encoded_frame->frame_id = ++last_encoded_frame_id_;
      if (key_frame)
        encoded_frame->referenced_frame_id = encoded_frame->frame_id;
      else
        encoded_frame->referenced_frame_id = encoded_frame->frame_id - 1;
      encoded_frame->reference_time =
          encoded_frame_data_storage_.front().capture_time;
      encoded_frame->rtp_timestamp =
          GetVideoRtpTimestamp(encoded_frame->reference_time);
      if (!stream_header_.empty()) {
        encoded_frame->data = stream_header_;
        stream_header_.clear();
      }
      encoded_frame->data.append(
          static_cast<const char*>(output_buffer->memory()), payload_size);

      cast_environment_->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(&LogFrameEncodedEvent,
                     cast_environment_,
                     cast_environment_->Clock()->NowTicks(),
                     encoded_frame->rtp_timestamp,
                     encoded_frame->frame_id));

      cast_environment_->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(encoded_frame_data_storage_.front().frame_encoded_callback,
                     base::Passed(&encoded_frame)));

      encoded_frame_data_storage_.pop_front();
    } else {
      VLOG(1) << "BitstreamBufferReady(): no encoded frame data available";
    }

    // We need to re-add the output buffer to the encoder after we are done
    // with it.
    video_encode_accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id]->handle(),
        output_buffers_[bitstream_buffer_id]->mapped_size()));
  }

 private:
  // Note: This method can be called on any thread.
  void OnCreateSharedMemory(scoped_ptr<base::SharedMemory> memory) {
    encoder_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalVideoEncodeAcceleratorClient::ReceivedSharedMemory,
                   this,
                   base::Passed(&memory)));
  }

  void ReceivedSharedMemory(scoped_ptr<base::SharedMemory> memory) {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    output_buffers_.push_back(memory.release());

    // Wait until all requested buffers are received.
    if (output_buffers_.size() < kOutputBufferCount)
      return;

    // Immediately provide all output buffers to the VEA.
    for (size_t i = 0; i < output_buffers_.size(); ++i) {
      video_encode_accelerator_->UseOutputBitstreamBuffer(
          media::BitstreamBuffer(static_cast<int32>(i),
                                 output_buffers_[i]->handle(),
                                 output_buffers_[i]->mapped_size()));
    }

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&ExternalVideoEncoder::EncoderInitialized, weak_owner_));
  }

  friend class base::RefCountedThreadSafe<LocalVideoEncodeAcceleratorClient>;

  virtual ~LocalVideoEncodeAcceleratorClient() {}

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;
  scoped_ptr<media::VideoEncodeAccelerator> video_encode_accelerator_;
  const CreateVideoEncodeMemoryCallback create_video_encode_memory_cb_;
  const base::WeakPtr<ExternalVideoEncoder> weak_owner_;
  int max_frame_rate_;
  transport::VideoCodec codec_;
  uint32 last_encoded_frame_id_;
  bool key_frame_encountered_;
  std::string stream_header_;

  // Shared memory buffers for output with the VideoAccelerator.
  ScopedVector<base::SharedMemory> output_buffers_;

  // FIFO list.
  std::list<EncodedFrameReturnData> encoded_frame_data_storage_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoEncodeAcceleratorClient);
};

ExternalVideoEncoder::ExternalVideoEncoder(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb)
    : video_config_(video_config),
      cast_environment_(cast_environment),
      encoder_active_(false),
      key_frame_requested_(false),
      weak_factory_(this) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  create_vea_cb.Run(base::Bind(&ProxyCreateVideoEncodeAccelerator,
                               cast_environment,
                               weak_factory_.GetWeakPtr(),
                               create_video_encode_mem_cb));
}

ExternalVideoEncoder::~ExternalVideoEncoder() {
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::Destroy,
                 video_accelerator_client_));
}

void ExternalVideoEncoder::EncoderInitialized() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_active_ = true;
}

void ExternalVideoEncoder::EncoderError() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_active_ = false;
}

void ExternalVideoEncoder::OnCreateVideoEncodeAccelerator(
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
    scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
    scoped_ptr<media::VideoEncodeAccelerator> vea) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_task_runner_ = encoder_task_runner;

  video_accelerator_client_ =
      new LocalVideoEncodeAcceleratorClient(cast_environment_,
                                            encoder_task_runner,
                                            vea.Pass(),
                                            create_video_encode_mem_cb,
                                            weak_factory_.GetWeakPtr());
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::Initialize,
                 video_accelerator_client_,
                 video_config_));
}

bool ExternalVideoEncoder::EncodeVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (!encoder_active_)
    return false;

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::EncodeVideoFrame,
                 video_accelerator_client_,
                 video_frame,
                 capture_time,
                 key_frame_requested_,
                 frame_encoded_callback));

  key_frame_requested_ = false;
  return true;
}

// Inform the encoder about the new target bit rate.
void ExternalVideoEncoder::SetBitRate(int new_bit_rate) {
  if (!encoder_active_) {
    // If we receive SetBitRate() before VEA creation callback is invoked,
    // cache the new bit rate in the encoder config and use the new settings
    // to initialize VEA.
    video_config_.start_bitrate = new_bit_rate;
    return;
  }

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::SetBitRate,
                 video_accelerator_client_,
                 new_bit_rate));
}

// Inform the encoder to encode the next frame as a key frame.
void ExternalVideoEncoder::GenerateKeyFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  key_frame_requested_ = true;
}

// Inform the encoder to only reference frames older or equal to frame_id;
void ExternalVideoEncoder::LatestFrameIdToReference(uint32 /*frame_id*/) {
  // Do nothing not supported.
}

}  //  namespace cast
}  //  namespace media
