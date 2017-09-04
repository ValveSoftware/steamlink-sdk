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

#include <map>
#include <memory>
#include <vector>

#include "base/atomicops.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;
class AudioManagerMac;
class DataBuffer;

class MEDIA_EXPORT AUAudioInputStream
    : public AgcAudioStream<AudioInputStream> {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  AUAudioInputStream(AudioManagerMac* manager,
                     const AudioParameters& input_params,
                     AudioDeviceID audio_device_id,
                     const AudioManager::LogCallback& log_callback);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioInputStream::Close().
  ~AUAudioInputStream() override;

  // Implementation of AudioInputStream.
  bool Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;

  // Returns the current hardware sample rate for the default input device.
  static int HardwareSampleRate();

  // Returns true if the audio unit is active/running.
  // The result is based on the kAudioOutputUnitProperty_IsRunning property
  // which exists for output units.
  bool IsRunning();

  AudioDeviceID device_id() const { return input_device_id_; }
  size_t requested_buffer_size() const { return number_of_frames_; }

 private:
  static const AudioObjectPropertyAddress kDeviceChangePropertyAddress;

  // Callback functions called on a real-time priority I/O thread from the audio
  // unit. These methods are called when recorded audio is available.
  static OSStatus DataIsAvailable(void* context,
                                  AudioUnitRenderActionFlags* flags,
                                  const AudioTimeStamp* time_stamp,
                                  UInt32 bus_number,
                                  UInt32 number_of_frames,
                                  AudioBufferList* io_data);
  OSStatus OnDataIsAvailable(AudioUnitRenderActionFlags* flags,
                             const AudioTimeStamp* time_stamp,
                             UInt32 bus_number,
                             UInt32 number_of_frames);

  // Pushes recorded data to consumer of the input audio stream.
  OSStatus Provide(UInt32 number_of_frames, AudioBufferList* io_data,
                   const AudioTimeStamp* time_stamp);

  // Callback functions called on different system threads from the Core Audio
  // framework. These methods are called when device properties are changed.
  static OSStatus OnDevicePropertyChanged(
      AudioObjectID object_id,
      UInt32 num_addresses,
      const AudioObjectPropertyAddress addresses[],
      void* context);
  OSStatus DevicePropertyChanged(AudioObjectID object_id,
                                 UInt32 num_addresses,
                                 const AudioObjectPropertyAddress addresses[]);

  // Updates the |device_property_changes_map_| on the main browser thread,
  // (CrBrowserMain) which is the same thread as this instance is created on.
  void DevicePropertyChangedOnMainThread(const std::vector<UInt32>& properties);

  // Updates |last_callback_time_| on the main browser thread.
  void UpdateDataCallbackTimeOnMainThread(base::TimeTicks now_tick);

  // Registers OnDevicePropertyChanged() to receive notifications when device
  // properties changes.
  void RegisterDeviceChangeListener();
  // Stop listening for changes in device properties.
  void DeRegisterDeviceChangeListener();

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

  // Helper methods to set and get atomic |input_callback_is_active_|.
  void SetInputCallbackIsActive(bool active);
  bool GetInputCallbackIsActive();

  // Checks if a stream was started successfully and the audio unit also starts
  // to call InputProc() as it should. This method is called once when a timer
  // expires 5 seconds after calling Start().
  void CheckInputStartupSuccess();

  // Checks (periodically) if a stream is alive by comparing the current time
  // with the last timestamp stored in a data callback. Calls RestartAudio()
  // when a restart is required.
  void CheckIfInputStreamIsAlive();

  // Uninitializes the audio unit if needed.
  void CloseAudioUnit();

  // Called by CheckIfInputStreamIsAlive() on the main thread when an audio
  // restarts is required. Restarts the existing audio stream reusing the
  // current audio parameters.
  void RestartAudio();

  // Adds extra UMA stats when it has been detected that startup failed.
  void AddHistogramsForFailedStartup();

  // Scans the map of all available property changes (notification types) and
  // filters out some that make sense to add to UMA stats.
  void AddDevicePropertyChangesToUMA(bool startup_failed);

  // Updates capture timestamp, current lost frames, and total lost frames and
  // glitches.
  void UpdateCaptureTimestamp(const AudioTimeStamp* timestamp);

  // Called from the dtor and when the stream is reset.
  void ReportAndResetStats();

  // Verifies that Open(), Start(), Stop() and Close() are all called on the
  // creating thread which is the main browser thread (CrBrowserMain) on Mac.
  base::ThreadChecker thread_checker_;

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* const manager_;

  // Contains the desired number of audio frames in each callback.
  const size_t number_of_frames_;

  // Stores the number of frames that we actually get callbacks for.
  // This may be different from what we ask for, so we use this for stats in
  // order to understand how often this happens and what are the typical values.
  size_t number_of_frames_provided_;

  // The actual I/O buffer size for the input device connected to the active
  // AUHAL audio unit.
  size_t io_buffer_frame_size_;

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
  const AudioDeviceID input_device_id_;

  // Provides a mechanism for encapsulating one or more buffers of audio data.
  AudioBufferList audio_buffer_list_;

  // Temporary storage for recorded data. The InputProc() renders into this
  // array as soon as a frame of the desired buffer size has been recorded.
  std::unique_ptr<uint8_t[]> audio_data_buffer_;

  // Fixed capture hardware latency in frames.
  double hardware_latency_frames_;

  // The number of channels in each frame of audio data, which is used
  // when querying the volume of each channel.
  int number_of_channels_in_frame_;

  // FIFO used to accumulates recorded data.
  media::AudioBlockFifo fifo_;

  // Used to defer Start() to workaround http://crbug.com/160920.
  base::CancelableClosure deferred_start_cb_;

  // Contains time of last successful call to AudioUnitRender().
  // Initialized first time in Start() and then updated for each valid
  // audio buffer. Used to detect long error sequences and to take actions
  // if length of error sequence is above a certain limit.
  base::TimeTicks last_success_time_;

  // Is set to true on the internal AUHAL IO thread in the first input callback
  // after Start() has bee called.
  base::subtle::Atomic32 input_callback_is_active_;

  // Timer which triggers CheckInputStartupSuccess() to verify that input
  // callbacks have started as intended after a successful call to Start().
  // This timer lives on the main browser thread.
  std::unique_ptr<base::OneShotTimer> input_callback_timer_;

  // Set to true if the Start() call was delayed.
  // See AudioManagerMac::ShouldDeferStreamStart() for details.
  bool start_was_deferred_;

  // Set to true if the audio unit's IO buffer was changed when Open() was
  // called.
  bool buffer_size_was_changed_;

  // Set to true once when AudioUnitRender() succeeds for the first time.
  bool audio_unit_render_has_worked_;

  // Maps unique representations of device property notification types and
  // number of times we have been notified about a change in such a type.
  // While the notifier is active, this member is modified by several different
  // internal thread. My guess is that a serial dispatch queue is used under
  // the hood and it executes one task at a time in the order in which they are
  // added to the queue. The currently executing task runs on a distinct thread
  // (which can vary from task to task) that is managed by the dispatch queue.
  // The map is always read on the creating thread but only while the notifier
  // is disabled, hence no lock is required.
  std::map<UInt32, int> device_property_changes_map_;

  // Set to true when we are listening for changes in device properties.
  // Only touched on the creating thread.
  bool device_listener_is_active_;

  // Stores the timestamp of the previous audio buffer provided by the OS.
  // We use this in combination with |last_number_of_frames_| to detect when
  // the OS has decided to skip providing frames (i.e. a glitch).
  // This can happen in case of high CPU load or excessive blocking on the
  // callback audio thread.
  // These variables are only touched on the callback thread and then read
  // in the dtor (when no longer receiving callbacks).
  // NOTE: Float64 and UInt32 types are used for native API compatibility.
  Float64 last_sample_time_;
  UInt32 last_number_of_frames_;
  UInt32 total_lost_frames_;
  UInt32 largest_glitch_frames_;
  int glitches_detected_;

  // Timer that provides periodic callbacks used to monitor if the input stream
  // is alive or not.
  std::unique_ptr<base::RepeatingTimer> check_alive_timer_;

  // Time tick set once in each input data callback. The time is measured on
  // the real-time priority I/O thread but this member is modified and read
  // on the main thread only.
  base::TimeTicks last_callback_time_;

  // Counts number of times we get a signal of that a restart seems required.
  // If it is above a threshold (kNumberOfIndicationsToTriggerRestart), the
  // current audio stream is closed and a new (using same audio parameters) is
  // started.
  size_t number_of_restart_indications_;

  // Counts number of times RestartAudio() has been called. The max number of
  // attempts is restricted by |kMaxNumberOfRestartAttempts|.
  // Note that this counter is reset to zero after each successful restart.
  size_t number_of_restart_attempts_;

  // Counts the total number of times RestartAudio() has been called. It is
  // set to zero once in the constructor and then never reset again.
  size_t total_number_of_restart_attempts_;

  // Callback to send statistics info.
  AudioManager::LogCallback log_callback_;

  // Used to ensure DevicePropertyChangedOnMainThread() is not called when
  // this object is destroyed.
  // Note that, all member variables should appear before the WeakPtrFactory.
  base::WeakPtrFactory<AUAudioInputStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AUAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_INPUT_MAC_H_
