// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace media {

class AudioOutputDispatcher;

// AudioManagerBase provides AudioManager functions common for all platforms.
class MEDIA_EXPORT AudioManagerBase : public AudioManager {
 public:
  ~AudioManagerBase() override;

  // AudioManager:
  base::string16 GetAudioInputDeviceModel() override;
  void ShowAudioInputSettings() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params,
      const std::string& device_id) override;

  // Listeners will be notified on the GetTaskRunner() task runner.
  void AddOutputDeviceChangeListener(AudioDeviceListener* listener) override;
  void RemoveOutputDeviceChangeListener(AudioDeviceListener* listener) override;

  AudioParameters GetDefaultOutputStreamParameters() override;
  AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
  std::unique_ptr<AudioLog> CreateAudioLog(
      AudioLogFactory::AudioComponent component) override;

  // AudioManagerBase:

  // Called internally by the audio stream when it has been closed.
  virtual void ReleaseOutputStream(AudioOutputStream* stream);
  virtual void ReleaseInputStream(AudioInputStream* stream);

  // Creates the output stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) = 0;

  // Creates the output stream for the |AUDIO_PCM_LOW_LATENCY| format.
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Creates the input stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Creates the input stream for the |AUDIO_PCM_LOW_LATENCY| format.
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Get number of input or output streams.
  int input_stream_count() const { return num_input_streams_; }
  int output_stream_count() const { return num_output_streams_; }

 protected:
  AudioManagerBase(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
      AudioLogFactory* audio_log_factory);

  // Releases all the audio output dispatchers.
  // All audio streams should be closed before Shutdown() is called.
  // This must be called in the destructor of every AudioManagerBase
  // implementation.
  void Shutdown();

  void SetMaxOutputStreamsAllowed(int max) { max_num_output_streams_ = max; }

  // Called by each platform specific AudioManager to notify output state change
  // listeners that a state change has occurred.  Must be called from the audio
  // thread.
  void NotifyAllOutputDeviceChangeListeners();

  // Returns user buffer size as specified on the command line or 0 if no buffer
  // size has been specified.
  int GetUserBufferSize();

  // Returns the preferred hardware audio output parameters for opening output
  // streams. If the users inject a valid |input_params|, each AudioManager
  // will decide if they should return the values from |input_params| or the
  // default hardware values. If the |input_params| is invalid, it will return
  // the default hardware audio parameters.
  // If |output_device_id| is empty, the implementation must treat that as
  // a request for the default output device.
  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) = 0;

  // Returns the ID of the default audio output device.
  // Implementations that don't yet support this should return an empty string.
  virtual std::string GetDefaultOutputDeviceID();

 private:
  struct DispatcherParams;
  typedef ScopedVector<DispatcherParams> AudioOutputDispatchers;

  class CompareByParams;

  // Max number of open output streams, modified by
  // SetMaxOutputStreamsAllowed().
  int max_num_output_streams_;

  // Max number of open input streams.
  int max_num_input_streams_;

  // Number of currently open output streams.
  int num_output_streams_;

  // Number of currently open input streams.
  int num_input_streams_;

  // Track output state change listeners.
  base::ObserverList<AudioDeviceListener> output_listeners_;

  // Map of cached AudioOutputDispatcher instances.  Must only be touched
  // from the audio thread (no locking).
  AudioOutputDispatchers output_dispatchers_;

  // Proxy for creating AudioLog objects.
  AudioLogFactory* const audio_log_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
