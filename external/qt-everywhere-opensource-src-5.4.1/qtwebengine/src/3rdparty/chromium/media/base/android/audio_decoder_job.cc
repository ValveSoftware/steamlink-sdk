// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/audio_decoder_job.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/audio_timestamp_helper.h"

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
    : MediaDecoderJob(g_audio_decoder_thread.Pointer()->message_loop_proxy(),
                      request_data_cb,
                      on_demuxer_config_changed_cb),
      audio_codec_(kUnknownAudioCodec),
      num_channels_(0),
      sampling_rate_(0),
      volume_(-1.0),
      bytes_per_frame_(0) {
}

AudioDecoderJob::~AudioDecoderJob() {}

bool AudioDecoderJob::HasStream() const {
  return audio_codec_ != kUnknownAudioCodec;
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

void AudioDecoderJob::ReleaseOutputBuffer(
    int output_buffer_index,
    size_t size,
    bool render_output,
    base::TimeDelta current_presentation_timestamp,
    const ReleaseOutputCompletionCallback& callback) {
  render_output = render_output && (size != 0u);
  if (render_output) {
    int64 head_position = (static_cast<AudioCodecBridge*>(
        media_codec_bridge_.get()))->PlayOutputBuffer(
            output_buffer_index, size);
    audio_timestamp_helper_->AddFrames(size / bytes_per_frame_);
    int64 frames_to_play =
        audio_timestamp_helper_->frame_count() - head_position;
    DCHECK_GE(frames_to_play, 0);
    current_presentation_timestamp =
        audio_timestamp_helper_->GetTimestamp() -
        audio_timestamp_helper_->GetFrameDuration(frames_to_play);
  } else {
    current_presentation_timestamp = kNoTimestamp();
  }
  media_codec_bridge_->ReleaseOutputBuffer(output_buffer_index, false);

  callback.Run(current_presentation_timestamp,
               audio_timestamp_helper_->GetTimestamp());
}

bool AudioDecoderJob::ComputeTimeToRender() const {
  return false;
}

void AudioDecoderJob::UpdateDemuxerConfigs(const DemuxerConfigs& configs) {
  // TODO(qinmin): split DemuxerConfig for audio and video separately so we
  // can simply store the stucture here.
  audio_codec_ = configs.audio_codec;
  num_channels_ = configs.audio_channels;
  sampling_rate_ = configs.audio_sampling_rate;
  set_is_content_encrypted(configs.is_audio_encrypted);
  audio_extra_data_ = configs.audio_extra_data;
  bytes_per_frame_ = kBytesPerAudioOutputSample * num_channels_;
}

bool AudioDecoderJob::AreDemuxerConfigsChanged(
    const DemuxerConfigs& configs) const {
  return audio_codec_ != configs.audio_codec ||
     num_channels_ != configs.audio_channels ||
     sampling_rate_ != configs.audio_sampling_rate ||
     is_content_encrypted() != configs.is_audio_encrypted ||
     audio_extra_data_.size() != configs.audio_extra_data.size() ||
     !std::equal(audio_extra_data_.begin(),
                 audio_extra_data_.end(),
                 configs.audio_extra_data.begin());
}

bool AudioDecoderJob::CreateMediaCodecBridgeInternal() {
  media_codec_bridge_.reset(AudioCodecBridge::Create(audio_codec_));
  if (!media_codec_bridge_)
    return false;

  if (!(static_cast<AudioCodecBridge*>(media_codec_bridge_.get()))->Start(
      audio_codec_, sampling_rate_, num_channels_, &audio_extra_data_[0],
      audio_extra_data_.size(), true, GetMediaCrypto().obj())) {
    media_codec_bridge_.reset();
    return false;
  }

  SetVolumeInternal();

  // Need to pass the base timestamp to the new decoder.
  if (audio_timestamp_helper_)
    base_timestamp_ = audio_timestamp_helper_->GetTimestamp();
  audio_timestamp_helper_.reset(new AudioTimestampHelper(sampling_rate_));
  audio_timestamp_helper_->SetBaseTimestamp(base_timestamp_);
  return true;
}

void AudioDecoderJob::SetVolumeInternal() {
  if (media_codec_bridge_) {
    static_cast<AudioCodecBridge*>(media_codec_bridge_.get())->SetVolume(
        volume_);
  }
}

}  // namespace media
