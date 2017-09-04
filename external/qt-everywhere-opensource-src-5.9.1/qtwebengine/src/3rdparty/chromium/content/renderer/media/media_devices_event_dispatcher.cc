// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_devices_event_dispatcher.h"

#include <stddef.h>

#include <algorithm>

#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/origin.h"

namespace content {

base::WeakPtr<MediaDevicesEventDispatcher>
MediaDevicesEventDispatcher::GetForRenderFrame(RenderFrame* render_frame) {
  MediaDevicesEventDispatcher* dispatcher =
      MediaDevicesEventDispatcher::Get(render_frame);
  if (!dispatcher)
    dispatcher = new MediaDevicesEventDispatcher(render_frame);

  return dispatcher->AsWeakPtr();
}

MediaDevicesEventDispatcher::MediaDevicesEventDispatcher(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<MediaDevicesEventDispatcher>(render_frame),
      current_id_(0) {}

MediaDevicesEventDispatcher::~MediaDevicesEventDispatcher() {}

MediaDevicesEventDispatcher::SubscriptionId
MediaDevicesEventDispatcher::SubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    const url::Origin& security_origin,
    const MediaDevicesEventDispatcher::DevicesChangedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidMediaDeviceType(type));

  SubscriptionId subscription_id = ++current_id_;
  GetMediaDevicesDispatcher()->SubscribeDeviceChangeNotifications(
      type, subscription_id, security_origin);
  SubscriptionList& subscriptions = device_change_subscriptions_[type];
  subscriptions.push_back(Subscription{subscription_id, callback});

  return current_id_;
}

void MediaDevicesEventDispatcher::UnsubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    SubscriptionId subscription_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidMediaDeviceType(type));

  SubscriptionList& subscriptions = device_change_subscriptions_[type];
  auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
                         [subscription_id](const Subscription& subscription) {
                           return subscription.first == subscription_id;
                         });
  if (it == subscriptions.end())
    return;

  GetMediaDevicesDispatcher()->UnsubscribeDeviceChangeNotifications(type,
                                                                    it->first);
  subscriptions.erase(it);
}

MediaDevicesEventDispatcher::SubscriptionIdList
MediaDevicesEventDispatcher::SubscribeDeviceChangeNotifications(
    const url::Origin& security_origin,
    const DevicesChangedCallback& callback) {
  SubscriptionIdList list;
  SubscriptionId id;
  id = SubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_AUDIO_INPUT,
                                          security_origin, callback);
  list.push_back(id);
  id = SubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_VIDEO_INPUT,
                                          security_origin, callback);
  list.push_back(id);
  id = SubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT,
                                          security_origin, callback);
  list.push_back(id);

  return list;
}

void MediaDevicesEventDispatcher::UnsubscribeDeviceChangeNotifications(
    const SubscriptionIdList& subscription_ids) {
  DCHECK_EQ(static_cast<size_t>(NUM_MEDIA_DEVICE_TYPES),
            subscription_ids.size());
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i)
    UnsubscribeDeviceChangeNotifications(static_cast<MediaDeviceType>(i),
                                         subscription_ids[i]);
}

void MediaDevicesEventDispatcher::DispatchDevicesChangedEvent(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidMediaDeviceType(type));
  SubscriptionList& subscriptions = device_change_subscriptions_[type];
  for (auto& subscription : subscriptions)
    subscription.second.Run(type, device_infos);
}

void MediaDevicesEventDispatcher::OnDestruct() {
  delete this;
}

void MediaDevicesEventDispatcher::SetMediaDevicesDispatcherForTesting(
    ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher) {
  media_devices_dispatcher_ = std::move(media_devices_dispatcher);
}

const ::mojom::MediaDevicesDispatcherHostPtr&
MediaDevicesEventDispatcher::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&media_devices_dispatcher_));
  }

  return media_devices_dispatcher_;
}

}  // namespace content
