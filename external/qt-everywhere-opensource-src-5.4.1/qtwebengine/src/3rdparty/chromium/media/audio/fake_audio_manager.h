// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_FAKE_AUDIO_MANAGER_H_

#include <string>
#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"

namespace media {

class MEDIA_EXPORT FakeAudioManager : public AudioManagerBase {
 public:
  FakeAudioManager(AudioLogFactory* audio_log_factory);

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(const AudioParameters& params,
                                                  const std::string& device_id)
      OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) OVERRIDE;

 protected:
  virtual ~FakeAudioManager();

  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioManager);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_MANAGER_H_
