// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_DEVICES_EVENT_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_DEVICES_EVENT_DISPATCHER_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "content/common/media/media_devices.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace url {
class Origin;
}

namespace content {

// This class implements the logic for managing media device-change
// subscriptions on the renderer process. It manages the generation of
// subscription IDs and  handles the interaction with the Mojo objects that
// communicate with the  browser process. There is at most one instance of this
// class per frame.
class CONTENT_EXPORT MediaDevicesEventDispatcher
    : public RenderFrameObserver,
      public RenderFrameObserverTracker<MediaDevicesEventDispatcher>,
      public base::SupportsWeakPtr<MediaDevicesEventDispatcher> {
 public:
  using SubscriptionId = uint32_t;
  using SubscriptionIdList = std::vector<SubscriptionId>;
  using DevicesChangedCallback =
      base::Callback<void(MediaDeviceType, const MediaDeviceInfoArray&)>;

  // Returns a weak pointer to the MediaDevicesEventDispatcher for the given
  // |render_frame|.
  static base::WeakPtr<MediaDevicesEventDispatcher> GetForRenderFrame(
      RenderFrame* render_frame);

  ~MediaDevicesEventDispatcher() override;

  // Register a callback for device-change notifications for a given device
  // type. Returns a subscription ID that can be used to cancel the
  // subscription.
  SubscriptionId SubscribeDeviceChangeNotifications(
      MediaDeviceType type,
      const url::Origin& security_origin,
      const DevicesChangedCallback& callback);

  // Cancels a subscription identified with |subscription_id| to device-change
  // notifications for device type |type|. If no subscription identified with
  // |subscription_id| exists for device type |type|, this function returns
  // without side effects.
  void UnsubscribeDeviceChangeNotifications(MediaDeviceType type,
                                            SubscriptionId subscription_id);

  // Register a callback for device-change notifications for any device type.
  // Returns a list of subscription IDs that can be used to cancel the
  // subscriptions.
  SubscriptionIdList SubscribeDeviceChangeNotifications(
      const url::Origin& security_origin,
      const DevicesChangedCallback& callback);

  // Cancels subscriptions identified by |subscription_ids| to device-change
  // notifications for all device types. This is implemented by invoking the
  // single-parameter version of UnsubscribeDeviceChangeNotifications() for all
  // device types.
  void UnsubscribeDeviceChangeNotifications(
      const SubscriptionIdList& subscription_ids);

  // Dispatches an event notification to all registered callbacks for device
  // type |type|. Should be invoked only by an object that detects changes in
  // the set of media devices.
  void DispatchDevicesChangedEvent(MediaDeviceType type,
                                   const MediaDeviceInfoArray& device_infos);

  // RenderFrameObserver override.
  void OnDestruct() override;

  void SetMediaDevicesDispatcherForTesting(
      ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher);

 private:
  explicit MediaDevicesEventDispatcher(RenderFrame* render_frame);

  const ::mojom::MediaDevicesDispatcherHostPtr& GetMediaDevicesDispatcher();

  SubscriptionId current_id_;

  using Subscription = std::pair<SubscriptionId, DevicesChangedCallback>;
  using SubscriptionList = std::vector<Subscription>;
  SubscriptionList device_change_subscriptions_[NUM_MEDIA_DEVICE_TYPES];

  ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesEventDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_DEVICES_EVENT_DISPATCHER_H_
