// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/media_channel_proxy.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/common/media/cma_messages.h"

namespace chromecast {
namespace media {

MediaChannelProxy::MediaChannelProxy()
    : is_open_(false),
      id_(0) {
  filter_ = CmaMessageFilterProxy::Get();
  DCHECK(filter_.get());
}

MediaChannelProxy::~MediaChannelProxy() {
}

void MediaChannelProxy::Open(LoadType load_type) {
  CHECK(!is_open_);
  // Renderer side.
  id_ = filter_->CreateChannel();
  is_open_ = true;

  // Browser side.
  bool success = Send(std::unique_ptr<IPC::Message>(
      new CmaHostMsg_CreateMedia(id_, load_type)));
  if (!success) {
    is_open_ = false;
    id_ = 0;
  }
}

void MediaChannelProxy::Close() {
  if (!is_open_)
    return;

  // Browser side.
  Send(std::unique_ptr<IPC::Message>(new CmaHostMsg_DestroyMedia(id_)));

  // Renderer side.
  is_open_ = false;
  filter_->DestroyChannel(id_);
  id_ = 0;
}

bool MediaChannelProxy::SetMediaDelegate(
    const CmaMessageFilterProxy::MediaDelegate& media_delegate) {
  if (!is_open_)
    return false;
  return filter_->SetMediaDelegate(id_, media_delegate);
}

bool MediaChannelProxy::SetAudioDelegate(
    const CmaMessageFilterProxy::AudioDelegate& audio_delegate) {
  if (!is_open_)
    return false;
  return filter_->SetAudioDelegate(id_, audio_delegate);
}

bool MediaChannelProxy::SetVideoDelegate(
    const CmaMessageFilterProxy::VideoDelegate& video_delegate) {
  if (!is_open_)
    return false;
  return filter_->SetVideoDelegate(id_, video_delegate);
}

bool MediaChannelProxy::Send(std::unique_ptr<IPC::Message> message) {
  if (!is_open_)
    return false;
  return filter_->Send(std::move(message));
}

}  // namespace media
}  // namespace chromecast
