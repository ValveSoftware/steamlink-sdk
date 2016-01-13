// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/page_transition_types.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class SkBitmap;
struct ViewHostMsg_CreateWindow_Params;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct ViewMsg_PostMessage_Params;
struct WebPreferences;

namespace base {
class ListValue;
class TimeTicks;
}

namespace IPC {
class Message;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace content {

class BrowserContext;
class CrossSiteTransferringRequest;
class FrameTree;
class PageState;
class RenderViewHost;
class RenderViewHostDelegateView;
class SessionStorageNamespace;
class SiteInstance;
class WebContents;
class WebContentsImpl;
struct AXEventNotificationDetails;
struct FileChooserParams;
struct GlobalRequestID;
struct NativeWebKeyboardEvent;
struct Referrer;
struct RendererPreferences;

//
// RenderViewHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderViewHost.
//
//  This interface currently encompasses every type of message that was
//  previously being sent by WebContents itself. Some of these notifications
//  may not be relevant to all users of RenderViewHost and we should consider
//  exposing a more generic Send function on RenderViewHost and a response
//  listener here to serve that need.
class CONTENT_EXPORT RenderViewHostDelegate {
 public:
  // Returns the current delegate associated with a feature. May return NULL if
  // there is no corresponding delegate.
  virtual RenderViewHostDelegateView* GetDelegateView();

  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderViewHost* render_view_host,
                                 const IPC::Message& message);

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns NULL. DEPRECATED: Be sure to include brettw or
  // jam as reviewers before you use this method. http://crbug.com/82582
  virtual WebContents* GetAsWebContents();

  // Return the rect where to display the resize corner, if any, otherwise
  // an empty rect.
  virtual gfx::Rect GetRootWindowResizerRect() const = 0;

  // The RenderView is being constructed (message sent to the renderer process
  // to construct a RenderView).  Now is a good time to send other setup events
  // to the RenderView.  This precedes any other commands to the RenderView.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // The RenderView has been constructed.
  virtual void RenderViewReady(RenderViewHost* render_view_host) {}

  // The RenderView died somehow (crashed or was killed by the user).
  virtual void RenderViewTerminated(RenderViewHost* render_view_host,
                                    base::TerminationStatus status,
                                    int error_code) {}

  // The RenderView is going to be deleted. This is called when each
  // RenderView is going to be destroyed
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // The state for the page changed and should be updated.
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const PageState& state) {}

  // The destination URL has changed should be updated
  virtual void UpdateTargetURL(int32 page_id, const GURL& url) {}

  // The page is trying to close the RenderView's representation in the client.
  virtual void Close(RenderViewHost* render_view_host) {}

  // The page is trying to move the RenderView's representation in the client.
  virtual void RequestMove(const gfx::Rect& new_bounds) {}

  // The pending page load was canceled.
  virtual void DidCancelLoading() {}

  // The RenderView's main frame document element is ready. This happens when
  // the document has finished parsing.
  virtual void DocumentAvailableInMainFrame(RenderViewHost* render_view_host) {}

  // The page wants to close the active view in this tab.
  virtual void RouteCloseEvent(RenderViewHost* rvh) {}

  // The page wants to post a message to the active view in this tab.
  virtual void RouteMessageEvent(
      RenderViewHost* rvh,
      const ViewMsg_PostMessage_Params& params) {}

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const = 0;

  // Returns a WebPreferences object that will be used by the renderer
  // associated with the owning render view host.
  virtual WebPreferences GetWebkitPrefs();

  // Notification the user has made a gesture while focus was on the
  // page. This is used to avoid uninitiated user downloads (aka carpet
  // bombing), see DownloadRequestLimiter for details.
  virtual void OnUserGesture() {}

  // Notification from the renderer host that blocked UI event occurred.
  // This happens when there are tab-modal dialogs. In this case, the
  // notification is needed to let us draw attention to the dialog (i.e.
  // refocus on the modal dialog, flash title etc).
  virtual void OnIgnoredUIEvent() {}

  // Notification that the renderer has become unresponsive. The
  // delegate can use this notification to show a warning to the user.
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_before_unload,
                                    bool is_during_unload) {}

  // Notification that a previously unresponsive renderer has become
  // responsive again. The delegate can use this notification to end the
  // warning shown to the user.
  virtual void RendererResponsive(RenderViewHost* render_view_host) {}

  // Notification that the RenderViewHost's load state changed.
  virtual void LoadStateChanged(const GURL& url,
                                const net::LoadStateWithParam& load_state,
                                uint64 upload_position,
                                uint64 upload_size) {}

  // The page wants the hosting window to activate/deactivate itself (it
  // called the JavaScript window.focus()/blur() method).
  virtual void Activate() {}
  virtual void Deactivate() {}

  // Notification that the view has lost capture.
  virtual void LostCapture() {}

  // Notifications about mouse events in this view.  This is useful for
  // implementing global 'on hover' features external to the view.
  virtual void HandleMouseMove() {}
  virtual void HandleMouseDown() {}
  virtual void HandleMouseLeave() {}
  virtual void HandleMouseUp() {}
  virtual void HandlePointerActivate() {}
  virtual void HandleGestureBegin() {}
  virtual void HandleGestureEnd() {}

  // Called when a file selection is to be done.
  virtual void RunFileChooser(
      RenderViewHost* render_view_host,
      const FileChooserParams& params) {}

  // Notification that the page wants to go into or out of fullscreen mode.
  virtual void ToggleFullscreenMode(bool enter_fullscreen) {}
  virtual bool IsFullscreenForCurrentTab() const;

  // The contents' preferred size changed.
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

  // The contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) {}

  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting render
  // view host.
  virtual void RequestToLockMouse(bool user_gesture,
                                  bool last_unlocked_by_target) {}

  // Notification that the view has lost the mouse lock.
  virtual void LostMouseLock() {}

  // The page is trying to open a new page (e.g. a popup window). The window
  // should be created associated with the given |route_id| in process
  // |render_process_id|, but it should not be shown yet. That should happen in
  // response to ShowCreatedWindow.
  // |params.window_container_type| describes the type of RenderViewHost
  // container that is requested -- in particular, the window.open call may
  // have specified 'background' and 'persistent' in the feature string.
  //
  // The passed |params.frame_name| parameter is the name parameter that was
  // passed to window.open(), and will be empty if none was passed.
  //
  // Note: this is not called "CreateWindow" because that will clash with
  // the Windows function which is actually a #define.
  virtual void CreateNewWindow(
      int render_process_id,
      int route_id,
      int main_frame_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace) {}

  // The page is trying to open a new widget (e.g. a select popup). The
  // widget should be created associated with the given |route_id| in the
  // process |render_process_id|, but it should not be shown yet. That should
  // happen in response to ShowCreatedWidget.
  // |popup_type| indicates if the widget is a popup and what kind of popup it
  // is (select, autofill...).
  virtual void CreateNewWidget(int render_process_id,
                               int route_id,
                               blink::WebPopupType popup_type) {}

  // Creates a full screen RenderWidget. Similar to above.
  virtual void CreateNewFullscreenWidget(int render_process_id, int route_id) {}

  // Show a previously created page with the specified disposition and bounds.
  // The window is identified by the route_id passed to CreateNewWindow.
  //
  // Note: this is not called "ShowWindow" because that will clash with
  // the Windows function which is actually a #define.
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {}

  // Show the newly created widget with the specified bounds.
  // The widget is identified by the route_id passed to CreateNewWidget.
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}

  // Show the newly created full screen widget. Similar to above.
  virtual void ShowCreatedFullscreenWidget(int route_id) {}

  // The render view has requested access to media devices listed in
  // |request|, and the client should grant or deny that permission by
  // calling |callback|.
  virtual void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback) {}

  // Returns the SessionStorageNamespace the render view should use. Might
  // create the SessionStorageNamespace on the fly.
  virtual SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance);

  // Returns a copy of the map of all session storage namespaces related
  // to this view.
  virtual SessionStorageNamespaceMap GetSessionStorageNamespaceMap();

  // Returns true if the RenderViewHost will never be visible.
  virtual bool IsNeverVisible();

  // Returns the FrameTree the render view should use. Guaranteed to be constant
  // for the lifetime of the render view.
  //
  // TODO(ajwong): Remove once the main frame RenderFrameHost is no longer
  // created by the RenderViewHost.
  virtual FrameTree* GetFrameTree();

  // Invoked when an accessibility event is received from the renderer.
  virtual void AccessibilityEventReceived(
      const std::vector<AXEventNotificationDetails>& details) {}

 protected:
  virtual ~RenderViewHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
