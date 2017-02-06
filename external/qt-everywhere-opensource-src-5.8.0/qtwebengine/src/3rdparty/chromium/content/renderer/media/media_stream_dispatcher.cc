// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include <stddef.h>

#include "base/logging.h"
#include "content/common/media/media_stream_messages.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_parameters.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "url/origin.h"

namespace content {

namespace {

bool RemoveStreamDeviceFromArray(const StreamDeviceInfo device_info,
                                 StreamDeviceInfoArray* array) {
  for (StreamDeviceInfoArray::iterator device_it = array->begin();
       device_it != array->end(); ++device_it) {
    if (StreamDeviceInfo::IsEqual(*device_it, device_info)) {
      array->erase(device_it);
      return true;
    }
  }
  return false;
}

}  // namespace

// A request is identified by pair (request_id, handler), or ipc_request.
// There could be multiple clients making requests and each has its own
// request_id sequence.
// The ipc_request is garanteed to be unique when it's created in
// MediaStreamDispatcher.
struct MediaStreamDispatcher::Request {
  Request(const base::WeakPtr<MediaStreamDispatcherEventHandler>& handler,
          int request_id,
          int ipc_request)
      : handler(handler),
        request_id(request_id),
        ipc_request(ipc_request) {
  }
  bool IsThisRequest(
      int request_id1,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& handler1) {
    return (request_id1 == request_id && handler1.get() == handler.get());
  }
  base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
  int request_id;
  int ipc_request;
};

struct MediaStreamDispatcher::Stream {
  Stream() {}
  ~Stream() {}
  base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
  StreamDeviceInfoArray audio_array;
  StreamDeviceInfoArray video_array;
};

MediaStreamDispatcher::MediaStreamDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      next_ipc_id_(0) {
}

MediaStreamDispatcher::~MediaStreamDispatcher() {
  DCHECK(device_change_subscribers_.empty());
}

void MediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const StreamControls& controls,
    const url::Origin& security_origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamDispatcher::GenerateStream(" << request_id << ")";

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  Send(new MediaStreamHostMsg_GenerateStream(
      routing_id(), next_ipc_id_++, controls, security_origin,
      blink::WebUserGestureIndicator::isProcessingUserGesture()));
}

void MediaStreamDispatcher::CancelGenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamDispatcher::CancelGenerateStream"
           << ", {request_id = " << request_id << "}";

  RequestList::iterator it = requests_.begin();
  for (; it != requests_.end(); ++it) {
    if (it->IsThisRequest(request_id, event_handler)) {
      int ipc_request = it->ipc_request;
      requests_.erase(it);
      Send(new MediaStreamHostMsg_CancelGenerateStream(routing_id(),
                                                       ipc_request));
      break;
    }
  }
}

void MediaStreamDispatcher::StopStreamDevice(
    const StreamDeviceInfo& device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamDispatcher::StopStreamDevice"
           << ", {device_id = " << device_info.device.id << "}";
  // Remove |device_info| from all streams in |label_stream_map_|.
  bool device_found = false;
  LabelStreamMap::iterator stream_it = label_stream_map_.begin();
  while (stream_it != label_stream_map_.end()) {
    StreamDeviceInfoArray& audio_array = stream_it->second.audio_array;
    StreamDeviceInfoArray& video_array = stream_it->second.video_array;

    if (RemoveStreamDeviceFromArray(device_info, &audio_array) ||
        RemoveStreamDeviceFromArray(device_info, &video_array)) {
      device_found = true;
      if (audio_array.empty() && video_array.empty()) {
        label_stream_map_.erase(stream_it++);
        continue;
      }
    }
    ++stream_it;
  }
  DCHECK(device_found);

  Send(new MediaStreamHostMsg_StopStreamDevice(routing_id(),
                                               device_info.device.id));
}

void MediaStreamDispatcher::EnumerateDevices(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    MediaStreamType type,
    const url::Origin& security_origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE ||
         type == MEDIA_DEVICE_AUDIO_OUTPUT);
  DVLOG(1) << "MediaStreamDispatcher::EnumerateDevices("
           << request_id << ")";

  for (RequestList::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    DCHECK(!it->IsThisRequest(request_id, event_handler));
  }

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  Send(new MediaStreamHostMsg_EnumerateDevices(routing_id(),
                                               next_ipc_id_++,
                                               type,
                                               security_origin));
}

void MediaStreamDispatcher::StopEnumerateDevices(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamDispatcher::StopEnumerateDevices("
           << request_id << ")";
  for (RequestList::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->IsThisRequest(request_id, event_handler)) {
      Send(new MediaStreamHostMsg_CancelEnumerateDevices(routing_id(),
                                                         it->ipc_request));
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OpenDevice(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const std::string& device_id,
    MediaStreamType type,
    const url::Origin& security_origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamDispatcher::OpenDevice(" << request_id << ")";

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  Send(new MediaStreamHostMsg_OpenDevice(routing_id(),
                                         next_ipc_id_++,
                                         device_id,
                                         type,
                                         security_origin));
}

void MediaStreamDispatcher::CancelOpenDevice(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  CancelGenerateStream(request_id, event_handler);
}

void MediaStreamDispatcher::CloseDevice(const std::string& label) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!label.empty());
  DVLOG(1) << "MediaStreamDispatcher::CloseDevice"
           << ", {label = " << label << "}";

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;
  label_stream_map_.erase(it);

  Send(new MediaStreamHostMsg_CloseDevice(routing_id(), label));
}

void MediaStreamDispatcher::SubscribeToDeviceChangeNotifications(
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const url::Origin& security_origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find_if(
             device_change_subscribers_.begin(),
             device_change_subscribers_.end(),
             [&event_handler](
                 const base::WeakPtr<MediaStreamDispatcherEventHandler>& item) {
               return event_handler.get() == item.get();
             }) == device_change_subscribers_.end());
  DVLOG(1) << "MediaStreamDispatcher::SubscribeToDeviceChangeNotifications";

  if (device_change_subscribers_.empty()) {
    Send(new MediaStreamHostMsg_SubscribeToDeviceChangeNotifications(
        routing_id(), security_origin));
  }
  device_change_subscribers_.push_back(event_handler);
}

void MediaStreamDispatcher::CancelDeviceChangeNotifications(
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamDispatcher::SubscribeToDeviceChangeNotifications";
  auto it = std::find_if(
      device_change_subscribers_.begin(), device_change_subscribers_.end(),
      [&event_handler](
          const base::WeakPtr<MediaStreamDispatcherEventHandler>& item) {
        return event_handler.get() == item.get();
      });
  CHECK(it != device_change_subscribers_.end());
  device_change_subscribers_.erase(it);

  if (device_change_subscribers_.empty())
    Send(new MediaStreamHostMsg_CancelDeviceChangeNotifications(routing_id()));
}

void MediaStreamDispatcher::OnDestruct() {
  // Do not self-destruct. UserMediaClientImpl owns |this|.
}

bool MediaStreamDispatcher::Send(IPC::Message* message) {
  if (!RenderThread::Get()) {
    delete message;
    return false;
  }

  return RenderThread::Get()->Send(message);
}

bool MediaStreamDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamDispatcher, message)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerated,
                        OnStreamGenerated)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerationFailed,
                        OnStreamGenerationFailed)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceStopped,
                        OnDeviceStopped)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DevicesEnumerated,
                        OnDevicesEnumerated)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceOpened,
                        OnDeviceOpened)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceOpenFailed,
                        OnDeviceOpenFailed)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DevicesChanged, OnDevicesChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamDispatcher::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      Stream new_stream;
      new_stream.handler = request.handler;
      new_stream.audio_array = audio_array;
      new_stream.video_array = video_array;
      label_stream_map_[label] = new_stream;
      if (request.handler.get()) {
        request.handler->OnStreamGenerated(
            request.request_id, label, audio_array, video_array);
        DVLOG(1) << "MediaStreamDispatcher::OnStreamGenerated("
                 << request.request_id << ", " << label << ")";
      }
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnStreamGenerationFailed(
    int request_id,
    content::MediaStreamRequestResult result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      if (request.handler.get()) {
        request.handler->OnStreamGenerationFailed(request.request_id, result);
        DVLOG(1) << "MediaStreamDispatcher::OnStreamGenerationFailed("
                 << request.request_id << ")\n";
      }
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamDispatcher::OnDeviceStopped("
           << "{label = " << label << "})"
           << ", {device_id = " << device_info.device.id << "})";

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user happen stop a the device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }
  Stream* stream = &it->second;
  if (IsAudioInputMediaType(device_info.device.type))
    RemoveStreamDeviceFromArray(device_info, &stream->audio_array);
  else
    RemoveStreamDeviceFromArray(device_info, &stream->video_array);

  if (stream->handler.get())
    stream->handler->OnDeviceStopped(label, device_info);

  // |it| could have already been invalidated in the function call above. So we
  // need to check if |label| is still in |label_stream_map_| again.
  // Note: this is a quick fix to the crash caused by erasing the invalidated
  // iterator from |label_stream_map_| (crbug.com/616884). Future work needs to
  // be done to resolve this re-entrancy issue.
  it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;
  stream = &it->second;
  if (stream->audio_array.empty() && stream->video_array.empty())
    label_stream_map_.erase(it);
}

void MediaStreamDispatcher::OnDevicesEnumerated(
    int request_id,
    const StreamDeviceInfoArray& device_array) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(request_id, 0);

  for (RequestList::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->ipc_request == request_id && it->handler.get()) {
      it->handler->OnDevicesEnumerated(it->request_id, device_array);
      break;
    }
  }
}

void MediaStreamDispatcher::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      Stream new_stream;
      new_stream.handler = request.handler;
      if (IsAudioInputMediaType(device_info.device.type)) {
        new_stream.audio_array.push_back(device_info);
      } else if (IsVideoMediaType(device_info.device.type)) {
        new_stream.video_array.push_back(device_info);
      } else {
        NOTREACHED();
      }
      label_stream_map_[label] = new_stream;
      if (request.handler.get()) {
        request.handler->OnDeviceOpened(request.request_id, label, device_info);
        DVLOG(1) << "MediaStreamDispatcher::OnDeviceOpened("
                 << request.request_id << ", " << label << ")";
      }
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnDeviceOpenFailed(int request_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      if (request.handler.get()) {
        request.handler->OnDeviceOpenFailed(request.request_id);
        DVLOG(1) << "MediaStreamDispatcher::OnDeviceOpenFailed("
                 << request.request_id << ")\n";
      }
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnDevicesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto event_handler : device_change_subscribers_) {
    DCHECK(event_handler);
    event_handler->OnDevicesChanged();
  }
}

int MediaStreamDispatcher::audio_session_id(const std::string& label,
                                            int index) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() ||
      it->second.audio_array.size() <= static_cast<size_t>(index)) {
    return StreamDeviceInfo::kNoId;
  }
  return it->second.audio_array[index].session_id;
}

bool MediaStreamDispatcher::IsStream(const std::string& label) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return label_stream_map_.find(label) != label_stream_map_.end();
}

int MediaStreamDispatcher::video_session_id(const std::string& label,
                                            int index) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() ||
      it->second.video_array.size() <= static_cast<size_t>(index)) {
    return StreamDeviceInfo::kNoId;
  }
  return it->second.video_array[index].session_id;
}

}  // namespace content
