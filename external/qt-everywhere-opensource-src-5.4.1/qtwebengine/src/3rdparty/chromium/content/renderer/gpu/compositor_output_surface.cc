// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_output_surface.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ipc/ipc_forwarding_message_filter.h"
#include "ipc/ipc_sync_channel.h"

namespace {
// There are several compositor surfaces in a process, but they share the same
// compositor thread, so we use a simple int here to track prefer-smoothness.
int g_prefer_smoothness_count = 0;
} // namespace

namespace content {

//------------------------------------------------------------------------------

// static
IPC::ForwardingMessageFilter* CompositorOutputSurface::CreateFilter(
    base::TaskRunner* target_task_runner)
{
  uint32 messages_to_filter[] = {
    ViewMsg_UpdateVSyncParameters::ID,
    ViewMsg_SwapCompositorFrameAck::ID,
    ViewMsg_ReclaimCompositorResources::ID,
#if defined(OS_ANDROID)
    ViewMsg_BeginFrame::ID
#endif
  };

  return new IPC::ForwardingMessageFilter(
      messages_to_filter, arraysize(messages_to_filter),
      target_task_runner);
}

CompositorOutputSurface::CompositorOutputSurface(
    int32 routing_id,
    uint32 output_surface_id,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    scoped_ptr<cc::SoftwareOutputDevice> software_device,
    bool use_swap_compositor_frame_message)
    : OutputSurface(context_provider, software_device.Pass()),
      output_surface_id_(output_surface_id),
      use_swap_compositor_frame_message_(use_swap_compositor_frame_message),
      output_surface_filter_(
          RenderThreadImpl::current()->compositor_output_surface_filter()),
      routing_id_(routing_id),
      prefers_smoothness_(false),
#if defined(OS_WIN)
      // TODO(epenner): Implement PlatformThread::CurrentHandle() on windows.
      main_thread_handle_(base::PlatformThreadHandle()),
#else
      main_thread_handle_(base::PlatformThread::CurrentHandle()),
#endif
      layout_test_mode_(RenderThreadImpl::current()->layout_test_mode()),
      weak_ptrs_(this) {
  DCHECK(output_surface_filter_.get());
  DetachFromThread();
  message_sender_ = RenderThreadImpl::current()->sync_message_filter();
  DCHECK(message_sender_.get());
  if (OutputSurface::software_device())
    capabilities_.max_frames_pending = 1;
}

CompositorOutputSurface::~CompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  SetNeedsBeginFrame(false);
  if (!HasClient())
    return;
  UpdateSmoothnessTakesPriority(false);
  if (output_surface_proxy_.get())
    output_surface_proxy_->ClearOutputSurface();
  output_surface_filter_->RemoveRoute(routing_id_);
}

bool CompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(CalledOnValidThread());

  if (!cc::OutputSurface::BindToClient(client))
    return false;

  output_surface_proxy_ = new CompositorOutputSurfaceProxy(this);
  output_surface_filter_->AddRoute(
      routing_id_,
      base::Bind(&CompositorOutputSurfaceProxy::OnMessageReceived,
                 output_surface_proxy_));

  if (!context_provider()) {
    // Without a GPU context, the memory policy otherwise wouldn't be set.
    client->SetMemoryPolicy(cc::ManagedMemoryPolicy(
        128 * 1024 * 1024,
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
        cc::ManagedMemoryPolicy::kDefaultNumResourcesLimit));
  }

  return true;
}

void CompositorOutputSurface::ShortcutSwapAck(
    uint32 output_surface_id,
    scoped_ptr<cc::GLFrameData> gl_frame_data,
    scoped_ptr<cc::SoftwareFrameData> software_frame_data) {
  if (!layout_test_previous_frame_ack_) {
    layout_test_previous_frame_ack_.reset(new cc::CompositorFrameAck);
    layout_test_previous_frame_ack_->gl_frame_data.reset(new cc::GLFrameData);
  }

  OnSwapAck(output_surface_id, *layout_test_previous_frame_ack_);

  layout_test_previous_frame_ack_->gl_frame_data = gl_frame_data.Pass();
  layout_test_previous_frame_ack_->last_software_frame_id =
      software_frame_data ? software_frame_data->id : 0;
}

void CompositorOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  if (layout_test_mode_ && use_swap_compositor_frame_message_) {
    // This code path is here to support layout tests that are currently
    // doing a readback in the renderer instead of the browser. So they
    // are using deprecated code paths in the renderer and don't need to
    // actually swap anything to the browser. We shortcut the swap to the
    // browser here and just ack directly within the renderer process.
    // Once crbug.com/311404 is fixed, this can be removed.

    // This would indicate that crbug.com/311404 is being fixed, and this
    // block needs to be removed.
    DCHECK(!frame->delegated_frame_data);

    base::Closure closure =
        base::Bind(&CompositorOutputSurface::ShortcutSwapAck,
                   weak_ptrs_.GetWeakPtr(),
                   output_surface_id_,
                   base::Passed(&frame->gl_frame_data),
                   base::Passed(&frame->software_frame_data));

    if (context_provider_) {
      gpu::gles2::GLES2Interface* context = context_provider_->ContextGL();
      context->Flush();
      uint32 sync_point = context->InsertSyncPointCHROMIUM();
      context_provider_->ContextSupport()->SignalSyncPoint(sync_point, closure);
    } else {
      base::MessageLoopProxy::current()->PostTask(FROM_HERE, closure);
    }
    client_->DidSwapBuffers();
    return;
  }

  if (use_swap_compositor_frame_message_) {
    Send(new ViewHostMsg_SwapCompositorFrame(routing_id_,
                                             output_surface_id_,
                                             *frame));
    client_->DidSwapBuffers();
    return;
  }

  if (frame->gl_frame_data) {
    context_provider_->ContextGL()->ShallowFlushCHROMIUM();
    ContextProviderCommandBuffer* provider_command_buffer =
        static_cast<ContextProviderCommandBuffer*>(context_provider_.get());
    CommandBufferProxyImpl* command_buffer_proxy =
        provider_command_buffer->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    command_buffer_proxy->SetLatencyInfo(frame->metadata.latency_info);
  }

  OutputSurface::SwapBuffers(frame);
}

void CompositorOutputSurface::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  if (!HasClient())
    return;
  IPC_BEGIN_MESSAGE_MAP(CompositorOutputSurface, message)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateVSyncParameters,
                        OnUpdateVSyncParametersFromBrowser);
    IPC_MESSAGE_HANDLER(ViewMsg_SwapCompositorFrameAck, OnSwapAck);
    IPC_MESSAGE_HANDLER(ViewMsg_ReclaimCompositorResources, OnReclaimResources);
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_BeginFrame, OnBeginFrame);
#endif
  IPC_END_MESSAGE_MAP()
}

void CompositorOutputSurface::OnUpdateVSyncParametersFromBrowser(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  CommitVSyncParameters(timebase, interval);
}

#if defined(OS_ANDROID)
void CompositorOutputSurface::SetNeedsBeginFrame(bool enable) {
  DCHECK(CalledOnValidThread());
  Send(new ViewHostMsg_SetNeedsBeginFrame(routing_id_, enable));
}

void CompositorOutputSurface::OnBeginFrame(const cc::BeginFrameArgs& args) {
  DCHECK(CalledOnValidThread());
  client_->BeginFrame(args);
}
#endif  // defined(OS_ANDROID)

void CompositorOutputSurface::OnSwapAck(uint32 output_surface_id,
                                        const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
  client_->DidSwapBuffersComplete();
}

void CompositorOutputSurface::OnReclaimResources(
    uint32 output_surface_id,
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

namespace {
#if defined(OS_ANDROID)
  void SetThreadPriorityToIdle(base::PlatformThreadHandle handle) {
    base::PlatformThread::SetThreadPriority(
       handle, base::kThreadPriority_Background);
  }
  void SetThreadPriorityToDefault(base::PlatformThreadHandle handle) {
    base::PlatformThread::SetThreadPriority(
       handle, base::kThreadPriority_Normal);
  }
#else
  void SetThreadPriorityToIdle(base::PlatformThreadHandle handle) {}
  void SetThreadPriorityToDefault(base::PlatformThreadHandle handle) {}
#endif
}

void CompositorOutputSurface::UpdateSmoothnessTakesPriority(
    bool prefers_smoothness) {
#ifndef NDEBUG
  // If we use different compositor threads, we need to
  // use an atomic int to track prefer smoothness count.
  base::PlatformThreadId g_last_thread = base::PlatformThread::CurrentId();
  DCHECK_EQ(g_last_thread, base::PlatformThread::CurrentId());
#endif
  if (prefers_smoothness_ == prefers_smoothness)
    return;
  // If this is the first surface to start preferring smoothness,
  // Throttle the main thread's priority.
  if (prefers_smoothness_ == false &&
      ++g_prefer_smoothness_count == 1) {
    SetThreadPriorityToIdle(main_thread_handle_);
  }
  // If this is the last surface to stop preferring smoothness,
  // Reset the main thread's priority to the default.
  if (prefers_smoothness_ == true &&
      --g_prefer_smoothness_count == 0) {
    SetThreadPriorityToDefault(main_thread_handle_);
  }
  prefers_smoothness_ = prefers_smoothness;
}

}  // namespace content
