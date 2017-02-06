// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

namespace content {

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    const std::string& salt,
    MediaStreamManager* media_stream_manager,
    bool use_fake_ui)
    : BrowserMessageFilter(MediaStreamMsgStart),
      render_process_id_(render_process_id),
      salt_(salt),
      media_stream_manager_(media_stream_manager),
      use_fake_ui_(use_fake_ui),
      weak_factory_(this) {}

void MediaStreamDispatcherHost::StreamGenerated(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerated("
           << ", {label = " << label <<  "})";

  Send(new MediaStreamMsg_StreamGenerated(
      render_frame_id, page_request_id, label, audio_devices, video_devices));
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    int render_frame_id,
    int page_request_id,
    content::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerationFailed("
           << ", {page_request_id = " << page_request_id <<  "}"
           << ", { result= " << result << "})";

  Send(new MediaStreamMsg_StreamGenerationFailed(render_frame_id,
                                                 page_request_id,
                                                 result));
}

void MediaStreamDispatcherHost::DeviceStopped(int render_frame_id,
                                              const std::string& label,
                                              const StreamDeviceInfo& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::DeviceStopped("
           << "{label = " << label << "}, "
           << "{type = " << device.device.type << "}, "
           << "{device_id = " << device.device.id << "})";

  Send(new MediaStreamMsg_DeviceStopped(render_frame_id, label, device));
}

void MediaStreamDispatcherHost::DevicesEnumerated(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::DevicesEnumerated("
           << ", {page_request_id = " << page_request_id <<  "})";

  Send(new MediaStreamMsg_DevicesEnumerated(render_frame_id, page_request_id,
                                            devices));
}

void MediaStreamDispatcherHost::DeviceOpened(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::DeviceOpened("
           << ", {page_request_id = " << page_request_id <<  "})";

  Send(new MediaStreamMsg_DeviceOpened(
      render_frame_id, page_request_id, label, video_device));
}

void MediaStreamDispatcherHost::DevicesChanged(MediaStreamType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::DevicesChanged("
           << "{type = " << type << "})";
  for (const auto& subscriber : device_change_subscribers_) {
    std::unique_ptr<MediaStreamUIProxy> ui_proxy = CreateMediaStreamUIProxy();
    ui_proxy->CheckAccess(
        subscriber.security_origin, type, render_process_id_,
        subscriber.render_frame_id,
        base::Bind(&MediaStreamDispatcherHost::HandleCheckAccessResponse,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(std::move(ui_proxy)),
                   subscriber.render_frame_id));
  }
}

bool MediaStreamDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamDispatcherHost, message)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GenerateStream, OnGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CancelGenerateStream,
                        OnCancelGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_StopStreamDevice,
                        OnStopStreamDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_EnumerateDevices,
                        OnEnumerateDevices)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CancelEnumerateDevices,
                        OnCancelEnumerateDevices)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_OpenDevice,
                        OnOpenDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CloseDevice,
                        OnCloseDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_SubscribeToDeviceChangeNotifications,
                        OnSubscribeToDeviceChangeNotifications)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CancelDeviceChangeNotifications,
                        OnCancelDeviceChangeNotifications)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_SetCapturingLinkSecured,
                        OnSetCapturingLinkSecured)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamDispatcherHost::OnChannelClosing() {
  DVLOG(1) << "MediaStreamDispatcherHost::OnChannelClosing";

  // Since the IPC sender is gone, close all requesting/requested streams.
  media_stream_manager_->CancelAllRequests(render_process_id_);
  if (!device_change_subscribers_.empty())
    media_stream_manager_->CancelDeviceChangeNotifications(this);
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
}

void MediaStreamDispatcherHost::OnGenerateStream(
    int render_frame_id,
    int page_request_id,
    const StreamControls& controls,
    const url::Origin& security_origin,
    bool user_gesture) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnGenerateStream(" << render_frame_id
           << ", " << page_request_id << ", ["
           << " audio:" << controls.audio.requested
           << " video:" << controls.video.requested << " ], " << security_origin
           << ", " << user_gesture << ")";

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_, security_origin))
    return;

  media_stream_manager_->GenerateStream(
      this, render_process_id_, render_frame_id, salt_, page_request_id,
      controls, security_origin, user_gesture);
}

void MediaStreamDispatcherHost::OnCancelGenerateStream(int render_frame_id,
                                                       int page_request_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCancelGenerateStream("
           << render_frame_id << ", "
           << page_request_id << ")";
  media_stream_manager_->CancelRequest(render_process_id_, render_frame_id,
                                       page_request_id);
}

void MediaStreamDispatcherHost::OnStopStreamDevice(
    int render_frame_id,
    const std::string& device_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnStopStreamDevice("
           << render_frame_id << ", "
           << device_id << ")";
  media_stream_manager_->StopStreamDevice(render_process_id_, render_frame_id,
                                          device_id);
}

void MediaStreamDispatcherHost::OnEnumerateDevices(
    int render_frame_id,
    int page_request_id,
    MediaStreamType type,
    const url::Origin& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnEnumerateDevices("
           << render_frame_id << ", " << page_request_id << ", " << type << ", "
           << security_origin << ")";

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_, security_origin))
    return;

  media_stream_manager_->EnumerateDevices(
      this, render_process_id_, render_frame_id, salt_, page_request_id, type,
      security_origin);
}

void MediaStreamDispatcherHost::OnCancelEnumerateDevices(
    int render_frame_id,
    int page_request_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCancelEnumerateDevices("
           << render_frame_id << ", "
           << page_request_id << ")";
  media_stream_manager_->CancelRequest(render_process_id_, render_frame_id,
                                       page_request_id);
}

void MediaStreamDispatcherHost::OnOpenDevice(
    int render_frame_id,
    int page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    const url::Origin& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnOpenDevice(" << render_frame_id
           << ", " << page_request_id << ", device_id: " << device_id.c_str()
           << ", type: " << type << ", " << security_origin << ")";

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_, security_origin))
    return;

  media_stream_manager_->OpenDevice(this, render_process_id_, render_frame_id,
                                    salt_, page_request_id, device_id, type,
                                    security_origin);
}

void MediaStreamDispatcherHost::OnCloseDevice(
    int render_frame_id,
    const std::string& label) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCloseDevice("
           << render_frame_id << ", "
           << label << ")";

  media_stream_manager_->CancelRequest(label);
}

void MediaStreamDispatcherHost::OnSubscribeToDeviceChangeNotifications(
    int render_frame_id,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1)
      << "MediaStreamDispatcherHost::OnSubscribeToDeviceChangeNotifications("
      << render_frame_id << ", " << security_origin << ")";
  DCHECK(
      std::find_if(
          device_change_subscribers_.begin(), device_change_subscribers_.end(),
          [render_frame_id](const DeviceChangeSubscriberInfo& subscriber_info) {
            return subscriber_info.render_frame_id == render_frame_id;
          }) == device_change_subscribers_.end());

  if (device_change_subscribers_.empty())
    media_stream_manager_->SubscribeToDeviceChangeNotifications(this);

  device_change_subscribers_.push_back({render_frame_id, security_origin});
}

void MediaStreamDispatcherHost::OnCancelDeviceChangeNotifications(
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "MediaStreamDispatcherHost::CancelDeviceChangeNotifications("
           << render_frame_id << ")";
  auto it = std::find_if(
      device_change_subscribers_.begin(), device_change_subscribers_.end(),
      [render_frame_id](const DeviceChangeSubscriberInfo& subscriber_info) {
        return subscriber_info.render_frame_id == render_frame_id;
      });
  if (it == device_change_subscribers_.end()) {
    bad_message::ReceivedBadMessage(this, bad_message::MSDH_INVALID_FRAME_ID);
    return;
  }
  device_change_subscribers_.erase(it);
  if (device_change_subscribers_.empty())
    media_stream_manager_->CancelDeviceChangeNotifications(this);
}

std::unique_ptr<MediaStreamUIProxy>
MediaStreamDispatcherHost::CreateMediaStreamUIProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (use_fake_ui_ ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
    return base::WrapUnique(new FakeMediaStreamUIProxy());
  }

  return MediaStreamUIProxy::Create();
}

void MediaStreamDispatcherHost::HandleCheckAccessResponse(
    std::unique_ptr<MediaStreamUIProxy> ui_proxy,
    int render_frame_id,
    bool have_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (have_access)
    Send(new MediaStreamMsg_DevicesChanged(render_frame_id));
}

void MediaStreamDispatcherHost::OnSetCapturingLinkSecured(int session_id,
                                                          MediaStreamType type,
                                                          bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturingLinkSecured(render_process_id_, session_id,
                                                 type, is_secure);
}

}  // namespace content
