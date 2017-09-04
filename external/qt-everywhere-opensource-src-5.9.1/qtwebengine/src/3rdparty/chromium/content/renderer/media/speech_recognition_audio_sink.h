// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_SPEECH_RECOGNITION_AUDIO_SINK_H_
#define CONTENT_RENDERER_MEDIA_SPEECH_RECOGNITION_AUDIO_SINK_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class AudioBus;
class AudioFifo;
}

namespace content {

// SpeechRecognitionAudioSink works as an audio sink to the
// WebRtcLocalAudioTrack. It stores the capture data into a FIFO.
// When the FIFO has enough data for resampling, it converts it,
// passes the buffer to the WebSpeechRecognizer via SharedMemory
// and notifies it via SyncSocket followed by incrementing the |buffer_index_|.
// WebSpeechRecognizer increments the shared buffer index to synchronize.
class CONTENT_EXPORT SpeechRecognitionAudioSink
    : NON_EXPORTED_BASE(public media::AudioConverter::InputCallback),
      NON_EXPORTED_BASE(public MediaStreamAudioSink) {
 public:
  typedef base::Callback<void()> OnStoppedCB;

  // Socket ownership is transferred to the class via constructor.
  SpeechRecognitionAudioSink(const blink::WebMediaStreamTrack& track,
                             const media::AudioParameters& params,
                             const base::SharedMemoryHandle memory,
                             std::unique_ptr<base::SyncSocket> socket,
                             const OnStoppedCB& on_stopped_cb);

  ~SpeechRecognitionAudioSink() override;

  // Returns whether the provided track is supported.
  static bool IsSupportedTrack(const blink::WebMediaStreamTrack& track);

 private:
  // content::MediaStreamAudioSink implementation.
  void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) override;

  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override;
  void OnSetFormat(const media::AudioParameters& params) override;

  // media::AudioConverter::Inputcallback implementation.
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed) override;

  // Returns the pointer to the audio input buffer mapped in the shared memory.
  media::AudioInputBuffer* GetAudioInputBuffer() const;

  // Number of frames per buffer in FIFO. When the buffer is full we convert and
  // consume it on the |output_bus_|. Size of the buffer depends on the
  // resampler. Example: for 44.1 to 16.0 conversion, it should be 4100 frames.
  int fifo_buffer_size_;

  // Used to DCHECK that some methods are called on the main render thread.
  base::ThreadChecker main_render_thread_checker_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;

  // The audio track that this audio sink is connected to.
  const blink::WebMediaStreamTrack track_;

  // Shared memory used by audio buses on both browser and renderer processes.
  base::SharedMemory shared_memory_;

  // Socket for synchronization of audio bus reads/writes.
  // Created on the renderer client and passed here. Accessed on capture thread.
  std::unique_ptr<base::SyncSocket> socket_;

  // Used as a resampler to deliver appropriate format to speech recognition.
  std::unique_ptr<media::AudioConverter> audio_converter_;

  // FIFO is used for queuing audio frames before we resample.
  std::unique_ptr<media::AudioFifo> fifo_;

  // Audio bus shared with the browser process via |shared_memory_|.
  std::unique_ptr<media::AudioBus> output_bus_;

  // Params of the source audio. Can change when |OnSetFormat()| occurs.
  media::AudioParameters input_params_;

  // Params used by speech recognition.
  const media::AudioParameters output_params_;

  // Whether the track has been stopped.
  bool track_stopped_;

  // Local counter of audio buffers for synchronization.
  uint32_t buffer_index_;

  // Callback for the renderer client. Called when the audio track was stopped.
  const OnStoppedCB on_stopped_cb_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionAudioSink);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_SPEECH_RECOGNITION_AUDIO_SINK_H_
