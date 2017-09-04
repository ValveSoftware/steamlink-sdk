// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/renderer_compositor_frame_sink.h"

#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/output/managed_memory_policy.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_sync_channel.h"

namespace content {

RendererCompositorFrameSink::RendererCompositorFrameSink(
    int32_t routing_id,
    uint32_t compositor_frame_sink_id,
    std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    cc::SharedBitmapManager* shared_bitmap_manager,
    scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue)
    : CompositorFrameSink(std::move(context_provider),
                          std::move(worker_context_provider),
                          gpu_memory_buffer_manager,
                          shared_bitmap_manager),
      compositor_frame_sink_id_(compositor_frame_sink_id),
      compositor_frame_sink_filter_(
          RenderThreadImpl::current()->compositor_message_filter()),
      message_sender_(RenderThreadImpl::current()->sync_message_filter()),
      frame_swap_message_queue_(swap_frame_message_queue),
      begin_frame_source_(std::move(begin_frame_source)),
      routing_id_(routing_id) {
  DCHECK(compositor_frame_sink_filter_);
  DCHECK(frame_swap_message_queue_);
  DCHECK(message_sender_);
  DCHECK(begin_frame_source_);
}

RendererCompositorFrameSink::RendererCompositorFrameSink(
    int32_t routing_id,
    uint32_t compositor_frame_sink_id,
    std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
    scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
    scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue)
    : CompositorFrameSink(std::move(vulkan_context_provider)),
      compositor_frame_sink_id_(compositor_frame_sink_id),
      compositor_frame_sink_filter_(
          RenderThreadImpl::current()->compositor_message_filter()),
      message_sender_(RenderThreadImpl::current()->sync_message_filter()),
      frame_swap_message_queue_(swap_frame_message_queue),
      begin_frame_source_(std::move(begin_frame_source)),
      routing_id_(routing_id) {
  DCHECK(compositor_frame_sink_filter_);
  DCHECK(frame_swap_message_queue_);
  DCHECK(message_sender_);
  DCHECK(begin_frame_source_);
}

RendererCompositorFrameSink::~RendererCompositorFrameSink() = default;

bool RendererCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* client) {
  if (!cc::CompositorFrameSink::BindToClient(client))
    return false;

  DCHECK(begin_frame_source_);
  client_->SetBeginFrameSource(begin_frame_source_.get());

  compositor_frame_sink_proxy_ = new RendererCompositorFrameSinkProxy(this);
  compositor_frame_sink_filter_handler_ =
      base::Bind(&RendererCompositorFrameSinkProxy::OnMessageReceived,
                 compositor_frame_sink_proxy_);
  compositor_frame_sink_filter_->AddHandlerOnCompositorThread(
      routing_id_, compositor_frame_sink_filter_handler_);
  return true;
}

void RendererCompositorFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  // Destroy the begin frame source on the same thread it was bound on.
  // The CompositorFrameSink itself is destroyed on the main thread.
  begin_frame_source_ = nullptr;
  compositor_frame_sink_proxy_->ClearCompositorFrameSink();
  compositor_frame_sink_filter_->RemoveHandlerOnCompositorThread(
      routing_id_, compositor_frame_sink_filter_handler_);

  cc::CompositorFrameSink::DetachFromClient();
}

void RendererCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  {
    std::unique_ptr<FrameSwapMessageQueue::SendMessageScope>
        send_message_scope =
            frame_swap_message_queue_->AcquireSendMessageScope();
    std::vector<std::unique_ptr<IPC::Message>> messages;
    std::vector<IPC::Message> messages_to_deliver_with_frame;
    frame_swap_message_queue_->DrainMessages(&messages);
    FrameSwapMessageQueue::TransferMessages(&messages,
                                            &messages_to_deliver_with_frame);
    Send(new ViewHostMsg_SwapCompositorFrame(routing_id_,
                                             compositor_frame_sink_id_, frame,
                                             messages_to_deliver_with_frame));
    // ~send_message_scope.
  }
}

void RendererCompositorFrameSink::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  IPC_BEGIN_MESSAGE_MAP(RendererCompositorFrameSink, message)
    IPC_MESSAGE_HANDLER(ViewMsg_ReclaimCompositorResources,
                        OnReclaimCompositorResources);
  IPC_END_MESSAGE_MAP()
}

void RendererCompositorFrameSink::OnReclaimCompositorResources(
    uint32_t compositor_frame_sink_id,
    bool is_swap_ack,
    const cc::ReturnedResourceArray& resources) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (compositor_frame_sink_id != compositor_frame_sink_id_)
    return;
  client_->ReclaimResources(resources);
  if (is_swap_ack)
    client_->DidReceiveCompositorFrameAck();
}

bool RendererCompositorFrameSink::Send(IPC::Message* message) {
  return message_sender_->Send(message);
}

}  // namespace content
