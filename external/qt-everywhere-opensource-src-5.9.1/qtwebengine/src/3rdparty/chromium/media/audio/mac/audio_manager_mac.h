// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>
#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/mac/audio_device_listener_mac.h"

namespace media {

class AUAudioInputStream;
class AUHALStream;

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerMac : public AudioManagerBase {
 public:
  AudioManagerMac(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
      AudioLogFactory* audio_log_factory);

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  std::string GetDefaultOutputDeviceID() override;

  // Used to track destruction of input and output streams.
  void ReleaseOutputStream(AudioOutputStream* stream) override;
  void ReleaseInputStream(AudioInputStream* stream) override;

  // Called by AUHALStream::Close() before releasing the stream.
  // This method is a special contract between the real stream and the audio
  // manager and it ensures that we only try to increase the IO buffer size
  // for real streams and not for fake or mocked streams.
  void ReleaseOutputStreamUsingRealDevice(AudioOutputStream* stream,
                                          AudioDeviceID device_id);

  static bool GetDeviceChannels(AudioDeviceID device,
                                AudioObjectPropertyScope scope,
                                int* channels);

  static int HardwareSampleRateForDevice(AudioDeviceID device_id);
  static int HardwareSampleRate();

  // OSX has issues with starting streams as the system goes into suspend and
  // immediately after it wakes up from resume.  See http://crbug.com/160920.
  // As a workaround we delay Start() when it occurs after suspend and for a
  // small amount of time after resume.
  //
  // Streams should consult ShouldDeferStreamStart() and if true check the value
  // again after |kStartDelayInSecsForPowerEvents| has elapsed. If false, the
  // stream may be started immediately.
  // TOOD(henrika): track UMA statistics related to defer start to come up with
  // a suitable delay value.
  enum { kStartDelayInSecsForPowerEvents = 5 };
  bool ShouldDeferStreamStart() const;

  // True if the device is on battery power.
  bool IsOnBatteryPower() const;

  // Number of times the device has resumed from power suspension.
  size_t GetNumberOfResumeNotifications() const;

  // True if the device is suspending.
  bool IsSuspending() const;

  // Changes the I/O buffer size for |device_id| if |desired_buffer_size| is
  // lower than the current device buffer size. The buffer size can also be
  // modified under other conditions. See comments in the corresponding cc-file
  // for more details.
  // |size_was_changed| is set to true if the device's buffer size was changed
  // and |io_buffer_frame_size| contains the new buffer size.
  // Returns false if an error occurred.
  bool MaybeChangeBufferSize(AudioDeviceID device_id,
                             AudioUnit audio_unit,
                             AudioUnitElement element,
                             size_t desired_buffer_size,
                             bool* size_was_changed,
                             size_t* io_buffer_frame_size);

  // Number of constructed output and input streams.
  size_t output_streams() const { return output_streams_.size(); }
  size_t low_latency_input_streams() const {
    return low_latency_input_streams_.size();
  }
  size_t basic_input_streams() const { return basic_input_streams_.size(); }

 protected:
  friend class media::AudioManagerDeleter;

  ~AudioManagerMac() override;
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  void InitializeOnAudioThread();

  int ChooseBufferSize(bool is_input, int sample_rate);

  // Notify streams of a device change if the default output device or its
  // sample rate has changed, otherwise does nothing.
  void HandleDeviceChanges();

  // Returns true if any active input stream is using the specified |device_id|.
  bool AudioDeviceIsUsedForInput(AudioDeviceID device_id);

  // This method is called when an output stream has been released and it takes
  // the given |device_id| and scans all active output streams that are
  // using this id. The goal is to find a new (larger) I/O buffer size which
  // can be applied to all active output streams since doing so will save
  // system resources.
  // Note that, it is only called if no input stream is also using the device.
  // Example: two active output streams where #1 wants 1024 as buffer size but
  // is using 256 since stream #2 wants it. Now, if stream #2 is closed down,
  // the native I/O buffer size will be increased to 1024 instead of 256.
  // Returns true if it was possible to increase the I/O buffer size and
  // false otherwise.
  // TODO(henrika): possibly extend the scheme to also take input streams into
  // account.
  bool IncreaseIOBufferSizeIfPossible(AudioDeviceID device_id);

  std::unique_ptr<AudioDeviceListenerMac> output_device_listener_;

  // Track the output sample-rate and the default output device
  // so we can intelligently handle device notifications only when necessary.
  int current_sample_rate_;
  AudioDeviceID current_output_device_;

  // Helper class which monitors power events to determine if output streams
  // should defer Start() calls.  Required to workaround an OSX bug.  See
  // http://crbug.com/160920 for more details.
  class AudioPowerObserver;
  std::unique_ptr<AudioPowerObserver> power_observer_;

  // Tracks all constructed input and output streams.
  // TODO(alokp): We used to track these streams to close before destruction.
  // We no longer close the streams, so we may be able to get rid of these
  // member variables. They are currently used by MaybeChangeBufferSize().
  // Investigate if we can remove these.
  std::list<AudioInputStream*> basic_input_streams_;
  std::list<AUAudioInputStream*> low_latency_input_streams_;
  std::list<AUHALStream*> output_streams_;

  // Maps device IDs and their corresponding actual (I/O) buffer sizes for
  // all output streams using the specific device.
  std::map<AudioDeviceID, size_t> output_io_buffer_size_map_;

  // Set to true in the destructor. Ensures that methods that touches native
  // Core Audio APIs are not executed during shutdown.
  bool in_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
