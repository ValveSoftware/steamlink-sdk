// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/stream_texture_host_android.h"

#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_message_macros.h"

namespace content {

StreamTextureHost::StreamTextureHost(GpuChannelHost* channel)
    : stream_id_(0),
      listener_(NULL),
      channel_(channel),
      weak_ptr_factory_(this) {
  DCHECK(channel);
}

StreamTextureHost::~StreamTextureHost() {
  if (channel_.get() && stream_id_)
    channel_->RemoveRoute(stream_id_);
}

bool StreamTextureHost::BindToCurrentThread(int32 stream_id,
                                            Listener* listener) {
  listener_ = listener;
  if (channel_.get() && stream_id && !stream_id_) {
    stream_id_ = stream_id;
    channel_->AddRoute(stream_id, weak_ptr_factory_.GetWeakPtr());
    channel_->Send(new GpuStreamTextureMsg_StartListening(stream_id));
    return true;
  }

  return false;
}

bool StreamTextureHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(StreamTextureHost, message)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_FrameAvailable,
                        OnFrameAvailable);
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_MatrixChanged,
                        OnMatrixChanged);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void StreamTextureHost::OnChannelError() {
}

void StreamTextureHost::OnFrameAvailable() {
  if (listener_)
    listener_->OnFrameAvailable();
}

void StreamTextureHost::OnMatrixChanged(
    const GpuStreamTextureMsg_MatrixChanged_Params& params) {
  COMPILE_ASSERT(sizeof(params) == sizeof(float) * 16,
                 bad_GpuStreamTextureMsg_MatrixChanged_Params_format);
  if (listener_)
    listener_->OnMatrixChanged((const float*)&params);
}

}  // namespace content
