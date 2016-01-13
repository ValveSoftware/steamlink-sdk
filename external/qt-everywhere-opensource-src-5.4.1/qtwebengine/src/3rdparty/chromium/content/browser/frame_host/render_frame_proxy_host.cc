// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_proxy_host.h"

#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "ipc/ipc_message.h"

namespace content {

RenderFrameProxyHost::RenderFrameProxyHost(SiteInstance* site_instance,
                                           FrameTreeNode* frame_tree_node)
    : routing_id_(site_instance->GetProcess()->GetNextRoutingID()),
      site_instance_(site_instance),
      frame_tree_node_(frame_tree_node) {
  GetProcess()->AddRoute(routing_id_, this);

  if (!frame_tree_node_->IsMainFrame() &&
      frame_tree_node_->parent()
              ->render_manager()
              ->current_frame_host()
              ->GetSiteInstance() == site_instance) {
    // The RenderFrameHost navigating cross-process is destroyed and a proxy for
    // it is created in the parent's process. CrossProcessFrameConnector
    // initialization only needs to happen on an initial cross-process
    // navigation, when the RenderFrameHost leaves the same process as its
    // parent. The same CrossProcessFrameConnector is used for subsequent cross-
    // process navigations, but it will be destroyed if the frame is
    // navigated back to the same SiteInstance as its parent.
    cross_process_frame_connector_.reset(new CrossProcessFrameConnector(this));
  }
}

RenderFrameProxyHost::~RenderFrameProxyHost() {
  if (GetProcess()->HasConnection())
    Send(new FrameMsg_DeleteProxy(routing_id_));

  GetProcess()->RemoveRoute(routing_id_);
}

void RenderFrameProxyHost::SetChildRWHView(RenderWidgetHostView* view) {
  cross_process_frame_connector_->set_view(
      static_cast<RenderWidgetHostViewChildFrame*>(view));
}

RenderViewHostImpl* RenderFrameProxyHost::GetRenderViewHost() {
  if (render_frame_host_.get())
    return render_frame_host_->render_view_host();
  return NULL;
}

scoped_ptr<RenderFrameHostImpl> RenderFrameProxyHost::PassFrameHostOwnership() {
  render_frame_host_->set_render_frame_proxy_host(NULL);
  return render_frame_host_.Pass();
}

bool RenderFrameProxyHost::Send(IPC::Message *msg) {
  // TODO(nasko): For now, RenderFrameHost uses this object to send IPC messages
  // while swapped out. This can be removed once we don't have a swapped out
  // state on RenderFrameHosts. See https://crbug.com/357747.
  msg->set_routing_id(routing_id_);
  return GetProcess()->Send(msg);
}

bool RenderFrameProxyHost::OnMessageReceived(const IPC::Message& msg) {
  if (cross_process_frame_connector_.get() &&
      cross_process_frame_connector_->OnMessageReceived(msg))
    return true;

  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrameHosts. See https://crbug.com/357747.
  if (render_frame_host_.get())
    return render_frame_host_->OnMessageReceived(msg);

  return false;
}

}  // namespace content
