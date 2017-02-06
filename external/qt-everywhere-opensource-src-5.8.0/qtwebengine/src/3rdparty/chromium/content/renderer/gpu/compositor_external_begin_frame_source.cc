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
    : begin_frame_source_filter_(filter),
      message_sender_(sync_message_filter),
      routing_id_(routing_id) {
  DCHECK(begin_frame_source_filter_.get());
  DCHECK(message_sender_.get());
  DetachFromThread();
}

CompositorExternalBeginFrameSource::~CompositorExternalBeginFrameSource() {
  DCHECK(CalledOnValidThread());
  if (begin_frame_source_proxy_.get()) {
    begin_frame_source_proxy_->ClearBeginFrameSource();
    begin_frame_source_filter_->RemoveHandlerOnCompositorThread(
                                    routing_id_,
                                    begin_frame_source_filter_handler_);
  }
}

void CompositorExternalBeginFrameSource::AddObserver(
    cc::BeginFrameObserver* obs) {
  DCHECK(CalledOnValidThread());
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  SetClientReady();
  bool observers_was_empty = observers_.empty();
  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(paused_);
  if (observers_was_empty)
    Send(new ViewHostMsg_SetNeedsBeginFrames(routing_id_, true));

  // Send a MISSED begin frame if necessary.
  if (missed_begin_frame_args_.IsValid()) {
    cc::BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        (missed_begin_frame_args_.frame_time > last_args.frame_time)) {
      obs->OnBeginFrame(missed_begin_frame_args_);
    }
  }
}

void CompositorExternalBeginFrameSource::RemoveObserver(
    cc::BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());

  observers_.erase(obs);
  if (observers_.empty()) {
    missed_begin_frame_args_ = cc::BeginFrameArgs();
    Send(new ViewHostMsg_SetNeedsBeginFrames(routing_id_, false));
  }
}

void CompositorExternalBeginFrameSource::SetClientReady() {
  DCHECK(CalledOnValidThread());
  if (begin_frame_source_proxy_)
    return;
  begin_frame_source_proxy_ =
      new CompositorExternalBeginFrameSourceProxy(this);
  begin_frame_source_filter_handler_ = base::Bind(
      &CompositorExternalBeginFrameSourceProxy::OnMessageReceived,
      begin_frame_source_proxy_);
  begin_frame_source_filter_->AddHandlerOnCompositorThread(
      routing_id_,
      begin_frame_source_filter_handler_);
}

void CompositorExternalBeginFrameSource::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  DCHECK(begin_frame_source_proxy_.get());
  IPC_BEGIN_MESSAGE_MAP(CompositorExternalBeginFrameSource, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBeginFramePaused,
                        OnSetBeginFrameSourcePaused)
    IPC_MESSAGE_HANDLER(ViewMsg_BeginFrame, OnBeginFrame)
  IPC_END_MESSAGE_MAP()
}

void CompositorExternalBeginFrameSource::OnSetBeginFrameSourcePaused(
    bool paused) {
  if (paused_ == paused)
    return;
  paused_ = paused;
  std::unordered_set<cc::BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers)
    obs->OnBeginFrameSourcePausedChanged(paused_);
}

void CompositorExternalBeginFrameSource::OnBeginFrame(
  const cc::BeginFrameArgs& args) {
  DCHECK(CalledOnValidThread());
  missed_begin_frame_args_ = args;
  missed_begin_frame_args_.type = cc::BeginFrameArgs::MISSED;
  std::unordered_set<cc::BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers)
    obs->OnBeginFrame(args);
}

bool CompositorExternalBeginFrameSource::Send(IPC::Message* message) {
  return message_sender_->Send(message);
}

}  // namespace content
