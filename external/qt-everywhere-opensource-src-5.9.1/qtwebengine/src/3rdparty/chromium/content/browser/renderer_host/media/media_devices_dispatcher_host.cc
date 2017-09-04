// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_devices.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/media_stream_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/origin.h"

namespace content {

namespace {

MediaDeviceInfo TranslateDeviceInfo(bool has_permission,
                                    const std::string& device_id_salt,
                                    const std::string& group_id_salt,
                                    const url::Origin& security_origin,
                                    const MediaDeviceInfo& device_info) {
  return MediaDeviceInfo(
      GetHMACForMediaDeviceID(device_id_salt, security_origin,
                              device_info.device_id),
      has_permission ? device_info.label : std::string(),
      device_info.group_id.empty()
          ? std::string()
          : GetHMACForMediaDeviceID(group_id_salt, security_origin,
                                    device_info.group_id));
}

MediaDeviceInfoArray TranslateMediaDeviceInfoArray(
    bool has_permission,
    const std::string& device_id_salt,
    const std::string& group_id_salt,
    const url::Origin& security_origin,
    const MediaDeviceInfoArray& input) {
  MediaDeviceInfoArray result;
  for (const auto& device_info : input) {
    result.push_back(TranslateDeviceInfo(has_permission, device_id_salt,
                                         group_id_salt, security_origin,
                                         device_info));
  }
  return result;
}

}  // namespace

struct MediaDevicesDispatcherHost::SubscriptionInfo {
  uint32_t subscription_id;
  url::Origin security_origin;
};

// static
void MediaDevicesDispatcherHost::Create(
    int render_process_id,
    int render_frame_id,
    const std::string& device_id_salt,
    MediaStreamManager* media_stream_manager,
    ::mojom::MediaDevicesDispatcherHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(base::MakeUnique<MediaDevicesDispatcherHost>(
                              render_process_id, render_frame_id,
                              device_id_salt, media_stream_manager),
                          std::move(request));
}

MediaDevicesDispatcherHost::MediaDevicesDispatcherHost(
    int render_process_id,
    int render_frame_id,
    const std::string& device_id_salt,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      device_id_salt_(device_id_salt),
      group_id_salt_(ResourceContext::CreateRandomMediaDeviceIDSalt()),
      media_stream_manager_(media_stream_manager),
      permission_checker_(base::MakeUnique<MediaDevicesPermissionChecker>()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

MediaDevicesDispatcherHost::~MediaDevicesDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!device_change_subscriptions_[i].empty()) {
      media_stream_manager_->media_devices_manager()
          ->UnsubscribeDeviceChangeNotifications(
              static_cast<MediaDeviceType>(i), this);
    }
  }
}

void MediaDevicesDispatcherHost::EnumerateDevices(
    bool request_audio_input,
    bool request_video_input,
    bool request_audio_output,
    const url::Origin& security_origin,
    const EnumerateDevicesCallback& client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!request_audio_input && !request_video_input && !request_audio_output) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_DEVICE_TYPE_REQUEST);
    return;
  }

  // Ignore requests from unique origins, but do not crash the renderer.
  if (security_origin.unique())
    return;

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           security_origin)) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::MDDH_UNAUTHORIZED_ORIGIN);
    return;
  }

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = request_audio_input;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = request_video_input;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = request_audio_output;

  permission_checker_->CheckPermissions(
      devices_to_enumerate, render_process_id_, render_frame_id_,
      security_origin,
      base::Bind(&MediaDevicesDispatcherHost::DoEnumerateDevices,
                 weak_factory_.GetWeakPtr(), devices_to_enumerate,
                 security_origin, client_callback));
}

void MediaDevicesDispatcherHost::SubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    uint32_t subscription_id,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           security_origin)) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::MDDH_UNAUTHORIZED_ORIGIN);
    return;
  }

  auto it = std::find_if(device_change_subscriptions_[type].begin(),
                         device_change_subscriptions_[type].end(),
                         [subscription_id](const SubscriptionInfo& info) {
                           return info.subscription_id == subscription_id;
                         });

  if (it != device_change_subscriptions_[type].end()) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_SUBSCRIPTION_REQUEST);
    return;
  }

  if (device_change_subscriptions_[type].empty()) {
    media_stream_manager_->media_devices_manager()
        ->SubscribeDeviceChangeNotifications(type, this);
  }

  device_change_subscriptions_[type].push_back(
      SubscriptionInfo{subscription_id, security_origin});
}

void MediaDevicesDispatcherHost::UnsubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    uint32_t subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  auto it = std::find_if(device_change_subscriptions_[type].begin(),
                         device_change_subscriptions_[type].end(),
                         [subscription_id](const SubscriptionInfo& info) {
                           return info.subscription_id == subscription_id;
                         });

  if (it == device_change_subscriptions_[type].end()) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_UNSUBSCRIPTION_REQUEST);
    return;
  }

  device_change_subscriptions_[type].erase(it);
  if (device_change_subscriptions_[type].empty()) {
    media_stream_manager_->media_devices_manager()
        ->UnsubscribeDeviceChangeNotifications(type, this);
  }
}

void MediaDevicesDispatcherHost::OnDevicesChanged(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaDevicesDispatcherHost::NotifyDeviceChangeOnUIThread,
                 weak_factory_.GetWeakPtr(), device_change_subscriptions_[type],
                 type, device_infos));
}

void MediaDevicesDispatcherHost::NotifyDeviceChangeOnUIThread(
    const std::vector<SubscriptionInfo>& subscriptions,
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsValidMediaDeviceType(type));

  ::mojom::MediaDevicesListenerPtr media_devices_listener;
  if (device_change_listener_) {
    media_devices_listener = std::move(device_change_listener_);
  } else {
    RenderFrameHost* render_frame_host =
        RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (!render_frame_host)
      return;

    render_frame_host->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&media_devices_listener));
    if (!media_devices_listener)
      return;
  }

  for (const auto& subscription : subscriptions) {
    bool has_permission = permission_checker_->CheckPermissionOnUIThread(
        type, render_process_id_, render_frame_id_,
        subscription.security_origin);
    media_devices_listener->OnDevicesChanged(
        type, subscription.subscription_id,
        TranslateMediaDeviceInfoArray(
            has_permission, device_id_salt_, group_id_salt_,
            subscription.security_origin, device_infos));
  }
}

void MediaDevicesDispatcherHost::SetPermissionChecker(
    std::unique_ptr<MediaDevicesPermissionChecker> permission_checker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(permission_checker);
  permission_checker_ = std::move(permission_checker);
}

void MediaDevicesDispatcherHost::SetDeviceChangeListenerForTesting(
    ::mojom::MediaDevicesListenerPtr listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  device_change_listener_ = std::move(listener);
}

void MediaDevicesDispatcherHost::DoEnumerateDevices(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    const url::Origin& security_origin,
    const EnumerateDevicesCallback& client_callback,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      requested_types,
      base::Bind(&MediaDevicesDispatcherHost::DevicesEnumerated,
                 weak_factory_.GetWeakPtr(), requested_types, security_origin,
                 client_callback, has_permissions));
}

void MediaDevicesDispatcherHost::DevicesEnumerated(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    const url::Origin& security_origin,
    const EnumerateDevicesCallback& client_callback,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<std::vector<MediaDeviceInfo>> result(NUM_MEDIA_DEVICE_TYPES);
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!requested_types[i])
      continue;

    for (const auto& device_info : enumeration[i]) {
      result[i].push_back(TranslateDeviceInfo(has_permissions[i],
                                              device_id_salt_, group_id_salt_,
                                              security_origin, device_info));
    }
  }
  client_callback.Run(result);
}

}  // namespace content
