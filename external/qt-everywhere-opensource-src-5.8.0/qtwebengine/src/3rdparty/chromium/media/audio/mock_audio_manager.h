// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_

#include "base/macros.h"
#include "media/audio/audio_manager.h"

namespace media {

// This class is a simple mock around AudioManager, used exclusively for tests,
// which avoids to use the actual (system and platform dependent) AudioManager.
// Some bots does not have input devices, thus using the actual AudioManager
// would causing failures on classes which expect that.
class MockAudioManager : public media::AudioManager {
 public:
  explicit MockAudioManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  bool HasAudioOutputDevices() override;

  bool HasAudioInputDevices() override;

  base::string16 GetAudioInputDeviceModel() override;

  void ShowAudioInputSettings() override;

  void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names) override;

  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override;

  media::AudioOutputStream* MakeAudioOutputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  media::AudioOutputStream* MakeAudioOutputStreamProxy(
      const media::AudioParameters& params,
      const std::string& device_id) override;

  media::AudioInputStream* MakeAudioInputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

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

 protected:
  ~MockAudioManager() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

}  // namespace media.

#endif  // MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
