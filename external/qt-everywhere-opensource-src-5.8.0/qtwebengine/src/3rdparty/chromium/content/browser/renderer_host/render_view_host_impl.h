// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/drag_event_source_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/window_container_type.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

class SkBitmap;
struct ViewHostMsg_CreateWindow_Params;

namespace content {

class PageState;
class RenderWidgetHostDelegate;
class SessionStorageNamespace;
struct FileChooserFileInfo;
struct FileChooserParams;
struct FrameReplicationState;

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
//
// DEPRECATED: RenderViewHostImpl is being removed as part of the SiteIsolation
// project. New code should not be added here, but to either RenderFrameHostImpl
// (if frame specific) or WebContentsImpl (if page specific).
//
// For context, please see https://crbug.com/467770 and
// http://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewHostImpl : public RenderViewHost,
                                          public RenderWidgetHostOwnerDelegate,
                                          public RenderProcessHostObserver {
 public:
  // Convenience function, just like RenderViewHost::FromID.
  static RenderViewHostImpl* FromID(int render_process_id, int render_view_id);

  // Convenience function, just like RenderViewHost::From.
  static RenderViewHostImpl* From(RenderWidgetHost* rwh);

  RenderViewHostImpl(SiteInstance* instance,
                     std::unique_ptr<RenderWidgetHostImpl> widget,
                     RenderViewHostDelegate* delegate,
                     int32_t main_frame_routing_id,
                     bool swapped_out,
                     bool has_initialized_audio_host);
  ~RenderViewHostImpl() override;

  // Shuts down this RenderViewHost and deletes it.
  void ShutdownAndDestroy();

  // RenderViewHost implementation.
  bool Send(IPC::Message* msg) override;
  RenderWidgetHostImpl* GetWidget() const override;
  RenderProcessHost* GetProcess() const override;
  int GetRoutingID() const override;
  RenderFrameHost* GetMainFrame() override;
  void AllowBindings(int binding_flags) override;
  void ClearFocusedElement() override;
  bool IsFocusedElementEditable() override;
  void DirectoryEnumerationFinished(
      int request_id,
      const std::vector<base::FilePath>& files) override;
  void DisableScrollbarsForThreshold(const gfx::Size& size) override;
  void DragSourceEndedAt(int client_x,
                         int client_y,
                         int screen_x,
                         int screen_y,
                         blink::WebDragOperation operation) override;
  void DragSourceSystemDragEnded() override;
  // |drop_data| must have been filtered. The embedder should call
  // FilterDropData before passing the drop data to RVHI.
  void DragTargetDragEnter(const DropData& drop_data,
                           const gfx::Point& client_pt,
                           const gfx::Point& screen_pt,
                           blink::WebDragOperationsMask operations_allowed,
                           int key_modifiers) override;
  void DragTargetDragEnterWithMetaData(
      const std::vector<DropData::Metadata>& metadata,
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) override;
  void DragTargetDragOver(const gfx::Point& client_pt,
                          const gfx::Point& screen_pt,
                          blink::WebDragOperationsMask operations_allowed,
                          int key_modifiers) override;
  void DragTargetDragLeave() override;
  // |drop_data| must have been filtered. The embedder should call
  // FilterDropData before passing the drop data to RVHI.
  void DragTargetDrop(const DropData& drop_data,
                      const gfx::Point& client_pt,
                      const gfx::Point& screen_pt,
                      int key_modifiers) override;
  void FilterDropData(DropData* drop_data) override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  void EnablePreferredSizeMode() override;
  void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const blink::WebMediaPlayerAction& action) override;
  void ExecutePluginActionAtLocation(
      const gfx::Point& location,
      const blink::WebPluginAction& action) override;
  RenderViewHostDelegate* GetDelegate() const override;
  int GetEnabledBindings() const override;
  SiteInstanceImpl* GetSiteInstance() const override;
  bool IsRenderViewLive() const override;
  void NotifyMoveOrResizeStarted() override;
  void SetWebUIProperty(const std::string& name,
                        const std::string& value) override;
  void Zoom(PageZoom zoom) override;
  void SyncRendererPrefs() override;
  WebPreferences GetWebkitPreferences() override;
  void UpdateWebkitPreferences(const WebPreferences& prefs) override;
  void OnWebkitPreferencesChanged() override;
  void SelectWordAroundCaret() override;

  // RenderProcessHostObserver implementation
  void RenderProcessReady(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;

  void set_delegate(RenderViewHostDelegate* d) {
    CHECK(d);  // http://crbug.com/82827
    delegate_ = d;
  }

  // Set up the RenderView child process. Virtual because it is overridden by
  // TestRenderViewHost.
  // The |opener_route_id| parameter indicates which RenderView created this
  // (MSG_ROUTING_NONE if none). If |max_page_id| is larger than -1, the
  // RenderView is told to start issuing page IDs at |max_page_id| + 1.
  // |window_was_created_with_opener| is true if this top-level frame was
  // created with an opener. (The opener may have been closed since.)
  // The |proxy_route_id| is only used when creating a RenderView in swapped out
  // state.  |replicated_frame_state| contains replicated data for the
  // top-level frame, such as its name and sandbox flags.
  virtual bool CreateRenderView(
      int opener_frame_route_id,
      int proxy_route_id,
      int32_t max_page_id,
      const FrameReplicationState& replicated_frame_state,
      bool window_was_created_with_opener);

  base::TerminationStatus render_view_termination_status() const {
    return render_view_termination_status_;
  }

  // Tracks whether this RenderViewHost is in an active state (rather than
  // pending swap out or swapped out), according to its main frame
  // RenderFrameHost.
  bool is_active() const { return is_active_; }
  void set_is_active(bool is_active) { is_active_ = is_active; }

  // Tracks whether this RenderViewHost is swapped out, according to its main
  // frame RenderFrameHost.
  void set_is_swapped_out(bool is_swapped_out) {
    is_swapped_out_ = is_swapped_out;
  }

  // TODO(creis): Remove as part of http://crbug.com/418265.
  bool is_waiting_for_close_ack() const { return is_waiting_for_close_ack_; }

  // Tells the renderer process to run the page's unload handler.
  // A ClosePage_ACK ack is sent back when the handler execution completes.
  void ClosePage();

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  void ClosePageIgnoringUnloadEvents();

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  // Notifies the RenderViewHost that its load state changed.
  void LoadStateChanged(const GURL& url,
                        const net::LoadStateWithParam& load_state,
                        uint64_t upload_position,
                        uint64_t upload_size);

  bool SuddenTerminationAllowed() const;
  void set_sudden_termination_allowed(bool enabled) {
    sudden_termination_allowed_ = enabled;
  }

  // Creates a new RenderView with the given route id.
  void CreateNewWindow(int32_t route_id,
                       int32_t main_frame_route_id,
                       int32_t main_frame_widget_route_id,
                       const ViewHostMsg_CreateWindow_Params& params,
                       SessionStorageNamespace* session_storage_namespace);

  // Creates a new RenderWidget with the given route id.  |popup_type| indicates
  // if this widget is a popup and what kind of popup it is (select, autofill).
  void CreateNewWidget(int32_t route_id, blink::WebPopupType popup_type);

  // Creates a full screen RenderWidget.
  void CreateNewFullscreenWidget(int32_t route_id);

  // TODO(creis): Remove after debugging https:/crbug.com/575245.
  int main_frame_routing_id() const {
    return main_frame_routing_id_;
  }

  void set_main_frame_routing_id(int routing_id) {
    main_frame_routing_id_ = routing_id;
  }

  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          size_t start_offset,
                                          size_t end_offset);

  // Increases the refcounting on this RVH. This is done by the FrameTree on
  // creation of a RenderFrameHost or RenderFrameProxyHost.
  void increment_ref_count() { ++frames_ref_count_; }

  // Decreases the refcounting on this RVH. This is done by the FrameTree on
  // destruction of a RenderFrameHost or RenderFrameProxyHost.
  void decrement_ref_count() { --frames_ref_count_; }

  // Returns the refcount on this RVH, that is the number of RenderFrameHosts
  // and RenderFrameProxyHosts currently using it.
  int ref_count() { return frames_ref_count_; }

  // NOTE: Do not add functions that just send an IPC message that are called in
  // one or two places. Have the caller send the IPC message directly (unless
  // the caller places are in different platforms, in which case it's better
  // to keep them consistent).

 protected:
  // RenderWidgetHostOwnerDelegate overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void RenderWidgetDidInit() override;
  void RenderWidgetWillSetIsLoading(bool is_loading) override;
  void RenderWidgetGotFocus() override;
  void RenderWidgetDidForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) override;
  bool MayRenderWidgetForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) override;

  // IPC message handlers.
  void OnShowView(int route_id,
                  WindowOpenDisposition disposition,
                  const gfx::Rect& initial_rect,
                  bool user_gesture);
  void OnShowWidget(int route_id, const gfx::Rect& initial_rect);
  void OnShowFullscreenWidget(int route_id);
  void OnRenderProcessGone(int status, int error_code);
  void OnUpdateState(int32_t page_id, const PageState& state);
  void OnUpdateTargetURL(const GURL& url);
  void OnClose();
  void OnRequestMove(const gfx::Rect& pos);
  void OnDocumentAvailableInMainFrame(bool uses_temporary_zoom_level);
  void OnDidContentsPreferredSizeChange(const gfx::Size& new_size);
  void OnPasteFromSelectionClipboard();
  void OnRouteCloseEvent();
  void OnStartDragging(const DropData& drop_data,
                       blink::WebDragOperationsMask operations_allowed,
                       const SkBitmap& bitmap,
                       const gfx::Vector2d& bitmap_offset_in_dip,
                       const DragEventSourceInfo& event_info);
  void OnUpdateDragCursor(blink::WebDragOperation drag_operation);
  void OnTakeFocus(bool reverse);
  void OnFocusedNodeChanged(bool is_editable_node,
                            const gfx::Rect& node_bounds_in_viewport);
  void OnClosePageACK();
  void OnDidZoomURL(double zoom_level, const GURL& url);
  void OnFocusedNodeTouched(bool editable);
  void OnFocus();

 private:
  // TODO(nasko): Temporarily friend RenderFrameHostImpl, so we don't duplicate
  // utility functions and state needed in both classes, while we move frame
  // specific code away from this class.
  friend class RenderFrameHostImpl;
  friend class TestRenderViewHost;
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, BasicRenderFrameHost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, RoutingIdSane);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CleanUpSwappedOutRVHOnProcessCrash);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigateMainFrameToChildSite);

  // Send RenderViewReady to observers once the process is launched, but not
  // re-entrantly.
  void PostRenderViewReady();
  void RenderViewReady();

  // TODO(creis): Move to a private namespace on RenderFrameHostImpl.
  // Delay to wait on closing the WebContents for a beforeunload/unload handler
  // to fire.
  static const int64_t kUnloadTimeoutMS;

  // Returns the content specific prefs for this RenderViewHost.
  // TODO(creis): Move most of this method to RenderProcessHost, since it's
  // mostly the same across all RVHs in a process.  Move the rest to RFH.
  // See https://crbug.com/304341.
  WebPreferences ComputeWebkitPrefs();

  // 1. Grants permissions to URL (if any)
  // 2. Grants permissions to filenames
  // 3. Grants permissions to file system files.
  // 4. Register the files with the IsolatedContext.
  void GrantFileAccessFromDropData(DropData* drop_data);

  // The RenderWidgetHost.
  std::unique_ptr<RenderWidgetHostImpl> render_widget_host_;

  // The number of RenderFrameHosts which have a reference to this RVH.
  int frames_ref_count_;

  // Our delegate, which wants to know about changes in the RenderView.
  RenderViewHostDelegate* delegate_;

  // The SiteInstance associated with this RenderViewHost.  All pages drawn
  // in this RenderViewHost are part of this SiteInstance.  Cannot change
  // over time.
  scoped_refptr<SiteInstanceImpl> instance_;

  // A bitwise OR of bindings types that have been enabled for this RenderView.
  // See BindingsPolicy for details.
  int enabled_bindings_;

  // The most recent page ID we've heard from the renderer process.  This is
  // used as context when other session history related IPCs arrive.
  // TODO(creis): Allocate this in WebContents/NavigationController instead.
  int32_t page_id_;

  // Tracks whether this RenderViewHost is in an active state.  False if the
  // main frame is pending swap out, pending deletion, or swapped out, because
  // it is not visible to the user in any of these cases.
  bool is_active_;

  // Tracks whether the main frame RenderFrameHost is swapped out.  Unlike
  // is_active_, this is false when the frame is pending swap out or deletion.
  // TODO(creis): Remove this when we no longer use swappedout://.
  // See http://crbug.com/357747.
  bool is_swapped_out_;

  // Routing ID for the main frame's RenderFrameHost.
  int main_frame_routing_id_;

  // Set to true when waiting for a ViewHostMsg_ClosePageACK.
  // TODO(creis): Move to RenderFrameHost and RenderWidgetHost.
  // See http://crbug.com/418265.
  bool is_waiting_for_close_ack_;

  // True if the render view can be shut down suddenly.
  bool sudden_termination_allowed_;

  // The termination status of the last render view that terminated.
  base::TerminationStatus render_view_termination_status_;

  // True if the current focused element is editable.
  bool is_focused_element_editable_;

  // This is updated every time UpdateWebkitPreferences is called. That method
  // is in turn called when any of the settings change that the WebPreferences
  // values depend on.
  std::unique_ptr<WebPreferences> web_preferences_;

  bool updating_web_preferences_;

  bool render_view_ready_on_process_launch_;

  base::WeakPtrFactory<RenderViewHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
