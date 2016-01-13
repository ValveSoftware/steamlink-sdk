// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/device_request_message_filter.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/browser/resource_context.h"

namespace content {

DeviceRequestMessageFilter::DeviceRequestMessageFilter(
    ResourceContext* resource_context,
    MediaStreamManager* media_stream_manager,
    int render_process_id)
    : BrowserMessageFilter(MediaStreamMsgStart),
      resource_context_(resource_context),
      media_stream_manager_(media_stream_manager),
      render_process_id_(render_process_id) {
  DCHECK(resource_context);
  DCHECK(media_stream_manager);
}

DeviceRequestMessageFilter::~DeviceRequestMessageFilter() {
  // CHECK rather than DCHECK to make sure this never happens in the
  // wild. We want to be sure due to http://crbug.com/341211
  CHECK(requests_.empty());
}

struct DeviceRequestMessageFilter::DeviceRequest {
  DeviceRequest(int request_id,
                const GURL& origin,
                const std::string& audio_devices_label,
                const std::string& video_devices_label)
      : request_id(request_id),
        origin(origin),
        has_audio_returned(false),
        has_video_returned(false),
        audio_devices_label(audio_devices_label),
        video_devices_label(video_devices_label) {}

  int request_id;
  GURL origin;
  bool has_audio_returned;
  bool has_video_returned;
  std::string audio_devices_label;
  std::string video_devices_label;
  StreamDeviceInfoArray audio_devices;
  StreamDeviceInfoArray video_devices;
};

void DeviceRequestMessageFilter::DevicesEnumerated(
    int render_view_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfoArray& new_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Look up the DeviceRequest by id.
  DeviceRequestList::iterator request_it = requests_.begin();
  for (; request_it != requests_.end(); ++request_it) {
    if (label == request_it->audio_devices_label ||
        label == request_it->video_devices_label) {
      break;
    }
  }
  DCHECK(request_it != requests_.end());

  StreamDeviceInfoArray* audio_devices = &request_it->audio_devices;
  StreamDeviceInfoArray* video_devices = &request_it->video_devices;

  // Store hmac'd device ids instead of raw device ids.
  if (label == request_it->audio_devices_label) {
    request_it->has_audio_returned = true;
    DCHECK(audio_devices->empty());
    *audio_devices = new_devices;
  } else {
    DCHECK(label == request_it->video_devices_label);
    request_it->has_video_returned = true;
    DCHECK(video_devices->empty());
    *video_devices = new_devices;
  }

  if (!request_it->has_audio_returned || !request_it->has_video_returned) {
    // Wait for the rest of the devices to complete.
    return;
  }

  // Both audio and video devices are ready for copying.
  StreamDeviceInfoArray all_devices = *audio_devices;
  all_devices.insert(
      all_devices.end(), video_devices->begin(), video_devices->end());

  Send(new MediaStreamMsg_GetSourcesACK(request_it->request_id, all_devices));

  media_stream_manager_->CancelRequest(request_it->audio_devices_label);
  media_stream_manager_->CancelRequest(request_it->video_devices_label);
  requests_.erase(request_it);
}

bool DeviceRequestMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceRequestMessageFilter, message)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GetSources, OnGetSources)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceRequestMessageFilter::OnChannelClosing() {
  // Since the IPC channel is gone, cancel outstanding device requests.
  for (DeviceRequestList::iterator request_it = requests_.begin();
       request_it != requests_.end(); ++request_it) {
    media_stream_manager_->CancelRequest(request_it->audio_devices_label);
    media_stream_manager_->CancelRequest(request_it->video_devices_label);
  }
  requests_.clear();
}

void DeviceRequestMessageFilter::OnGetSources(int request_id,
                                              const GURL& security_origin) {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id_, security_origin)) {
    LOG(ERROR) << "Disallowed URL in DRMF::OnGetSources: " << security_origin;
    return;
  }

  // Make request to get audio devices.
  const std::string& audio_label = media_stream_manager_->EnumerateDevices(
      this, -1, -1, resource_context_->GetMediaDeviceIDSalt(), -1,
      MEDIA_DEVICE_AUDIO_CAPTURE, security_origin,
      resource_context_->AllowMicAccess(security_origin));
  DCHECK(!audio_label.empty());

  // Make request for video devices.
  const std::string& video_label = media_stream_manager_->EnumerateDevices(
      this, -1, -1, resource_context_->GetMediaDeviceIDSalt(), -1,
      MEDIA_DEVICE_VIDEO_CAPTURE, security_origin,
      resource_context_->AllowCameraAccess(security_origin));
  DCHECK(!video_label.empty());

  requests_.push_back(DeviceRequest(
      request_id, security_origin, audio_label, video_label));
}

}  // namespace content
