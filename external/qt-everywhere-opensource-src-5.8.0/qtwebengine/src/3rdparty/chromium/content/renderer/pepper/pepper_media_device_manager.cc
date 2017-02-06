// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_device_manager.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/render_frame_impl.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"

namespace content {

namespace {

ppapi::DeviceRefData FromStreamDeviceInfo(const StreamDeviceInfo& info) {
  ppapi::DeviceRefData data;
  data.id = info.device.id;
  // Some Flash content can't handle an empty string, so stick a space in to
  // make them happy. See crbug.com/408404.
  data.name = info.device.name.empty() ? std::string(" ") : info.device.name;
  data.type = PepperMediaDeviceManager::FromMediaStreamType(info.device.type);
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
  DCHECK(enumerate_callbacks_.empty());
  DCHECK(open_callbacks_.empty());
}

int PepperMediaDeviceManager::EnumerateDevices(
    PP_DeviceType_Dev type,
    const GURL& document_url,
    const EnumerateDevicesCallback& callback) {
  enumerate_callbacks_[next_id_] = callback;
  int request_id = next_id_++;

#if defined(ENABLE_WEBRTC)
  GetMediaStreamDispatcher()->EnumerateDevices(
      request_id, AsWeakPtr(),
      PepperMediaDeviceManager::FromPepperDeviceType(type),
      url::Origin(document_url.GetOrigin()));
#else
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PepperMediaDeviceManager::OnDevicesEnumerated,
                            AsWeakPtr(), request_id, StreamDeviceInfoArray()));
#endif

  return request_id;
}

void PepperMediaDeviceManager::StopEnumerateDevices(int request_id) {
  enumerate_callbacks_.erase(request_id);

#if defined(ENABLE_WEBRTC)
  // Need to post task since this function might be called inside the callback
  // of EnumerateDevices.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&PepperMediaDeviceManager::StopEnumerateDevicesDelayed,
                 AsWeakPtr(), request_id));
#endif
}

void PepperMediaDeviceManager::StopEnumerateDevicesDelayed(int request_id) {
#if defined(ENABLE_WEBRTC)
  // This method is being invoked by the message loop at some unknown
  // point-in-time after StopEnumerateDevices().  Therefore, check that
  // render_frame() is not NULL, in order to guarantee
  // GetMediaStreamDispatcher() won't return NULL.
  if (render_frame())
    GetMediaStreamDispatcher()->StopEnumerateDevices(request_id, AsWeakPtr());
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

void PepperMediaDeviceManager::OnDevicesEnumerated(
    int request_id,
    const StreamDeviceInfoArray& device_array) {
  EnumerateCallbackMap::iterator iter = enumerate_callbacks_.find(request_id);
  if (iter == enumerate_callbacks_.end()) {
    // This might be enumerated result sent before StopEnumerateDevices is
    // called since EnumerateDevices is persistent request.
    return;
  }

  EnumerateDevicesCallback callback = iter->second;

  std::vector<ppapi::DeviceRefData> devices;
  devices.reserve(device_array.size());
  for (StreamDeviceInfoArray::const_iterator info = device_array.begin();
       info != device_array.end();
       ++info) {
    devices.push_back(FromStreamDeviceInfo(*info));
  }
  callback.Run(request_id, devices);
}

void PepperMediaDeviceManager::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  NotifyDeviceOpened(request_id, true, label);
}

void PepperMediaDeviceManager::OnDeviceOpenFailed(int request_id) {
  NotifyDeviceOpened(request_id, false, std::string());
}

void PepperMediaDeviceManager::OnDevicesChanged() {}

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

// static
PP_DeviceType_Dev PepperMediaDeviceManager::FromMediaStreamType(
    MediaStreamType type) {
  switch (type) {
    case MEDIA_NO_SERVICE:
      return PP_DEVICETYPE_DEV_INVALID;
    case MEDIA_DEVICE_AUDIO_CAPTURE:
      return PP_DEVICETYPE_DEV_AUDIOCAPTURE;
    case MEDIA_DEVICE_VIDEO_CAPTURE:
      return PP_DEVICETYPE_DEV_VIDEOCAPTURE;
    default:
      NOTREACHED();
      return PP_DEVICETYPE_DEV_INVALID;
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

MediaStreamDispatcher* PepperMediaDeviceManager::GetMediaStreamDispatcher()
    const {
  DCHECK(render_frame());
  MediaStreamDispatcher* const dispatcher =
      static_cast<RenderFrameImpl*>(render_frame())->GetMediaStreamDispatcher();
  DCHECK(dispatcher);
  return dispatcher;
}

void PepperMediaDeviceManager::OnDestruct() {
  delete this;
}

}  // namespace content
