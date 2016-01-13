// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_

#include <string>

#include "media/audio/audio_manager_base.h"

namespace media {

class AudioDeviceListenerWin;

// Windows implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerWin : public AudioManagerBase {
 public:
  AudioManagerWin(AudioLogFactory* audio_log_factory);

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual base::string16 GetAudioInputDeviceModel() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;
  virtual void GetAudioInputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual void GetAudioOutputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) OVERRIDE;
  virtual std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual std::string GetDefaultOutputDeviceID() OVERRIDE;

 protected:
  virtual ~AudioManagerWin();

  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) OVERRIDE;

 private:
  enum EnumerationType {
    kMMDeviceEnumeration,
    kWaveEnumeration,
  };

  // Allow unit test to modify the utilized enumeration API.
  friend class AudioManagerTest;

  EnumerationType enumeration_type_;
  EnumerationType enumeration_type() { return enumeration_type_; }
  void SetEnumerationType(EnumerationType type) {
    enumeration_type_ = type;
  }

  inline bool core_audio_supported() const {
    return enumeration_type_ == kMMDeviceEnumeration;
  }

  // Returns a PCMWaveInAudioInputStream instance or NULL on failure.
  // This method converts MMDevice-style device ID to WaveIn-style device ID if
  // necessary.
  // (Please see device_enumeration_win.h for more info about the two kinds of
  // device IDs.)
  AudioInputStream* CreatePCMWaveInAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id);

  // Helper methods for performing expensive initialization tasks on the audio
  // thread instead of on the UI thread which AudioManager is constructed on.
  void InitializeOnAudioThread();
  void ShutdownOnAudioThread();

  void GetAudioDeviceNamesImpl(bool input, AudioDeviceNames* device_names);

  // Listen for output device changes.
  scoped_ptr<AudioDeviceListenerWin> output_device_listener_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerWin);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
