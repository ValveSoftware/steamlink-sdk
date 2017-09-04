// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_device_manager.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/media_devices_event_dispatcher.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/render_frame_impl.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

PP_DeviceType_Dev FromMediaDeviceType(MediaDeviceType type) {
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return PP_DEVICETYPE_DEV_AUDIOCAPTURE;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return PP_DEVICETYPE_DEV_VIDEOCAPTURE;
    default:
      NOTREACHED();
      return PP_DEVICETYPE_DEV_INVALID;
  }
}

MediaDeviceType ToMediaDeviceType(PP_DeviceType_Dev type) {
  switch (type) {
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return MEDIA_DEVICE_TYPE_AUDIO_INPUT;
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return MEDIA_DEVICE_TYPE_VIDEO_INPUT;
    default:
      NOTREACHED();
      return MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
  }
}

ppapi::DeviceRefData FromMediaDeviceInfo(MediaDeviceType type,
                                         const MediaDeviceInfo& info) {
  ppapi::DeviceRefData data;
  data.id = info.device_id;
  // Some Flash content can't handle an empty string, so stick a space in to
  // make them happy. See crbug.com/408404.
  data.name = info.label.empty() ? std::string(" ") : info.label;
  data.type = FromMediaDeviceType(type);
  return data;
}

}  // namespace

base::WeakPtr<PepperMediaDeviceManager>
PepperMediaDeviceManager::GetForRenderFrame(
    RenderFrame* render_frame) {
  PepperMediaDeviceManager* handler =
      PepperMediaDeviceManager::Get(render_frame);
  if (!handler)
    handler = new PepperMediaDeviceManager(render_frame);
  return handler->AsWeakPtr();
}

PepperMediaDeviceManager::PepperMediaDeviceManager(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<PepperMediaDeviceManager>(render_frame),
      next_id_(1) {}

PepperMediaDeviceManager::~PepperMediaDeviceManager() {
  DCHECK(open_callbacks_.empty());
}

void PepperMediaDeviceManager::EnumerateDevices(
    PP_DeviceType_Dev type,
    const GURL& document_url,
    const DevicesCallback& callback) {
#if defined(ENABLE_WEBRTC)
  bool request_audio_input = type == PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  bool request_video_input = type == PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  CHECK(request_audio_input || request_video_input);
  GetMediaDevicesDispatcher()->EnumerateDevices(
      request_audio_input, request_video_input, false /* audio_output */,
      url::Origin(document_url.GetOrigin()),
      base::Bind(&PepperMediaDeviceManager::DevicesEnumerated, AsWeakPtr(),
                 callback, ToMediaDeviceType(type)));
#else
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&PepperMediaDeviceManager::DevicesEnumerated, AsWeakPtr(),
                 callback, ToMediaDeviceType(type), std::vector<MediaDeviceInfoArray>()));
#endif
}

uint32_t PepperMediaDeviceManager::StartMonitoringDevices(
    PP_DeviceType_Dev type,
    const GURL& document_url,
    const DevicesCallback& callback) {
#if defined(ENABLE_WEBRTC)
  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
      MediaDevicesEventDispatcher::GetForRenderFrame(render_frame());
  return event_dispatcher->SubscribeDeviceChangeNotifications(
      ToMediaDeviceType(type), url::Origin(document_url.GetOrigin()),
      base::Bind(&PepperMediaDeviceManager::DevicesChanged, AsWeakPtr(),
                 callback));
#else
  return 0;
#endif
}

void PepperMediaDeviceManager::StopMonitoringDevices(PP_DeviceType_Dev type,
                                                     uint32_t subscription_id) {
#if defined(ENABLE_WEBRTC)
  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
      MediaDevicesEventDispatcher::GetForRenderFrame(render_frame());
  event_dispatcher->UnsubscribeDeviceChangeNotifications(
      ToMediaDeviceType(type), subscription_id);
#endif
}

int PepperMediaDeviceManager::OpenDevice(PP_DeviceType_Dev type,
                                         const std::string& device_id,
                                         const GURL& document_url,
                                         const OpenDeviceCallback& callback) {
  open_callbacks_[next_id_] = callback;
  int request_id = next_id_++;

#if defined(ENABLE_WEBRTC)
  GetMediaStreamDispatcher()->OpenDevice(
      request_id, AsWeakPtr(), device_id,
      PepperMediaDeviceManager::FromPepperDeviceType(type),
      url::Origin(document_url.GetOrigin()));
#else
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PepperMediaDeviceManager::OnDeviceOpenFailed,
                            AsWeakPtr(), request_id));
#endif

  return request_id;
}

void PepperMediaDeviceManager::CancelOpenDevice(int request_id) {
  open_callbacks_.erase(request_id);

#if defined(ENABLE_WEBRTC)
  GetMediaStreamDispatcher()->CancelOpenDevice(request_id, AsWeakPtr());
#endif
}

void PepperMediaDeviceManager::CloseDevice(const std::string& label) {
#if defined(ENABLE_WEBRTC)
  GetMediaStreamDispatcher()->CloseDevice(label);
#endif
}

int PepperMediaDeviceManager::GetSessionID(PP_DeviceType_Dev type,
                                           const std::string& label) {
#if defined(ENABLE_WEBRTC)
  switch (type) {
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return GetMediaStreamDispatcher()->audio_session_id(label, 0);
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return GetMediaStreamDispatcher()->video_session_id(label, 0);
    default:
      NOTREACHED();
      return 0;
  }
#else
  return 0;
#endif
}

void PepperMediaDeviceManager::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_device_array,
    const StreamDeviceInfoArray& video_device_array) {}

void PepperMediaDeviceManager::OnStreamGenerationFailed(
    int request_id,
    content::MediaStreamRequestResult result) {}

void PepperMediaDeviceManager::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {}

void PepperMediaDeviceManager::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  NotifyDeviceOpened(request_id, true, label);
}

void PepperMediaDeviceManager::OnDeviceOpenFailed(int request_id) {
  NotifyDeviceOpened(request_id, false, std::string());
}

// static
MediaStreamType PepperMediaDeviceManager::FromPepperDeviceType(
    PP_DeviceType_Dev type) {
  switch (type) {
    case PP_DEVICETYPE_DEV_INVALID:
      return MEDIA_NO_SERVICE;
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return MEDIA_DEVICE_AUDIO_CAPTURE;
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return MEDIA_DEVICE_VIDEO_CAPTURE;
    default:
      NOTREACHED();
      return MEDIA_NO_SERVICE;
  }
}

void PepperMediaDeviceManager::NotifyDeviceOpened(int request_id,
                                                  bool succeeded,
                                                  const std::string& label) {
  OpenCallbackMap::iterator iter = open_callbacks_.find(request_id);
  if (iter == open_callbacks_.end()) {
    // The callback may have been unregistered.
    return;
  }

  OpenDeviceCallback callback = iter->second;
  open_callbacks_.erase(iter);

  callback.Run(request_id, succeeded, label);
}

void PepperMediaDeviceManager::DevicesEnumerated(
    const DevicesCallback& client_callback,
    MediaDeviceType type,
    const std::vector<MediaDeviceInfoArray>& enumeration) {
  DevicesChanged(client_callback, type, enumeration[type]);
}

void PepperMediaDeviceManager::DevicesChanged(
    const DevicesCallback& client_callback,
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  std::vector<ppapi::DeviceRefData> devices;
  devices.reserve(device_infos.size());
  for (const auto& device_info : device_infos)
    devices.push_back(FromMediaDeviceInfo(type, device_info));

  client_callback.Run(devices);
}

MediaStreamDispatcher* PepperMediaDeviceManager::GetMediaStreamDispatcher()
    const {
  DCHECK(render_frame());
  MediaStreamDispatcher* const dispatcher =
      static_cast<RenderFrameImpl*>(render_frame())->GetMediaStreamDispatcher();
  DCHECK(dispatcher);
  return dispatcher;
}

const ::mojom::MediaDevicesDispatcherHostPtr&
PepperMediaDeviceManager::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    DCHECK(render_frame());
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&media_devices_dispatcher_));
  }

  return media_devices_dispatcher_;
}

void PepperMediaDeviceManager::OnDestruct() {
  delete this;
}

}  // namespace content
