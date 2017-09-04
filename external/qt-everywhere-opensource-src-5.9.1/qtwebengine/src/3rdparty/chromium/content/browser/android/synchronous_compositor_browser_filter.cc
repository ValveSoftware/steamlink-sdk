// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_browser_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/bad_message.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ui/android/window_android.h"

namespace content {

SynchronousCompositorBrowserFilter::SynchronousCompositorBrowserFilter(
    int process_id)
    : BrowserMessageFilter(SyncCompositorMsgStart),
      render_process_host_(RenderProcessHost::FromID(process_id)) {
  DCHECK(render_process_host_);
}

SynchronousCompositorBrowserFilter::~SynchronousCompositorBrowserFilter() {
  DCHECK(compositor_host_pending_renderer_state_.empty());
  DCHECK(future_map_.empty());
}

void SynchronousCompositorBrowserFilter::SyncStateAfterVSync(
    ui::WindowAndroid* window_android,
    SynchronousCompositorHost* compositor_host) {
  DCHECK(!window_android_in_vsync_ ||
         window_android_in_vsync_ == window_android)
      << !!window_android_in_vsync_;
  DCHECK(compositor_host);
  DCHECK(!base::ContainsValue(compositor_host_pending_renderer_state_,
                              compositor_host));
  compositor_host_pending_renderer_state_.push_back(compositor_host);
  if (window_android_in_vsync_)
    return;
  window_android_in_vsync_ = window_android;
  window_android_in_vsync_->AddObserver(this);
}

bool SynchronousCompositorBrowserFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorBrowserFilter, message)
    IPC_MESSAGE_HANDLER_GENERIC(SyncCompositorHostMsg_ReturnFrame,
                                ReceiveFrame(message))
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SynchronousCompositorBrowserFilter::ReceiveFrame(
    const IPC::Message& message) {
  SyncCompositorHostMsg_ReturnFrame::Param param;
  if (!SyncCompositorHostMsg_ReturnFrame::Read(&message, &param))
    return false;

  int routing_id = message.routing_id();
  scoped_refptr<SynchronousCompositor::FrameFuture> future;
  {
    base::AutoLock lock(future_map_lock_);
    auto itr = future_map_.find(routing_id);
    if (itr == future_map_.end() || itr->second.empty()) {
      bad_message::ReceivedBadMessage(render_process_host_,
                                      bad_message::SCO_INVALID_ARGUMENT);
      return true;
    }
    future = std::move(itr->second.front());
    DCHECK(future);
    itr->second.pop_front();
    if (itr->second.empty())
      future_map_.erase(itr);
  }

  auto frame_ptr = base::MakeUnique<SynchronousCompositor::Frame>();
  frame_ptr->compositor_frame_sink_id = std::get<0>(param);
  base::Optional<cc::CompositorFrame>& compositor_frame = std::get<1>(param);
  if (compositor_frame) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &SynchronousCompositorBrowserFilter::ProcessFrameMetadataOnUIThread,
            this, routing_id,
            base::Passed(compositor_frame->metadata.Clone())));
    frame_ptr->frame.reset(new cc::CompositorFrame);
    *frame_ptr->frame = std::move(*compositor_frame);
  }
  future->SetFrame(std::move(frame_ptr));
  return true;
}

void SynchronousCompositorBrowserFilter::ProcessFrameMetadataOnUIThread(
    int routing_id,
    cc::CompositorFrameMetadata metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto itr = hosts_.find(routing_id);
  if (itr == hosts_.end())
    return;
  itr->second->UpdateFrameMetaData(std::move(metadata));
}

void SynchronousCompositorBrowserFilter::RegisterHost(
    SynchronousCompositorHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);
  DCHECK(!base::ContainsKey(hosts_, host->routing_id()));
  hosts_[host->routing_id()] = host;
}

void SynchronousCompositorBrowserFilter::UnregisterHost(
    SynchronousCompositorHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);
  int routing_id = host->routing_id();
  DCHECK(base::ContainsKey(hosts_, routing_id));
  DCHECK_EQ(host, hosts_[routing_id]);
  hosts_.erase(routing_id);
}

void SynchronousCompositorBrowserFilter::SetFrameFuture(
    int routing_id,
    scoped_refptr<SynchronousCompositor::FrameFuture> frame_future) {
  DCHECK(frame_future);
  base::AutoLock lock(future_map_lock_);
  if (!filter_ready_) {
    frame_future->SetFrame(nullptr);
    return;
  }

  auto itr = future_map_.find(routing_id);
  if (itr == future_map_.end()) {
    auto emplace_result = future_map_.emplace(routing_id, FrameFutureQueue());
    DCHECK(emplace_result.second);
    itr = emplace_result.first;
  }

  // Allowing arbitrary number of pending futures can lead to increase in frame
  // latency. Due to this, Android platform already ensures that here that there
  // can be at most 2 pending frames. Here, we rely on Android to do the
  // necessary blocking, which allows more parallelism without increasing
  // latency. But DCHECK Android blocking is working.
  DCHECK_LT(itr->second.size(), 2u);
  itr->second.emplace_back(std::move(frame_future));
}

void SynchronousCompositorBrowserFilter::OnFilterAdded(IPC::Channel* channel) {
  base::AutoLock lock(future_map_lock_);
  filter_ready_ = true;
}

void SynchronousCompositorBrowserFilter::OnFilterRemoved() {
  SignalAllFutures();
}

void SynchronousCompositorBrowserFilter::OnChannelClosing() {
  SignalAllFutures();
}

void SynchronousCompositorBrowserFilter::SignalAllFutures() {
  base::AutoLock lock(future_map_lock_);
  for (auto& pair : future_map_) {
    for (auto& future_ptr : pair.second) {
      future_ptr->SetFrame(nullptr);
    }
  }
  future_map_.clear();
  filter_ready_ = false;
}

void SynchronousCompositorBrowserFilter::OnCompositingDidCommit() {
  NOTREACHED();
}

void SynchronousCompositorBrowserFilter::OnRootWindowVisibilityChanged(
    bool visible) {
  NOTREACHED();
}

void SynchronousCompositorBrowserFilter::OnAttachCompositor() {
  NOTREACHED();
}

void SynchronousCompositorBrowserFilter::OnDetachCompositor() {
  NOTREACHED();
}

void SynchronousCompositorBrowserFilter::OnVSync(base::TimeTicks frame_time,
                                                 base::TimeDelta vsync_period) {
  // This is called after DidSendBeginFrame for SynchronousCompositorHosts
  // belonging to this WindowAndroid, since this is added as an Observer after
  // the observer iteration has started.
  DCHECK(window_android_in_vsync_);
  window_android_in_vsync_->RemoveObserver(this);
  window_android_in_vsync_ = nullptr;

  std::vector<int> routing_ids;
  routing_ids.reserve(compositor_host_pending_renderer_state_.size());
  for (const auto host : compositor_host_pending_renderer_state_)
    routing_ids.push_back(host->routing_id());

  std::vector<SyncCompositorCommonRendererParams> params;
  params.reserve(compositor_host_pending_renderer_state_.size());

  if (!render_process_host_->Send(
          new SyncCompositorMsg_SynchronizeRendererState(routing_ids,
                                                         &params))) {
    return;
  }

  if (compositor_host_pending_renderer_state_.size() != params.size()) {
    bad_message::ReceivedBadMessage(render_process_host_,
                                    bad_message::SCO_INVALID_ARGUMENT);
    return;
  }

  for (size_t i = 0; i < compositor_host_pending_renderer_state_.size(); ++i) {
    compositor_host_pending_renderer_state_[i]->ProcessCommonParams(params[i]);
  }
  compositor_host_pending_renderer_state_.clear();
}

void SynchronousCompositorBrowserFilter::OnActivityStopped() {
  NOTREACHED();
}

void SynchronousCompositorBrowserFilter::OnActivityStarted() {
  NOTREACHED();
}

}  // namespace content
