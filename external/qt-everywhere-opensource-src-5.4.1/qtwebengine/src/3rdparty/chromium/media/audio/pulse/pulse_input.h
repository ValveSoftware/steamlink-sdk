// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_PULSE_INPUT_H_
#define MEDIA_AUDIO_PULSE_PULSE_INPUT_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

struct pa_context;
struct pa_source_info;
struct pa_stream;
struct pa_threaded_mainloop;

namespace media {

class AudioManagerPulse;
class SeekableBuffer;

class PulseAudioInputStream : public AgcAudioStream<AudioInputStream> {
 public:
  PulseAudioInputStream(AudioManagerPulse* audio_manager,
                        const std::string& device_name,
                        const AudioParameters& params,
                        pa_threaded_mainloop* mainloop,
                        pa_context* context);

  virtual ~PulseAudioInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;

 private:
  // PulseAudio Callbacks.
  static void ReadCallback(pa_stream* handle, size_t length, void* user_data);
  static void StreamNotifyCallback(pa_stream* stream, void* user_data);
  static void VolumeCallback(pa_context* context, const pa_source_info* info,
                             int error, void* user_data);

  // Helper for the ReadCallback.
  void ReadData();

  AudioManagerPulse* audio_manager_;
  AudioInputCallback* callback_;
  std::string device_name_;
  AudioParameters params_;
  int channels_;
  double volume_;
  bool stream_started_;

  // Holds the data from the OS.
  scoped_ptr<media::SeekableBuffer> buffer_;

  // Temporary storage for recorded data. It gets a packet of data from
  // |buffer_| and deliver the data to OnData() callback.
  scoped_ptr<uint8[]> audio_data_buffer_;

  // PulseAudio API structs.
  pa_threaded_mainloop* pa_mainloop_; // Weak.
  pa_context* pa_context_;  // Weak.
  pa_stream* handle_;

  // Flag indicating the state of the context has been changed.
  bool context_state_changed_;

  scoped_ptr<AudioBus> audio_bus_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_INPUT_H_
