// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates a unified stream based on the cras (ChromeOS audio server) interface.
//
// CrasUnifiedStream object is *not* thread-safe and should only be used
// from the audio thread.

#ifndef MEDIA_AUDIO_LINUX_CRAS_UNIFIED_H_
#define MEDIA_AUDIO_LINUX_CRAS_UNIFIED_H_

#include <cras_client.h>

#include "base/compiler_specific.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerCras;

// Implementation of AudioOuputStream for Chrome OS using the Chrome OS audio
// server.
// TODO(dgreid): This class is used for only output, either remove all the
// relevant input code and change the class to CrasOutputStream or merge
// cras_input.cc into this unified implementation.
class MEDIA_EXPORT CrasUnifiedStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // audio manager who is creating this object.
  CrasUnifiedStream(const AudioParameters& params, AudioManagerCras* manager);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioUnifiedStream::Close().
  virtual ~CrasUnifiedStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

 private:
  // Convert Latency in time to bytes.
  uint32 GetBytesLatency(const struct timespec& latency);

  // Handles captured audio and fills the ouput with audio to be played.
  static int UnifiedCallback(cras_client* client,
                             cras_stream_id_t stream_id,
                             uint8* input_samples,
                             uint8* output_samples,
                             unsigned int frames,
                             const timespec* input_ts,
                             const timespec* output_ts,
                             void* arg);

  // Handles notification that there was an error with the playback stream.
  static int StreamError(cras_client* client,
                         cras_stream_id_t stream_id,
                         int err,
                         void* arg);

  // Chooses the correct audio callback based on stream direction.
  uint32 DispatchCallback(size_t frames,
                          uint8* input_samples,
                          uint8* output_samples,
                          const timespec* input_ts,
                          const timespec* output_ts);

  // Receives input samples and write output samples for a unified I/O stream.
  uint32 ReadWriteAudio(size_t frames,
                        uint8* input_samples,
                        uint8* output_samples,
                        const timespec* input_ts,
                        const timespec* output_ts);

  // Writes audio for a playback stream.
  uint32 WriteAudio(size_t frames, uint8* buffer, const timespec* sample_ts);

  // Deals with an error that occured in the stream.  Called from StreamError().
  void NotifyStreamError(int err);

  // The client used to communicate with the audio server.
  cras_client* client_;

  // ID of the playing stream.
  cras_stream_id_t stream_id_;

  // PCM parameters for the stream.
  AudioParameters params_;

  // Size of frame in bytes.
  uint32 bytes_per_frame_;

  // True if stream is playing.
  bool is_playing_;

  // Volume level from 0.0 to 1.0.
  float volume_;

  // Audio manager that created us.  Used to report that we've been closed.
  AudioManagerCras* manager_;

  // Callback to get audio samples.
  AudioSourceCallback* source_callback_;

  // Container for exchanging data with AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> input_bus_;
  scoped_ptr<AudioBus> output_bus_;

  // Direciton of the stream.
  CRAS_STREAM_DIRECTION stream_direction_;

  DISALLOW_COPY_AND_ASSIGN(CrasUnifiedStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_LINUX_CRAS_UNIFIED_H_
