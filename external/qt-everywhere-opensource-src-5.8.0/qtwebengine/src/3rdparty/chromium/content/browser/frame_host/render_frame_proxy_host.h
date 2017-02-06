// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "content/browser/site_instance_impl.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebFocusType.h"

struct FrameHostMsg_OpenURL_Params;
struct FrameMsg_PostMessage_Params;

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
// RenderFrameProxyHost is created whenever a cross-site
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
  static RenderFrameProxyHost* FromID(int process_id, int routing_id);

  RenderFrameProxyHost(SiteInstance* site_instance,
                       RenderViewHostImpl* render_view_host,
                       FrameTreeNode* frame_tree_node);
  ~RenderFrameProxyHost() override;

  RenderProcessHost* GetProcess() {
    return process_;
  }

  // Initializes the object and creates the RenderFrameProxy in the process
  // for the SiteInstance.
  bool InitRenderFrameProxy();

  int GetRoutingID() {
    return routing_id_;
  }

  SiteInstance* GetSiteInstance() {
    return site_instance_.get();
  }

  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; };

  void SetChildRWHView(RenderWidgetHostView* view);

  RenderViewHostImpl* GetRenderViewHost();
  RenderWidgetHostView* GetRenderWidgetHostView();

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  CrossProcessFrameConnector* cross_process_frame_connector() {
    return cross_process_frame_connector_.get();
  }

  // Update the frame's opener in the renderer process in response to the
  // opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener();

  // Set this proxy as the focused frame in the renderer process.  This is
  // called to replicate the focused frame when a frame in a different process
  // becomes focused.
  void SetFocusedFrame();

  void set_render_frame_proxy_created(bool created) {
    render_frame_proxy_created_ = created;
  }

  // Returns if the RenderFrameProxy for this host is alive.
  bool is_render_frame_proxy_live() { return render_frame_proxy_created_; }

 private:
  // IPC Message handlers.
  void OnDetach();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnRouteMessageEvent(const FrameMsg_PostMessage_Params& params);
  void OnDidChangeOpener(int32_t opener_routing_id);
  void OnAdvanceFocus(blink::WebFocusType type, int32_t source_routing_id);
  void OnFrameFocused();

  // This RenderFrameProxyHost's routing id.
  int routing_id_;

  // The SiteInstance this proxy is associated with.
  scoped_refptr<SiteInstance> site_instance_;

  // The renderer process this RenderFrameHostProxy is associated with. It is
  // equivalent to the result of site_instance_->GetProcess(), but that
  // method has the side effect of creating the process if it doesn't exist.
  // Cache a pointer to avoid unnecessary process creation.
  RenderProcessHost* process_;

  // The node in the frame tree where this proxy is located.
  FrameTreeNode* frame_tree_node_;

  // True if we have a live RenderFrameProxy for this host.
  bool render_frame_proxy_created_;

  // When a RenderFrameHost is in a different process from its parent in the
  // frame tree, this class connects its associated RenderWidgetHostView
  // to this RenderFrameProxyHost, which corresponds to the same frame in the
  // parent's renderer process.
  std::unique_ptr<CrossProcessFrameConnector> cross_process_frame_connector_;

  // The RenderViewHost that this RenderFrameProxyHost is associated with. It is
  // kept alive as long as any RenderFrameHosts or RenderFrameProxyHosts
  // are associated with it.
  RenderViewHostImpl* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameProxyHost);
};

}  // namespace

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_
