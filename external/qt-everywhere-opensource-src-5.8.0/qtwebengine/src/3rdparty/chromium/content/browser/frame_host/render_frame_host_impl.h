// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/loader/global_routing_id.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/common/ax_content_node_data.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_replication_state.h"
#include "content/common/image_downloader/image_downloader.mojom.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/javascript_message_type.h"
#include "net/http/http_response_headers.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/platform/WebInsecureRequestPolicy.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/service_registry_android.h"
#endif

class GURL;
struct AccessibilityHostMsg_EventParams;
struct AccessibilityHostMsg_FindInPageResultParams;
struct AccessibilityHostMsg_LocationChangeParams;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;
struct FrameHostMsg_OpenURL_Params;
struct FrameMsg_TextTrackSettings_Params;
#if defined(USE_EXTERNAL_POPUP_MENU)
struct FrameHostMsg_ShowPopup_Params;
#endif

namespace base {
class FilePath;
class ListValue;
}

namespace blink {
namespace mojom {
class WebBluetoothService;
}
}

namespace content {

class CrossProcessFrameConnector;
class CrossSiteTransferringRequest;
class FrameMojoShell;
class FrameTree;
class FrameTreeNode;
class NavigationHandleImpl;
class PermissionServiceContext;
class RenderFrameHostDelegate;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostDelegate;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class ResourceRequestBody;
class StreamHandle;
class TimeoutMonitor;
class WebBluetoothServiceImpl;
struct ContentSecurityPolicyHeader;
struct ContextMenuParams;
struct FileChooserParams;
struct GlobalRequestID;
struct FileChooserParams;
struct Referrer;
struct ResourceResponse;

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      NON_EXPORTED_BASE(public mojom::FrameHost),
      public BrowserAccessibilityDelegate,
      public SiteInstanceImpl::Observer {
 public:
  using AXTreeSnapshotCallback =
      base::Callback<void(
          const ui::AXTreeUpdate&)>;

  // An accessibility reset is only allowed to prevent very rare corner cases
  // or race conditions where the browser and renderer get out of sync. If
  // this happens more than this many times, kill the renderer.
  static const int kMaxAccessibilityResets = 5;

  static RenderFrameHostImpl* FromID(int process_id, int routing_id);
  static RenderFrameHostImpl* FromAXTreeID(
      AXTreeIDRegistry::AXTreeID ax_tree_id);

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  int GetRoutingID() override;
  AXTreeIDRegistry::AXTreeID GetAXTreeID() override;
  SiteInstanceImpl* GetSiteInstance() override;
  RenderProcessHost* GetProcess() override;
  RenderWidgetHostView* GetView() override;
  RenderFrameHostImpl* GetParent() override;
  int GetFrameTreeNodeId() override;
  const std::string& GetFrameName() override;
  bool IsCrossProcessSubframe() override;
  const GURL& GetLastCommittedURL() override;
  url::Origin GetLastCommittedOrigin() override;
  gfx::NativeView GetNativeView() override;
  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message) override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         const JavaScriptResultCallback& callback) override;
  void ExecuteJavaScriptInIsolatedWorld(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback,
      int world_id) override;
  void ExecuteJavaScriptForTests(const base::string16& javascript) override;
  void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback) override;
  void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  void InsertVisualStateCallback(const VisualStateCallback& callback) override;
  void CopyImageAt(int x, int y) override;
  void SaveImageAt(int x, int y) override;
  RenderViewHost* GetRenderViewHost() override;
  shell::InterfaceRegistry* GetInterfaceRegistry() override;
  shell::InterfaceProvider* GetRemoteInterfaces() override;
  blink::WebPageVisibilityState GetVisibilityState() override;
  bool IsRenderFrameLive() override;
  int GetProxyCount() override;
  void FilesSelectedInChooser(const std::vector<FileChooserFileInfo>& files,
                              FileChooserParams::Mode permissions) override;

  // mojom::FrameHost
  void GetInterfaceProvider(
      shell::mojom::InterfaceProviderRequest interfaces) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // BrowserAccessibilityDelegate
  void AccessibilitySetFocus(int acc_obj_id) override;
  void AccessibilityDoDefaultAction(int acc_obj_id) override;
  void AccessibilityShowContextMenu(int acc_obj_id) override;
  void AccessibilityScrollToMakeVisible(int acc_obj_id,
                                        const gfx::Rect& subfocus) override;
  void AccessibilityScrollToPoint(int acc_obj_id,
                                  const gfx::Point& point) override;
  void AccessibilitySetScrollOffset(int acc_obj_id,
                                    const gfx::Point& offset) override;
  void AccessibilitySetSelection(int anchor_object_id,
                                 int anchor_offset,
                                 int focus_object_id,
                                 int focus_offset) override;
  void AccessibilitySetValue(int acc_obj_id, const base::string16& value)
      override;
  bool AccessibilityViewHasFocus() const override;
  gfx::Rect AccessibilityGetViewBounds() const override;
  gfx::Point AccessibilityOriginInScreen(
      const gfx::Rect& bounds) const override;
  gfx::Rect AccessibilityTransformToRootCoordSpace(
      const gfx::Rect& bounds) override;
  SiteInstance* AccessibilityGetSiteInstance() override;
  void AccessibilityHitTest(const gfx::Point& point) override;
  void AccessibilitySetAccessibilityFocus(int acc_obj_id) override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;

  // SiteInstanceImpl::Observer
  void RenderProcessGone(SiteInstanceImpl* site_instance) override;

  // Creates a RenderFrame in the renderer process.
  bool CreateRenderFrame(int proxy_routing_id,
                         int opener_routing_id,
                         int parent_routing_id,
                         int previous_sibling_routing_id);

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  This is currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  void SetRenderFrameCreated(bool created);

  // Called for renderer-created windows to resume requests from this frame,
  // after they are blocked in RenderWidgetHelper::CreateNewWindow.
  void Init();

  int routing_id() const { return routing_id_; }
  void OnCreateChildFrame(
      int new_routing_id,
      blink::WebTreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      blink::WebSandboxFlags sandbox_flags,
      const blink::WebFrameOwnerProperties& frame_owner_properties);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  const GURL& last_committed_url() const { return last_committed_url_; }

  // Allows FrameTreeNode::SetCurrentURL to update this frame's last committed
  // URL.  Do not call this directly, since we rely on SetCurrentURL to track
  // whether a real load has committed or not.
  void set_last_committed_url(const GURL& url) {
    last_committed_url_ = url;
  }

  // The most recent non-net-error URL to commit in this frame.  In almost all
  // cases, use GetLastCommittedURL instead.
  const GURL& last_successful_url() { return last_successful_url_; }
  void set_last_successful_url(const GURL& url) {
    last_successful_url_ = url;
  }

  // Returns the associated WebUI or null if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }

  // Returns the pending WebUI, or null if none applies.
  WebUIImpl* pending_web_ui() const {
    return should_reuse_web_ui_ ? web_ui_.get() : pending_web_ui_.get();
  }

  // Returns this RenderFrameHost's loading state. This method is only used by
  // FrameTreeNode. The proper way to check whether a frame is loading is to
  // call FrameTreeNode::IsLoading.
  bool is_loading() const { return is_loading_; }

  // Sets this RenderFrameHost loading state. This is only used in the case of
  // transfer navigations, where no DidStart/DidStopLoading notifications
  // should be sent during the transfer.
  // TODO(clamy): Remove this once PlzNavigate ships.
  void set_is_loading(bool is_loading) { is_loading_ = is_loading; }

  // Returns true if this is a top-level frame, or if this frame's RenderFrame
  // is in a different process from its parent frame. Local roots are
  // distinguished by owning a RenderWidgetHost, which manages input events
  // and painting for this frame and its contiguous local subtree in the
  // renderer process.
  bool is_local_root() { return !!render_widget_host_; }

  // Returns the RenderWidgetHostImpl attached to this frame or the nearest
  // ancestor frame, which could potentially be the root. For most input
  // and rendering related purposes, GetView() should be preferred and
  // RenderWidgetHostViewBase methods used. GetRenderWidgetHost() will not
  // return a nullptr, whereas GetView() potentially will (for instance,
  // after a renderer crash).
  //
  // This method crashes if this RenderFrameHostImpl does not own a
  // a RenderWidgetHost and nor does any of its ancestors. That would
  // typically mean that the frame has been detached from the frame tree.
  RenderWidgetHostImpl* GetRenderWidgetHost();

  GlobalFrameRoutingId GetGlobalFrameRoutingId();

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

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.
  int nav_entry_id() const { return nav_entry_id_; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

  // A NavigationHandle for the pending navigation in this frame, if any. This
  // is cleared when the navigation commits.
  NavigationHandleImpl* navigation_handle() const {
    return navigation_handle_.get();
  }

  // Called when a new navigation starts in this RenderFrameHost. Ownership of
  // |navigation_handle| is transferred.
  // PlzNavigate: called when a navigation is ready to commit in this
  // RenderFrameHost.
  void SetNavigationHandle(
      std::unique_ptr<NavigationHandleImpl> navigation_handle);

  // Gives the ownership of |navigation_handle_| to the caller.
  // This happens during transfer navigations, where it should be transferred
  // from the RenderFrameHost that issued the initial request to the new
  // RenderFrameHost that will issue the transferring request.
  std::unique_ptr<NavigationHandleImpl> PassNavigationHandleOwnership();

  // Called on the pending RenderFrameHost when the network response is ready to
  // commit.  We should ensure that the old RenderFrameHost runs its unload
  // handler and determine whether a transfer to a different RenderFrameHost is
  // needed.
  void OnCrossSiteResponse(const GlobalRequestID& global_request_id,
                           std::unique_ptr<CrossSiteTransferringRequest>
                               cross_site_transferring_request,
                           const std::vector<GURL>& transfer_url_chain,
                           const Referrer& referrer,
                           ui::PageTransition page_transition,
                           bool should_replace_current_entry);

  // Tells the renderer that this RenderFrame is being swapped out for one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // RenderFrameProxy to replace the RenderFrame and set it to |is_loading|
  // state. The renderer should preserve the RenderFrameProxy object until it
  // exits, in case we come back.  The renderer can exit if it has no other
  // active RenderFrames, but not until WasSwappedOut is called.
  void SwapOut(RenderFrameProxyHost* proxy, bool is_loading);

  // Whether an ongoing navigation is waiting for a BeforeUnload ACK from the
  // RenderFrame. Currently this only happens in cross-site navigations.
  // PlzNavigate: this happens in every browser-initiated navigation that is not
  // same-page.
  bool is_waiting_for_beforeunload_ack() const {
    return is_waiting_for_beforeunload_ack_;
  }

  // Whether the RFH is waiting for an unload ACK from the renderer.
  bool IsWaitingForUnloadACK() const;

  // Called when either the SwapOut request has been acknowledged or has timed
  // out.
  void OnSwappedOut();

  // This method returns true from the time this RenderFrameHost is created
  // until SwapOut is called, at which point it is pending deletion.
  bool is_active() { return !is_waiting_for_swapout_ack_; }

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const CommonNavigationParams& common_params,
                const StartNavigationParams& start_params,
                const RequestNavigationParams& request_params);

  // Navigates to an interstitial page represented by the provided data URL.
  void NavigateToInterstitialURL(const GURL& data_url);

  // Treat this prospective navigation as though it originated from the frame.
  // Used, e.g., for a navigation request that originated from a RemoteFrame.
  // |source_site_instance| is the SiteInstance of the frame that initiated the
  // navigation.
  // TODO(creis): Remove this method and have RenderFrameProxyHost call
  // RequestOpenURL with its FrameTreeNode.
  void OpenURL(const FrameHostMsg_OpenURL_Params& params,
               SiteInstance* source_site_instance);

  // Stop the load in progress.
  void Stop();

  // Returns whether navigation messages are currently suspended for this
  // RenderFrameHost. Only true during a cross-site navigation, while waiting
  // for the onbeforeunload handler.
  bool are_navigations_suspended() const { return navigations_suspended_; }

  // Suspends (or unsuspends) any navigation messages from being sent from this
  // RenderFrameHost. This is called when a pending RenderFrameHost is created
  // for a cross-site navigation, because we must suspend any navigations until
  // we hear back from the old renderer's onbeforeunload handler. Note that it
  // is important that only one navigation event happen after calling this
  // method with |suspend| equal to true. If |suspend| is false and there is a
  // suspended_nav_message_, this will send the message. This function should
  // only be called to toggle the state; callers should check
  // are_navigations_suspended() first. If |suspend| is false, the time that the
  // user decided the navigation should proceed should be passed as
  // |proceed_time|.
  void SetNavigationsSuspended(bool suspend,
                               const base::TimeTicks& proceed_time);

  // Clears any suspended navigation state after a cross-site navigation is
  // canceled or suspended. This is important if we later return to this
  // RenderFrameHost.
  void CancelSuspendedNavigations();

  // Runs the beforeunload handler for this frame. |for_navigation| indicates
  // whether this call is for the current frame during a cross-process
  // navigation. False means we're closing the entire tab. |is_reload|
  // indicates whether the navigation is a reload of the page or not. If
  // |for_navigation| is false, |is_reload| should be false as well.
  // PlzNavigate: this call happens on all browser-initiated navigations.
  void DispatchBeforeUnload(bool for_navigation, bool is_reload);

  // Simulate beforeunload ack on behalf of renderer if it's unrenresponsive.
  void SimulateBeforeUnloadAck();

  // Returns true if a call to DispatchBeforeUnload will actually send the
  // BeforeUnload IPC. This is the case if the current renderer is live and this
  // frame is the main frame.
  bool ShouldDispatchBeforeUnload();

  // Update the frame's opener in the renderer process in response to the
  // opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener();

  // Set this frame as focused in the renderer process.  This supports
  // cross-process window.focus() calls.
  void SetFocusedFrame();

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  void ExtendSelectionAndDelete(size_t before, size_t after);

  // Notifies the RenderFrame that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input,
                              bool dialog_was_suppressed);

  // Send a message to the renderer process to change the accessibility mode.
  void SetAccessibilityMode(AccessibilityMode AccessibilityMode);

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode.
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback);

  // Resets the accessibility serializer in the renderer.
  void AccessibilityReset();

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process, and the accessibility tree it sent can be
  // retrieved using GetAXTreeForTesting().
  void SetAccessibilityCallbackForTesting(
      const base::Callback<void(RenderFrameHostImpl*, ui::AXEvent, int)>&
          callback);

  // Called when the metadata about the accessibility tree for this frame
  // changes due to a browser-side change, as opposed to due to an IPC from
  // a renderer.
  void UpdateAXTreeData();

  // Set the AX tree ID of the embedder RFHI, if this is a browser plugin guest.
  void set_browser_plugin_embedder_ax_tree_id(
      AXTreeIDRegistry::AXTreeID ax_tree_id) {
    browser_plugin_embedder_ax_tree_id_ = ax_tree_id;
  }

  // Send a message to the render process to change text track style settings.
  void SetTextTrackSettings(const FrameMsg_TextTrackSettings_Params& params);

  // Returns a snapshot of the accessibility tree received from the
  // renderer as of the last time an accessibility notification was
  // received.
  const ui::AXTree* GetAXTreeForTesting();

  // Access the BrowserAccessibilityManager if it already exists.
  BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // NULL.
  BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

#if defined(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  // Select popup menu related methods (for external popup menus).
  void DidSelectPopupMenuItem(int selected_index);
  void DidCancelPopupMenu();
#else
  void DidSelectPopupMenuItems(const std::vector<int>& selected_indices);
  void DidCancelPopupMenu();
#endif
#endif

  // PlzNavigate: Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  void CommitNavigation(ResourceResponse* response,
                        std::unique_ptr<StreamHandle> body,
                        const CommonNavigationParams& common_params,
                        const RequestNavigationParams& request_params,
                        bool is_view_source);

  // PlzNavigate
  // Indicates that a navigation failed and that this RenderFrame should display
  // an error page.
  void FailedNavigation(const CommonNavigationParams& common_params,
                        const RequestNavigationParams& request_params,
                        bool has_stale_copy_in_cache,
                        int error_code);

  // Sets up the Mojo connection between this instance and its associated render
  // frame if it has not yet been set up.
  void SetUpMojoIfNeeded();

  // Tears down the browser-side state relating to the Mojo connection between
  // this instance and its associated render frame.
  void InvalidateMojoConnection();

  // Returns whether the frame is focused. A frame is considered focused when it
  // is the parent chain of the focused frame within the frame tree. In
  // addition, its associated RenderWidgetHost has to be focused.
  bool IsFocused();

  // Updates the pending WebUI of this RenderFrameHost based on the provided
  // |dest_url|, setting it to either none, a new instance or to reuse the
  // currently active one. Returns true if the pending WebUI was updated.
  // If this is a history navigation its NavigationEntry bindings should be
  // provided through |entry_bindings| to allow verifying that they are not
  // being set differently this time around. Otherwise |entry_bindings| should
  // be set to NavigationEntryImpl::kInvalidBindings so that no checks are done.
  bool UpdatePendingWebUI(const GURL& dest_url, int entry_bindings);

  // Updates the active WebUI with the pending one set by the last call to
  // UpdatePendingWebUI and then clears any pending data. If UpdatePendingWebUI
  // was not called the active WebUI will simply be cleared.
  void CommitPendingWebUI();

  // Destroys the pending WebUI and resets related data.
  void ClearPendingWebUI();

  // Destroys all WebUI instances and resets related data.
  void ClearAllWebUI();

  // Returns the Mojo ImageDownloader service.
  const content::mojom::ImageDownloaderPtr& GetMojoImageDownloader();

  // Resets the loading state. Following this call, the RenderFrameHost will be
  // in a non-loading state.
  void ResetLoadingState();

  // Tells the renderer that this RenderFrame will soon be swapped out, and thus
  // not to create any new modal dialogs until it happens.  This must be done
  // separately so that the ScopedPageLoadDeferrers of any current dialogs are
  // no longer on the stack when we attempt to swap it out.
  void SuppressFurtherDialogs();

  // PlzNavigate: returns the LoFi state of the last successful navigation that
  // made a network request.
  LoFiState last_navigation_lofi_state() const {
    return last_navigation_lofi_state_;
  }

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(SiteInstance* site_instance,
                      RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      RenderWidgetHostDelegate* rwh_delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int32_t routing_id,
                      int32_t widget_routing_id,
                      bool hidden);

 private:
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;

  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CreateRenderViewAfterProcessKillAndClosedProxy);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreSubframeFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RenderViewInitAfterNewProxyAndProcessKill);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           UnloadPushStateOnCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           WebUIJavascriptDisallowedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrashSubframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostIsNotReusedAfterDelayedSwapOutACK);

  // IPC Message handlers.
  void OnAddMessageToConsole(int32_t level,
                             const base::string16& message,
                             int32_t line_no,
                             const base::string16& source_id);
  void OnDetach();
  void OnFrameFocused();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnDocumentOnLoadCompleted(
      FrameMsg_UILoadMetricsReportType::Value report_type,
      base::TimeTicks ui_timestamp);
  void OnDidStartProvisionalLoad(
      const GURL& url,
      const base::TimeTicks& navigation_start);
  void OnDidFailProvisionalLoadWithError(
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidFailLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler);
  void OnDidCommitProvisionalLoad(const IPC::Message& msg);
  void OnUpdateState(const PageState& state);
  void OnBeforeUnloadACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnSwapOutACK();
  void OnRenderProcessGone(int status, int error_code);
  void OnContextMenu(const ContextMenuParams& params);
  void OnJavaScriptExecuteResponse(int id, const base::ListValue& result);
  void OnVisualStateResponse(uint64_t id);
  void OnRunJavaScriptMessage(const base::string16& message,
                              const base::string16& default_prompt,
                              const GURL& frame_url,
                              JavaScriptMessageType type,
                              IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(const GURL& frame_url,
                                bool is_reload,
                                IPC::Message* reply_msg);
  void OnRunFileChooser(const FileChooserParams& params);
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          uint32_t start_offset,
                                          uint32_t end_offset);
  void OnDidAccessInitialDocument();
  void OnDidChangeOpener(int32_t opener_routing_id);
  void OnDidChangeName(const std::string& name, const std::string& unique_name);
  void OnDidAddContentSecurityPolicy(const ContentSecurityPolicyHeader& header);
  void OnEnforceInsecureRequestPolicy(blink::WebInsecureRequestPolicy policy);
  void OnUpdateToUniqueOrigin(bool is_potentially_trustworthy_unique_origin);
  void OnDidAssignPageId(int32_t page_id);
  void OnDidChangeSandboxFlags(int32_t frame_routing_id,
                               blink::WebSandboxFlags flags);
  void OnDidChangeFrameOwnerProperties(
      int32_t frame_routing_id,
      const blink::WebFrameOwnerProperties& properties);
  void OnUpdateTitle(const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnUpdateEncoding(const std::string& encoding);
  void OnBeginNavigation(const CommonNavigationParams& common_params,
                         const BeginNavigationParams& begin_params);
  void OnDispatchLoad();
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params,
      int reset_token);
  void OnAccessibilityLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);
  void OnAccessibilityFindInPageResult(
      const AccessibilityHostMsg_FindInPageResultParams& params);
  void OnAccessibilityChildFrameHitTestResult(const gfx::Point& point,
                                              int hit_obj_id);
  void OnAccessibilitySnapshotResponse(
      int callback_id,
      const AXContentTreeUpdate& snapshot);
  void OnToggleFullscreen(bool enter_fullscreen);
  void OnDidStartLoading(bool to_different_document);
  void OnDidStopLoading();
  void OnDidChangeLoadProgress(double load_progress);
  void OnSerializeAsMHTMLResponse(
      int job_id,
      bool success,
      const std::set<std::string>& digests_of_uris_of_serialized_resources);

#if defined(USE_EXTERNAL_POPUP_MENU)
  void OnShowPopup(const FrameHostMsg_ShowPopup_Params& params);
  void OnHidePopup();
#endif

  // Registers Mojo interfaces that this frame host makes available.
  void RegisterMojoInterfaces();

  // Resets any waiting state of this RenderFrameHost that is no longer
  // relevant.
  void ResetWaitingState();

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  // Returns whether the given origin is allowed to commit in the current
  // RenderFrameHost. The |url| is used to ensure it matches the origin in cases
  // where it is applicable. This is a more conservative check than
  // RenderProcessHost::FilterURL, since it will be used to kill processes that
  // commit unauthorized origins.
  bool CanCommitOrigin(const url::Origin& origin, const GURL& url);

  // Asserts that the given RenderFrameHostImpl is part of the same browser
  // context (and crashes if not), then returns whether the given frame is
  // part of the same site instance.
  bool IsSameSiteInstance(RenderFrameHostImpl* other_render_frame_host);

  // Informs the content client that geolocation permissions were used.
  void DidUseGeolocationPermission();

  // Returns whether the current RenderProcessHost has read access to all the
  // files reported in |state|.
  bool CanAccessFilesOfPageState(const PageState& state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |validated_state|.  It is important that the PageState has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromPageState(const PageState& validated_state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |body|.  It is important that the ResourceRequestBody has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromResourceRequestBody(
      const ResourceRequestBodyImpl& body);

  void UpdatePermissionsForNavigation(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params);

  // Returns true if the ExecuteJavaScript() API can be used on this host.
  bool CanExecuteJavaScript();

  // Map a routing ID from a frame in the same frame tree to a globally
  // unique AXTreeID.
  AXTreeIDRegistry::AXTreeID RoutingIDToAXTreeID(int routing_id);

  // Map a browser plugin instance ID to the AXTreeID of the plugin's
  // main frame.
  AXTreeIDRegistry::AXTreeID BrowserPluginInstanceIDToAXTreeID(int routing_id);

  // Convert the content-layer-specific AXContentNodeData to a general-purpose
  // AXNodeData structure.
  void AXContentNodeDataToAXNodeData(const AXContentNodeData& src,
                                     ui::AXNodeData* dst);

  // Convert the content-layer-specific AXContentTreeData to a general-purpose
  // AXTreeData structure.
  void AXContentTreeDataToAXTreeData(ui::AXTreeData* dst);

  // Returns the RenderWidgetHostView used for accessibility. For subframes,
  // this function will return the platform view on the main frame; for main
  // frames, it will return the current frame's view.
  RenderWidgetHostViewBase* GetViewForAccessibility();

  // Sends a navigate message to the RenderFrame and notifies DevTools about
  // navigation happening. Should be used instead of sending the message
  // directly.
  void SendNavigateMessage(
      const content::CommonNavigationParams& common_params,
      const content::StartNavigationParams& start_params,
      const content::RequestNavigationParams& request_params);

  // Returns the child FrameTreeNode if |child_frame_routing_id| is an
  // immediate child of this FrameTreeNode.  |child_frame_routing_id| is
  // considered untrusted, so the renderer process is killed if it refers to a
  // FrameTreeNode that is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(int32_t child_frame_routing_id,
                                    bad_message::BadMessageReason reason);

  // Creates a Web Bluetooth Service owned by the frame.
  void CreateWebBluetoothService(
      mojo::InterfaceRequest<blink::mojom::WebBluetoothService> request);

  // Deletes the Web Bluetooth Service owned by the frame.
  void DeleteWebBluetoothService();

  // Allows tests to disable the swapout event timer to simulate bugs that
  // happen before it fires (to avoid flakiness).
  void DisableSwapOutTimerForTesting();

  // For now, RenderFrameHosts indirectly keep RenderViewHosts alive via a
  // refcount that calls Shutdown when it reaches zero.  This allows each
  // RenderFrameHostManager to just care about RenderFrameHosts, while ensuring
  // we have a RenderViewHost for each RenderFrameHost.
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  RenderViewHostImpl* render_view_host_;

  RenderFrameHostDelegate* delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  scoped_refptr<SiteInstanceImpl> site_instance_;

  // The renderer process this RenderFrameHost is associated with. It is
  // equivalent to the result of site_instance_->GetProcess(), but that
  // method has the side effect of creating the process if it doesn't exist.
  // Cache a pointer to avoid unnecessary process creation.
  RenderProcessHost* process_;

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

  // The active parent RenderFrameHost for this frame, if it is a subframe.
  // Null for the main frame.  This is cached because the parent FrameTreeNode
  // may change its current RenderFrameHost while this child is pending
  // deletion, and GetParent() should never return a different value.
  RenderFrameHostImpl* parent_;

  // Track this frame's last committed URL.
  GURL last_committed_url_;

  // The most recent non-error URL to commit in this frame.  Remove this in
  // favor of GetLastCommittedURL() once PlzNavigate is enabled or cross-process
  // transfers work for net errors.  See https://crbug.com/588314.
  GURL last_successful_url_;

  // The mapping of pending JavaScript calls created by
  // ExecuteJavaScript and their corresponding callbacks.
  std::map<int, JavaScriptResultCallback> javascript_callbacks_;
  std::map<uint64_t, VisualStateCallback> visual_state_callbacks_;

  // RenderFrameHosts that need management of the rendering and input events
  // for their frame subtrees require RenderWidgetHosts. This typically
  // means frames that are rendered in different processes from their parent
  // frames.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  RenderWidgetHostImpl* render_widget_host_;

  int routing_id_;

  // Boolean indicating whether this RenderFrameHost is being actively used or
  // is waiting for FrameHostMsg_SwapOut_ACK and thus pending deletion.
  bool is_waiting_for_swapout_ack_;

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  Currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  bool render_frame_created_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them. This will be true when a RenderFrameHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderFrameHost.
  bool navigations_suspended_;

  // Holds the parameters for a suspended navigation. This can only happen while
  // this RFH is the pending RenderFrameHost of a RenderFrameHostManager. There
  // will only ever be one suspended navigation, because RenderFrameHostManager
  // will destroy the pending RenderFrameHost and create a new one if a second
  // navigation occurs.
  // PlzNavigate: unused as navigations are never suspended.
  std::unique_ptr<NavigationParams> suspended_nav_params_;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Set to true when there is a pending FrameMsg_BeforeUnload message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_ack_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true). Currently only cross-site navigations
  // require a beforeUnload/unload ACK.
  // PlzNavigate: all navigations require a beforeUnload ACK.
  bool unload_ack_is_for_navigation_;

  // Indicates whether this RenderFrameHost is in the process of loading a
  // document or not.
  bool is_loading_;

  // PlzNavigate
  // Used to track whether a commit is expected in this frame. Only used in
  // tests.
  bool pending_commit_;

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.  Tracking this
  // allows us to send things like title and state updates to the latest
  // relevant NavigationEntry.
  int nav_entry_id_;

  // Used to swap out or shut down this RFH when the unload event is taking too
  // long to execute, depending on the number of active frames in the
  // SiteInstance.  May be null in tests.
  std::unique_ptr<TimeoutMonitor> swapout_event_monitor_timeout_;

  std::unique_ptr<shell::InterfaceRegistry> interface_registry_;
  std::unique_ptr<shell::InterfaceProvider> remote_interfaces_;

#if defined(OS_ANDROID)
  std::unique_ptr<ServiceRegistryAndroid> service_registry_android_;
#endif

  std::unique_ptr<WebBluetoothServiceImpl> web_bluetooth_service_;

  // The object managing the accessibility tree for this frame.
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // This is nonzero if we sent an accessibility reset to the renderer and
  // we're waiting for an IPC containing this reset token (sequentially
  // assigned) and a complete replacement accessibility tree.
  int accessibility_reset_token_;

  // A count of the number of times we needed to reset accessibility, so
  // we don't keep trying to reset forever.
  int accessibility_reset_count_;

  // The last AXContentTreeData for this frame received from the RenderFrame.
  AXContentTreeData ax_content_tree_data_;

  // The AX tree ID of the embedder, if this is a browser plugin guest.
  AXTreeIDRegistry::AXTreeID browser_plugin_embedder_ax_tree_id_;

  // The mapping from callback id to corresponding callback for pending
  // accessibility tree snapshot calls created by RequestAXTreeSnapshot.
  std::map<int, AXTreeSnapshotCallback> ax_tree_snapshot_callbacks_;

  // Callback when an event is received, for testing.
  base::Callback<void(RenderFrameHostImpl*, ui::AXEvent, int)>
      accessibility_testing_callback_;
  // The most recently received accessibility tree - for testing only.
  std::unique_ptr<ui::AXTree> ax_tree_for_testing_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_;

  // PlzNavigate: Owns the stream used in navigations to store the body of the
  // response once it has started.
  std::unique_ptr<StreamHandle> stream_handle_;

  // Context shared for each mojom::PermissionService instance created for this
  // RFH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // The frame's Mojo Shell service.
  std::unique_ptr<FrameMojoShell> frame_mojo_shell_;

  // Holder of Mojo connection with ImageDownloader service in RenderFrame.
  content::mojom::ImageDownloaderPtr mojo_image_downloader_;

  // Tracks a navigation happening in this frame. Note that while there can be
  // two navigations in the same FrameTreeNode, there can only be one
  // navigation per RenderFrameHost.
  // PlzNavigate: before the navigation is ready to be committed, the
  // NavigationHandle for it is owned by the NavigationRequest.
  std::unique_ptr<NavigationHandleImpl> navigation_handle_;

  // The associated WebUIImpl and its type. They will be set if the current
  // document is from WebUI source. Otherwise they will be null and
  // WebUI::kNoWebUI, respectively.
  std::unique_ptr<WebUIImpl> web_ui_;
  WebUI::TypeID web_ui_type_;

  // The pending WebUIImpl and its type. These values will be used exclusively
  // for same-site navigations to keep a transition of a WebUI in a pending
  // state until the navigation commits.
  std::unique_ptr<WebUIImpl> pending_web_ui_;
  WebUI::TypeID pending_web_ui_type_;

  // If true the associated WebUI should be reused when CommitPendingWebUI is
  // called (no pending instance should be set).
  bool should_reuse_web_ui_;

  // PlzNavigate: The LoFi state of the last navigation. This is used during
  // history navigation of subframes to ensure that subframes navigate with the
  // same LoFi status as the top-level frame.
  LoFiState last_navigation_lofi_state_;

  mojo::Binding<mojom::FrameHost> frame_host_binding_;
  mojom::FramePtr frame_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
