// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webaudio_capturer_source.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"

using media::AudioBus;
using media::AudioFifo;
using media::AudioParameters;
using media::ChannelLayout;
using media::CHANNEL_LAYOUT_MONO;
using media::CHANNEL_LAYOUT_STEREO;

static const int kMaxNumberOfBuffersInFifo = 5;

namespace content {

WebAudioCapturerSource::WebAudioCapturerSource()
    : track_(NULL),
      capturer_(NULL),
      audio_format_changed_(false) {
}

WebAudioCapturerSource::~WebAudioCapturerSource() {
}

void WebAudioCapturerSource::setFormat(
    size_t number_of_channels, float sample_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebAudioCapturerSource::setFormat(sample_rate="
           << sample_rate << ")";
  if (number_of_channels > 2) {
    // TODO(xians): Handle more than just the mono and stereo cases.
    LOG(WARNING) << "WebAudioCapturerSource::setFormat() : unhandled format.";
    return;
  }

  ChannelLayout channel_layout =
      number_of_channels == 1 ? CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;

  base::AutoLock auto_lock(lock_);
  // Set the format used by this WebAudioCapturerSource. We are using 10ms data
  // as buffer size since that is the native buffer size of WebRtc packet
  // running on.
  params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                channel_layout, number_of_channels, 0, sample_rate, 16,
                sample_rate / 100);
  audio_format_changed_ = true;

  wrapper_bus_ = AudioBus::CreateWrapper(params_.channels());
  capture_bus_ = AudioBus::Create(params_);
  audio_data_.reset(
      new int16[params_.frames_per_buffer() * params_.channels()]);
  fifo_.reset(new AudioFifo(
      params_.channels(),
      kMaxNumberOfBuffersInFifo * params_.frames_per_buffer()));
}

void WebAudioCapturerSource::Start(
    WebRtcLocalAudioTrack* track, WebRtcAudioCapturer* capturer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(track);
  base::AutoLock auto_lock(lock_);
  track_ = track;
  capturer_ = capturer;
}

void WebAudioCapturerSource::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  track_ = NULL;
  capturer_ = NULL;
}

void WebAudioCapturerSource::consumeAudio(
    const blink::WebVector<const float*>& audio_data,
    size_t number_of_frames) {
  base::AutoLock auto_lock(lock_);
  if (!track_)
    return;

  // Update the downstream client if the audio format has been changed.
  if (audio_format_changed_) {
    track_->OnSetFormat(params_);
    audio_format_changed_ = false;
  }

  wrapper_bus_->set_frames(number_of_frames);

  // Make sure WebKit is honoring what it told us up front
  // about the channels.
  DCHECK_EQ(params_.channels(), static_cast<int>(audio_data.size()));

  for (size_t i = 0; i < audio_data.size(); ++i)
    wrapper_bus_->SetChannelData(i, const_cast<float*>(audio_data[i]));

  // Handle mismatch between WebAudio buffer-size and WebRTC.
  int available = fifo_->max_frames() - fifo_->frames();
  if (available < static_cast<int>(number_of_frames)) {
    NOTREACHED() << "WebAudioCapturerSource::Consume() : FIFO overrun.";
    return;
  }

  fifo_->Push(wrapper_bus_.get());
  int capture_frames = params_.frames_per_buffer();
  base::TimeDelta delay;
  int volume = 0;
  bool key_pressed = false;
  if (capturer_) {
    capturer_->GetAudioProcessingParams(&delay, &volume, &key_pressed);
  }

  // Turn off audio processing if the delay value is 0, since in such case,
  // it indicates the data is not from microphone.
  // TODO(xians): remove the flag when supporting one APM per audio track.
  // See crbug/264611 for details.
  bool need_audio_processing = (delay.InMilliseconds() != 0);
  while (fifo_->frames() >= capture_frames) {
    fifo_->Consume(capture_bus_.get(), 0, capture_frames);
    // TODO(xians): Avoid this interleave/deinterleave operation.
    capture_bus_->ToInterleaved(capture_bus_->frames(),
                                params_.bits_per_sample() / 8,
                                audio_data_.get());
    track_->Capture(audio_data_.get(), delay, volume, key_pressed,
                    need_audio_processing);
  }
}

}  // namespace content
