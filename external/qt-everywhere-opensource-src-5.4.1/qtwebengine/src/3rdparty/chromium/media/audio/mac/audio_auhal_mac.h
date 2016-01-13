// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes:
//
// - It is recommended to first acquire the native sample rate of the default
//   output device and then use the same rate when creating this object.
//   Use AudioManagerMac::HardwareSampleRate() to retrieve the sample rate.
// - Calling Close() also leads to self destruction.
// - The latency consists of two parts:
//   1) Hardware latency, which includes Audio Unit latency, audio device
//      latency;
//   2) The delay between the moment getting the callback and the scheduled time
//      stamp that tells when the data is going to be played out.
//
#ifndef MEDIA_AUDIO_MAC_AUDIO_AUHAL_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_AUHAL_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerMac;
class AudioPullFifo;

// Implementation of AudioOuputStream for Mac OS X using the
// AUHAL Audio Unit present in OS 10.4 and later.
// It is useful for low-latency output.
//
// Overview of operation:
// 1) An object of AUHALStream is created by the AudioManager
// factory: audio_man->MakeAudioStream().
// 2) Next some thread will call Open(), at that point the underlying
// AUHAL Audio Unit is created and configured to use the |device|.
// 3) Then some thread will call Start(source).
// Then the AUHAL is started which creates its own thread which
// periodically will call the source for more data as buffers are being
// consumed.
// 4) At some point some thread will call Stop(), which we handle by directly
// stopping the default output Audio Unit.
// 6) The same thread that called stop will call Close() where we cleanup
// and notify the audio manager, which likely will destroy this object.

class AUHALStream : public AudioOutputStream {
 public:
  // |manager| creates this object.
  // |device| is the CoreAudio device to use for the stream.
  // It will often be the default output device.
  AUHALStream(AudioManagerMac* manager,
              const AudioParameters& params,
              AudioDeviceID device);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~AUHALStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

 private:
  // AUHAL callback.
  static OSStatus InputProc(void* user_data,
                            AudioUnitRenderActionFlags* flags,
                            const AudioTimeStamp* time_stamp,
                            UInt32 bus_number,
                            UInt32 number_of_frames,
                            AudioBufferList* io_data);

  OSStatus Render(AudioUnitRenderActionFlags* flags,
                  const AudioTimeStamp* output_time_stamp,
                  UInt32 bus_number,
                  UInt32 number_of_frames,
                  AudioBufferList* io_data);

  // Called by either |audio_fifo_| or Render() to provide audio data.
  void ProvideInput(int frame_delay, AudioBus* dest);

  // Sets the stream format on the AUHAL to PCM Float32 non-interleaved
  // for the given number of channels on the given scope and element.
  // The created stream description will be stored in |desc|.
  bool SetStreamFormat(AudioStreamBasicDescription* desc,
                       int channels,
                       UInt32 scope,
                       UInt32 element);

  // Creates the AUHAL, sets its stream format, buffer-size, etc.
  bool ConfigureAUHAL();

  // Creates the input and output busses.
  void CreateIOBusses();

  // Gets the fixed playout device hardware latency and stores it. Returns 0
  // if not available.
  double GetHardwareLatency();

  // Gets the current playout latency value.
  double GetPlayoutLatency(const AudioTimeStamp* output_time_stamp);

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* const manager_;

  const AudioParameters params_;
  // For convenience - same as in params_.
  const int output_channels_;

  // Buffer-size.
  const size_t number_of_frames_;

  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;

  // Protects |source_|.  Necessary since Render() calls seem to be in flight
  // when |audio_unit_| is supposedly stopped.  See http://crbug.com/178765.
  base::Lock source_lock_;

  // Holds the stream format details such as bitrate.
  AudioStreamBasicDescription output_format_;

  // The audio device to use with the AUHAL.
  // We can potentially handle both input and output with this device.
  const AudioDeviceID device_;

  // The AUHAL Audio Unit which talks to |device_|.
  AudioUnit audio_unit_;

  // Volume level from 0 to 1.
  float volume_;

  // Fixed playout hardware latency in frames.
  double hardware_latency_frames_;

  // The flag used to stop the streaming.
  bool stopped_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> output_bus_;

  // Dynamically allocated FIFO used when CoreAudio asks for unexpected frame
  // sizes.
  scoped_ptr<AudioPullFifo> audio_fifo_;

  // Current buffer delay.  Set by Render().
  uint32 current_hardware_pending_bytes_;

  // Used to defer Start() to workaround http://crbug.com/160920.
  base::CancelableClosure deferred_start_cb_;

  DISALLOW_COPY_AND_ASSIGN(AUHALStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_AUHAL_MAC_H_
