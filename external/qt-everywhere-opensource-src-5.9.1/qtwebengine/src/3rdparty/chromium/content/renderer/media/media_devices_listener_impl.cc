// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_devices_listener_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/media_devices_event_dispatcher.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// static
void MediaDevicesListenerImpl::Create(
    int render_frame_id,
    ::mojom::MediaDevicesListenerRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<MediaDevicesListenerImpl>(render_frame_id),
      std::move(request));
}

MediaDevicesListenerImpl::MediaDevicesListenerImpl(int render_frame_id)
    : render_frame_id_(render_frame_id) {}

MediaDevicesListenerImpl::~MediaDevicesListenerImpl() = default;

void MediaDevicesListenerImpl::OnDevicesChanged(
    MediaDeviceType type,
    uint32_t subscription_id,
    const MediaDeviceInfoArray& device_infos) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
      MediaDevicesEventDispatcher::GetForRenderFrame(
          RenderFrame::FromRoutingID(render_frame_id_));
  if (event_dispatcher)
    event_dispatcher->DispatchDevicesChangedEvent(type, device_infos);
}

}  // namespace content
