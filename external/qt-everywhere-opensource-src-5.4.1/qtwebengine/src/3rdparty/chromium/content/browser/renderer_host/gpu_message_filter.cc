// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/browser/renderer_host/gpu_message_filter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace content {

struct GpuMessageFilter::CreateViewCommandBufferRequest {
  CreateViewCommandBufferRequest(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      scoped_ptr<IPC::Message> reply)
      : surface_id(surface_id),
        init_params(init_params),
        reply(reply.Pass()) {
  }
  int32 surface_id;
  GPUCreateCommandBufferConfig init_params;
  scoped_ptr<IPC::Message> reply;
};

struct GpuMessageFilter::FrameSubscription {
  FrameSubscription(
      int in_route_id,
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> in_subscriber)
      : route_id(in_route_id),
        surface_id(0),
        subscriber(in_subscriber.Pass()),
        factory(subscriber.get()) {
  }

  int route_id;
  int surface_id;
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber;
  base::WeakPtrFactory<RenderWidgetHostViewFrameSubscriber> factory;
};

GpuMessageFilter::GpuMessageFilter(int render_process_id,
                                   RenderWidgetHelper* render_widget_helper)
    : BrowserMessageFilter(GpuMsgStart),
      gpu_process_id_(0),
      render_process_id_(render_process_id),
      render_widget_helper_(render_widget_helper),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GpuMessageFilter::~GpuMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EndAllFrameSubscriptions();
}

bool GpuMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_EstablishGpuChannel,
                                    OnEstablishGpuChannel)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_CreateViewCommandBuffer,
                                    OnCreateViewCommandBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuMessageFilter::BeginFrameSubscription(
    int route_id,
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  linked_ptr<FrameSubscription> subscription(
      new FrameSubscription(route_id, subscriber.Pass()));
  BeginFrameSubscriptionInternal(subscription);
}

void GpuMessageFilter::EndFrameSubscription(int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FrameSubscriptionList frame_subscription_list;
  frame_subscription_list.swap(frame_subscription_list_);
  for (FrameSubscriptionList::iterator it = frame_subscription_list.begin();
       it != frame_subscription_list.end(); ++it) {
    if ((*it)->route_id != route_id)
      frame_subscription_list_.push_back(*it);
    else
      EndFrameSubscriptionInternal(*it);
  }
}

void GpuMessageFilter::OnEstablishGpuChannel(
    CauseForGpuLaunch cause_for_gpu_launch,
    IPC::Message* reply_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_ptr<IPC::Message> reply(reply_ptr);

  // TODO(apatrick): Eventually, this will return the route ID of a
  // GpuProcessStub, from which the renderer process will create a
  // GpuProcessProxy. The renderer will use the proxy for all subsequent
  // communication with the GPU process. This means if the GPU process
  // terminates, the renderer process will not find itself unknowingly sending
  // IPCs to a newly launched GPU process. Also, I will rename this function
  // to something like OnCreateGpuProcess.
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               cause_for_gpu_launch);
    if (!host) {
      reply->set_reply_error();
      Send(reply.release());
      return;
    }

    gpu_process_id_ = host->host_id();

    // Apply all frame subscriptions to the new GpuProcessHost.
    BeginAllFrameSubscriptions();
  }

  bool share_contexts = true;
  host->EstablishGpuChannel(
      render_process_id_,
      share_contexts,
      base::Bind(&GpuMessageFilter::EstablishChannelCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&reply)));
}

void GpuMessageFilter::OnCreateViewCommandBuffer(
    int32 surface_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id,
    IPC::Message* reply_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_ptr<IPC::Message> reply(reply_ptr);

  GpuSurfaceTracker* surface_tracker = GpuSurfaceTracker::Get();
  gfx::GLSurfaceHandle compositing_surface;

  int renderer_id = 0;
  int render_widget_id = 0;
  bool result = surface_tracker->GetRenderWidgetIDForSurface(
      surface_id, &renderer_id, &render_widget_id);
  if (result && renderer_id == render_process_id_) {
    compositing_surface = surface_tracker->GetSurfaceHandle(surface_id);
  } else {
    DLOG(ERROR) << "Renderer " << render_process_id_
                << " tried to access a surface for renderer " << renderer_id;
  }

  if (compositing_surface.parent_client_id &&
      !GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
    // For the renderer to fall back to software also.
    compositing_surface = gfx::GLSurfaceHandle();
  }

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host || compositing_surface.is_null()) {
    // TODO(apatrick): Eventually, this IPC message will be routed to a
    // GpuProcessStub with a particular routing ID. The error will be set if
    // the GpuProcessStub with that routing ID is not in the MessageRouter.
    reply->set_reply_error();
    Send(reply.release());
    return;
  }

  host->CreateViewCommandBuffer(
      compositing_surface,
      surface_id,
      render_process_id_,
      init_params,
      route_id,
      base::Bind(&GpuMessageFilter::CreateCommandBufferCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&reply)));
}

void GpuMessageFilter::EstablishChannelCallback(
    scoped_ptr<IPC::Message> reply,
    const IPC::ChannelHandle& channel,
    const gpu::GPUInfo& gpu_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GpuHostMsg_EstablishGpuChannel::WriteReplyParams(
      reply.get(), render_process_id_, channel, gpu_info);
  Send(reply.release());
}

void GpuMessageFilter::CreateCommandBufferCallback(
    scoped_ptr<IPC::Message> reply, CreateCommandBufferResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GpuHostMsg_CreateViewCommandBuffer::WriteReplyParams(reply.get(), result);
  Send(reply.release());
}

void GpuMessageFilter::BeginAllFrameSubscriptions() {
  FrameSubscriptionList frame_subscription_list;
  frame_subscription_list.swap(frame_subscription_list_);
  for (FrameSubscriptionList::iterator it = frame_subscription_list.begin();
       it != frame_subscription_list.end(); ++it) {
    BeginFrameSubscriptionInternal(*it);
  }
}

void GpuMessageFilter::EndAllFrameSubscriptions() {
  for (FrameSubscriptionList::iterator it = frame_subscription_list_.begin();
       it != frame_subscription_list_.end(); ++it) {
    EndFrameSubscriptionInternal(*it);
  }
  frame_subscription_list_.clear();
}

void GpuMessageFilter::BeginFrameSubscriptionInternal(
    linked_ptr<FrameSubscription> subscription) {
  if (!subscription->surface_id) {
    GpuSurfaceTracker* surface_tracker = GpuSurfaceTracker::Get();
    subscription->surface_id = surface_tracker->LookupSurfaceForRenderer(
        render_process_id_, subscription->route_id);

    // If the surface ID cannot be found this subscription is dropped.
    if (!subscription->surface_id)
      return;
  }
  frame_subscription_list_.push_back(subscription);

  // Frame subscriber is owned by this object, but it is shared with
  // GpuProcessHost. GpuProcessHost can be destroyed in the case of crashing
  // and we do not get a signal. This object can also be destroyed independent
  // of GpuProcessHost. To ensure that GpuProcessHost does not reference a
  // deleted frame subscriber, a weak reference is shared.
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host)
    return;
  host->BeginFrameSubscription(subscription->surface_id,
                               subscription->factory.GetWeakPtr());
}

void GpuMessageFilter::EndFrameSubscriptionInternal(
    linked_ptr<FrameSubscription> subscription) {
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);

  // An empty surface ID means subscription has never started in GpuProcessHost
  // so it is not necessary to end it.
  if (!host || !subscription->surface_id)
    return;

  // Note that GpuProcessHost here might not be the same one that frame
  // subscription has applied.
  host->EndFrameSubscription(subscription->surface_id);
}

}  // namespace content
