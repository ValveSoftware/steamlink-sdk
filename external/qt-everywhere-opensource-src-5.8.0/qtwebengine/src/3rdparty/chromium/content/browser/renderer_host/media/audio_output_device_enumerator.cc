// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_device_enumerator.h"

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"

namespace content {

namespace {

AudioOutputDeviceEnumeration EnumerateDevicesOnDeviceThread(
    media::AudioManager* audio_manager) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());

  AudioOutputDeviceEnumeration snapshot;
  media::AudioDeviceNames device_names;
  audio_manager->GetAudioOutputDeviceNames(&device_names);

  snapshot.has_actual_devices = !device_names.empty();

  // If no devices in enumeration, return a list with a default device
  if (!snapshot.has_actual_devices) {
    snapshot.devices.push_back(
        {media::AudioDeviceDescription::kDefaultDeviceId,
         media::AudioDeviceDescription::GetDefaultDeviceName(),
         audio_manager->GetDefaultOutputStreamParameters()});
    return snapshot;
  }

  for (const media::AudioDeviceName& name : device_names) {
    snapshot.devices.push_back(
        {name.unique_id, name.device_name,
         name.unique_id == media::AudioDeviceDescription::kDefaultDeviceId
             ? audio_manager->GetDefaultOutputStreamParameters()
             : audio_manager->GetOutputStreamParameters(name.unique_id)});
  }
  return snapshot;
}

}  // namespace

AudioOutputDeviceEnumeration::AudioOutputDeviceEnumeration(
    const std::vector<AudioOutputDeviceInfo>& devices,
    bool has_actual_devices)
    : devices(devices), has_actual_devices(has_actual_devices) {}

AudioOutputDeviceEnumeration::AudioOutputDeviceEnumeration()
    : has_actual_devices(false) {}

AudioOutputDeviceEnumeration::AudioOutputDeviceEnumeration(
    const AudioOutputDeviceEnumeration& other) = default;

AudioOutputDeviceEnumeration::~AudioOutputDeviceEnumeration() {}

AudioOutputDeviceEnumerator::AudioOutputDeviceEnumerator(
    media::AudioManager* audio_manager,
    CachePolicy cache_policy)
    : audio_manager_(audio_manager),
      cache_policy_(cache_policy),
      current_event_sequence_(0),
      seq_last_enumeration_(0),
      seq_last_invalidation_(0),
      is_enumeration_ongoing_(false),
      weak_factory_(this) {}

AudioOutputDeviceEnumerator::~AudioOutputDeviceEnumerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioOutputDeviceEnumerator::Enumerate(
    const AudioOutputDeviceEnumerationCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If caching is disabled, force a cache invalidation
  if (cache_policy_ == CACHE_POLICY_NO_CACHING) {
    InvalidateCache();
  }

  if (IsLastEnumerationValid()) {
    DCHECK(pending_callbacks_.empty());
    callback.Run(cache_);
  } else {
    pending_callbacks_.push_back(callback);
    if (!is_enumeration_ongoing_) {
      DoEnumerateDevices();
    }
  }
}

void AudioOutputDeviceEnumerator::InvalidateCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  seq_last_invalidation_ = NewEventSequence();
}

void AudioOutputDeviceEnumerator::SetCachePolicy(CachePolicy policy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (policy == CACHE_POLICY_NO_CACHING)
    InvalidateCache();

  cache_policy_ = policy;
}

bool AudioOutputDeviceEnumerator::IsCacheEnabled() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return cache_policy_ != CACHE_POLICY_NO_CACHING;
}

void AudioOutputDeviceEnumerator::DoEnumerateDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_enumeration_ongoing_ = true;
  seq_last_enumeration_ = NewEventSequence();
  base::PostTaskAndReplyWithResult(
      audio_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&EnumerateDevicesOnDeviceThread, audio_manager_),
      base::Bind(&AudioOutputDeviceEnumerator::DevicesEnumerated,
                 weak_factory_.GetWeakPtr()));
}

void AudioOutputDeviceEnumerator::DevicesEnumerated(
    const AudioOutputDeviceEnumeration& snapshot) {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_enumeration_ongoing_ = false;
  if (IsLastEnumerationValid()) {
    cache_ = snapshot;
    while (!pending_callbacks_.empty()) {
      AudioOutputDeviceEnumerationCB callback = pending_callbacks_.front();
      pending_callbacks_.pop_front();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(callback, snapshot));
    }
    pending_callbacks_.clear();
  } else {
    DoEnumerateDevices();
  }
}

int64_t AudioOutputDeviceEnumerator::NewEventSequence() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ++current_event_sequence_;
}

bool AudioOutputDeviceEnumerator::IsLastEnumerationValid() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return seq_last_enumeration_ > seq_last_invalidation_ &&
         !is_enumeration_ongoing_;
}

}  // namespace content
