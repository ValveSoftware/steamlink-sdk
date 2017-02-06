// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/media_service.h"

#include <memory>
#include <utility>

#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_channel.h"

namespace media {

MediaService::MediaService(gpu::GpuChannelManager* channel_manager)
    : channel_manager_(channel_manager) {}

MediaService::~MediaService() {}

void MediaService::AddChannel(int32_t client_id) {
  gpu::GpuChannel* gpu_channel = channel_manager_->LookupChannel(client_id);
  DCHECK(gpu_channel);
  std::unique_ptr<MediaChannel> media_channel(new MediaChannel(gpu_channel));
  gpu_channel->SetUnhandledMessageListener(media_channel.get());
  media_channels_.set(client_id, std::move(media_channel));
}

void MediaService::RemoveChannel(int32_t client_id) {
  media_channels_.erase(client_id);
}

void MediaService::DestroyAllChannels() {
  media_channels_.clear();
}

}  // namespace media
