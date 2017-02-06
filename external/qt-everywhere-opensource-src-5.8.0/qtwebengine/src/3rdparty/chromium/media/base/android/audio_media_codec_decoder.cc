// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/audio_media_codec_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/android/media_statistics.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/demuxer_stream.h"

namespace {

// Use 16bit PCM for audio output. Keep this value in sync with the output
// format we passed to AudioTrack in MediaCodecBridge.
const int kBytesPerAudioOutputSample = 2;
}

namespace media {

AudioMediaCodecDecoder::AudioMediaCodecDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    FrameStatistics* frame_statistics,
    const base::Closure& request_data_cb,
    const base::Closure& starvation_cb,
    const base::Closure& decoder_drained_cb,
    const base::Closure& stop_done_cb,
    const base::Closure& waiting_for_decryption_key_cb,
    const base::Closure& error_cb,
    const SetTimeCallback& update_current_time_cb)
    : MediaCodecDecoder("AudioDecoder",
                        media_task_runner,
                        frame_statistics,
                        request_data_cb,
                        starvation_cb,
                        decoder_drained_cb,
                        stop_done_cb,
                        waiting_for_decryption_key_cb,
                        error_cb),
      volume_(-1.0),
      output_sampling_rate_(0),
      output_num_channels_(0),
      frame_count_(0),
      update_current_time_cb_(update_current_time_cb) {}

AudioMediaCodecDecoder::~AudioMediaCodecDecoder() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "AudioDecoder::~AudioDecoder()";
  ReleaseDecoderResources();
}

const char* AudioMediaCodecDecoder::class_name() const {
  return "AudioDecoder";
}

bool AudioMediaCodecDecoder::HasStream() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return configs_.audio_codec != kUnknownAudioCodec;
}

void AudioMediaCodecDecoder::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  DVLOG(1) << class_name() << "::" << __FUNCTION__ << " " << configs;

  configs_ = configs;
  if (!media_codec_bridge_) {
    output_sampling_rate_ = configs.audio_sampling_rate;
    output_num_channels_ = configs.audio_channels;
  }
}

bool AudioMediaCodecDecoder::IsContentEncrypted() const {
  // Make sure SetDemuxerConfigs() as been called.
  DCHECK(configs_.audio_codec != kUnknownAudioCodec);
  return configs_.is_audio_encrypted;
}

void AudioMediaCodecDecoder::ReleaseDecoderResources() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DoEmergencyStop();

  ReleaseMediaCodec();
}

void AudioMediaCodecDecoder::Flush() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  MediaCodecDecoder::Flush();
  frame_count_ = 0;
}

void AudioMediaCodecDecoder::SetVolume(double volume) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__ << " " << volume;

  volume_ = volume;
  SetVolumeInternal();
}

void AudioMediaCodecDecoder::SetBaseTimestamp(base::TimeDelta base_timestamp) {
  // Called from Media thread and Decoder thread. When called from Media thread
  // Decoder thread should not be running.

  DVLOG(1) << __FUNCTION__ << " " << base_timestamp;

  base_timestamp_ = base_timestamp;
  if (audio_timestamp_helper_)
    audio_timestamp_helper_->SetBaseTimestamp(base_timestamp_);
}

bool AudioMediaCodecDecoder::IsCodecReconfigureNeeded(
    const DemuxerConfigs& next) const {
  if (always_reconfigure_for_tests_)
    return true;

  return configs_.audio_codec != next.audio_codec ||
         configs_.audio_channels != next.audio_channels ||
         configs_.audio_sampling_rate != next.audio_sampling_rate ||
         configs_.is_audio_encrypted != next.is_audio_encrypted ||
         configs_.audio_extra_data.size() != next.audio_extra_data.size() ||
         !std::equal(configs_.audio_extra_data.begin(),
                     configs_.audio_extra_data.end(),
                     next.audio_extra_data.begin());
}

MediaCodecDecoder::ConfigStatus AudioMediaCodecDecoder::ConfigureInternal(
    jobject media_crypto) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  if (configs_.audio_codec == kUnknownAudioCodec) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << " configuration parameters are required";
    return kConfigFailure;
  }

  media_codec_bridge_.reset(AudioCodecBridge::Create(configs_.audio_codec));
  if (!media_codec_bridge_)
    return kConfigFailure;

  if (!(static_cast<AudioCodecBridge*>(media_codec_bridge_.get()))
           ->ConfigureAndStart(
               configs_.audio_codec, configs_.audio_sampling_rate,
               configs_.audio_channels, &configs_.audio_extra_data[0],
               configs_.audio_extra_data.size(), configs_.audio_codec_delay_ns,
               configs_.audio_seek_preroll_ns, true, media_crypto)) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << " failed: cannot start audio codec";

    media_codec_bridge_.reset();
    return kConfigFailure;
  }

  DVLOG(0) << class_name() << "::" << __FUNCTION__ << " succeeded";

  // ConfigureAndStart() creates AudioTrack with sampling rate and channel count
  // from |configs_|. Keep |output_...| in sync to detect the changes that might
  // come with OnOutputFormatChanged().
  output_sampling_rate_ = configs_.audio_sampling_rate;
  output_num_channels_ = configs_.audio_channels;

  SetVolumeInternal();

  frame_count_ = 0;
  ResetTimestampHelper();

  if (!codec_created_for_tests_cb_.is_null())
    media_task_runner_->PostTask(FROM_HERE, codec_created_for_tests_cb_);

  return kConfigOk;
}

bool AudioMediaCodecDecoder::OnOutputFormatChanged() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DCHECK(media_codec_bridge_);

  // Recreate AudioTrack if either sample rate or output channel count changed.
  // If we cannot obtain these values we assume they did not change.
  bool needs_recreate_audio_track = false;

  const int old_sampling_rate = output_sampling_rate_;
  MediaCodecStatus status =
      media_codec_bridge_->GetOutputSamplingRate(&output_sampling_rate_);

  if (status == MEDIA_CODEC_OK && old_sampling_rate != output_sampling_rate_) {
    DCHECK_GT(output_sampling_rate_, 0);
    DVLOG(2) << __FUNCTION__ << ": new sampling rate " << output_sampling_rate_;
    needs_recreate_audio_track = true;

    ResetTimestampHelper();
  }

  const int old_num_channels = output_num_channels_;
  status = media_codec_bridge_->GetOutputChannelCount(&output_num_channels_);

  if (status == MEDIA_CODEC_OK && old_num_channels != output_num_channels_) {
    DCHECK_GT(output_num_channels_, 0);
    DVLOG(2) << __FUNCTION__ << ": new channel count " << output_num_channels_;
    needs_recreate_audio_track = true;
  }

  if (needs_recreate_audio_track &&
      !static_cast<AudioCodecBridge*>(media_codec_bridge_.get())
           ->CreateAudioTrack(output_sampling_rate_, output_num_channels_)) {
    DLOG(ERROR) << __FUNCTION__ << ": cannot create AudioTrack";
    return false;
  }

  return true;
}

bool AudioMediaCodecDecoder::Render(int buffer_index,
                                    size_t offset,
                                    size_t size,
                                    RenderMode render_mode,
                                    base::TimeDelta pts,
                                    bool eos_encountered) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts << " "
           << AsString(render_mode);

  const bool do_play = (render_mode != kRenderSkip);

  if (do_play) {
    AudioCodecBridge* audio_codec =
        static_cast<AudioCodecBridge*>(media_codec_bridge_.get());

    DCHECK(audio_codec);

    const bool postpone = (render_mode == kRenderAfterPreroll);

    int64_t head_position;
    MediaCodecStatus status = audio_codec->PlayOutputBuffer(
        buffer_index, size, offset, postpone, &head_position);

    if (status != MEDIA_CODEC_OK) {
      DLOG(ERROR) << class_name() << "::" << __FUNCTION__ << " pts:" << pts
                  << " PlayOutputBuffer failed for index:" << buffer_index;
      media_codec_bridge_->ReleaseOutputBuffer(buffer_index, false);
      return false;
    }

    base::TimeTicks current_time = base::TimeTicks::Now();

    frame_statistics_->IncrementFrameCount();

    // Reset the base timestamp if we have not started playing.
    // SetBaseTimestamp() must be called before AddFrames() since it resets the
    // internal frame count.
    if (postpone && !frame_count_)
      SetBaseTimestamp(pts);

    const size_t bytes_per_frame =
        kBytesPerAudioOutputSample * output_num_channels_;
    const size_t new_frames_count = size / bytes_per_frame;
    frame_count_ += new_frames_count;
    audio_timestamp_helper_->AddFrames(new_frames_count);

    if (postpone) {
      DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts
               << " POSTPONE";

      // Let the player adjust the start time.
      media_task_runner_->PostTask(
          FROM_HERE, base::Bind(update_current_time_cb_, pts, pts, true));
    } else {
      int64_t frames_to_play = frame_count_ - head_position;

      DCHECK_GE(frames_to_play, 0) << class_name() << "::" << __FUNCTION__
                                   << " pts:" << pts
                                   << " frame_count_:" << frame_count_
                                   << " head_position:" << head_position;

      base::TimeDelta last_buffered = audio_timestamp_helper_->GetTimestamp();
      base::TimeDelta now_playing =
          last_buffered -
          audio_timestamp_helper_->GetFrameDuration(frames_to_play);

      DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts
               << " will play: [" << now_playing << "," << last_buffered << "]";

      // Statistics
      if (!next_frame_time_limit_.is_null() &&
          next_frame_time_limit_ < current_time) {
        DVLOG(2) << class_name() << "::" << __FUNCTION__ << " LATE FRAME delay:"
                 << current_time - next_frame_time_limit_;
        frame_statistics_->IncrementLateFrameCount();
      }

      next_frame_time_limit_ = current_time + (last_buffered - now_playing);

      media_task_runner_->PostTask(
          FROM_HERE, base::Bind(update_current_time_cb_, now_playing,
                                last_buffered, false));
    }
  }

  media_codec_bridge_->ReleaseOutputBuffer(buffer_index, false);

  CheckLastFrame(eos_encountered, false);  // no delayed tasks

  return true;
}

void AudioMediaCodecDecoder::SetVolumeInternal() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (media_codec_bridge_) {
    static_cast<AudioCodecBridge*>(media_codec_bridge_.get())
        ->SetVolume(volume_);
  }
}

void AudioMediaCodecDecoder::ResetTimestampHelper() {
  // Media thread or Decoder thread
  // When this method is called on Media thread, decoder thread
  // should not be running.

  if (audio_timestamp_helper_)
    base_timestamp_ = audio_timestamp_helper_->GetTimestamp();

  audio_timestamp_helper_.reset(
      new AudioTimestampHelper(output_sampling_rate_));

  audio_timestamp_helper_->SetBaseTimestamp(base_timestamp_);
}

}  // namespace media
