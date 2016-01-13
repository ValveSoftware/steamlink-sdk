// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/drag_event_source_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/window_container_type.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/window_open_disposition.h"

class SkBitmap;
class FrameMsg_Navigate;
struct AccessibilityHostMsg_EventParams;
struct AccessibilityHostMsg_LocationChangeParams;
struct MediaPlayerAction;
struct ViewHostMsg_CreateWindow_Params;
struct ViewHostMsg_ShowPopup_Params;
struct FrameMsg_Navigate_Params;
struct ViewMsg_PostMessage_Params;

namespace base {
class ListValue;
}

namespace gfx {
class Range;
}

namespace ui {
class AXTree;
struct SelectedFileInfo;
}

namespace content {

class MediaWebContentsObserver;
class ChildProcessSecurityPolicyImpl;
class PageState;
class RenderWidgetHostDelegate;
class SessionStorageNamespace;
class SessionStorageNamespaceImpl;
class TestRenderViewHost;
class TimeoutMonitor;
struct FileChooserParams;

#if defined(COMPILER_MSVC)
// RenderViewHostImpl is the bottom of a diamond-shaped hierarchy,
// with RenderWidgetHost at the root. VS warns when methods from the
// root are overridden in only one of the base classes and not both
// (in this case, RenderWidgetHostImpl provides implementations of
// many of the methods).  This is a silly warning when dealing with
// pure virtual methods that only have a single implementation in the
// hierarchy above this class, and is safe to ignore in this case.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// This implements the RenderViewHost interface that is exposed to
// embedders of content, and adds things only visible to content.
//
// The exact API of this object needs to be more thoroughly designed. Right
// now it mimics what WebContentsImpl exposed, which is a fairly large API and
// may contain things that are not relevant to a common subset of views. See
// also the comment in render_view_host_delegate.h about the size and scope of
// the delegate API.
//
// Right now, the concept of page navigation (both top level and frame) exists
// in the WebContentsImpl still, so if you instantiate one of these elsewhere,
// you will not be able to traverse pages back and forward. We need to determine
// if we want to bring that and other functionality down into this object so it
// can be shared by others.
class CONTENT_EXPORT RenderViewHostImpl
    : public RenderViewHost,
      public RenderWidgetHostImpl {
 public:
  // Keeps track of the state of the RenderViewHostImpl, particularly with
  // respect to swap out.
  enum RenderViewHostImplState {
    // The standard state for a RVH handling the communication with a
    // RenderView.
    STATE_DEFAULT = 0,
    // The RVH has sent the SwapOut request to the renderer, but has not
    // received the SwapOutACK yet. The new page has not been committed yet
    // either.
    STATE_WAITING_FOR_UNLOAD_ACK,
    // The RVH received the SwapOutACK from the RenderView, but the new page has
    // not been committed yet.
    STATE_WAITING_FOR_COMMIT,
    // The RVH is waiting for the CloseACK from the RenderView.
    STATE_WAITING_FOR_CLOSE,
    // The RVH has not received the SwapOutACK yet, but the new page has
    // committed in a different RVH. The number of active views of the RVH
    // SiteInstanceImpl is not zero. Upon reception of the SwapOutACK, the RVH
    // will be swapped out.
    STATE_PENDING_SWAP_OUT,
    // The RVH has not received the SwapOutACK yet, but the new page has
    // committed in a different RVH. The number of active views of the RVH
    // SiteInstanceImpl is zero. Upon reception of the SwapOutACK, the RVH will
    // be shutdown.
    STATE_PENDING_SHUTDOWN,
    // The RVH is swapped out, and it is being used as a placeholder to allow
    // for cross-process communication.
    STATE_SWAPPED_OUT,
  };
  // Helper function to determine whether the RVH state should contribute to the
  // number of active views of a SiteInstance or not.
  static bool IsRVHStateActive(RenderViewHostImplState rvh_state);

  // Convenience function, just like RenderViewHost::FromID.
  static RenderViewHostImpl* FromID(int render_process_id, int render_view_id);

  // |routing_id| could be a valid route id, or it could be MSG_ROUTING_NONE, in
  // which case RenderWidgetHost will create a new one.  |swapped_out| indicates
  // whether the view should initially be swapped out (e.g., for an opener
  // frame being rendered by another process). |hidden| indicates whether the
  // view is initially hidden or visible.
  //
  // The |session_storage_namespace| parameter allows multiple render views and
  // WebContentses to share the same session storage (part of the WebStorage
  // spec) space. This is useful when restoring contentses, but most callers
  // should pass in NULL which will cause a new SessionStorageNamespace to be
  // created.
  RenderViewHostImpl(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int routing_id,
      int main_frame_routing_id,
      bool swapped_out,
      bool hidden);
  virtual ~RenderViewHostImpl();

  // RenderViewHost implementation.
  virtual RenderFrameHost* GetMainFrame() OVERRIDE;
  virtual void AllowBindings(int binding_flags) OVERRIDE;
  virtual void ClearFocusedElement() OVERRIDE;
  virtual bool IsFocusedElementEditable() OVERRIDE;
  virtual void ClosePage() OVERRIDE;
  virtual void CopyImageAt(int x, int y) OVERRIDE;
  virtual void SaveImageAt(int x, int y) OVERRIDE;
  virtual void DirectoryEnumerationFinished(
      int request_id,
      const std::vector<base::FilePath>& files) OVERRIDE;
  virtual void DisableScrollbarsForThreshold(const gfx::Size& size) OVERRIDE;
  virtual void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y,
      blink::WebDragOperation operation) OVERRIDE;
  virtual void DragSourceSystemDragEnded() OVERRIDE;
  virtual void DragTargetDragEnter(
      const DropData& drop_data,
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) OVERRIDE;
  virtual void DragTargetDragOver(
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) OVERRIDE;
  virtual void DragTargetDragLeave() OVERRIDE;
  virtual void DragTargetDrop(const gfx::Point& client_pt,
                              const gfx::Point& screen_pt,
                              int key_modifiers) OVERRIDE;
  virtual void EnableAutoResize(const gfx::Size& min_size,
                                const gfx::Size& max_size) OVERRIDE;
  virtual void DisableAutoResize(const gfx::Size& new_size) OVERRIDE;
  virtual void EnablePreferredSizeMode() OVERRIDE;
  virtual void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const blink::WebMediaPlayerAction& action) OVERRIDE;
  virtual void ExecutePluginActionAtLocation(
      const gfx::Point& location,
      const blink::WebPluginAction& action) OVERRIDE;
  virtual void ExitFullscreen() OVERRIDE;
  virtual void FilesSelectedInChooser(
      const std::vector<ui::SelectedFileInfo>& files,
      FileChooserParams::Mode permissions) OVERRIDE;
  virtual RenderViewHostDelegate* GetDelegate() const OVERRIDE;
  virtual int GetEnabledBindings() const OVERRIDE;
  virtual SiteInstance* GetSiteInstance() const OVERRIDE;
  virtual bool IsRenderViewLive() const OVERRIDE;
  virtual void NotifyMoveOrResizeStarted() OVERRIDE;
  virtual void SetWebUIProperty(const std::string& name,
                                const std::string& value) OVERRIDE;
  virtual void Zoom(PageZoom zoom) OVERRIDE;
  virtual void SyncRendererPrefs() OVERRIDE;
  virtual WebPreferences GetWebkitPreferences() OVERRIDE;
  virtual void UpdateWebkitPreferences(
      const WebPreferences& prefs) OVERRIDE;
  virtual void GetAudioOutputControllers(
      const GetAudioOutputControllersCallback& callback) const OVERRIDE;
  virtual void SetWebUIHandle(mojo::ScopedMessagePipeHandle handle) OVERRIDE;
  virtual void SelectWordAroundCaret() OVERRIDE;

#if defined(OS_ANDROID)
  virtual void ActivateNearestFindResult(int request_id,
                                         float x,
                                         float y) OVERRIDE;
  virtual void RequestFindMatchRects(int current_version) OVERRIDE;
#endif

  void set_delegate(RenderViewHostDelegate* d) {
    CHECK(d);  // http://crbug.com/82827
    delegate_ = d;
  }

  // Set up the RenderView child process. Virtual because it is overridden by
  // TestRenderViewHost. If the |frame_name| parameter is non-empty, it is used
  // as the name of the new top-level frame.
  // The |opener_route_id| parameter indicates which RenderView created this
  // (MSG_ROUTING_NONE if none). If |max_page_id| is larger than -1, the
  // RenderView is told to start issuing page IDs at |max_page_id| + 1.
  // |window_was_created_with_opener| is true if this top-level frame was
  // created with an opener. (The opener may have been closed since.)
  // The |proxy_route_id| is only used when creating a RenderView in swapped out
  // state.
  virtual bool CreateRenderView(const base::string16& frame_name,
                                int opener_route_id,
                                int proxy_route_id,
                                int32 max_page_id,
                                bool window_was_created_with_opener);

  base::TerminationStatus render_view_termination_status() const {
    return render_view_termination_status_;
  }

  // Returns the content specific prefs for this RenderViewHost.
  WebPreferences GetWebkitPrefs(const GURL& url);

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  // TODO(nasko): Remove this method once all callers are converted to use
  // RenderFrameHostImpl.
  void Navigate(const FrameMsg_Navigate_Params& message);

  // Load the specified URL, this is a shortcut for Navigate().
  // TODO(nasko): Remove this method once all callers are converted to use
  // RenderFrameHostImpl.
  void NavigateToURL(const GURL& url);

  // Returns whether navigation messages are currently suspended for this
  // RenderViewHost.  Only true during a cross-site navigation, while waiting
  // for the onbeforeunload handler.
  bool are_navigations_suspended() const { return navigations_suspended_; }

  // Suspends (or unsuspends) any navigation messages from being sent from this
  // RenderViewHost.  This is called when a pending RenderViewHost is created
  // for a cross-site navigation, because we must suspend any navigations until
  // we hear back from the old renderer's onbeforeunload handler.  Note that it
  // is important that only one navigation event happen after calling this
  // method with |suspend| equal to true.  If |suspend| is false and there is
  // a suspended_nav_message_, this will send the message.  This function
  // should only be called to toggle the state; callers should check
  // are_navigations_suspended() first. If |suspend| is false, the time that the
  // user decided the navigation should proceed should be passed as
  // |proceed_time|.
  void SetNavigationsSuspended(bool suspend,
                               const base::TimeTicks& proceed_time);

  // Clears any suspended navigation state after a cross-site navigation is
  // canceled or suspended.  This is important if we later return to this
  // RenderViewHost.
  void CancelSuspendedNavigations();

  // Whether this RenderViewHost has been swapped out to be displayed by a
  // different process.
  bool IsSwappedOut() const { return rvh_state_ == STATE_SWAPPED_OUT; }

  // The current state of this RVH.
  RenderViewHostImplState rvh_state() const { return rvh_state_; }

  // Tells the renderer that this RenderView will soon be swapped out, and thus
  // not to create any new modal dialogs until it happens.  This must be done
  // separately so that the PageGroupLoadDeferrers of any current dialogs are no
  // longer on the stack when we attempt to swap it out.
  void SuppressDialogsUntilSwapOut();

  // Called when either the SwapOut request has been acknowledged or has timed
  // out.
  void OnSwappedOut(bool timed_out);

  // Called when the RenderFrameHostManager has swapped in a new
  // RenderFrameHost. Should |this| RVH switch to the pending shutdown state,
  // |pending_delete_on_swap_out| will be executed upon reception of the
  // SwapOutACK, or when the unload timer times out.
  void WasSwappedOut(const base::Closure& pending_delete_on_swap_out);

  // Set |this| as pending shutdown. |on_swap_out| will be called
  // when the SwapOutACK is received, or when the unload timer times out.
  void SetPendingShutdown(const base::Closure& on_swap_out);

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  void ClosePageIgnoringUnloadEvents();

  // Returns whether this RenderViewHost has an outstanding cross-site request.
  // Cleared when we hear the response and start to swap out the old
  // RenderViewHost, or if we hear a commit here without a network request.
  bool HasPendingCrossSiteRequest();

  // Sets whether this RenderViewHost has an outstanding cross-site request,
  // for which another renderer will need to run an onunload event handler.
  // This is called before the first navigation event for this RenderViewHost,
  // and cleared when we hear the response or commit.
  void SetHasPendingCrossSiteRequest(bool has_pending_request);

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  // The parameter links contain original URLs of all saved links.
  // The parameter local_paths contain corresponding local file paths of
  // all saved links, which matched with vector:links one by one.
  // The parameter local_directory_name is relative path of directory which
  // contain all saved auxiliary files included all sub frames and resouces.
  void GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<base::FilePath>& local_paths,
      const base::FilePath& local_directory_name);

  // Notifies the RenderViewHost that its load state changed.
  void LoadStateChanged(const GURL& url,
                        const net::LoadStateWithParam& load_state,
                        uint64 upload_position,
                        uint64 upload_size);

  bool SuddenTerminationAllowed() const;
  void set_sudden_termination_allowed(bool enabled) {
    sudden_termination_allowed_ = enabled;
  }

  // RenderWidgetHost public overrides.
  virtual void Init() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual bool IsRenderView() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual void LostMouseLock() OVERRIDE;
  virtual void ForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) OVERRIDE;
  virtual void OnPointerEventActivate() OVERRIDE;
  virtual void ForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;

  // Creates a new RenderView with the given route id.
  void CreateNewWindow(
      int route_id,
      int main_frame_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace);

  // Creates a new RenderWidget with the given route id.  |popup_type| indicates
  // if this widget is a popup and what kind of popup it is (select, autofill).
  void CreateNewWidget(int route_id, blink::WebPopupType popup_type);

  // Creates a full screen RenderWidget.
  void CreateNewFullscreenWidget(int route_id);

#if defined(OS_MACOSX)
  // Select popup menu related methods (for external popup menus).
  void DidSelectPopupMenuItem(int selected_index);
  void DidCancelPopupMenu();
#endif

#if defined(ENABLE_BROWSER_CDMS)
  MediaWebContentsObserver* media_web_contents_observer() {
    return media_web_contents_observer_.get();
  }
#endif

#if defined(OS_ANDROID)
  void DidSelectPopupMenuItems(const std::vector<int>& selected_indices);
  void DidCancelPopupMenu();
#endif

  int main_frame_routing_id() const {
    return main_frame_routing_id_;
  }

  // Set the opener to null in the renderer process.
  void DisownOpener();

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process, and the accessibility tree it sent can be
  // retrieved using accessibility_tree_for_testing().
  void SetAccessibilityCallbackForTesting(
      const base::Callback<void(ui::AXEvent, int)>& callback);

  // Only valid if SetAccessibilityCallbackForTesting was called and
  // the callback was run at least once. Returns a snapshot of the
  // accessibility tree received from the renderer as of the last time
  // an accessibility notification was received.
  const ui::AXTree& ax_tree_for_testing() {
    CHECK(ax_tree_.get());
    return *ax_tree_.get();
  }

  // Set accessibility callbacks.
  void SetAccessibilityLayoutCompleteCallbackForTesting(
      const base::Closure& callback);
  void SetAccessibilityLoadCompleteCallbackForTesting(
      const base::Closure& callback);
  void SetAccessibilityOtherCallbackForTesting(
      const base::Closure& callback);

  bool is_waiting_for_beforeunload_ack() {
    return is_waiting_for_beforeunload_ack_;
  }

  // Whether the RVH is waiting for the unload ack from the renderer.
  bool IsWaitingForUnloadACK() const;

  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          size_t start_offset,
                                          size_t end_offset);

  // Update the FrameTree to use this RenderViewHost's main frame
  // RenderFrameHost. Called when the RenderViewHost is committed.
  //
  // TODO(ajwong): Remove once RenderViewHost no longer owns the main frame
  // RenderFrameHost.
  void AttachToFrameTree();

  // Increases the refcounting on this RVH. This is done by the FrameTree on
  // creation of a RenderFrameHost.
  void increment_ref_count() { ++frames_ref_count_; }

  // Decreases the refcounting on this RVH. This is done by the FrameTree on
  // destruction of a RenderFrameHost.
  void decrement_ref_count() { --frames_ref_count_; }

  // Returns the refcount on this RVH, that is the number of RenderFrameHosts
  // currently using it.
  int ref_count() { return frames_ref_count_; }

  // NOTE: Do not add functions that just send an IPC message that are called in
  // one or two places. Have the caller send the IPC message directly (unless
  // the caller places are in different platforms, in which case it's better
  // to keep them consistent).

 protected:
  // RenderWidgetHost protected overrides.
  virtual void OnUserGesture() OVERRIDE;
  virtual void NotifyRendererUnresponsive() OVERRIDE;
  virtual void NotifyRendererResponsive() OVERRIDE;
  virtual void OnRenderAutoResized(const gfx::Size& size) OVERRIDE;
  virtual void RequestToLockMouse(bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // IPC message handlers.
  void OnShowView(int route_id,
                  WindowOpenDisposition disposition,
                  const gfx::Rect& initial_pos,
                  bool user_gesture);
  void OnShowWidget(int route_id, const gfx::Rect& initial_pos);
  void OnShowFullscreenWidget(int route_id);
  void OnRunModal(int opener_id, IPC::Message* reply_msg);
  void OnRenderViewReady();
  void OnRenderProcessGone(int status, int error_code);
  void OnUpdateState(int32 page_id, const PageState& state);
  void OnUpdateTargetURL(int32 page_id, const GURL& url);
  void OnClose();
  void OnRequestMove(const gfx::Rect& pos);
  void OnDocumentAvailableInMainFrame(bool uses_temporary_zoom_level);
  void OnToggleFullscreen(bool enter_fullscreen);
  void OnDidContentsPreferredSizeChange(const gfx::Size& new_size);
  void OnDidChangeScrollOffset();
  void OnPasteFromSelectionClipboard();
  void OnRouteCloseEvent();
  void OnRouteMessageEvent(const ViewMsg_PostMessage_Params& params);
  void OnStartDragging(const DropData& drop_data,
                       blink::WebDragOperationsMask operations_allowed,
                       const SkBitmap& bitmap,
                       const gfx::Vector2d& bitmap_offset_in_dip,
                       const DragEventSourceInfo& event_info);
  void OnUpdateDragCursor(blink::WebDragOperation drag_operation);
  void OnTargetDropACK();
  void OnTakeFocus(bool reverse);
  void OnFocusedNodeChanged(bool is_editable_node);
  void OnUpdateInspectorSetting(const std::string& key,
                                const std::string& value);
  void OnClosePageACK();
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params);
  void OnAccessibilityLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);
  void OnDidZoomURL(double zoom_level, const GURL& url);
  void OnRunFileChooser(const FileChooserParams& params);
  void OnFocusedNodeTouched(bool editable);

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void OnShowPopup(const ViewHostMsg_ShowPopup_Params& params);
  void OnHidePopup();
#endif

 private:
  // TODO(nasko): Temporarily friend RenderFrameHostImpl, so we don't duplicate
  // utility functions and state needed in both classes, while we move frame
  // specific code away from this class.
  friend class RenderFrameHostImpl;
  friend class TestRenderViewHost;
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, BasicRenderFrameHost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, RoutingIdSane);

  // TODO(creis): Move to a private namespace on RenderFrameHostImpl.
  // Delay to wait on closing the WebContents for a beforeunload/unload handler
  // to fire.
  static const int kUnloadTimeoutMS;

  // Updates the state of this RenderViewHost and clears any waiting state
  // that is no longer relevant.
  void SetState(RenderViewHostImplState rvh_state);

  bool CanAccessFilesOfPageState(const PageState& state) const;

  // The number of RenderFrameHosts which have a reference to this RVH.
  int frames_ref_count_;

  // Our delegate, which wants to know about changes in the RenderView.
  RenderViewHostDelegate* delegate_;

  // The SiteInstance associated with this RenderViewHost.  All pages drawn
  // in this RenderViewHost are part of this SiteInstance.  Should not change
  // over time.
  scoped_refptr<SiteInstanceImpl> instance_;

  // true if we are currently waiting for a response for drag context
  // information.
  bool waiting_for_drag_context_response_;

  // A bitwise OR of bindings types that have been enabled for this RenderView.
  // See BindingsPolicy for details.
  int enabled_bindings_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them.  This will be true when a RenderViewHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderViewHost.
  // TODO(nasko): Move to RenderFrameHost, as this is per-frame state.
  bool navigations_suspended_;

  // We only buffer the params for a suspended navigation while we have a
  // pending RVH for a WebContentsImpl.  There will only ever be one suspended
  // navigation, because WebContentsImpl will destroy the pending RVH and create
  // a new one if a second navigation occurs.
  // TODO(nasko): Move to RenderFrameHost, as this is per-frame state.
  scoped_ptr<FrameMsg_Navigate_Params> suspended_nav_params_;

  // The current state of this RVH.
  // TODO(nasko): Move to RenderFrameHost, as this is per-frame state.
  RenderViewHostImplState rvh_state_;

  // Routing ID for the main frame's RenderFrameHost.
  int main_frame_routing_id_;

  // If we were asked to RunModal, then this will hold the reply_msg that we
  // must return to the renderer to unblock it.
  IPC::Message* run_modal_reply_msg_;
  // This will hold the routing id of the RenderView that opened us.
  int run_modal_opener_id_;

  // Set to true when there is a pending ViewMsg_ShouldClose message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  // TODO(nasko): Move to RenderFrameHost, as this is per-frame state.
  bool is_waiting_for_beforeunload_ack_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderViewHost in
  // the case of a cross-site transition ( = true).
  // TODO(nasko): Move to RenderFrameHost, as this is per-frame state.
  bool unload_ack_is_for_cross_site_transition_;

  // Accessibility callback for testing.
  base::Callback<void(ui::AXEvent, int)> accessibility_testing_callback_;

  // The most recently received accessibility tree - for testing only.
  scoped_ptr<ui::AXTree> ax_tree_;

  // True if the render view can be shut down suddenly.
  bool sudden_termination_allowed_;

  // The termination status of the last render view that terminated.
  base::TerminationStatus render_view_termination_status_;

  // Set to true if we requested the on screen keyboard to be displayed.
  bool virtual_keyboard_requested_;

#if defined(ENABLE_BROWSER_CDMS)
  // Manages all the media player and CDM managers and forwards IPCs to them.
  scoped_ptr<MediaWebContentsObserver> media_web_contents_observer_;
#endif

  // Used to swap out or shutdown this RVH when the unload event is taking too
  // long to execute, depending on the number of active views in the
  // SiteInstance.
  // TODO(nasko): Move to RenderFrameHost, as this is per-frame state.
  scoped_ptr<TimeoutMonitor> unload_event_monitor_timeout_;

  // Called after receiving the SwapOutACK when the RVH is in state pending
  // shutdown. Also called if the unload timer times out.
  // TODO(nasko): Move to RenderFrameHost, as this is per-frame state.
  base::Closure pending_shutdown_on_swap_out_;

  base::WeakPtrFactory<RenderViewHostImpl> weak_factory_;

  // True if the current focused element is editable.
  bool is_focused_element_editable_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImpl);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
