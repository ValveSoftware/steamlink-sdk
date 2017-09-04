// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_filter.h"

#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/common/input_messages.h"
#include "content/renderer/android/synchronous_compositor_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "ui/events/blink/synchronous_input_handler_proxy.h"

namespace content {

SynchronousCompositorFilter::SynchronousCompositorFilter(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner)
    : compositor_task_runner_(compositor_task_runner), filter_ready_(false) {
  DCHECK(compositor_task_runner_);
}

SynchronousCompositorFilter::~SynchronousCompositorFilter() {}

void SynchronousCompositorFilter::OnFilterAdded(IPC::Channel* channel) {
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  sender_ = channel;
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SynchronousCompositorFilter::FilterReadyOnCompositorThread,
                 this));
}

void SynchronousCompositorFilter::OnFilterRemoved() {
  sender_ = nullptr;
}

void SynchronousCompositorFilter::OnChannelClosing() {
  sender_ = nullptr;
}

bool SynchronousCompositorFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_EQ(SyncCompositorMsgStart, IPC_MESSAGE_ID_CLASS(message.type()));
  bool result = compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &SynchronousCompositorFilter::OnMessageReceivedOnCompositorThread,
          this, message));
  if (!result && message.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    SendOnIOThread(reply);
  }
  return result;
}

SynchronousCompositorProxy* SynchronousCompositorFilter::FindProxy(
    int routing_id) {
  auto itr = sync_compositor_map_.find(routing_id);
  if (itr == sync_compositor_map_.end()) {
    return nullptr;
  }
  return itr->second;
}

bool SynchronousCompositorFilter::GetSupportedMessageClasses(
    std::vector<uint32_t>* supported_message_classes) const {
  supported_message_classes->push_back(SyncCompositorMsgStart);
  return true;
}

void SynchronousCompositorFilter::OnMessageReceivedOnCompositorThread(
    const IPC::Message& message) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());

  SynchronousCompositorProxy* proxy = FindProxy(message.routing_id());
  if (proxy) {
    proxy->OnMessageReceived(message);
    return;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorFilter, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SynchronizeRendererState,
                        OnSynchronizeRendererState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return;

  if (!message.is_sync())
    return;
  IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
  reply->set_reply_error();
  Send(reply);
}

bool SynchronousCompositorFilter::Send(IPC::Message* message) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(filter_ready_);
  if (!io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&SynchronousCompositorFilter::SendOnIOThread,
                                this, message))) {
    delete message;
    DLOG(WARNING) << "IO PostTask failed";
    return false;
  }
  return true;
}

void SynchronousCompositorFilter::SendOnIOThread(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(sender_);
  if (!sender_) {
    delete message;
    return;
  }
  bool result = sender_->Send(message);
  if (!result)
    DLOG(WARNING) << "Failed to send message";
}

void SynchronousCompositorFilter::FilterReadyOnCompositorThread() {
  DCHECK(!filter_ready_);
  filter_ready_ = true;
  for (const auto& entry_pair : synchronous_input_handler_proxy_map_) {
    DCHECK(entry_pair.second);
    int routing_id = entry_pair.first;
    CreateSynchronousCompositorProxy(routing_id, entry_pair.second);
    auto compositor_frame_sink_entry =
        compositor_frame_sink_map_.find(routing_id);
    if (compositor_frame_sink_entry != compositor_frame_sink_map_.end()) {
      SetProxyCompositorFrameSink(routing_id,
                                  compositor_frame_sink_entry->second);
    }
  }
}

void SynchronousCompositorFilter::RegisterCompositorFrameSink(
    int routing_id,
    SynchronousCompositorFrameSink* compositor_frame_sink) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(compositor_frame_sink);
  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  if (proxy) {
    proxy->SetCompositorFrameSink(compositor_frame_sink);
  } else {
    DCHECK(compositor_frame_sink_map_.find(routing_id) ==
           compositor_frame_sink_map_.end());
    compositor_frame_sink_map_[routing_id] = compositor_frame_sink;
  }
}

void SynchronousCompositorFilter::OnSynchronizeRendererState(
    const std::vector<int>& routing_ids,
    std::vector<SyncCompositorCommonRendererParams>* out) {
  for (int routing_id : routing_ids) {
    SynchronousCompositorProxy* proxy = FindProxy(routing_id);
    SyncCompositorCommonRendererParams param;
    if (proxy)
      proxy->PopulateCommonParams(&param);
    out->push_back(param);
  }
}

void SynchronousCompositorFilter::UnregisterCompositorFrameSink(
    int routing_id,
    SynchronousCompositorFrameSink* compositor_frame_sink) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(compositor_frame_sink);
  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  if (proxy) {
    proxy->SetCompositorFrameSink(nullptr);
  }
  auto entry = compositor_frame_sink_map_.find(routing_id);
  if (entry != compositor_frame_sink_map_.end())
    compositor_frame_sink_map_.erase(entry);
}

void SynchronousCompositorFilter::CreateSynchronousCompositorProxy(
    int routing_id,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(!sync_compositor_map_.contains(routing_id));
  std::unique_ptr<SynchronousCompositorProxy> proxy(
      new SynchronousCompositorProxy(routing_id, this,
                                     synchronous_input_handler_proxy));
  sync_compositor_map_.add(routing_id, std::move(proxy));
}

void SynchronousCompositorFilter::SetProxyCompositorFrameSink(
    int routing_id,
    SynchronousCompositorFrameSink* compositor_frame_sink) {
  DCHECK(compositor_frame_sink);
  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  DCHECK(proxy);
  proxy->SetCompositorFrameSink(compositor_frame_sink);
}

void SynchronousCompositorFilter::DidAddSynchronousHandlerProxy(
    int routing_id,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(synchronous_input_handler_proxy);
  if (filter_ready_) {
    CreateSynchronousCompositorProxy(routing_id,
                                     synchronous_input_handler_proxy);
    auto entry = compositor_frame_sink_map_.find(routing_id);
    if (entry != compositor_frame_sink_map_.end())
      SetProxyCompositorFrameSink(routing_id, entry->second);
  } else {
    auto*& mapped_synchronous_input_handler_proxy =
        synchronous_input_handler_proxy_map_[routing_id];
    DCHECK(!mapped_synchronous_input_handler_proxy);
    mapped_synchronous_input_handler_proxy = synchronous_input_handler_proxy;
  }
}

void SynchronousCompositorFilter::DidRemoveSynchronousHandlerProxy(
    int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (base::ContainsKey(sync_compositor_map_, routing_id)) {
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    DCHECK(sync_compositor_map_.contains(routing_id));
    sync_compositor_map_.erase(routing_id);
  }
  if (base::ContainsKey(synchronous_input_handler_proxy_map_, routing_id))
    synchronous_input_handler_proxy_map_.erase(routing_id);
}

}  // namespace content
