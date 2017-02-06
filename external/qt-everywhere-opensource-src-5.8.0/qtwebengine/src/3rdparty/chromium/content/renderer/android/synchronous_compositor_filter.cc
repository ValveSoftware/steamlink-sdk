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
#include "ui/events/blink/synchronous_input_handler_proxy.h"

namespace content {

SynchronousCompositorFilter::SynchronousCompositorFilter(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner)
    : compositor_task_runner_(compositor_task_runner), filter_ready_(false) {
  DCHECK(compositor_task_runner_);
}

SynchronousCompositorFilter::~SynchronousCompositorFilter() {}

void SynchronousCompositorFilter::OnFilterAdded(IPC::Sender* sender) {
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  sender_ = sender;
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SynchronousCompositorFilter::FilterReadyyOnCompositorThread,
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

void SynchronousCompositorFilter::FilterReadyyOnCompositorThread() {
  DCHECK(!filter_ready_);
  filter_ready_ = true;
  for (const auto& entry_pair : entry_map_) {
    CheckIsReady(entry_pair.first);
  }
}

void SynchronousCompositorFilter::RegisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(output_surface);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.output_surface);
  entry.output_surface = output_surface;

  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  if (proxy) {
    proxy->SetOutputSurface(output_surface);
  }
}

void SynchronousCompositorFilter::UnregisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(output_surface);
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];
  DCHECK_EQ(output_surface, entry.output_surface);

  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  if (proxy) {
    proxy->SetOutputSurface(nullptr);
  }
  entry.output_surface = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorFilter::CheckIsReady(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];
  if (filter_ready_ && entry.IsReady()) {
    DCHECK(!sync_compositor_map_.contains(routing_id));
    std::unique_ptr<SynchronousCompositorProxy> proxy(
        new SynchronousCompositorProxy(routing_id, this,
                                       entry.synchronous_input_handler_proxy));
    if (entry.output_surface)
      proxy->SetOutputSurface(entry.output_surface);
    sync_compositor_map_.add(routing_id, std::move(proxy));
  }
}

void SynchronousCompositorFilter::UnregisterObjects(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(sync_compositor_map_.contains(routing_id));
  sync_compositor_map_.erase(routing_id);
}

void SynchronousCompositorFilter::RemoveEntryIfNeeded(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];
  if (!entry.output_surface && !entry.synchronous_input_handler_proxy) {
    entry_map_.erase(routing_id);
  }
}

void SynchronousCompositorFilter::DidAddSynchronousHandlerProxy(
    int routing_id,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(synchronous_input_handler_proxy);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.synchronous_input_handler_proxy);
  entry.synchronous_input_handler_proxy = synchronous_input_handler_proxy;
  CheckIsReady(routing_id);
}

void SynchronousCompositorFilter::DidRemoveSynchronousHandlerProxy(
    int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.synchronous_input_handler_proxy = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

SynchronousCompositorFilter::Entry::Entry()
    : output_surface(nullptr),
      synchronous_input_handler_proxy(nullptr) {}

// TODO(boliu): refactor this
bool SynchronousCompositorFilter::Entry::IsReady() {
  return synchronous_input_handler_proxy;
}

}  // namespace content
