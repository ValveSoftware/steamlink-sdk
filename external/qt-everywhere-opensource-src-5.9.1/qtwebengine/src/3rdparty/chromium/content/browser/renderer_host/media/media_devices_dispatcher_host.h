// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.mojom.h"

using ::mojom::MediaDeviceType;

namespace url {
class Origin;
}

namespace content {

class MediaStreamManager;

class CONTENT_EXPORT MediaDevicesDispatcherHost
    : public ::mojom::MediaDevicesDispatcherHost,
      public MediaDeviceChangeSubscriber {
 public:
  MediaDevicesDispatcherHost(int render_process_id,
                             int render_frame_id,
                             const std::string& device_id_salt,
                             MediaStreamManager* media_stream_manager);
  ~MediaDevicesDispatcherHost() override;

  static void Create(int render_process_id,
                     int render_frame_id,
                     const std::string& device_id_salt,
                     MediaStreamManager* media_stream_manager,
                     ::mojom::MediaDevicesDispatcherHostRequest request);

  // ::mojom::MediaDevicesDispatcherHost implementation.
  void EnumerateDevices(
      bool request_audio_input,
      bool request_video_input,
      bool request_audio_output,
      const url::Origin& security_origin,
      const EnumerateDevicesCallback& client_callback) override;
  void SubscribeDeviceChangeNotifications(
      MediaDeviceType type,
      uint32_t subscription_id,
      const url::Origin& security_origin) override;
  void UnsubscribeDeviceChangeNotifications(MediaDeviceType type,
                                            uint32_t subscription_id) override;

  // MediaDeviceChangeSubscriber implementation.
  void OnDevicesChanged(MediaDeviceType type,
                        const MediaDeviceInfoArray& device_infos) override;

  void SetPermissionChecker(
      std::unique_ptr<MediaDevicesPermissionChecker> permission_checker);

  void SetDeviceChangeListenerForTesting(
      ::mojom::MediaDevicesListenerPtr listener);

 private:
  void DoEnumerateDevices(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      const url::Origin& security_origin,
      const EnumerateDevicesCallback& client_callback,
      const MediaDevicesManager::BoolDeviceTypes& permissions);

  void DevicesEnumerated(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      const url::Origin& security_origin,
      const EnumerateDevicesCallback& client_callback,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions,
      const MediaDeviceEnumeration& enumeration);

  struct SubscriptionInfo;
  void NotifyDeviceChangeOnUIThread(
      const std::vector<SubscriptionInfo>& subscriptions,
      MediaDeviceType type,
      const MediaDeviceInfoArray& device_infos);

  // The following const fields can be accessed on any thread.
  const int render_process_id_;
  const int render_frame_id_;
  const std::string device_id_salt_;
  const std::string group_id_salt_;

  // The following fields can only be accessed on the IO thread.
  MediaStreamManager* media_stream_manager_;
  std::unique_ptr<MediaDevicesPermissionChecker> permission_checker_;
  std::vector<SubscriptionInfo>
      device_change_subscriptions_[NUM_MEDIA_DEVICE_TYPES];

  // This field can only be accessed on the UI thread.
  ::mojom::MediaDevicesListenerPtr device_change_listener_;

  base::WeakPtrFactory<MediaDevicesDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
