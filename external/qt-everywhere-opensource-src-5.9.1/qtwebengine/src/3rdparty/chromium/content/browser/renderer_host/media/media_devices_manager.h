// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_MANAGER_H_

#include <array>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/system_monitor/system_monitor.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "media/capture/video/video_capture_device_descriptor.h"

namespace media {
class AudioManager;
}

namespace content {

class MediaStreamManager;
class VideoCaptureManager;

// Use MediaDeviceType values to index on this type.
using MediaDeviceEnumeration =
    std::array<MediaDeviceInfoArray, NUM_MEDIA_DEVICE_TYPES>;

// MediaDeviceChangeSubscriber is an interface to be implemented by classes
// that can register with MediaDevicesManager to get notifications about changes
// in the set of media devices.
class CONTENT_EXPORT MediaDeviceChangeSubscriber {
 public:
  // This function is invoked to notify about changes in the set of media
  // devices of type |type|. |device_infos| contains the updated list of
  // devices of type |type|.
  virtual void OnDevicesChanged(MediaDeviceType type,
                                const MediaDeviceInfoArray& device_infos) = 0;
};

// MediaDevicesManager is responsible for doing media-device enumerations.
// In addition it implements caching for enumeration results and device
// monitoring in order to keep caches consistent.
// All its methods must be called on the IO thread.
class CONTENT_EXPORT MediaDevicesManager
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  // Use MediaDeviceType values to index on this type. By default all device
  // types are false.
  class BoolDeviceTypes final
      : public std::array<bool, NUM_MEDIA_DEVICE_TYPES> {
   public:
    BoolDeviceTypes() { fill(false); }
  };

  using EnumerationCallback =
      base::Callback<void(const MediaDeviceEnumeration&)>;

  MediaDevicesManager(
      media::AudioManager* audio_manager,
      const scoped_refptr<VideoCaptureManager>& video_capture_manager,
      MediaStreamManager* media_stream_manager);
  ~MediaDevicesManager() override;

  // Performs a possibly cached device enumeration for the requested device
  // types and reports the results to |callback|.
  // The enumeration results passed to |callback| are guaranteed to be valid
  // only for the types specified in |requested_types|.
  // Note that this function is not reentrant, so if |callback| needs to perform
  // another call to EnumerateDevices, it must do so by posting a task to the
  // IO thread.
  void EnumerateDevices(const BoolDeviceTypes& requested_types,
                        const EnumerationCallback& callback);

  // Subscribes |subscriber| to receive device-change notifications for devices
  // of type |type|. If |subscriber| is already subscribed, this function has
  // no side effects. MediaDevicesManager does not own |subscriber|. It is the
  // responsibility of the caller to ensure that all registered subscribers
  // remain valid while the they are subscribed.
  void SubscribeDeviceChangeNotifications(
      MediaDeviceType type,
      MediaDeviceChangeSubscriber* subscriber);

  // Unubscribes |subscriber| from device-change notifications for the devices
  // of type |type|. If |subscriber| is not subscribed, this function has no
  // side effects.
  void UnsubscribeDeviceChangeNotifications(
      MediaDeviceType type,
      MediaDeviceChangeSubscriber* subscriber);

  // Tries to start device monitoring. If successful, enables caching of
  // enumeration results for the device types supported by the monitor.
  void StartMonitoring();

  // Stops device monitoring and disables caching for all device types.
  void StopMonitoring();

  // Returns true if device monitoring is active, false otherwise.
  bool IsMonitoringStarted();

  // Implements base::SystemMonitor::DevicesChangedObserver.
  // This function is only called in response to physical audio/video device
  // changes.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

  // TODO(guidou): Remove this function once content::GetMediaDeviceIDForHMAC
  // is rewritten to receive devices via a callback.
  // See http://crbug.com/648155.
  MediaDeviceInfoArray GetCachedDeviceInfo(MediaDeviceType type);

 private:
  friend class MediaDevicesManagerTest;
  struct EnumerationRequest;

  // The NO_CACHE policy is such that no previous results are used when
  // EnumerateDevices is called. The results of a new or in-progress low-level
  // device enumeration are used.
  // The SYSTEM_MONITOR policy is such that previous results are re-used,
  // provided they were produced by a low-level device enumeration issued after
  // the last call to OnDevicesChanged.
  enum class CachePolicy {
    NO_CACHE,
    SYSTEM_MONITOR,
  };

  // Manually sets a caching policy for a given device type.
  void SetCachePolicy(MediaDeviceType type, CachePolicy policy);

  // Helpers to issue low-level device enumerations.
  void DoEnumerateDevices(MediaDeviceType type);
  void EnumerateAudioDevices(bool is_input);

  // Callback for VideoCaptureManager::EnumerateDevices.
  void VideoInputDevicesEnumerated(
      const media::VideoCaptureDeviceDescriptors& descriptors);

  // Helpers to handle enumeration results.
  void DevicesEnumerated(MediaDeviceType type,
                         const MediaDeviceInfoArray& snapshot);
  void UpdateSnapshot(MediaDeviceType type,
                      const MediaDeviceInfoArray& new_snapshot);
  void ProcessRequests();
  bool IsEnumerationRequestReady(const EnumerationRequest& request_info);

  // Helpers to handle device-change notification.
  void HandleDevicesChanged(MediaDeviceType type);
  void NotifyMediaStreamManager(MediaDeviceType type,
                                const MediaDeviceInfoArray& new_snapshot);
  void NotifyDeviceChangeSubscribers(MediaDeviceType type,
                                     const MediaDeviceInfoArray& snapshot);

#if defined(OS_MACOSX)
  void StartMonitoringOnUIThread();
#endif

  bool use_fake_devices_;
  media::AudioManager* const audio_manager_;  // not owned
  scoped_refptr<VideoCaptureManager> video_capture_manager_;
  MediaStreamManager* const media_stream_manager_;  // not owned

  using CachePolicies = std::array<CachePolicy, NUM_MEDIA_DEVICE_TYPES>;
  CachePolicies cache_policies_;

  class CacheInfo;
  using CacheInfos = std::vector<CacheInfo>;
  CacheInfos cache_infos_;

  BoolDeviceTypes has_seen_result_;
  std::vector<EnumerationRequest> requests_;
  MediaDeviceEnumeration current_snapshot_;
  bool monitoring_started_;

  std::vector<MediaDeviceChangeSubscriber*>
      device_change_subscribers_[NUM_MEDIA_DEVICE_TYPES];

  base::WeakPtrFactory<MediaDevicesManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_MANAGER_H_
