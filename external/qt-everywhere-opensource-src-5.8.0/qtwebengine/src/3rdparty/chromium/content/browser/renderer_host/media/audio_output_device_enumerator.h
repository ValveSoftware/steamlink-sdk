// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioOutputDeviceEnumerator is used to enumerate audio output devices.
// It can return cached results of previous enumerations in order to boost
// performance.
// All its public methods must be called on the thread where the object is
// created.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DEVICE_ENUMERATOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DEVICE_ENUMERATOR_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class AudioManager;
}

namespace content {

// AudioOutputDeviceInfo describes information about an audio output device.
// The enumerations returned by AudioOutputDeviceEnumerator::Enumerate() contain
// elements of this type. It is used only in the browser side.
struct AudioOutputDeviceInfo {
  std::string unique_id;
  std::string device_name;
  media::AudioParameters output_params;
};

// The result of an enumeration. It is used only in the browser side.
struct AudioOutputDeviceEnumeration {
 public:
  AudioOutputDeviceEnumeration(
      const std::vector<AudioOutputDeviceInfo>& devices,
      bool has_actual_devices);
  AudioOutputDeviceEnumeration();
  AudioOutputDeviceEnumeration(const AudioOutputDeviceEnumeration& other);
  ~AudioOutputDeviceEnumeration();

  std::vector<AudioOutputDeviceInfo> devices;
  bool has_actual_devices;
};

typedef base::Callback<void(const AudioOutputDeviceEnumeration&)>
    AudioOutputDeviceEnumerationCB;

class CONTENT_EXPORT AudioOutputDeviceEnumerator {
 public:
  enum CachePolicy {
    CACHE_POLICY_NO_CACHING,
    CACHE_POLICY_MANUAL_INVALIDATION
  };
  AudioOutputDeviceEnumerator(media::AudioManager* audio_manager,
                              CachePolicy cache_policy);
  ~AudioOutputDeviceEnumerator();

  // Does an enumeration and provides the results to the callback.
  // If there are no physical devices, the result contains a single entry with
  // the default parameters provided by the underlying audio manager and with
  // the |has_actual_devices| field set to false.
  // The behavior with no physical devices is there to ease the transition
  // from the use of RenderThreadImpl::GetAudioHardwareConfig(), which always
  // provides default parameters, even if there are no devices.
  // See https://crbug.com/549125.
  // Some audio managers always report a single device, regardless of the
  // physical devices in the system. In this case the |has_actual_devices| field
  // is set to true to differentiate from the case of no physical devices.
  void Enumerate(const AudioOutputDeviceEnumerationCB& callback);

  // Invalidates the current cache.
  void InvalidateCache();

  // Sets the cache policy.
  void SetCachePolicy(CachePolicy cache_policy);

  // Returns true if the caching policy is different from
  // CACHE_POLICY_NO_CACHING, false otherwise.
  bool IsCacheEnabled();

 private:
  void InitializeOnIOThread();
  void DoEnumerateDevices();
  AudioOutputDeviceEnumeration DoEnumerateDevicesOnDeviceThread();
  void DevicesEnumerated(const AudioOutputDeviceEnumeration& snapshot);
  int64_t NewEventSequence();
  bool IsLastEnumerationValid() const;

  media::AudioManager* const audio_manager_;
  CachePolicy cache_policy_;
  AudioOutputDeviceEnumeration cache_;
  std::list<AudioOutputDeviceEnumerationCB> pending_callbacks_;

  // sequential number that serves as logical clock
  int64_t current_event_sequence_;

  int64_t seq_last_enumeration_;
  int64_t seq_last_invalidation_;
  bool is_enumeration_ongoing_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<AudioOutputDeviceEnumerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceEnumerator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DEVICE_ENUMERATOR_H_
