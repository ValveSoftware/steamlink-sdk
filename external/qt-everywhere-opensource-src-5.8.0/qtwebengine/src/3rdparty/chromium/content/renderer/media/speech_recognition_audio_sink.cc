// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/speech_recognition_audio_sink.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"

namespace content {

SpeechRecognitionAudioSink::SpeechRecognitionAudioSink(
    const blink::WebMediaStreamTrack& track,
    const media::AudioParameters& params,
    const base::SharedMemoryHandle memory,
    std::unique_ptr<base::SyncSocket> socket,
    const OnStoppedCB& on_stopped_cb)
    : track_(track),
      shared_memory_(memory, false),
      socket_(std::move(socket)),
      output_params_(params),
      track_stopped_(false),
      buffer_index_(0),
      on_stopped_cb_(on_stopped_cb) {
  DCHECK(socket_.get());
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(params.IsValid());
  DCHECK(IsSupportedTrack(track));
  const size_t kSharedMemorySize = sizeof(media::AudioInputBufferParameters) +
                                   media::AudioBus::CalculateMemorySize(params);
  CHECK(shared_memory_.Map(kSharedMemorySize));

  media::AudioInputBuffer* buffer =
      static_cast<media::AudioInputBuffer*>(shared_memory_.memory());

  // The peer must manage their own counter and reset it to 0.
  DCHECK_EQ(0U, buffer->params.size);
  output_bus_ = media::AudioBus::WrapMemory(params, buffer->audio);

  // Connect this audio sink to the track
  MediaStreamAudioSink::AddToAudioTrack(this, track_);
}

SpeechRecognitionAudioSink::~SpeechRecognitionAudioSink() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (audio_converter_.get())
    audio_converter_->RemoveInput(this);

  // Notify the track before this sink goes away.
  if (!track_stopped_)
    MediaStreamAudioSink::RemoveFromAudioTrack(this, track_);
}

// static
bool SpeechRecognitionAudioSink::IsSupportedTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamAudioSource* native_source =
      MediaStreamAudioSource::From(track.source());
  if (!native_source)
    return false;

  const StreamDeviceInfo& device_info = native_source->device_info();
  // Purposely only support tracks from an audio device. Dissallow WebAudio.
  return (device_info.device.type == content::MEDIA_DEVICE_AUDIO_CAPTURE);
}

void SpeechRecognitionAudioSink::OnSetFormat(
    const media::AudioParameters& input_params) {
  DCHECK(input_params.IsValid());
  DCHECK_LE(
      input_params.frames_per_buffer() * 1000 / input_params.sample_rate(),
      output_params_.frames_per_buffer() * 1000 / output_params_.sample_rate());

  // Detach the thread here because it will be a new capture thread
  // calling OnSetFormat() and OnData() if the source is restarted.
  capture_thread_checker_.DetachFromThread();

  input_params_ = input_params;
  fifo_buffer_size_ =
      std::ceil(output_params_.frames_per_buffer() *
                static_cast<double>(input_params_.sample_rate()) /
                    output_params_.sample_rate());
  DCHECK_GE(fifo_buffer_size_, input_params_.frames_per_buffer());

  // Allows for some delays on the peer.
  static const int kNumberOfBuffersInFifo = 2;
  int frames_in_fifo = kNumberOfBuffersInFifo * fifo_buffer_size_;
  fifo_.reset(new media::AudioFifo(input_params.channels(), frames_in_fifo));

  // Create the audio converter with |disable_fifo| as false so that the
  // converter will request input_params.frames_per_buffer() each time.
  // This will not increase the complexity as there is only one client to
  // the converter.
  audio_converter_.reset(
      new media::AudioConverter(input_params, output_params_, false));
  audio_converter_->AddInput(this);
}

void SpeechRecognitionAudioSink::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!track_stopped_);

  if (state == blink::WebMediaStreamSource::ReadyStateEnded) {
    track_stopped_ = true;

    if (!on_stopped_cb_.is_null())
      on_stopped_cb_.Run();
  }
}

void SpeechRecognitionAudioSink::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(audio_bus.frames(), input_params_.frames_per_buffer());
  DCHECK_EQ(audio_bus.channels(), input_params_.channels());
  if (fifo_->frames() + audio_bus.frames() > fifo_->max_frames()) {
    // This would indicate a serious issue with the browser process or the
    // SyncSocket and/or SharedMemory. We drop any previous buffers and try to
    // recover by resuming where the peer left of.
    DLOG(ERROR) << "Audio FIFO overflow";
    fifo_->Clear();
    buffer_index_ = GetAudioInputBuffer()->params.size;
  }

  fifo_->Push(&audio_bus);
  // Wait for FIFO to have at least |fifo_buffer_size_| frames ready.
  if (fifo_->frames() < fifo_buffer_size_)
    return;

  // Make sure the previous output buffer was consumed by the peer before we
  // send the next buffer.
  // The peer must write to it (incrementing by 1) once the the buffer was
  // consumed. This is intentional not to block this audio capturing thread.
  if (buffer_index_ != GetAudioInputBuffer()->params.size) {
    DVLOG(1) << "Buffer synchronization lag";
    return;
  }

  audio_converter_->Convert(output_bus_.get());

  // Notify peer to consume buffer |buffer_index_| on |output_bus_|.
  const size_t bytes_sent =
      socket_->Send(&buffer_index_, sizeof(buffer_index_));
  if (bytes_sent != sizeof(buffer_index_)) {
    // The send ocasionally fails if the user changes their input audio device.
    DVLOG(1) << "Failed sending buffer index to peer";
    // We have discarded this buffer, but could still recover on the next one.
    return;
  }

  // Count the sent buffer. We expect the peer to do the same on their end.
  ++buffer_index_;
}

double SpeechRecognitionAudioSink::ProvideInput(media::AudioBus* audio_bus,
                                                uint32_t frames_delayed) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  if (fifo_->frames() >= audio_bus->frames())
    fifo_->Consume(audio_bus, 0, audio_bus->frames());
  else
    audio_bus->Zero();

  // Return volume greater than zero to indicate we have more data.
  return 1.0;
}

media::AudioInputBuffer*
SpeechRecognitionAudioSink::GetAudioInputBuffer() const {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(shared_memory_.memory());
  return static_cast<media::AudioInputBuffer*>(shared_memory_.memory());
}

}  // namespace content
