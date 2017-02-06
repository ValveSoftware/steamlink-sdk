// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_output_surface.h"

#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
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

CompositorOutputSurface::CompositorOutputSurface(
    int32_t routing_id,
    uint32_t output_surface_id,
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue)
    : OutputSurface(std::move(context_provider),
                    std::move(worker_context_provider),
                    nullptr),
      output_surface_id_(output_surface_id),
      output_surface_filter_(
          RenderThreadImpl::current()->compositor_message_filter()),
      message_sender_(RenderThreadImpl::current()->sync_message_filter()),
      frame_swap_message_queue_(swap_frame_message_queue),
      routing_id_(routing_id) {
  DCHECK(output_surface_filter_);
  DCHECK(frame_swap_message_queue_);
  DCHECK(message_sender_);
  capabilities_.delegated_rendering = true;
}

CompositorOutputSurface::CompositorOutputSurface(
    int32_t routing_id,
    uint32_t output_surface_id,
    scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
    scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue)
    : OutputSurface(std::move(vulkan_context_provider)),
      output_surface_id_(output_surface_id),
      output_surface_filter_(
          RenderThreadImpl::current()->compositor_message_filter()),
      message_sender_(RenderThreadImpl::current()->sync_message_filter()),
      frame_swap_message_queue_(swap_frame_message_queue),
      routing_id_(routing_id) {
  DCHECK(output_surface_filter_);
  DCHECK(frame_swap_message_queue_);
  DCHECK(message_sender_);
  capabilities_.delegated_rendering = true;
}

CompositorOutputSurface::~CompositorOutputSurface() = default;

bool CompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  if (!cc::OutputSurface::BindToClient(client))
    return false;

  output_surface_proxy_ = new CompositorOutputSurfaceProxy(this);
  output_surface_filter_handler_ =
      base::Bind(&CompositorOutputSurfaceProxy::OnMessageReceived,
                 output_surface_proxy_);
  output_surface_filter_->AddHandlerOnCompositorThread(
                              routing_id_,
                              output_surface_filter_handler_);

  if (!context_provider()) {
    // Without a GPU context, the memory policy otherwise wouldn't be set.
    client->SetMemoryPolicy(cc::ManagedMemoryPolicy(
        128 * 1024 * 1024,
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
        base::SharedMemory::GetHandleLimit() / 3));
  }

  return true;
}

void CompositorOutputSurface::DetachFromClient() {
  if (!HasClient())
    return;
  if (output_surface_proxy_) {
    output_surface_proxy_->ClearOutputSurface();
    output_surface_filter_->RemoveHandlerOnCompositorThread(
        routing_id_, output_surface_filter_handler_);
  }
  cc::OutputSurface::DetachFromClient();
}

void CompositorOutputSurface::SwapBuffers(cc::CompositorFrame frame) {
  {
    std::unique_ptr<FrameSwapMessageQueue::SendMessageScope>
        send_message_scope =
            frame_swap_message_queue_->AcquireSendMessageScope();
    std::vector<std::unique_ptr<IPC::Message>> messages;
    std::vector<IPC::Message> messages_to_deliver_with_frame;
    frame_swap_message_queue_->DrainMessages(&messages);
    FrameSwapMessageQueue::TransferMessages(&messages,
                                            &messages_to_deliver_with_frame);
    Send(new ViewHostMsg_SwapCompositorFrame(routing_id_, output_surface_id_,
                                             frame,
                                             messages_to_deliver_with_frame));
    // ~send_message_scope.
  }
  client_->DidSwapBuffers();
}

void CompositorOutputSurface::BindFramebuffer() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
}

uint32_t CompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
  return 0;
}

void CompositorOutputSurface::OnMessageReceived(const IPC::Message& message) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  if (!HasClient())
    return;
  IPC_BEGIN_MESSAGE_MAP(CompositorOutputSurface, message)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateVSyncParameters,
                        OnUpdateVSyncParametersFromBrowser);
    IPC_MESSAGE_HANDLER(ViewMsg_SwapCompositorFrameAck, OnSwapAck);
    IPC_MESSAGE_HANDLER(ViewMsg_ReclaimCompositorResources, OnReclaimResources);
  IPC_END_MESSAGE_MAP()
}

void CompositorOutputSurface::OnUpdateVSyncParametersFromBrowser(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  TRACE_EVENT2("cc",
               "CompositorOutputSurface::OnUpdateVSyncParametersFromBrowser",
               "timebase", (timebase - base::TimeTicks()).InSecondsF(),
               "interval", interval.InSecondsF());
  client_->CommitVSyncParameters(timebase, interval);
}

void CompositorOutputSurface::OnSwapAck(uint32_t output_surface_id,
                                        const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
  client_->DidSwapBuffersComplete();
}

void CompositorOutputSurface::OnReclaimResources(
    uint32_t output_surface_id,
    const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
}

bool CompositorOutputSurface::Send(IPC::Message* message) {
  return message_sender_->Send(message);
}

}  // namespace content
