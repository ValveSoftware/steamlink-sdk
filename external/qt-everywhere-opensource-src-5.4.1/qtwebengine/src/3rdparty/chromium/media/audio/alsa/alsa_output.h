// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates an output stream based on the ALSA PCM interface.
//
// On device write failure, the stream will move itself to an invalid state.
// No more data will be pulled from the data source, or written to the device.
// All calls to public API functions will either no-op themselves, or return an
// error if possible.  Specifically, If the stream is in an error state, Open()
// will return false, and Start() will call OnError() immediately on the
// provided callback.
//
// If the stream is successfully opened, Close() must be called.  After Close
// has been called, the object should be regarded as deleted and not touched.
//
// AlsaPcmOutputStream is a single threaded class that should only be used from
// the audio thread. When modifying the code in this class, please read the
// threading assumptions at the top of the implementation.

#ifndef MEDIA_AUDIO_ALSA_ALSA_OUTPUT_H_
#define MEDIA_AUDIO_ALSA_ALSA_OUTPUT_H_

#include <alsa/asoundlib.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace base {
class MessageLoop;
}

namespace media {

class AlsaWrapper;
class AudioManagerBase;
class ChannelMixer;
class SeekableBuffer;

class MEDIA_EXPORT AlsaPcmOutputStream : public AudioOutputStream {
 public:
  // String for the generic "default" ALSA device that has the highest
  // compatibility and chance of working.
  static const char kDefaultDevice[];

  // Pass this to the AlsaPcmOutputStream if you want to attempt auto-selection
  // of the audio device.
  static const char kAutoSelectDevice[];

  // Prefix for device names to enable ALSA library resampling.
  static const char kPlugPrefix[];

  // The minimum latency that is accepted by the device.
  static const uint32 kMinLatencyMicros;

  // Create a PCM Output stream for the ALSA device identified by
  // |device_name|.  The AlsaPcmOutputStream uses |wrapper| to communicate with
  // the alsa libraries, allowing for dependency injection during testing.  All
  // requesting of data, and writing to the alsa device will be done on
  // |message_loop|.
  //
  // If unsure of what to use for |device_name|, use |kAutoSelectDevice|.
  AlsaPcmOutputStream(const std::string& device_name,
                      const AudioParameters& params,
                      AlsaWrapper* wrapper,
                      AudioManagerBase* manager);

  virtual ~AlsaPcmOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

 private:
  friend class AlsaPcmOutputStreamTest;
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest,
                           AutoSelectDevice_DeviceSelect);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest,
                           AutoSelectDevice_FallbackDevices);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, AutoSelectDevice_HintFail);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, BufferPacket);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, BufferPacket_Negative);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, BufferPacket_StopStream);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, BufferPacket_Underrun);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, BufferPacket_FullBuffer);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, ConstructedState);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, LatencyFloor);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, OpenClose);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, PcmOpenFailed);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, PcmSetParamsFailed);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, ScheduleNextWrite);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest,
                           ScheduleNextWrite_StopStream);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, StartStop);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, WritePacket_FinishedPacket);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, WritePacket_NormalPacket);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, WritePacket_StopStream);
  FRIEND_TEST_ALL_PREFIXES(AlsaPcmOutputStreamTest, WritePacket_WriteFails);

  // Flags indicating the state of the stream.
  enum InternalState {
    kInError = 0,
    kCreated,
    kIsOpened,
    kIsPlaying,
    kIsStopped,
    kIsClosed
  };
  friend std::ostream& operator<<(std::ostream& os, InternalState);

  // Functions to get another packet from the data source and write it into the
  // ALSA device.
  void BufferPacket(bool* source_exhausted);
  void WritePacket();
  void WriteTask();
  void ScheduleNextWrite(bool source_exhausted);

  // Utility functions for talking with the ALSA API.
  static base::TimeDelta FramesToTimeDelta(int frames, double sample_rate);
  std::string FindDeviceForChannels(uint32 channels);
  snd_pcm_sframes_t GetAvailableFrames();
  snd_pcm_sframes_t GetCurrentDelay();

  // Attempts to find the best matching linux audio device for the given number
  // of channels.  This function will set |device_name_| and |channel_mixer_|.
  snd_pcm_t* AutoSelectDevice(uint32 latency);

  // Functions to safeguard state transitions.  All changes to the object state
  // should go through these functions.
  bool CanTransitionTo(InternalState to);
  InternalState TransitionTo(InternalState to);
  InternalState state();

  // Returns true when we're on the audio thread or if the audio thread's
  // message loop is NULL (which will happen during shutdown).
  bool IsOnAudioThread() const;

  // API for Proxying calls to the AudioSourceCallback provided during
  // Start().
  //
  // TODO(ajwong): This is necessary because the ownership semantics for the
  // |source_callback_| object are incorrect in AudioRenderHost. The callback
  // is passed into the output stream, but ownership is not transfered which
  // requires a synchronization on access of the |source_callback_| to avoid
  // using a deleted callback.
  int RunDataCallback(AudioBus* audio_bus, AudioBuffersState buffers_state);
  void RunErrorCallback(int code);

  // Changes the AudioSourceCallback to proxy calls to.  Pass in NULL to
  // release ownership of the currently registered callback.
  void set_source_callback(AudioSourceCallback* callback);

  // Configuration constants from the constructor.  Referenceable by all threads
  // since they are constants.
  const std::string requested_device_name_;
  const snd_pcm_format_t pcm_format_;
  const uint32 channels_;
  const ChannelLayout channel_layout_;
  const uint32 sample_rate_;
  const uint32 bytes_per_sample_;
  const uint32 bytes_per_frame_;

  // Device configuration data. Populated after OpenTask() completes.
  std::string device_name_;
  uint32 packet_size_;
  base::TimeDelta latency_;
  uint32 bytes_per_output_frame_;
  uint32 alsa_buffer_frames_;

  // Flag indicating the code should stop reading from the data source or
  // writing to the ALSA device.  This is set because the device has entered
  // an unrecoverable error state, or the ClosedTask() has executed.
  bool stop_stream_;

  // Wrapper class to invoke all the ALSA functions.
  AlsaWrapper* wrapper_;

  // Audio manager that created us.  Used to report that we've been closed.
  AudioManagerBase* manager_;

  // Message loop to use for polling. The object is owned by the AudioManager.
  // We hold a reference to the audio thread message loop since
  // AudioManagerBase::ShutDown() can invalidate the message loop pointer
  // before the stream gets deleted.
  base::MessageLoop* message_loop_;

  // Handle to the actual PCM playback device.
  snd_pcm_t* playback_handle_;

  scoped_ptr<media::SeekableBuffer> buffer_;
  uint32 frames_per_packet_;

  InternalState state_;
  float volume_;  // Volume level from 0.0 to 1.0.

  AudioSourceCallback* source_callback_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  // Channel mixer and temporary bus for the final mixed channel data.
  scoped_ptr<ChannelMixer> channel_mixer_;
  scoped_ptr<AudioBus> mixed_audio_bus_;

  // Allows us to run tasks on the AlsaPcmOutputStream instance which are
  // bound by its lifetime.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<AlsaPcmOutputStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AlsaPcmOutputStream);
};

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      AlsaPcmOutputStream::InternalState);

};  // namespace media

#endif  // MEDIA_AUDIO_ALSA_ALSA_OUTPUT_H_
