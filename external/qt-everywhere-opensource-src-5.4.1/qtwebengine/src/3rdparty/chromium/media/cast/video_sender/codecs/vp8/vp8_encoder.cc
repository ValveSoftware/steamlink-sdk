// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO (pwestin): add a link to the design document describing the generic
// protocol and the VP8 specific details.
#include "media/cast/video_sender/codecs/vp8/vp8_encoder.h"

#include <vector>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/transport/cast_transport_config.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"

namespace media {
namespace cast {

static const uint32 kMinIntra = 300;

static int ComputeMaxNumOfRepeatedBuffes(int max_unacked_frames) {
  if (max_unacked_frames > kNumberOfVp8VideoBuffers)
    return (max_unacked_frames - 1) / kNumberOfVp8VideoBuffers;

  return 0;
}

Vp8Encoder::Vp8Encoder(const VideoSenderConfig& video_config,
                       int max_unacked_frames)
    : cast_config_(video_config),
      use_multiple_video_buffers_(
          cast_config_.max_number_of_video_buffers_used ==
          kNumberOfVp8VideoBuffers),
      max_number_of_repeated_buffers_in_a_row_(
          ComputeMaxNumOfRepeatedBuffes(max_unacked_frames)),
      key_frame_requested_(true),
      first_frame_received_(false),
      last_encoded_frame_id_(kStartFrameId),
      number_of_repeated_buffers_(0) {
  // TODO(pwestin): we need to figure out how to synchronize the acking with the
  // internal state of the encoder, ideally the encoder will tell if we can
  // send another frame.
  DCHECK(!use_multiple_video_buffers_ ||
         max_number_of_repeated_buffers_in_a_row_ == 0)
      << "Invalid config";

  // VP8 have 3 buffers available for prediction, with
  // max_number_of_video_buffers_used set to 1 we maximize the coding efficiency
  // however in this mode we can not skip frames in the receiver to catch up
  // after a temporary network outage; with max_number_of_video_buffers_used
  // set to 3 we allow 2 frames to be skipped by the receiver without error
  // propagation.
  DCHECK(cast_config_.max_number_of_video_buffers_used == 1 ||
         cast_config_.max_number_of_video_buffers_used ==
             kNumberOfVp8VideoBuffers)
      << "Invalid argument";

  thread_checker_.DetachFromThread();
}

Vp8Encoder::~Vp8Encoder() {
  vpx_codec_destroy(encoder_.get());
  vpx_img_free(raw_image_);
}

void Vp8Encoder::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  config_.reset(new vpx_codec_enc_cfg_t());
  encoder_.reset(new vpx_codec_ctx_t());

  // Creating a wrapper to the image - setting image data to NULL. Actual
  // pointer will be set during encode. Setting align to 1, as it is
  // meaningless (actual memory is not allocated).
  raw_image_ = vpx_img_wrap(
      NULL, IMG_FMT_I420, cast_config_.width, cast_config_.height, 1, NULL);

  for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
    acked_frame_buffers_[i] = true;
    used_buffers_frame_id_[i] = kStartFrameId;
  }
  InitEncode(cast_config_.number_of_encode_threads);
}

void Vp8Encoder::InitEncode(int number_of_encode_threads) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Populate encoder configuration with default values.
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), config_.get(), 0)) {
    DCHECK(false) << "Invalid return value";
  }
  config_->g_w = cast_config_.width;
  config_->g_h = cast_config_.height;
  config_->rc_target_bitrate = cast_config_.start_bitrate / 1000;  // In kbit/s.

  // Setting the codec time base.
  config_->g_timebase.num = 1;
  config_->g_timebase.den = kVideoFrequency;
  config_->g_lag_in_frames = 0;
  config_->kf_mode = VPX_KF_DISABLED;
  if (use_multiple_video_buffers_) {
    // We must enable error resilience when we use multiple buffers, due to
    // codec requirements.
    config_->g_error_resilient = 1;
  }
  config_->g_threads = number_of_encode_threads;

  // Rate control settings.
  // Never allow the encoder to drop frame internally.
  config_->rc_dropframe_thresh = 0;
  config_->rc_end_usage = VPX_CBR;
  config_->g_pass = VPX_RC_ONE_PASS;
  config_->rc_resize_allowed = 0;
  config_->rc_min_quantizer = cast_config_.min_qp;
  config_->rc_max_quantizer = cast_config_.max_qp;
  config_->rc_undershoot_pct = 100;
  config_->rc_overshoot_pct = 15;
  config_->rc_buf_initial_sz = 500;
  config_->rc_buf_optimal_sz = 600;
  config_->rc_buf_sz = 1000;

  // set the maximum target size of any key-frame.
  uint32 rc_max_intra_target = MaxIntraTarget(config_->rc_buf_optimal_sz);
  vpx_codec_flags_t flags = 0;
  if (vpx_codec_enc_init(
          encoder_.get(), vpx_codec_vp8_cx(), config_.get(), flags)) {
    DCHECK(false) << "vpx_codec_enc_init() failed.";
    encoder_.reset();
    return;
  }
  vpx_codec_control(encoder_.get(), VP8E_SET_STATIC_THRESHOLD, 1);
  vpx_codec_control(encoder_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
  vpx_codec_control(encoder_.get(), VP8E_SET_CPUUSED, -6);
  vpx_codec_control(
      encoder_.get(), VP8E_SET_MAX_INTRA_BITRATE_PCT, rc_max_intra_target);
}

bool Vp8Encoder::Encode(const scoped_refptr<media::VideoFrame>& video_frame,
                        transport::EncodedFrame* encoded_image) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Image in vpx_image_t format.
  // Input image is const. VP8's raw image is not defined as const.
  raw_image_->planes[PLANE_Y] =
      const_cast<uint8*>(video_frame->data(VideoFrame::kYPlane));
  raw_image_->planes[PLANE_U] =
      const_cast<uint8*>(video_frame->data(VideoFrame::kUPlane));
  raw_image_->planes[PLANE_V] =
      const_cast<uint8*>(video_frame->data(VideoFrame::kVPlane));

  raw_image_->stride[VPX_PLANE_Y] = video_frame->stride(VideoFrame::kYPlane);
  raw_image_->stride[VPX_PLANE_U] = video_frame->stride(VideoFrame::kUPlane);
  raw_image_->stride[VPX_PLANE_V] = video_frame->stride(VideoFrame::kVPlane);

  uint8 latest_frame_id_to_reference;
  Vp8Buffers buffer_to_update;
  vpx_codec_flags_t flags = 0;
  if (key_frame_requested_) {
    flags = VPX_EFLAG_FORCE_KF;
    // Self reference.
    latest_frame_id_to_reference = last_encoded_frame_id_ + 1;
    // We can pick any buffer as buffer_to_update since we update
    // them all.
    buffer_to_update = kLastBuffer;
  } else {
    // Reference all acked frames (buffers).
    latest_frame_id_to_reference = GetLatestFrameIdToReference();
    GetCodecReferenceFlags(&flags);
    buffer_to_update = GetNextBufferToUpdate();
    GetCodecUpdateFlags(buffer_to_update, &flags);
  }

  // Note: The duration does not reflect the real time between frames. This is
  // done to keep the encoder happy.
  //
  // TODO(miu): This is a semi-hack.  We should consider using
  // |video_frame->timestamp()| instead.
  uint32 duration = kVideoFrequency / cast_config_.max_frame_rate;

  // Note: Timestamp here is used for bitrate calculation. The absolute value
  // is not important.
  if (!first_frame_received_) {
    first_frame_received_ = true;
    first_frame_timestamp_ = video_frame->timestamp();
  }

  vpx_codec_pts_t timestamp =
      (video_frame->timestamp() - first_frame_timestamp_).InMicroseconds() *
      kVideoFrequency / base::Time::kMicrosecondsPerSecond;

  if (vpx_codec_encode(encoder_.get(),
                       raw_image_,
                       timestamp,
                       duration,
                       flags,
                       VPX_DL_REALTIME) != VPX_CODEC_OK) {
    LOG(ERROR) << "Failed to encode for once.";
    return false;
  }

  // Get encoded frame.
  const vpx_codec_cx_pkt_t* pkt = NULL;
  vpx_codec_iter_t iter = NULL;
  bool is_key_frame = false;
  while ((pkt = vpx_codec_get_cx_data(encoder_.get(), &iter)) != NULL) {
    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
      continue;
    encoded_image->data.assign(
        static_cast<const uint8*>(pkt->data.frame.buf),
        static_cast<const uint8*>(pkt->data.frame.buf) + pkt->data.frame.sz);
    is_key_frame = !!(pkt->data.frame.flags & VPX_FRAME_IS_KEY);
    break;  // Done, since all data is provided in one CX_FRAME_PKT packet.
  }
  // Don't update frame_id for zero size frames.
  if (encoded_image->data.empty())
    return true;

  // Populate the encoded frame.
  encoded_image->frame_id = ++last_encoded_frame_id_;
  if (is_key_frame) {
    encoded_image->dependency = transport::EncodedFrame::KEY;
    encoded_image->referenced_frame_id = encoded_image->frame_id;
  } else {
    encoded_image->dependency = transport::EncodedFrame::DEPENDENT;
    encoded_image->referenced_frame_id = latest_frame_id_to_reference;
  }

  DVLOG(1) << "VP8 encoded frame_id " << encoded_image->frame_id
           << ", sized:" << encoded_image->data.size();

  if (is_key_frame) {
    key_frame_requested_ = false;

    for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
      used_buffers_frame_id_[i] = encoded_image->frame_id;
    }
    // We can pick any buffer as last_used_vp8_buffer_ since we update
    // them all.
    last_used_vp8_buffer_ = buffer_to_update;
  } else {
    if (buffer_to_update != kNoBuffer) {
      acked_frame_buffers_[buffer_to_update] = false;
      used_buffers_frame_id_[buffer_to_update] = encoded_image->frame_id;
      last_used_vp8_buffer_ = buffer_to_update;
    }
  }
  return true;
}

void Vp8Encoder::GetCodecReferenceFlags(vpx_codec_flags_t* flags) {
  if (!use_multiple_video_buffers_)
    return;

  // We need to reference something.
  DCHECK(acked_frame_buffers_[kAltRefBuffer] ||
         acked_frame_buffers_[kGoldenBuffer] ||
         acked_frame_buffers_[kLastBuffer])
      << "Invalid state";

  if (!acked_frame_buffers_[kAltRefBuffer]) {
    *flags |= VP8_EFLAG_NO_REF_ARF;
  }
  if (!acked_frame_buffers_[kGoldenBuffer]) {
    *flags |= VP8_EFLAG_NO_REF_GF;
  }
  if (!acked_frame_buffers_[kLastBuffer]) {
    *flags |= VP8_EFLAG_NO_REF_LAST;
  }
}

uint32 Vp8Encoder::GetLatestFrameIdToReference() {
  if (!use_multiple_video_buffers_)
    return last_encoded_frame_id_;

  int latest_frame_id_to_reference = -1;
  if (acked_frame_buffers_[kAltRefBuffer]) {
    latest_frame_id_to_reference = used_buffers_frame_id_[kAltRefBuffer];
  }
  if (acked_frame_buffers_[kGoldenBuffer]) {
    if (latest_frame_id_to_reference == -1) {
      latest_frame_id_to_reference = used_buffers_frame_id_[kGoldenBuffer];
    } else {
      if (IsNewerFrameId(used_buffers_frame_id_[kGoldenBuffer],
                         latest_frame_id_to_reference)) {
        latest_frame_id_to_reference = used_buffers_frame_id_[kGoldenBuffer];
      }
    }
  }
  if (acked_frame_buffers_[kLastBuffer]) {
    if (latest_frame_id_to_reference == -1) {
      latest_frame_id_to_reference = used_buffers_frame_id_[kLastBuffer];
    } else {
      if (IsNewerFrameId(used_buffers_frame_id_[kLastBuffer],
                         latest_frame_id_to_reference)) {
        latest_frame_id_to_reference = used_buffers_frame_id_[kLastBuffer];
      }
    }
  }
  DCHECK(latest_frame_id_to_reference != -1) << "Invalid state";
  return static_cast<uint32>(latest_frame_id_to_reference);
}

Vp8Encoder::Vp8Buffers Vp8Encoder::GetNextBufferToUpdate() {
  if (!use_multiple_video_buffers_)
    return kNoBuffer;

  // Update at most one buffer, except for key-frames.

  Vp8Buffers buffer_to_update = kNoBuffer;
  if (number_of_repeated_buffers_ < max_number_of_repeated_buffers_in_a_row_) {
    // TODO(pwestin): experiment with this. The issue with only this change is
    // that we can end up with only 4 frames in flight when we expect 6.
    // buffer_to_update = last_used_vp8_buffer_;
    buffer_to_update = kNoBuffer;
    ++number_of_repeated_buffers_;
  } else {
    number_of_repeated_buffers_ = 0;
    switch (last_used_vp8_buffer_) {
      case kAltRefBuffer:
        buffer_to_update = kLastBuffer;
        VLOG(1) << "VP8 update last buffer";
        break;
      case kLastBuffer:
        buffer_to_update = kGoldenBuffer;
        VLOG(1) << "VP8 update golden buffer";
        break;
      case kGoldenBuffer:
        buffer_to_update = kAltRefBuffer;
        VLOG(1) << "VP8 update alt-ref buffer";
        break;
      case kNoBuffer:
        DCHECK(false) << "Invalid state";
        break;
    }
  }
  return buffer_to_update;
}

void Vp8Encoder::GetCodecUpdateFlags(Vp8Buffers buffer_to_update,
                                     vpx_codec_flags_t* flags) {
  if (!use_multiple_video_buffers_)
    return;

  // Update at most one buffer, except for key-frames.
  switch (buffer_to_update) {
    case kAltRefBuffer:
      *flags |= VP8_EFLAG_NO_UPD_GF;
      *flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kLastBuffer:
      *flags |= VP8_EFLAG_NO_UPD_GF;
      *flags |= VP8_EFLAG_NO_UPD_ARF;
      break;
    case kGoldenBuffer:
      *flags |= VP8_EFLAG_NO_UPD_ARF;
      *flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kNoBuffer:
      *flags |= VP8_EFLAG_NO_UPD_ARF;
      *flags |= VP8_EFLAG_NO_UPD_GF;
      *flags |= VP8_EFLAG_NO_UPD_LAST;
      *flags |= VP8_EFLAG_NO_UPD_ENTROPY;
      break;
  }
}

void Vp8Encoder::UpdateRates(uint32 new_bitrate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  uint32 new_bitrate_kbit = new_bitrate / 1000;
  if (config_->rc_target_bitrate == new_bitrate_kbit)
    return;

  config_->rc_target_bitrate = new_bitrate_kbit;

  // Update encoder context.
  if (vpx_codec_enc_config_set(encoder_.get(), config_.get())) {
    DCHECK(false) << "Invalid return value";
  }
}

void Vp8Encoder::LatestFrameIdToReference(uint32 frame_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!use_multiple_video_buffers_)
    return;

  VLOG(1) << "VP8 ok to reference frame:" << static_cast<int>(frame_id);
  for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
    if (frame_id == used_buffers_frame_id_[i]) {
      acked_frame_buffers_[i] = true;
    }
  }
}

void Vp8Encoder::GenerateKeyFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  key_frame_requested_ = true;
}

// Calculate the max size of the key frame relative to a normal delta frame.
uint32 Vp8Encoder::MaxIntraTarget(uint32 optimal_buffer_size_ms) const {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_parameter.
  // Max target size = scalePar * optimalBufferSize * targetBR[Kbps].
  // This values is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / frameRate.
  // The target in % is as follows:

  float scale_parameter = 0.5;
  uint32 target_pct = optimal_buffer_size_ms * scale_parameter *
                      cast_config_.max_frame_rate / 10;

  // Don't go below 3 times the per frame bandwidth.
  return std::max(target_pct, kMinIntra);
}

}  // namespace cast
}  // namespace media
