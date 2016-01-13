// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <map>

#include "base/lazy_instance.h"
#include "content/common/frame_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

namespace {

typedef std::map<int, RenderFrameProxy*> RoutingIDProxyMap;
static base::LazyInstance<RoutingIDProxyMap> g_routing_id_proxy_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(int routing_id,
                                                     int frame_routing_id) {
  DCHECK_NE(routing_id, MSG_ROUTING_NONE);
  RenderFrameProxy* proxy = new RenderFrameProxy(routing_id, frame_routing_id);
  return proxy;
}

// static
RenderFrameProxy* RenderFrameProxy::FromRoutingID(int32 routing_id) {
  RoutingIDProxyMap* proxies = g_routing_id_proxy_map.Pointer();
  RoutingIDProxyMap::iterator it = proxies->find(routing_id);
  return it == proxies->end() ? NULL : it->second;
}

RenderFrameProxy::RenderFrameProxy(int routing_id, int frame_routing_id)
    : routing_id_(routing_id),
      frame_routing_id_(frame_routing_id) {
  std::pair<RoutingIDProxyMap::iterator, bool> result =
    g_routing_id_proxy_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";
  RenderThread::Get()->AddRoute(routing_id_, this);

  render_frame_ = RenderFrameImpl::FromRoutingID(frame_routing_id);
  CHECK(render_frame_);
  render_frame_->render_view()->RegisterRenderFrameProxy(this);
}

RenderFrameProxy::~RenderFrameProxy() {
  render_frame_->render_view()->UnregisterRenderFrameProxy(this);
  RenderThread::Get()->RemoveRoute(routing_id_);
  g_routing_id_proxy_map.Get().erase(routing_id_);
}

blink::WebFrame* RenderFrameProxy::GetWebFrame() {
  return render_frame_->GetWebFrame();
}

void RenderFrameProxy::DidCommitCompositorFrame() {
  if (compositing_helper_)
    compositing_helper_->DidCommitCompositorFrame();
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxy, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_DeleteProxy, OnDeleteProxy)
    IPC_MESSAGE_HANDLER(FrameMsg_ChildFrameProcessGone, OnChildFrameProcessGone)
    IPC_MESSAGE_HANDLER(FrameMsg_BuffersSwapped, OnBuffersSwapped)
    IPC_MESSAGE_HANDLER_GENERIC(FrameMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(msg))
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled)
    return render_frame_->OnMessageReceived(msg);

  return handled;
}

bool RenderFrameProxy::Send(IPC::Message* message) {
  if (!SwappedOutMessages::CanSendWhileSwappedOut(message)) {
    delete message;
    return false;
  }
  message->set_routing_id(routing_id_);
  return RenderThread::Get()->Send(message);
}

void RenderFrameProxy::OnDeleteProxy() {
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(frame_routing_id_);
  CHECK(render_frame);
  render_frame->set_render_frame_proxy(NULL);

  delete this;
}

void RenderFrameProxy::OnChildFrameProcessGone() {
  if (compositing_helper_)
    compositing_helper_->ChildFrameGone();
}

void RenderFrameProxy::OnBuffersSwapped(
    const FrameMsg_BuffersSwapped_Params& params) {
  if (!compositing_helper_.get()) {
    compositing_helper_ =
        ChildFrameCompositingHelper::CreateCompositingHelperForRenderFrame(
            GetWebFrame(), this, routing_id_);
    compositing_helper_->EnableCompositing(true);
  }
  compositing_helper_->OnBuffersSwapped(
      params.size,
      params.mailbox,
      params.gpu_route_id,
      params.gpu_host_id,
      render_frame_->render_view()->GetWebView()->deviceScaleFactor());
}

void RenderFrameProxy::OnCompositorFrameSwapped(const IPC::Message& message) {
  FrameMsg_CompositorFrameSwapped::Param param;
  if (!FrameMsg_CompositorFrameSwapped::Read(&message, &param))
    return;

  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  param.a.frame.AssignTo(frame.get());

  if (!compositing_helper_.get()) {
    compositing_helper_ =
        ChildFrameCompositingHelper::CreateCompositingHelperForRenderFrame(
            GetWebFrame(), this, routing_id_);
    compositing_helper_->EnableCompositing(true);
  }
  compositing_helper_->OnCompositorFrameSwapped(frame.Pass(),
                                                param.a.producing_route_id,
                                                param.a.output_surface_id,
                                                param.a.producing_host_id,
                                                param.a.shared_memory_handle);
}

}  // namespace
