// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_CRAS_INPUT_H_
#define MEDIA_AUDIO_CRAS_CRAS_INPUT_H_

#include <cras_client.h>

#include <string>

#include "base/compiler_specific.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerCras;

// Provides an input stream for audio capture based on CRAS, the ChromeOS Audio
// Server.  This object is not thread safe and all methods should be invoked in
// the thread that created the object.
class CrasInputStream : public AgcAudioStream<AudioInputStream> {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // audio manager who is creating this object.
  CrasInputStream(const AudioParameters& params, AudioManagerCras* manager,
                  const std::string& device_id);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~CrasInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;

 private:
  // Handles requests to get samples from the provided buffer.  This will be
  // called by the audio server when it has samples ready.
  static int SamplesReady(cras_client* client,
                          cras_stream_id_t stream_id,
                          uint8* samples,
                          size_t frames,
                          const timespec* sample_ts,
                          void* arg);

  // Handles notification that there was an error with the playback stream.
  static int StreamError(cras_client* client,
                         cras_stream_id_t stream_id,
                         int err,
                         void* arg);

  // Reads one or more buffers of audio from the device, passes on to the
  // registered callback. Called from SamplesReady().
  void ReadAudio(size_t frames, uint8* buffer, const timespec* sample_ts);

  // Deals with an error that occured in the stream.  Called from StreamError().
  void NotifyStreamError(int err);

  // Convert from dB * 100 to a volume ratio.
  double GetVolumeRatioFromDecibels(double dB) const;

  // Convert from a volume ratio to dB.
  double GetDecibelsFromVolumeRatio(double volume_ratio) const;

  // Non-refcounted pointer back to the audio manager.
  // The AudioManager indirectly holds on to stream objects, so we don't
  // want circular references.  Additionally, stream objects live on the audio
  // thread, which is owned by the audio manager and we don't want to addref
  // the manager from that thread.
  AudioManagerCras* const audio_manager_;

  // Size of frame in bytes.
  uint32 bytes_per_frame_;

  // Callback to pass audio samples too, valid while recording.
  AudioInputCallback* callback_;

  // The client used to communicate with the audio server.
  cras_client* client_;

  // PCM parameters for the stream.
  const AudioParameters params_;

  // True if the stream has been started.
  bool started_;

  // ID of the playing stream.
  cras_stream_id_t stream_id_;

  // Direction of the stream.
  const CRAS_STREAM_DIRECTION stream_direction_;

  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(CrasInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_INPUT_H_
