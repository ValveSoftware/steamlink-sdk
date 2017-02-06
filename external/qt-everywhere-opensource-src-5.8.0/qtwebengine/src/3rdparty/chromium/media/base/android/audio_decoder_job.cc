// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/audio_decoder_job.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/timestamp_constants.h"

namespace {

// Use 16bit PCM for audio output. Keep this value in sync with the output
// format we passed to AudioTrack in MediaCodecBridge.
const int kBytesPerAudioOutputSample = 2;
}

namespace media {

class AudioDecoderThread : public base::Thread {
 public:
  AudioDecoderThread() : base::Thread("MediaSource_AudioDecoderThread") {
    Start();
  }
};

// TODO(qinmin): Check if it is tolerable to use worker pool to handle all the
// decoding tasks so that we don't need a global thread here.
// http://crbug.com/245750
base::LazyInstance<AudioDecoderThread>::Leaky
    g_audio_decoder_thread = LAZY_INSTANCE_INITIALIZER;

AudioDecoderJob::AudioDecoderJob(
    const base::Closure& request_data_cb,
    const base::Closure& on_demuxer_config_changed_cb)
    : MediaDecoderJob(g_audio_decoder_thread.Pointer()->task_runner(),
                      request_data_cb,
                      on_demuxer_config_changed_cb),
      audio_codec_(kUnknownAudioCodec),
      config_num_channels_(0),
      config_sampling_rate_(0),
      volume_(-1.0),
      output_sampling_rate_(0),
      output_num_channels_(0),
      frame_count_(0) {}

AudioDecoderJob::~AudioDecoderJob() {}

bool AudioDecoderJob::HasStream() const {
  return audio_codec_ != kUnknownAudioCodec;
}

void AudioDecoderJob::Flush() {
  MediaDecoderJob::Flush();
  frame_count_ = 0;
}

void AudioDecoderJob::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  // TODO(qinmin): split DemuxerConfig for audio and video separately so we
  // can simply store the stucture here.
  audio_codec_ = configs.audio_codec;
  config_num_channels_ = configs.audio_channels;
  config_sampling_rate_ = configs.audio_sampling_rate;
  set_is_content_encrypted(configs.is_audio_encrypted);
  audio_extra_data_ = configs.audio_extra_data;
  audio_codec_delay_ns_ = configs.audio_codec_delay_ns;
  audio_seek_preroll_ns_ = configs.audio_seek_preroll_ns;

  if (!media_codec_bridge_) {
    output_sampling_rate_ = config_sampling_rate_;
    output_num_channels_ = config_num_channels_;
  }
}

void AudioDecoderJob::SetVolume(double volume) {
  volume_ = volume;
  SetVolumeInternal();
}

void AudioDecoderJob::SetBaseTimestamp(base::TimeDelta base_timestamp) {
  DCHECK(!is_decoding());
  base_timestamp_ = base_timestamp;
  if (audio_timestamp_helper_)
    audio_timestamp_helper_->SetBaseTimestamp(base_timestamp_);
}

void AudioDecoderJob::ResetTimestampHelper() {
  if (audio_timestamp_helper_)
    base_timestamp_ = audio_timestamp_helper_->GetTimestamp();
  audio_timestamp_helper_.reset(
      new AudioTimestampHelper(output_sampling_rate_));
  audio_timestamp_helper_->SetBaseTimestamp(base_timestamp_);
}

void AudioDecoderJob::ReleaseOutputBuffer(
    int output_buffer_index,
    size_t offset,
    size_t size,
    bool render_output,
    bool /* is_late_frame */,
    base::TimeDelta current_presentation_timestamp,
    MediaCodecStatus status,
    const DecoderCallback& callback) {
  render_output = render_output && (size != 0u);
  bool is_audio_underrun = false;

  // Ignore input value.
  current_presentation_timestamp = kNoTimestamp();

  if (render_output) {
    int64_t head_position;
    MediaCodecStatus play_status =
        (static_cast<AudioCodecBridge*>(media_codec_bridge_.get()))
            ->PlayOutputBuffer(output_buffer_index, size, offset, false,
                               &head_position);
    if (play_status == MEDIA_CODEC_OK) {
      base::TimeTicks current_time = base::TimeTicks::Now();

      size_t bytes_per_frame =
          kBytesPerAudioOutputSample * output_num_channels_;
      size_t new_frames_count = size / bytes_per_frame;
      frame_count_ += new_frames_count;
      audio_timestamp_helper_->AddFrames(new_frames_count);
      int64_t frames_to_play = frame_count_ - head_position;
      DCHECK_GE(frames_to_play, 0);

      const base::TimeDelta last_buffered =
          audio_timestamp_helper_->GetTimestamp();

      current_presentation_timestamp =
          last_buffered -
          audio_timestamp_helper_->GetFrameDuration(frames_to_play);

      // Potential audio underrun is considered a late frame for UMA.
      is_audio_underrun = !next_frame_time_limit_.is_null() &&
                          next_frame_time_limit_ < current_time;

      next_frame_time_limit_ =
          current_time + (last_buffered - current_presentation_timestamp);
    } else {
      DLOG(ERROR) << __FUNCTION__ << ": PlayOutputBuffer failed for index:"
                  << output_buffer_index;

      // Override output status.
      status = MEDIA_CODEC_ERROR;
    }
  }

  media_codec_bridge_->ReleaseOutputBuffer(output_buffer_index, false);

  callback.Run(status, is_audio_underrun, current_presentation_timestamp,
               audio_timestamp_helper_->GetTimestamp());
}

bool AudioDecoderJob::ComputeTimeToRender() const {
  return false;
}

bool AudioDecoderJob::AreDemuxerConfigsChanged(
    const DemuxerConfigs& configs) const {
  return audio_codec_ != configs.audio_codec ||
         config_num_channels_ != configs.audio_channels ||
         config_sampling_rate_ != configs.audio_sampling_rate ||
         is_content_encrypted() != configs.is_audio_encrypted ||
         audio_extra_data_.size() != configs.audio_extra_data.size() ||
         !std::equal(audio_extra_data_.begin(), audio_extra_data_.end(),
                     configs.audio_extra_data.begin());
}

MediaDecoderJob::MediaDecoderJobStatus
    AudioDecoderJob::CreateMediaCodecBridgeInternal() {
  media_codec_bridge_.reset(AudioCodecBridge::Create(audio_codec_));
  if (!media_codec_bridge_)
    return STATUS_FAILURE;

  if (!(static_cast<AudioCodecBridge*>(media_codec_bridge_.get()))
           ->ConfigureAndStart(audio_codec_, config_sampling_rate_,
                               config_num_channels_, &audio_extra_data_[0],
                               audio_extra_data_.size(), audio_codec_delay_ns_,
                               audio_seek_preroll_ns_, true,
                               GetMediaCrypto())) {
    media_codec_bridge_.reset();
    return STATUS_FAILURE;
  }

  // ConfigureAndStart() creates AudioTrack with |config_sampling_rate_|
  // and |config_num_channels_|. Keep |output_...| in sync to detect the changes
  // that might come with OnOutputFormatChanged().
  output_sampling_rate_ = config_sampling_rate_;
  output_num_channels_ = config_num_channels_;

  SetVolumeInternal();

  // Reset values used to track codec bridge output
  frame_count_ = 0;
  ResetTimestampHelper();

  return STATUS_SUCCESS;
}

void AudioDecoderJob::SetVolumeInternal() {
  if (media_codec_bridge_) {
    static_cast<AudioCodecBridge*>(media_codec_bridge_.get())->SetVolume(
        volume_);
  }
}

bool AudioDecoderJob::OnOutputFormatChanged() {
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

}  // namespace media
