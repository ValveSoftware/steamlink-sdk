// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_

// This file describes various types used to describe and filter notifications
// that pass through the NotificationService.
//
// Only notifications that are fired from the content module should be here. We
// should never have a notification that is fired by the embedder and listened
// to by content.
namespace content {

enum NotificationType {
  NOTIFICATION_CONTENT_START = 0,

  // General -----------------------------------------------------------------

  // Special signal value to represent an interest in all notifications.
  // Not valid when posting a notification.
  NOTIFICATION_ALL = NOTIFICATION_CONTENT_START,

  // NavigationController ----------------------------------------------------

  // A new pending navigation has been created. Pending entries are created
  // when the user requests the navigation. We don't know if it will actually
  // happen until it does (at this point, it will be "committed." Note that
  // renderer- initiated navigations such as link clicks will never be
  // pending.
  //
  // This notification is called after the pending entry is created, but
  // before we actually try to navigate. The source will be the
  // NavigationController that owns the pending entry, and the details
  // will be a NavigationEntry.
  NOTIFICATION_NAV_ENTRY_PENDING,

  // A new non-pending navigation entry has been created. This will
  // correspond to one NavigationController entry being created (in the case
  // of new navigations) or renavigated to (for back/forward navigations).
  //
  // The source will be the navigation controller doing the commit. The
  // details will be NavigationController::LoadCommittedDetails.
  // DEPRECATED: Use WebContentsObserver::NavigationEntryCommitted()
  NOTIFICATION_NAV_ENTRY_COMMITTED,

  // Indicates that the NavigationController given in the Source has
  // decreased its back/forward list count by removing entries from either
  // the front or back of its list. This is usually the result of going back
  // and then doing a new navigation, meaning all the "forward" items are
  // deleted.
  //
  // This normally happens as a result of a new navigation. It will be
  // followed by a NAV_ENTRY_COMMITTED message for the new page that
  // caused the pruning. It could also be a result of removing an item from
  // the list to fix up after interstitials.
  //
  // The details are NavigationController::PrunedDetails.
  NOTIFICATION_NAV_LIST_PRUNED,

  // Indicates that a NavigationEntry has changed. The source will be the
  // NavigationController that owns the NavigationEntry. The details will be
  // a NavigationController::EntryChangedDetails struct.
  //
  // This will NOT be sent on navigation, interested parties should also
  // listen for NAV_ENTRY_COMMITTED to handle that case. This will be
  // sent when the entry is updated outside of navigation (like when a new
  // title comes).
  NOTIFICATION_NAV_ENTRY_CHANGED,

  // Other load-related (not from NavigationController) ----------------------

  // Corresponds to ViewHostMsg_DocumentOnLoadCompletedInMainFrame. The source
  // is the WebContents.
  // DEPRECATED: Use WebContentsObserver::DocumentOnLoadCompletedInMainFrame()
  NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,

  // A content load is starting.  The source will be a
  // Source<NavigationController> corresponding to the tab in which the load
  // is occurring.  No details are expected for this notification.
  // DEPRECATED: Use WebContentsObserver::DidStartLoading()
  NOTIFICATION_LOAD_START,

  // A content load has stopped. The source will be a
  // Source<NavigationController> corresponding to the tab in which the load
  // is occurring.  Details in the form of a LoadNotificationDetails object
  // are optional.
  // DEPRECATED: Use WebContentsObserver::DidStopLoading()
  NOTIFICATION_LOAD_STOP,

  // A redirect was received while requesting a resource.  The source will be
  // a Source<WebContents> corresponding to the tab in which the request was
  // issued.  Details in the form of a ResourceRedirectDetails are provided.
  // DEPRECATED: Use WebContentsObserver::DidGetRedirectForResourceRequest()
  NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,

  // WebContents ---------------------------------------------------------------

  // This notification is sent when a render view host has connected to a
  // renderer process. The source is a Source<WebContents> with a pointer to
  // the WebContents.  A WEB_CONTENTS_DISCONNECTED notification is
  // guaranteed before the source pointer becomes junk.  No details are
  // expected.
  // DEPRECATED: Use WebContentsObserver::RenderViewReady()
  NOTIFICATION_WEB_CONTENTS_CONNECTED,

  // This message is sent after a WebContents is disconnected from the
  // renderer process.  The source is a Source<WebContents> with a pointer to
  // the WebContents (the pointer is usable).  No details are expected.
  // DEPRECATED: This is fired in two situations: when the render process
  // crashes, in which case use WebContentsObserver::RenderProcessGone, and when
  // the WebContents is being torn down, in which case use
  // WebContentsObserver::WebContentsDestroyed()
  NOTIFICATION_WEB_CONTENTS_DISCONNECTED,

  // This notification is sent when a WebContents is being destroyed. Any
  // object holding a reference to a WebContents can listen to that
  // notification to properly reset the reference. The source is a
  // Source<WebContents>.
  // DEPRECATED: Use WebContentsObserver::WebContentsDestroyed()
  NOTIFICATION_WEB_CONTENTS_DESTROYED,

  // A RenderViewHost was created for a WebContents. The source is the
  // associated WebContents, and the details is the RenderViewHost
  // pointer.
  NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,

  // Indicates that a RenderProcessHost was created and its handle is now
  // available. The source will be the RenderProcessHost that corresponds to
  // the process.
  NOTIFICATION_RENDERER_PROCESS_CREATED,

  // Indicates that a RenderProcessHost is destructing. The source will be the
  // RenderProcessHost that corresponds to the process.
  NOTIFICATION_RENDERER_PROCESS_TERMINATED,

  // Indicates that a render process was closed (meaning it exited, but the
  // RenderProcessHost might be reused).  The source will be the corresponding
  // RenderProcessHost.  The details will be a RendererClosedDetails struct.
  // This may get sent along with RENDERER_PROCESS_TERMINATED.
  NOTIFICATION_RENDERER_PROCESS_CLOSED,

  // Indicates that a RenderWidgetHost has become unresponsive for a period of
  // time. The source will be the RenderWidgetHost that corresponds to the
  // hung view, and no details are expected.
  NOTIFICATION_RENDER_WIDGET_HOST_HANG,

  // This is sent when a RenderWidgetHost is being destroyed. The source is
  // the RenderWidgetHost, the details are not used.
  NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,

  // Sent after the backing store has been updated but before the widget has
  // painted. The source is the RenderWidgetHost, the details are not used.
  NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,

  // Sent from RenderViewHost::ClosePage.  The hosted RenderView has
  // processed the onbeforeunload handler and is about to be sent a
  // ViewMsg_ClosePage message to complete the tear-down process.  The source
  // is the RenderViewHost sending the message, and no details are provided.
  // Note:  This message is not sent in response to RenderView closure
  // initiated by window.close().
  NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,

  // Indicates a RenderWidgetHost has been hidden or restored. The source is
  // the RWH whose visibility changed, the details is a bool set to true if
  // the new state is "visible."
  NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,

  // The focused element inside a page has changed.  The source is the
  // RenderViewHost. The details is a Details<const bool> that indicates whether
  // or not an editable node was focused.
  NOTIFICATION_FOCUS_CHANGED_IN_PAGE,

  // Notification from WebContents that we have received a response from the
  // renderer in response to a dom automation controller action. The source is
  // the RenderViewHost, and the details is a string with the response.
  NOTIFICATION_DOM_OPERATION_RESPONSE,

  // Custom notifications used by the embedder should start from here.
  NOTIFICATION_CONTENT_END,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_
