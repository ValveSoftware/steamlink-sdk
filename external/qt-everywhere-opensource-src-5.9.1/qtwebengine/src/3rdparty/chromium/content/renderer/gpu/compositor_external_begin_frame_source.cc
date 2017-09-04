// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_external_begin_frame_source.h"

#include "content/common/view_messages.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"

namespace content {

CompositorExternalBeginFrameSource::CompositorExternalBeginFrameSource(
    CompositorForwardingMessageFilter* filter,
    IPC::SyncMessageFilter* sync_message_filter,
    int routing_id)
    : external_begin_frame_source_(this),
      begin_frame_source_filter_(filter),
      message_sender_(sync_message_filter),
      routing_id_(routing_id) {
  DCHECK(begin_frame_source_filter_);
  DCHECK(message_sender_);
  DetachFromThread();
}

CompositorExternalBeginFrameSource::~CompositorExternalBeginFrameSource() {
  DCHECK(CalledOnValidThread());
  if (begin_frame_source_proxy_) {
    begin_frame_source_proxy_->ClearBeginFrameSource();
    begin_frame_source_filter_->RemoveHandlerOnCompositorThread(
                                    routing_id_,
                                    begin_frame_source_filter_handler_);
  }
}

void CompositorExternalBeginFrameSource::AddObserver(
    cc::BeginFrameObserver* obs) {
  DCHECK(CalledOnValidThread());

  if (!begin_frame_source_proxy_) {
    begin_frame_source_proxy_ =
        new CompositorExternalBeginFrameSourceProxy(this);
    begin_frame_source_filter_handler_ =
        base::Bind(&CompositorExternalBeginFrameSourceProxy::OnMessageReceived,
                   begin_frame_source_proxy_);
    begin_frame_source_filter_->AddHandlerOnCompositorThread(
        routing_id_, begin_frame_source_filter_handler_);
  }

  external_begin_frame_source_.AddObserver(obs);
}

void CompositorExternalBeginFrameSource::RemoveObserver(
    cc::BeginFrameObserver* obs) {
  external_begin_frame_source_.RemoveObserver(obs);
}

bool CompositorExternalBeginFrameSource::IsThrottled() const {
  return true;
}

void CompositorExternalBeginFrameSource::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  Send(new ViewHostMsg_SetNeedsBeginFrames(routing_id_, needs_begin_frames));
}

void CompositorExternalBeginFrameSource::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  DCHECK(begin_frame_source_proxy_);
  IPC_BEGIN_MESSAGE_MAP(CompositorExternalBeginFrameSource, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBeginFramePaused,
                        OnSetBeginFrameSourcePaused)
    IPC_MESSAGE_HANDLER(ViewMsg_BeginFrame, OnBeginFrame)
  IPC_END_MESSAGE_MAP()
}

void CompositorExternalBeginFrameSource::OnSetBeginFrameSourcePaused(
    bool paused) {
  external_begin_frame_source_.OnSetBeginFrameSourcePaused(paused);
}

void CompositorExternalBeginFrameSource::OnBeginFrame(
    const cc::BeginFrameArgs& args) {
  external_begin_frame_source_.OnBeginFrame(args);
}

bool CompositorExternalBeginFrameSource::Send(IPC::Message* message) {
  return message_sender_->Send(message);
}

}  // namespace content
