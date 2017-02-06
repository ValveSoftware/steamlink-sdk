// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service.h"

namespace extensions {

class AudioServiceImpl : public AudioService {
 public:
  AudioServiceImpl() {}
  ~AudioServiceImpl() override {}

  // Called by listeners to this service to add/remove themselves as observers.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Start to query audio device information.
  bool GetInfo(OutputInfo* output_info_out, InputInfo* input_info_out) override;
  void SetActiveDevices(const DeviceIdList& device_list) override;
  bool SetDeviceProperties(const std::string& device_id,
                           bool muted,
                           int volume,
                           int gain) override;
};

void AudioServiceImpl::AddObserver(Observer* observer) {
  // TODO: implement this for platforms other than Chrome OS.
}

void AudioServiceImpl::RemoveObserver(Observer* observer) {
  // TODO: implement this for platforms other than Chrome OS.
}

AudioService* AudioService::CreateInstance() {
  return new AudioServiceImpl;
}

bool AudioServiceImpl::GetInfo(OutputInfo* output_info_out,
                               InputInfo* input_info_out) {
  // TODO: implement this for platforms other than Chrome OS.
  return false;
}

void AudioServiceImpl::SetActiveDevices(const DeviceIdList& device_list) {
}

bool AudioServiceImpl::SetDeviceProperties(const std::string& device_id,
                                           bool muted,
                                           int volume,
                                           int gain) {
  return false;
}

}  // namespace extensions
