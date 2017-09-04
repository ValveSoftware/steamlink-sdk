// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/local_media_stream_audio_source.h"

#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/render_frame_impl.h"

namespace content {

LocalMediaStreamAudioSource::LocalMediaStreamAudioSource(
    int consumer_render_frame_id,
    const StreamDeviceInfo& device_info)
    : MediaStreamAudioSource(true /* is_local_source */),
      consumer_render_frame_id_(consumer_render_frame_id) {
  DVLOG(1) << "LocalMediaStreamAudioSource::LocalMediaStreamAudioSource()";
  MediaStreamSource::SetDeviceInfo(device_info);

  // If the device buffer size was not provided, use a default.
  int frames_per_buffer = device_info.device.input.frames_per_buffer;
  if (frames_per_buffer <= 0) {
    // TODO(miu): Like in ProcessedLocalAudioSource::GetBufferSize(), we should
    // re-evaluate whether Android needs special treatment here. Or, perhaps we
    // should just DCHECK_GT(device_info...frames_per_buffer, 0)?
    // http://crbug.com/638081
#if defined(OS_ANDROID)
    frames_per_buffer = device_info.device.input.sample_rate / 50;  // 20 ms
#else
    frames_per_buffer = device_info.device.input.sample_rate / 100;  // 10 ms
#endif
  }

  MediaStreamAudioSource::SetFormat(media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      static_cast<media::ChannelLayout>(
          device_info.device.input.channel_layout),
      device_info.device.input.sample_rate,
      16,  // Legacy parameter (data is always in 32-bit float format).
      frames_per_buffer));
}

LocalMediaStreamAudioSource::~LocalMediaStreamAudioSource() {
  DVLOG(1) << "LocalMediaStreamAudioSource::~LocalMediaStreamAudioSource()";
  EnsureSourceIsStopped();
}

bool LocalMediaStreamAudioSource::EnsureSourceIsStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (source_)
    return true;

  // Sanity-check that the consuming RenderFrame still exists. This is required
  // by AudioDeviceFactory.
  if (!RenderFrameImpl::FromRoutingID(consumer_render_frame_id_))
    return false;

  VLOG(1) << "Starting local audio input device (session_id="
          << device_info().session_id << ") for render frame "
          << consumer_render_frame_id_ << " with audio parameters={"
          << GetAudioParameters().AsHumanReadableString() << "}.";

  source_ =
      AudioDeviceFactory::NewAudioCapturerSource(consumer_render_frame_id_);
  source_->Initialize(GetAudioParameters(), this, device_info().session_id);
  source_->Start();
  return true;
}

void LocalMediaStreamAudioSource::EnsureSourceIsStopped() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!source_)
    return;

  source_->Stop();
  source_ = nullptr;

  VLOG(1) << "Stopped local audio input device (session_id="
          << device_info().session_id << ") for render frame "
          << consumer_render_frame_id_ << " with audio parameters={"
          << GetAudioParameters().AsHumanReadableString() << "}.";
}

void LocalMediaStreamAudioSource::Capture(const media::AudioBus* audio_bus,
                                          int audio_delay_milliseconds,
                                          double volume,
                                          bool key_pressed) {
  DCHECK(audio_bus);
  // TODO(miu): Plumbing is needed to determine the actual capture timestamp
  // of the audio, instead of just snapshotting TimeTicks::Now(), for proper
  // audio/video sync. http://crbug.com/335335
  MediaStreamAudioSource::DeliverDataToTracks(
      *audio_bus,
      base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds));
}

void LocalMediaStreamAudioSource::OnCaptureError(const std::string& why) {
  StopSourceOnError(why);
}

}  // namespace content
