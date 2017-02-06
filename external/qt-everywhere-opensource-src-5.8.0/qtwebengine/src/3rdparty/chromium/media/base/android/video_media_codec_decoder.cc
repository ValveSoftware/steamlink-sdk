// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/video_media_codec_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/android/media_statistics.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/demuxer_stream.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {
const int kDelayForStandAloneEOS = 2;  // milliseconds
}

VideoMediaCodecDecoder::VideoMediaCodecDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    FrameStatistics* frame_statistics,
    const base::Closure& request_data_cb,
    const base::Closure& starvation_cb,
    const base::Closure& decoder_drained_cb,
    const base::Closure& stop_done_cb,
    const base::Closure& waiting_for_decryption_key_cb,
    const base::Closure& error_cb,
    const SetTimeCallback& update_current_time_cb,
    const VideoSizeChangedCallback& video_size_changed_cb)
    : MediaCodecDecoder("VideoDecoder",
                        media_task_runner,
                        frame_statistics,
                        request_data_cb,
                        starvation_cb,
                        decoder_drained_cb,
                        stop_done_cb,
                        waiting_for_decryption_key_cb,
                        error_cb),
      is_protected_surface_required_(false),
      update_current_time_cb_(update_current_time_cb),
      video_size_changed_cb_(video_size_changed_cb) {}

VideoMediaCodecDecoder::~VideoMediaCodecDecoder() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "VideoDecoder::~VideoDecoder()";
  ReleaseDecoderResources();
}

const char* VideoMediaCodecDecoder::class_name() const {
  return "VideoDecoder";
}

bool VideoMediaCodecDecoder::HasStream() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return configs_.video_codec != kUnknownVideoCodec;
}

void VideoMediaCodecDecoder::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  DVLOG(1) << class_name() << "::" << __FUNCTION__ << " " << configs;

  configs_ = configs;

  if (video_size_.IsEmpty()) {
    video_size_ = configs_.video_size;
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(video_size_changed_cb_, video_size_));
  }
}

bool VideoMediaCodecDecoder::IsContentEncrypted() const {
  // Make sure SetDemuxerConfigs() as been called.
  DCHECK(configs_.video_codec != kUnknownVideoCodec);
  return configs_.is_video_encrypted;
}

void VideoMediaCodecDecoder::ReleaseDecoderResources() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DoEmergencyStop();

  ReleaseMediaCodec();

  surface_ = gl::ScopedJavaSurface();
}

void VideoMediaCodecDecoder::ReleaseMediaCodec() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  MediaCodecDecoder::ReleaseMediaCodec();
  delayed_buffers_.clear();
}

void VideoMediaCodecDecoder::SetVideoSurface(gl::ScopedJavaSurface surface) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__
           << (surface.IsEmpty() ? " empty" : " non-empty");

  surface_ = std::move(surface);

  needs_reconfigure_ = true;
}

bool VideoMediaCodecDecoder::HasVideoSurface() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return !surface_.IsEmpty();
}

void VideoMediaCodecDecoder::SetProtectedSurfaceRequired(bool value) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  is_protected_surface_required_ = value;
}

bool VideoMediaCodecDecoder::IsProtectedSurfaceRequired() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return is_protected_surface_required_;
}

bool VideoMediaCodecDecoder::IsCodecReconfigureNeeded(
    const DemuxerConfigs& next) const {
  if (always_reconfigure_for_tests_)
    return true;

  if (configs_.video_codec != next.video_codec ||
      configs_.is_video_encrypted != next.is_video_encrypted) {
    return true;
  }

  // Only size changes below this point

  if (configs_.video_size.width() == next.video_size.width() &&
      configs_.video_size.height() == next.video_size.height()) {
    return false;  // i.e. configs_ == next
  }

  return !static_cast<VideoCodecBridge*>(media_codec_bridge_.get())
              ->IsAdaptivePlaybackSupported(next.video_size.width(),
                                            next.video_size.height());
}

MediaCodecDecoder::ConfigStatus VideoMediaCodecDecoder::ConfigureInternal(
    jobject media_crypto) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  // If we cannot find a key frame in cache, the browser seek is needed.
  if (!au_queue_.RewindToLastKeyFrame()) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << " key frame required";
    return kConfigKeyFrameRequired;
  }

  if (configs_.video_codec == kUnknownVideoCodec) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": configuration parameters are required";
    return kConfigFailure;
  }

  if (surface_.IsEmpty()) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__ << ": surface is required";
    return kConfigFailure;
  }

  bool is_secure = IsContentEncrypted() && is_protected_surface_required_;

  media_codec_bridge_.reset(VideoCodecBridge::CreateDecoder(
      configs_.video_codec, is_secure, configs_.video_size,
      surface_.j_surface().obj(), media_crypto));

  if (!media_codec_bridge_) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << " failed: cannot create video codec";
    return kConfigFailure;
  }

  DVLOG(0) << class_name() << "::" << __FUNCTION__ << " succeeded";

  if (!codec_created_for_tests_cb_.is_null())
    media_task_runner_->PostTask(FROM_HERE, codec_created_for_tests_cb_);

  return kConfigOk;
}

void VideoMediaCodecDecoder::AssociateCurrentTimeWithPTS(base::TimeDelta pts) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__ << " pts:" << pts;

  start_time_ticks_ = base::TimeTicks::Now();
  start_pts_ = pts;
  last_seen_pts_ = pts;
}

void VideoMediaCodecDecoder::DissociatePTSFromTime() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  start_pts_ = last_seen_pts_ = kNoTimestamp();
}

bool VideoMediaCodecDecoder::OnOutputFormatChanged() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  gfx::Size prev_size = video_size_;

  // See b/18224769. The values reported from MediaCodecBridge::GetOutputSize
  // correspond to the actual video frame size, but this is not necessarily the
  // size that should be output.
  video_size_ = configs_.video_size;
  if (video_size_ != prev_size) {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(video_size_changed_cb_, video_size_));
  }
  return true;
}

bool VideoMediaCodecDecoder::Render(int buffer_index,
                                    size_t offset,
                                    size_t size,
                                    RenderMode render_mode,
                                    base::TimeDelta pts,
                                    bool eos_encountered) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts
           << (eos_encountered ? " EOS " : " ") << AsString(render_mode);

  // Normally EOS comes as a separate access unit that does not have data,
  // the corresponding |size| will be 0.
  if (!size && eos_encountered) {
    // Stand-alone EOS
    // Discard the PTS that comes with it and ensure it is released last.
    pts = last_seen_pts_ +
          base::TimeDelta::FromMilliseconds(kDelayForStandAloneEOS);
  } else {
    // Keep track of last seen PTS
    last_seen_pts_ = pts;
  }

  // Do not update time for stand-alone EOS.
  const bool update_time = !(eos_encountered && size == 0u);

  // For video we simplify the preroll operation and render the first frame
  // after preroll during the preroll phase, i.e. without waiting for audio
  // stream to finish prerolling.
  switch (render_mode) {
    case kRenderSkip:
      ReleaseOutputBuffer(buffer_index, pts, false, false, eos_encountered);
      return true;
    case kRenderAfterPreroll:
      // We get here in the preroll phase. Render now as explained above.
      // |start_pts_| is not set yet, thus we cannot calculate |time_to_render|.
      ReleaseOutputBuffer(buffer_index, pts, (size > 0), update_time,
                          eos_encountered);
      return true;
    case kRenderNow:
      break;
  }

  DCHECK_EQ(kRenderNow, render_mode);
  DCHECK_NE(kNoTimestamp(), start_pts_);  // start_pts_ must be set

  base::TimeDelta time_to_render =
      pts - (base::TimeTicks::Now() - start_time_ticks_ + start_pts_);

  DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts
           << " ticks delta:" << (base::TimeTicks::Now() - start_time_ticks_)
           << " time_to_render:" << time_to_render;

  const bool render = (size > 0);

  if (render)
    frame_statistics_->IncrementFrameCount();

  if (time_to_render < base::TimeDelta()) {
    if (render) {
      DVLOG(2) << class_name() << "::" << __FUNCTION__
               << " LATE FRAME delay:" << (-1) * time_to_render;

      frame_statistics_->IncrementLateFrameCount();
    }

    // Render late frames immediately.
    ReleaseOutputBuffer(buffer_index, pts, render, update_time,
                        eos_encountered);
    return true;
  }

  delayed_buffers_.insert(buffer_index);

  decoder_thread_.task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&VideoMediaCodecDecoder::ReleaseOutputBuffer,
                            base::Unretained(this), buffer_index, pts, render,
                            update_time, eos_encountered),
      time_to_render);

  return true;
}

int VideoMediaCodecDecoder::NumDelayedRenderTasks() const {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  return delayed_buffers_.size();
}

void VideoMediaCodecDecoder::ReleaseDelayedBuffers() {
  // Media thread
  // Called when there is no decoder thread
  for (int index : delayed_buffers_)
    media_codec_bridge_->ReleaseOutputBuffer(index, false);

  delayed_buffers_.clear();
}

#ifndef NDEBUG
void VideoMediaCodecDecoder::VerifyUnitIsKeyFrame(
    const AccessUnit* unit) const {
  // The first video frame in a sequence must be a key frame or stand-alone EOS.
  DCHECK(unit);
  bool stand_alone_eos = unit->is_end_of_stream && unit->data.empty();
  DCHECK(stand_alone_eos || unit->is_key_frame);
}
#endif

void VideoMediaCodecDecoder::ReleaseOutputBuffer(int buffer_index,
                                                 base::TimeDelta pts,
                                                 bool render,
                                                 bool update_time,
                                                 bool eos_encountered) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts;

  // Do not render if we are in emergency stop, there might be no surface.
  if (InEmergencyStop())
    render = false;

  media_codec_bridge_->ReleaseOutputBuffer(buffer_index, render);

  delayed_buffers_.erase(buffer_index);

  CheckLastFrame(eos_encountered, !delayed_buffers_.empty());

  // |update_current_time_cb_| might be null if there is audio stream.
  // Do not update current time for stand-alone EOS frames.
  if (!update_current_time_cb_.is_null() && update_time) {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(update_current_time_cb_, pts, pts, false));
  }
}

}  // namespace media
