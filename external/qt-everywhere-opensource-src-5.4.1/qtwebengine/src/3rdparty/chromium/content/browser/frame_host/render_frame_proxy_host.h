// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace content {

class CrossProcessFrameConnector;
class FrameTreeNode;
class RenderProcessHost;
class RenderFrameHostImpl;
class RenderViewHostImpl;
class RenderWidgetHostView;

// When a page's frames are rendered by multiple processes, each renderer has a
// full copy of the frame tree. It has full RenderFrames for the frames it is
// responsible for rendering and placeholder objects (i.e., RenderFrameProxies)
// for frames rendered by other processes.
//
// This class is the browser-side host object for the placeholder. Each node in
// the frame tree has a RenderFrameHost for the active SiteInstance and a set
// of RenderFrameProxyHost objects - one for all other SiteInstances with
// references to this frame. The proxies allow us to keep existing window
// references valid over cross-process navigations and route cross-site
// asynchronous JavaScript calls, such as postMessage.
//
// For now, RenderFrameProxyHost is created when a RenderFrameHost is swapped
// out and acts just as a wrapper. It is destroyed when the RenderFrameHost is
// swapped back in or is no longer referenced and is therefore deleted.
//
// Long term, RenderFrameProxyHost will be created whenever a cross-site
// navigation occurs and a reference to the frame navigating needs to be kept
// alive. A RenderFrameProxyHost and a RenderFrameHost for the same SiteInstance
// can exist at the same time, but only one will be "active" at a time.
// There are two cases where the two objects will coexist:
// * When navigating cross-process and there is already a RenderFrameProxyHost
// for the new SiteInstance. A pending RenderFrameHost is created, but it is
// not used until it commits. At that point, RenderFrameHostManager transitions
// the pending RenderFrameHost to the active one and deletes the proxy.
// * When navigating cross-process and the existing document has an unload
// event handler. When the new navigation commits, RenderFrameHostManager
// creates a RenderFrameProxyHost for the old SiteInstance and uses it going
// forward. It also instructs the RenderFrameHost to run the unload event
// handler and is kept alive for the duration. Once the event handling is
// complete, the RenderFrameHost is deleted.
class RenderFrameProxyHost
    : public IPC::Listener,
      public IPC::Sender {
 public:
  RenderFrameProxyHost(SiteInstance* site_instance,
                       FrameTreeNode* frame_tree_node);
  virtual ~RenderFrameProxyHost();

  RenderProcessHost* GetProcess() {
    return site_instance_->GetProcess();
  }

  int GetRoutingID() {
    return routing_id_;
  }

  SiteInstance* GetSiteInstance() {
    return site_instance_.get();
  }

  void SetChildRWHView(RenderWidgetHostView* view);

  // TODO(nasko): The following methods should be removed once we don't have a
  // swapped out state on RenderFrameHosts. See https://crbug.com/357747.
  RenderFrameHostImpl* render_frame_host() {
    return render_frame_host_.get();
  }
  RenderViewHostImpl* GetRenderViewHost();

  void TakeFrameHostOwnership(
      scoped_ptr<RenderFrameHostImpl> render_frame_host) {
    render_frame_host_ = render_frame_host.Pass();
  }
  scoped_ptr<RenderFrameHostImpl> PassFrameHostOwnership();

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  CrossProcessFrameConnector* cross_process_frame_connector() {
    return cross_process_frame_connector_.get();
  }

 private:
  // This RenderFrameProxyHost's routing id.
  int routing_id_;

  // The SiteInstance this proxy is associated with.
  scoped_refptr<SiteInstance> site_instance_;

  // The node in the frame tree where this proxy is located.
  FrameTreeNode* frame_tree_node_;

  // When a RenderFrameHost is in a different process from its parent in the
  // frame tree, this class connects its associated RenderWidgetHostView
  // to this RenderFrameProxyHost, which corresponds to the same frame in the
  // parent's renderer process.
  scoped_ptr<CrossProcessFrameConnector> cross_process_frame_connector_;

  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrameHosts. See https://crbug.com/357747.
  scoped_ptr<RenderFrameHostImpl> render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameProxyHost);
};

}  // namespace

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_
