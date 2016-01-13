// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/page_transition_types.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"

class GURL;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;
struct FrameHostMsg_OpenURL_Params;
struct FrameMsg_Navigate_Params;

namespace base {
class FilePath;
class ListValue;
}

namespace content {

class CrossProcessFrameConnector;
class CrossSiteTransferringRequest;
class FrameTree;
class FrameTreeNode;
class RenderFrameHostDelegate;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostImpl;
struct ContextMenuParams;
struct GlobalRequestID;
struct Referrer;
struct ShowDesktopNotificationHostMsgParams;

class CONTENT_EXPORT RenderFrameHostImpl : public RenderFrameHost {
 public:
  static RenderFrameHostImpl* FromID(int process_id, int routing_id);

  virtual ~RenderFrameHostImpl();

  // RenderFrameHost
  virtual int GetRoutingID() OVERRIDE;
  virtual SiteInstance* GetSiteInstance() OVERRIDE;
  virtual RenderProcessHost* GetProcess() OVERRIDE;
  virtual RenderFrameHost* GetParent() OVERRIDE;
  virtual const std::string& GetFrameName() OVERRIDE;
  virtual bool IsCrossProcessSubframe() OVERRIDE;
  virtual GURL GetLastCommittedURL() OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual void ExecuteJavaScript(
      const base::string16& javascript) OVERRIDE;
  virtual void ExecuteJavaScript(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback) OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() OVERRIDE;

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void Init();
  int routing_id() const { return routing_id_; }
  void OnCreateChildFrame(int new_routing_id,
                          const std::string& frame_name);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }
  // TODO(nasko): The RenderWidgetHost will be owned by RenderFrameHost in
  // the future, so update this accessor to return the right pointer.
  RenderWidgetHostImpl* GetRenderWidgetHost();

  // This function is called when this is a swapped out RenderFrameHost that
  // lives in the same process as the parent frame. The
  // |cross_process_frame_connector| allows the non-swapped-out
  // RenderFrameHost for a frame to communicate with the parent process
  // so that it may composite drawing data.
  //
  // Ownership is not transfered.
  void set_cross_process_frame_connector(
      CrossProcessFrameConnector* cross_process_frame_connector) {
    cross_process_frame_connector_ = cross_process_frame_connector;
  }

  void set_render_frame_proxy_host(RenderFrameProxyHost* proxy) {
    render_frame_proxy_host_ = proxy;
  }

  // Returns a bitwise OR of bindings types that have been enabled for this
  // RenderFrameHostImpl's RenderView. See BindingsPolicy for details.
  // TODO(creis): Make bindings frame-specific, to support cases like <webview>.
  int GetEnabledBindings();

  // Called on the pending RenderFrameHost when the network response is ready to
  // commit.  We should ensure that the old RenderFrameHost runs its unload
  // handler and determine whether a transfer to a different RenderFrameHost is
  // needed.
  void OnCrossSiteResponse(
      const GlobalRequestID& global_request_id,
      scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
      const std::vector<GURL>& transfer_url_chain,
      const Referrer& referrer,
      PageTransition page_transition,
      bool should_replace_current_entry);

  // Tells the renderer that this RenderFrame is being swapped out for one in a
  // different renderer process.  It should run its unload handler, move to
  // a blank document and create a RenderFrameProxy to replace the RenderFrame.
  // The renderer should preserve the Proxy object until it exits, in case we
  // come back.  The renderer can exit if it has no other active RenderFrames,
  // but not until WasSwappedOut is called (when it is no longer visible).
  void SwapOut(RenderFrameProxyHost* proxy);

  void OnSwappedOut(bool timed_out);
  bool is_swapped_out() { return is_swapped_out_; }
  void set_swapped_out(bool is_swapped_out) {
    is_swapped_out_ = is_swapped_out;
  }

  // Sets the RVH for |this| as pending shutdown. |on_swap_out| will be called
  // when the SwapOutACK is received.
  void SetPendingShutdown(const base::Closure& on_swap_out);

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const FrameMsg_Navigate_Params& params);

  // Load the specified URL; this is a shortcut for Navigate().
  void NavigateToURL(const GURL& url);

  // Runs the beforeunload handler for this frame. |for_cross_site_transition|
  // indicates whether this call is for the current frame during a cross-process
  // navigation. False means we're closing the entire tab.
  void DispatchBeforeUnload(bool for_cross_site_transition);

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  void ExtendSelectionAndDelete(size_t before, size_t after);

  // Notifies the RenderFrame that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input,
                              bool dialog_was_suppressed);

  // Called when an HTML5 notification is closed.
  void NotificationClosed(int notification_id);

 protected:
  friend class RenderFrameHostFactory;

  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int routing_id,
                      bool is_swapped_out);

 private:
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;

  // IPC Message handlers.
  void OnAddMessageToConsole(int32 level,
                             const base::string16& message,
                             int32 line_no,
                             const base::string16& source_id);
  void OnDetach();
  void OnFrameFocused();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnDocumentOnLoadCompleted();
  void OnDidStartProvisionalLoadForFrame(int parent_routing_id,
                                         const GURL& url);
  void OnDidFailProvisionalLoadWithError(
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidFailLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description);
  void OnDidRedirectProvisionalLoad(int32 page_id,
                                    const GURL& source_url,
                                    const GURL& target_url);
  void OnNavigate(const IPC::Message& msg);
  void OnBeforeUnloadACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnSwapOutACK();
  void OnContextMenu(const ContextMenuParams& params);
  void OnJavaScriptExecuteResponse(int id, const base::ListValue& result);
  void OnRunJavaScriptMessage(const base::string16& message,
                              const base::string16& default_prompt,
                              const GURL& frame_url,
                              JavaScriptMessageType type,
                              IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(const GURL& frame_url,
                                const base::string16& message,
                                bool is_reload,
                                IPC::Message* reply_msg);
  void OnRequestDesktopNotificationPermission(const GURL& origin,
                                              int callback_id);
  void OnShowDesktopNotification(
      int notification_id,
      const ShowDesktopNotificationHostMsgParams& params);
  void OnCancelDesktopNotification(int notification_id);
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          size_t start_offset,
                                          size_t end_offset);
  void OnDidAccessInitialDocument();
  void OnDidDisownOpener();
  void OnUpdateTitle(int32 page_id,
                     const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnUpdateEncoding(const std::string& encoding);

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  void DesktopNotificationPermissionRequestDone(int callback_context);

  // For now, RenderFrameHosts indirectly keep RenderViewHosts alive via a
  // refcount that calls Shutdown when it reaches zero.  This allows each
  // RenderFrameHostManager to just care about RenderFrameHosts, while ensuring
  // we have a RenderViewHost for each RenderFrameHost.
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  RenderViewHostImpl* render_view_host_;

  RenderFrameHostDelegate* delegate_;

  // |cross_process_frame_connector_| passes messages from an out-of-process
  // child frame to the parent process for compositing.
  //
  // This is only non-NULL when this is the swapped out RenderFrameHost in
  // the same site instance as this frame's parent.
  //
  // See the class comment above CrossProcessFrameConnector for more
  // information.
  //
  // This will move to RenderFrameProxyHost when that class is created.
  CrossProcessFrameConnector* cross_process_frame_connector_;

  // The proxy created for this RenderFrameHost. It is used to send and receive
  // IPC messages while in swapped out state.
  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrameHosts. See https://crbug.com/357747.
  RenderFrameProxyHost* render_frame_proxy_host_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  FrameTree* frame_tree_;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  FrameTreeNode* frame_tree_node_;

  // The mapping of pending JavaScript calls created by
  // ExecuteJavaScript and their corresponding callbacks.
  std::map<int, JavaScriptResultCallback> javascript_callbacks_;

  // Map from notification_id to a callback to cancel them.
  std::map<int, base::Closure> cancel_notification_callbacks_;

  int routing_id_;
  bool is_swapped_out_;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
