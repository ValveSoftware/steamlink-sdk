// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ALSA_ALSA_INPUT_H_
#define MEDIA_AUDIO_ALSA_ALSA_INPUT_H_

#include <alsa/asoundlib.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AlsaWrapper;
class AudioManagerBase;

// Provides an input stream for audio capture based on the ALSA PCM interface.
// This object is not thread safe and all methods should be invoked in the
// thread that created the object.
class AlsaPcmInputStream : public AgcAudioStream<AudioInputStream> {
 public:
  // Pass this to the constructor if you want to attempt auto-selection
  // of the audio recording device.
  static const char kAutoSelectDevice[];

  // Create a PCM Output stream for the ALSA device identified by
  // |device_name|. If unsure of what to use for |device_name|, use
  // |kAutoSelectDevice|.
  AlsaPcmInputStream(AudioManagerBase* audio_manager,
                     const std::string& device_name,
                     const AudioParameters& params,
                     AlsaWrapper* wrapper);

  virtual ~AlsaPcmInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;

 private:
  // Logs the error and invokes any registered callbacks.
  void HandleError(const char* method, int error);

  // Reads one or more buffers of audio from the device, passes on to the
  // registered callback and schedules the next read.
  void ReadAudio();

  // Recovers from any device errors if possible.
  bool Recover(int error);

  // Utility function for talking with the ALSA API.
  snd_pcm_sframes_t GetCurrentDelay();

  // Non-refcounted pointer back to the audio manager.
  // The AudioManager indirectly holds on to stream objects, so we don't
  // want circular references.  Additionally, stream objects live on the audio
  // thread, which is owned by the audio manager and we don't want to addref
  // the manager from that thread.
  AudioManagerBase* audio_manager_;
  std::string device_name_;
  AudioParameters params_;
  int bytes_per_buffer_;
  AlsaWrapper* wrapper_;
  base::TimeDelta buffer_duration_;  // Length of each recorded buffer.
  AudioInputCallback* callback_;  // Valid during a recording session.
  base::TimeTicks next_read_time_;  // Scheduled time for next read callback.
  snd_pcm_t* device_handle_;  // Handle to the ALSA PCM recording device.
  snd_mixer_t* mixer_handle_; // Handle to the ALSA microphone mixer.
  snd_mixer_elem_t* mixer_element_handle_; // Handle to the capture element.
  scoped_ptr<uint8[]> audio_buffer_;  // Buffer used for reading audio data.
  bool read_callback_behind_schedule_;
  scoped_ptr<AudioBus> audio_bus_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<AlsaPcmInputStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AlsaPcmInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ALSA_ALSA_INPUT_H_
