// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mock_audio_manager.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_parameters.h"

namespace media {

MockAudioManager::MockAudioManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : AudioManager(task_runner, task_runner) {}

MockAudioManager::~MockAudioManager() {
}

bool MockAudioManager::HasAudioOutputDevices() {
  return true;
}

bool MockAudioManager::HasAudioInputDevices() {
  return true;
}

base::string16 MockAudioManager::GetAudioInputDeviceModel() {
  return base::string16();
}

void MockAudioManager::ShowAudioInputSettings() {
}

void MockAudioManager::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  // Do not inject fake devices here, use
  // AudioInputDeviceManager::GetFakeDeviceNames() instead.
}

void MockAudioManager::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStream(
    const media::AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return NULL;
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStreamProxy(
    const media::AudioParameters& params,
    const std::string& device_id) {
  NOTREACHED();
  return NULL;
}

media::AudioInputStream* MockAudioManager::MakeAudioInputStream(
    const media::AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return NULL;
}

void MockAudioManager::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

void MockAudioManager::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

AudioParameters MockAudioManager::GetDefaultOutputStreamParameters() {
  return AudioParameters();
}

AudioParameters MockAudioManager::GetOutputStreamParameters(
      const std::string& device_id) {
  return AudioParameters();
}

AudioParameters MockAudioManager::GetInputStreamParameters(
    const std::string& device_id) {
  return AudioParameters();
}

std::string MockAudioManager::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  return std::string();
}

std::unique_ptr<AudioLog> MockAudioManager::CreateAudioLog(
    AudioLogFactory::AudioComponent component) {
  return nullptr;
}

}  // namespace media.
