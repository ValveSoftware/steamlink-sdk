// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of AudioInputStream for Mac OS X using the special AUHAL
// input Audio Unit present in OS 10.4 and later.
// The AUHAL input Audio Unit is for low-latency audio I/O.
//
// Overview of operation:
//
// - An object of AUAudioInputStream is created by the AudioManager
//   factory: audio_man->MakeAudioInputStream().
// - Next some thread will call Open(), at that point the underlying
//   AUHAL output Audio Unit is created and configured.
// - Then some thread will call Start(sink).
//   Then the Audio Unit is started which creates its own thread which
//   periodically will provide the sink with more data as buffers are being
//   produced/recorded.
// - At some point some thread will call Stop(), which we handle by directly
//   stopping the AUHAL output Audio Unit.
// - The same thread that called stop will call Close() where we cleanup
//   and notify the audio manager, which likely will destroy this object.
//
// Implementation notes:
//
// - It is recommended to first acquire the native sample rate of the default
//   input device and then use the same rate when creating this object.
//   Use AUAudioInputStream::HardwareSampleRate() to retrieve the sample rate.
// - Calling Close() also leads to self destruction.
// - The latency consists of two parts:
//   1) Hardware latency, which includes Audio Unit latency, audio device
//      latency;
//   2) The delay between the actual recording instant and the time when the
//      data packet is provided as a callback.
//
#ifndef MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_INPUT_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_INPUT_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/seekable_buffer.h"

namespace media {

class AudioBus;
class AudioManagerMac;
class DataBuffer;

class AUAudioInputStream : public AgcAudioStream<AudioInputStream> {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  AUAudioInputStream(AudioManagerMac* manager,
                     const AudioParameters& input_params,
                     const AudioParameters& output_params,
                     AudioDeviceID audio_device_id);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioInputStream::Close().
  virtual ~AUAudioInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;

  // Returns the current hardware sample rate for the default input device.
  MEDIA_EXPORT static int HardwareSampleRate();

  bool started() const { return started_; }
  AudioUnit audio_unit() { return audio_unit_; }
  AudioBufferList* audio_buffer_list() { return &audio_buffer_list_; }

 private:
  // AudioOutputUnit callback.
  static OSStatus InputProc(void* user_data,
                            AudioUnitRenderActionFlags* flags,
                            const AudioTimeStamp* time_stamp,
                            UInt32 bus_number,
                            UInt32 number_of_frames,
                            AudioBufferList* io_data);

  // Pushes recorded data to consumer of the input audio stream.
  OSStatus Provide(UInt32 number_of_frames, AudioBufferList* io_data,
                   const AudioTimeStamp* time_stamp);

  // Gets the fixed capture hardware latency and store it during initialization.
  // Returns 0 if not available.
  double GetHardwareLatency();

  // Gets the current capture delay value.
  double GetCaptureLatency(const AudioTimeStamp* input_time_stamp);

  // Gets the number of channels for a stream of audio data.
  int GetNumberOfChannelsFromStream();

  // Issues the OnError() callback to the |sink_|.
  void HandleError(OSStatus err);

  // Helper function to check if the volume control is avialable on specific
  // channel.
  bool IsVolumeSettableOnChannel(int channel);

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* manager_;

  // Contains the desired number of audio frames in each callback.
  size_t number_of_frames_;

  // Pointer to the object that will receive the recorded audio samples.
  AudioInputCallback* sink_;

  // Structure that holds the desired output format of the stream.
  // Note that, this format can differ from the device(=input) format.
  AudioStreamBasicDescription format_;

  // The special Audio Unit called AUHAL, which allows us to pass audio data
  // directly from a microphone, through the HAL, and to our application.
  // The AUHAL also enables selection of non default devices.
  AudioUnit audio_unit_;

  // The UID refers to the current input audio device.
  AudioDeviceID input_device_id_;

  // Provides a mechanism for encapsulating one or more buffers of audio data.
  AudioBufferList audio_buffer_list_;

  // Temporary storage for recorded data. The InputProc() renders into this
  // array as soon as a frame of the desired buffer size has been recorded.
  scoped_ptr<uint8[]> audio_data_buffer_;

  // True after successfull Start(), false after successful Stop().
  bool started_;

  // Fixed capture hardware latency in frames.
  double hardware_latency_frames_;

  // Delay due to the FIFO in bytes.
  int fifo_delay_bytes_;

  // The number of channels in each frame of audio data, which is used
  // when querying the volume of each channel.
  int number_of_channels_in_frame_;

  // Accumulates recorded data packets until the requested size has been stored.
  scoped_ptr<media::SeekableBuffer> fifo_;

   // Intermediate storage of data from the FIFO before sending it to the
   // client using the OnData() callback.
  scoped_refptr<media::DataBuffer> data_;

  // The client requests that the recorded data shall be delivered using
  // OnData() callbacks where each callback contains this amount of bytes.
  int requested_size_bytes_;

  // Used to defer Start() to workaround http://crbug.com/160920.
  base::CancelableClosure deferred_start_cb_;

  // Extra audio bus used for storage of deinterleaved data for the OnData
  // callback.
  scoped_ptr<media::AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(AUAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_INPUT_MAC_H_
