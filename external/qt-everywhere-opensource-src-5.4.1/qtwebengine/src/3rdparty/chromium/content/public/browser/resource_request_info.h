// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"
#include "webkit/common/resource_type.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceContext;

// Each URLRequest allocated by the ResourceDispatcherHost has a
// ResourceRequestInfo instance associated with it.
class ResourceRequestInfo {
 public:
  // Returns the ResourceRequestInfo associated with the given URLRequest.
  CONTENT_EXPORT static const ResourceRequestInfo* ForRequest(
      const net::URLRequest* request);

  // Allocates a new, dummy ResourceRequestInfo and associates it with the
  // given URLRequest.
  // NOTE: Add more parameters if you need to initialize other fields.
  CONTENT_EXPORT static void AllocateForTesting(
      net::URLRequest* request,
      ResourceType::Type resource_type,
      ResourceContext* context,
      int render_process_id,
      int render_view_id,
      int render_frame_id,
      bool is_async);

  // Returns the associated RenderFrame for a given process. Returns false, if
  // there is no associated RenderFrame. This method does not rely on the
  // request being allocated by the ResourceDispatcherHost, but works for all
  // URLRequests that are associated with a RenderFrame.
  CONTENT_EXPORT static bool GetRenderFrameForRequest(
      const net::URLRequest* request,
      int* render_process_id,
      int* render_frame_id);

  // Returns the associated ResourceContext.
  virtual ResourceContext* GetContext() const = 0;

  // The child process unique ID of the requestor.
  virtual int GetChildID() const = 0;

  // The IPC route identifier for this request (this identifies the RenderView
  // or like-thing in the renderer that the request gets routed to).
  virtual int GetRouteID() const = 0;

  // The pid of the originating process, if the request is sent on behalf of a
  // another process.  Otherwise it is 0.
  virtual int GetOriginPID() const = 0;

  // Unique identifier (within the scope of the child process) for this request.
  virtual int GetRequestID() const = 0;

  // The IPC route identifier of the RenderFrame.
  // TODO(jam): once all navigation and resource requests are sent between
  // frames and RenderView/RenderViewHost aren't involved we can remove this and
  // just use GetRouteID above.
  virtual int GetRenderFrameID() const = 0;

  // True if GetRenderFrameID() represents a main frame in the RenderView.
  virtual bool IsMainFrame() const = 0;

  // True if GetParentRenderFrameID() represents a main frame in the RenderView.
  virtual bool ParentIsMainFrame() const = 0;

  // Routing ID of parent frame of frame that sent this resource request.
  // -1 if unknown / invalid.
  virtual int GetParentRenderFrameID() const = 0;

  // Returns the associated resource type.
  virtual ResourceType::Type GetResourceType() const = 0;

  // Returns the process type that initiated this request.
  virtual int GetProcessType() const = 0;

  // Returns the associated referrer policy.
  virtual blink::WebReferrerPolicy GetReferrerPolicy() const = 0;

  // Returns the associated visibility state at the time the request was started
  // in the renderer.
  virtual blink::WebPageVisibilityState GetVisibilityState() const = 0;

  // Returns the associated page transition type.
  virtual PageTransition GetPageTransition() const = 0;

  // True if the request was initiated by a user action (like a tap to follow
  // a link).
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

 protected:
  virtual ~ResourceRequestInfo() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
