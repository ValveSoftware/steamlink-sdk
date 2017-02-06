// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_logging.h"
#include "media/base/audio_parameters.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioInputStream;
class AudioManager;
class AudioOutputStream;

class MEDIA_EXPORT AudioManagerDeleter {
 public:
  void operator()(const AudioManager* instance) const;
};
using ScopedAudioManagerPtr =
    std::unique_ptr<AudioManager, AudioManagerDeleter>;

// Manages all audio resources.  Provides some convenience functions that avoid
// the need to provide iterators over the existing streams.
//
// Except on OSX, a hang monitor for the audio thread is always created. When a
// thread hang is detected, it is reported to UMA.  Optionally, if called prior,
// EnableCrashKeyLoggingForAudioThreadHangs() will cause a non-crash dump to be
// logged on Windows (this allows us to report driver hangs to Microsoft).
class MEDIA_EXPORT AudioManager {
 public:
  // Construct the audio manager; only one instance is allowed.
  // The returned instance must be deleted on AudioManager::GetTaskRunnner().
  //
  // The manager will forward CreateAudioLog() calls to the provided
  // AudioLogFactory; as such |audio_log_factory| must outlive the AudioManager.
  //
  // The manager will use |task_runner| for audio IO. This same task runner
  // is returned by GetTaskRunner().
  // On OS_MACOSX, CoreAudio requires that |task_runner| must belong to the
  // main thread of the process, which in our case is sadly the browser UI
  // thread. Failure to execute calls on the right thread leads to crashes and
  // odd behavior. See http://crbug.com/158170.
  //
  // The manager will use |worker_task_runner| for heavyweight tasks.
  // The |worker_task_runner| may be the same as |task_runner|. This same
  // task runner is returned by GetWorkerTaskRunner.
  static ScopedAudioManagerPtr Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
      AudioLogFactory* audio_log_factory);

  // A convenience wrapper of AudioManager::Create for testing.
  // The given |task_runner| is shared for both audio io and heavyweight tasks.
  static ScopedAudioManagerPtr CreateForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Starts monitoring AudioManager task runner for hangs.
  // Runs the monitor on the given |task_runner|, which must be different from
  // AudioManager::GetTaskRunner to be meaningful.
  // This must be called only after an AudioManager instance is created.
  static void StartHangMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Enables non-crash dumps when audio thread hangs are detected.
  // TODO(dalecurtis): There are no callers to this function at present. A list
  // of bad drivers has been given to Microsoft. This should be re-enabled in
  // the future if Microsoft is able to triage third party drivers.
  // See http://crbug.com/422522
  static void EnableCrashKeyLoggingForAudioThreadHangs();

#if defined(OS_LINUX)
  // Sets the name of the audio source as seen by external apps. Only actually
  // used with PulseAudio as of this writing.
  static void SetGlobalAppName(const std::string& app_name);

  // Returns the app name or an empty string if it is not set.
  static const std::string& GetGlobalAppName();
#endif

  // Returns the pointer to the last created instance, or NULL if not yet
  // created. This is a utility method for the code outside of media directory,
  // like src/chrome.
  static AudioManager* Get();

  // Returns true if the OS reports existence of audio devices. This does not
  // guarantee that the existing devices support all formats and sample rates.
  virtual bool HasAudioOutputDevices() = 0;

  // Returns true if the OS reports existence of audio recording devices. This
  // does not guarantee that the existing devices support all formats and
  // sample rates.
  virtual bool HasAudioInputDevices() = 0;

  // Returns a human readable string for the model/make of the active audio
  // input device for this computer.
  virtual base::string16 GetAudioInputDeviceModel() = 0;

  // Opens the platform default audio input settings UI.
  // Note: This could invoke an external application/preferences pane, so
  // ideally must not be called from the UI thread or other time sensitive
  // threads to avoid blocking the rest of the application.
  virtual void ShowAudioInputSettings() = 0;

  // Appends a list of available input devices to |device_names|,
  // which must initially be empty. It is not guaranteed that all the
  // devices in the list support all formats and sample rates for
  // recording.
  //
  // Not threadsafe; in production this should only be called from the
  // Audio worker thread (see GetWorkerTaskRunner()).
  virtual void GetAudioInputDeviceNames(AudioDeviceNames* device_names) = 0;

  // Appends a list of available output devices to |device_names|,
  // which must initially be empty.
  //
  // Not threadsafe; in production this should only be called from the
  // Audio worker thread (see GetWorkerTaskRunner()).
  virtual void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) = 0;

  // Log callback used for sending log messages from a stream to the object
  // that manages the stream.
  using LogCallback = base::Callback<void(const std::string&)>;

  // Factory for all the supported stream formats. |params| defines parameters
  // of the audio stream to be created.
  //
  // |params.sample_per_packet| is the requested buffer allocation which the
  // audio source thinks it can usually fill without blocking. Internally two
  // or three buffers are created, one will be locked for playback and one will
  // be ready to be filled in the call to AudioSourceCallback::OnMoreData().
  //
  // To create a stream for the default output device, pass an empty string
  // for |device_id|, otherwise the specified audio device will be opened.
  //
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // |params.format| can be set to AUDIO_PCM_LOW_LATENCY and that has two
  // effects:
  // 1- Instead of triple buffered the audio will be double buffered.
  // 2- A low latency driver or alternative audio subsystem will be used when
  //    available.
  //
  // Do not free the returned AudioOutputStream. It is owned by AudioManager.
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Creates new audio output proxy. A proxy implements
  // AudioOutputStream interface, but unlike regular output stream
  // created with MakeAudioOutputStream() it opens device only when a
  // sound is actually playing.
  virtual AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params,
      const std::string& device_id) = 0;

  // Factory to create audio recording streams.
  // |channels| can be 1 or 2.
  // |sample_rate| is in hertz and can be any value supported by the platform.
  // |bits_per_sample| can be any value supported by the platform.
  // |samples_per_packet| is in hertz as well and can be 0 to |sample_rate|,
  // with 0 suggesting that the implementation use a default value for that
  // platform.
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // Do not free the returned AudioInputStream. It is owned by AudioManager.
  // When you are done with it, call |Stop()| and |Close()| to release it.
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Returns the task runner used for audio IO.
  // TODO(alokp): Rename to task_runner().
  base::SingleThreadTaskRunner* GetTaskRunner() const {
    return task_runner_.get();
  }

  // Heavyweight tasks should use GetWorkerTaskRunner() instead of
  // GetTaskRunner(). On most platforms they are the same, but some share the
  // UI loop with the audio IO loop.
  // TODO(alokp): Rename to worker_task_runner().
  base::SingleThreadTaskRunner* GetWorkerTaskRunner() const {
    return worker_task_runner_.get();
  }

  // Allows clients to listen for device state changes; e.g. preferred sample
  // rate or channel layout changes.  The typical response to receiving this
  // callback is to recreate the stream.
  class AudioDeviceListener {
   public:
    virtual void OnDeviceChange() = 0;
  };

  virtual void AddOutputDeviceChangeListener(AudioDeviceListener* listener) = 0;
  virtual void RemoveOutputDeviceChangeListener(
      AudioDeviceListener* listener) = 0;

  // Returns the default output hardware audio parameters for opening output
  // streams. It is a convenience interface to
  // AudioManagerBase::GetPreferredOutputStreamParameters and each AudioManager
  // does not need their own implementation to this interface.
  // TODO(tommi): Remove this method and use GetOutputStreamParameteres instead.
  virtual AudioParameters GetDefaultOutputStreamParameters() = 0;

  // Returns the output hardware audio parameters for a specific output device.
  virtual AudioParameters GetOutputStreamParameters(
      const std::string& device_id) = 0;

  // Returns the input hardware audio parameters of the specific device
  // for opening input streams. Each AudioManager needs to implement their own
  // version of this interface.
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) = 0;

  // Returns the device id of an output device that belongs to the same hardware
  // as the specified input device.
  // If the hardware has only an input device (e.g. a webcam), the return value
  // will be empty (which the caller can then interpret to be the default output
  // device).  Implementations that don't yet support this feature, must return
  // an empty string. Must be called on the audio worker thread (see
  // GetWorkerTaskRunner()).
  virtual std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) = 0;

  // Create a new AudioLog object for tracking the behavior for one or more
  // instances of the given component.  See AudioLogFactory for more details.
  virtual std::unique_ptr<AudioLog> CreateAudioLog(
      AudioLogFactory::AudioComponent component) = 0;

 protected:
  AudioManager(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner);
  virtual ~AudioManager();

 private:
  friend class base::DeleteHelper<AudioManager>;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;
  DISALLOW_COPY_AND_ASSIGN(AudioManager);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_H_
