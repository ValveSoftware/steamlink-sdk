// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/common/content_export.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class SkBitmap;
struct ViewHostMsg_CreateWindow_Params;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace base {
class ListValue;
class TimeTicks;
}

namespace IPC {
class Message;
}

namespace gfx {
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
struct FileChooserParams;
struct GlobalRequestID;
struct NativeWebKeyboardEvent;
struct Referrer;
struct RendererPreferences;
struct WebPreferences;

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

  // The RenderView is being constructed (message sent to the renderer process
  // to construct a RenderView).  Now is a good time to send other setup events
  // to the RenderView.  This precedes any other commands to the RenderView.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // The RenderView has been constructed.
  virtual void RenderViewReady(RenderViewHost* render_view_host) {}

  // The process containing the RenderView exited somehow (either cleanly,
  // crash, or user kill).
  virtual void RenderViewTerminated(RenderViewHost* render_view_host,
                                    base::TerminationStatus status,
                                    int error_code) {}

  // The RenderView is going to be deleted. This is called when each
  // RenderView is going to be destroyed
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // The state for the page changed and should be updated.
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32_t page_id,
                           const PageState& state) {}

  // The destination URL has changed should be updated.
  virtual void UpdateTargetURL(RenderViewHost* render_view_host,
                               const GURL& url) {}

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

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const = 0;

  // Notification from the renderer host that blocked UI event occurred.
  // This happens when there are tab-modal dialogs. In this case, the
  // notification is needed to let us draw attention to the dialog (i.e.
  // refocus on the modal dialog, flash title etc).
  virtual void OnIgnoredUIEvent() {}

  // Notification that the RenderViewHost's load state changed.
  virtual void LoadStateChanged(const GURL& url,
                                const net::LoadStateWithParam& load_state,
                                uint64_t upload_position,
                                uint64_t upload_size) {}

  // The page wants the hosting window to activate itself (it called the
  // JavaScript window.focus() method).
  virtual void Activate() {}

  // The contents' preferred size changed.
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

  // The page is trying to open a new page (e.g. a popup window). The window
  // should be created associated with the given |route_id| in the process of
  // |source_site_instance|, but it should not be shown yet. That
  // should happen in response to ShowCreatedWindow.
  // |params.window_container_type| describes the type of RenderViewHost
  // container that is requested -- in particular, the window.open call may
  // have specified 'background' and 'persistent' in the feature string.
  //
  // The passed |params.frame_name| parameter is the name parameter that was
  // passed to window.open(), and will be empty if none was passed.
  //
  // Note: this is not called "CreateWindow" because that will clash with
  // the Windows function which is actually a #define.
  //
  // TODO(alexmos): This should be moved to RenderFrameHostDelegate, and the
  // corresponding IPC message should be sent by the RenderFrame creating the
  // new window.
  virtual void CreateNewWindow(
      SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace) {}

  // The page is trying to open a new widget (e.g. a select popup). The
  // widget should be created associated with the given |route_id| in the
  // process |render_process_id|, but it should not be shown yet. That should
  // happen in response to ShowCreatedWidget.
  // |popup_type| indicates if the widget is a popup and what kind of popup it
  // is (select, autofill...).
  virtual void CreateNewWidget(int32_t render_process_id,
                               int32_t route_id,
                               blink::WebPopupType popup_type) {}

  // Creates a full screen RenderWidget. Similar to above.
  virtual void CreateNewFullscreenWidget(int32_t render_process_id,
                                         int32_t route_id) {}

  // Show a previously created page with the specified disposition and bounds.
  // The window is identified by the route_id passed to CreateNewWindow.
  //
  // Note: this is not called "ShowWindow" because that will clash with
  // the Windows function which is actually a #define.
  virtual void ShowCreatedWindow(int process_id,
                                 int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_rect,
                                 bool user_gesture) {}

  // Show the newly created widget with the specified bounds.
  // The widget is identified by the route_id passed to CreateNewWidget.
  virtual void ShowCreatedWidget(int process_id,
                                 int route_id,
                                 const gfx::Rect& initial_rect) {}

  // Show the newly created full screen widget. Similar to above.
  virtual void ShowCreatedFullscreenWidget(int process_id, int route_id) {}

  // Returns the SessionStorageNamespace the render view should use. Might
  // create the SessionStorageNamespace on the fly.
  virtual SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance);

  // Returns a copy of the map of all session storage namespaces related
  // to this view.
  virtual SessionStorageNamespaceMap GetSessionStorageNamespaceMap();

  // Returns the zoom level for the pending navigation for the page. If there
  // is no pending navigation, this returns the zoom level for the current
  // page.
  virtual double GetPendingPageZoomLevel();

  // Returns true if the RenderViewHost will never be visible.
  virtual bool IsNeverVisible();

  // Returns the FrameTree the render view should use. Guaranteed to be constant
  // for the lifetime of the render view.
  //
  // TODO(ajwong): Remove once the main frame RenderFrameHost is no longer
  // created by the RenderViewHost.
  virtual FrameTree* GetFrameTree();

  // Optional state storage for if the Virtual Keyboard has been requested by
  // this page or not. If it has, this can be used to suppress things like the
  // link disambiguation dialog, which doesn't interact well with the virtual
  // keyboard.
  virtual void SetIsVirtualKeyboardRequested(bool requested) {}
  virtual bool IsVirtualKeyboardRequested();

  // Whether the user agent is overridden using the Chrome for Android "Request
  // Desktop Site" feature.
  virtual bool IsOverridingUserAgent();

 protected:
  virtual ~RenderViewHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
