// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/page_transition_types.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceContext;
class WebContents;

// Each URLRequest allocated by the ResourceDispatcherHost has a
// ResourceRequestInfo instance associated with it.
class ResourceRequestInfo {
 public:
  // Returns the ResourceRequestInfo associated with the given URLRequest.
  CONTENT_EXPORT static const ResourceRequestInfo* ForRequest(
      const net::URLRequest* request);

  // Allocates a new, dummy ResourceRequestInfo and associates it with the
  // given URLRequest.
  //
  // The RenderView routing ID must correspond to the RenderView of the
  // RenderFrame, both of which share the same RenderProcess. This may be a
  // different RenderView than the WebContents' main RenderView. If the
  // download is not associated with a frame, the IDs can be all -1.
  //
  // NOTE: Add more parameters if you need to initialize other fields.
  CONTENT_EXPORT static void AllocateForTesting(net::URLRequest* request,
                                                ResourceType resource_type,
                                                ResourceContext* context,
                                                int render_process_id,
                                                int render_view_id,
                                                int render_frame_id,
                                                bool is_main_frame,
                                                bool parent_is_main_frame,
                                                bool allow_download,
                                                bool is_async,
                                                bool is_using_lofi);

  // Returns the associated RenderFrame for a given process. Returns false, if
  // there is no associated RenderFrame. This method does not rely on the
  // request being allocated by the ResourceDispatcherHost, but works for all
  // URLRequests that are associated with a RenderFrame.
  CONTENT_EXPORT static bool GetRenderFrameForRequest(
      const net::URLRequest* request,
      int* render_process_id,
      int* render_frame_id);

  // Returns true if the request originated from a Service Worker.
  CONTENT_EXPORT static bool OriginatedFromServiceWorker(
      const net::URLRequest* request);

  // A callback that returns a pointer to a WebContents. The callback can
  // always be used, but it may return nullptr: if the info used to
  // instantiate the callback can no longer be used to return a WebContents,
  // nullptr will be returned instead.
  // The callback should only run on the UI thread and it should always be
  // non-null.
  using WebContentsGetter = base::Callback<WebContents*(void)>;

  // Returns a callback that returns a pointer to the WebContents this request
  // is associated with, or nullptr if it no longer exists or the request is
  // not associated with a WebContents. The callback should only run on the UI
  // thread.
  // Note: Not all resource requests will be owned by a WebContents. For
  // example, requests made by a ServiceWorker.
  virtual WebContentsGetter GetWebContentsGetterForRequest() const = 0;

  // Returns the associated ResourceContext.
  virtual ResourceContext* GetContext() const = 0;

  // The child process unique ID of the requestor.
  // To get a WebContents, use GetWebContentsGetterForRequest instead.
  virtual int GetChildID() const = 0;

  // The IPC route identifier for this request (this identifies the RenderView
  // or like-thing in the renderer that the request gets routed to).
  // To get a WebContents, use GetWebContentsGetterForRequest instead.
  // Don't use this method for new code, as RenderViews are going away.
  virtual int GetRouteID() const = 0;

  // The pid of the originating process, if the request is sent on behalf of a
  // another process.  Otherwise it is 0.
  virtual int GetOriginPID() const = 0;

  // The IPC route identifier of the RenderFrame.
  // To get a WebContents, use GetWebContentsGetterForRequest instead.
  // TODO(jam): once all navigation and resource requests are sent between
  // frames and RenderView/RenderViewHost aren't involved we can remove this and
  // just use GetRouteID above.
  virtual int GetRenderFrameID() const = 0;

  // True if GetRenderFrameID() represents a main frame in the RenderView.
  virtual bool IsMainFrame() const = 0;

  // True if the frame's parent represents a main frame in the RenderView.
  virtual bool ParentIsMainFrame() const = 0;

  // Returns the associated resource type.
  virtual ResourceType GetResourceType() const = 0;

  // Returns the process type that initiated this request.
  virtual int GetProcessType() const = 0;

  // Returns the associated referrer policy.
  virtual blink::WebReferrerPolicy GetReferrerPolicy() const = 0;

  // Returns the associated visibility state at the time the request was started
  // in the renderer.
  virtual blink::WebPageVisibilityState GetVisibilityState() const = 0;

  // Returns the associated page transition type.
  virtual ui::PageTransition GetPageTransition() const = 0;

  // True if the request was initiated by a user action (like a tap to follow
  // a link).
  //
  // Note that a false value does not mean the request was not initiated by a
  // user gesture. Also note that the fact that a user gesture was active
  // while the request was created does not imply that the user consciously
  // wanted this request to happen nor is aware of it.
  //
  // DO NOT BASE SECURITY DECISIONS ON THIS FLAG!
  virtual bool HasUserGesture() const = 0;

  // True if ResourceController::CancelAndIgnore() was called.  For example,
  // the requested URL may be being loaded by an external program.
  virtual bool WasIgnoredByHandler() const = 0;

  // Returns false if there is NOT an associated render frame.
  virtual bool GetAssociatedRenderFrame(int* render_process_id,
                                        int* render_frame_id) const = 0;

  // Returns true if this is associated with an asynchronous request.
  virtual bool IsAsync() const = 0;

  // Whether this is a download.
  virtual bool IsDownload() const = 0;

  // Whether this request if using Lo-Fi mode.
  virtual bool IsUsingLoFi() const = 0;

 protected:
  virtual ~ResourceRequestInfo() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
