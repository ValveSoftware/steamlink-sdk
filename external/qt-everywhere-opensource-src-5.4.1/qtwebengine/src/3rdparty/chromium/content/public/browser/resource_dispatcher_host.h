// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace net {
class URLRequest;
}

namespace content {

class DownloadItem;
class ResourceContext;
class ResourceDispatcherHostDelegate;
struct DownloadSaveInfo;
struct Referrer;

class CONTENT_EXPORT ResourceDispatcherHost {
 public:
  typedef base::Callback<void(DownloadItem*, DownloadInterruptReason)>
      DownloadStartedCallback;

  // Returns the singleton instance of the ResourceDispatcherHost.
  static ResourceDispatcherHost* Get();

  // This does not take ownership of the delegate. It is expected that the
  // delegate have a longer lifetime than the ResourceDispatcherHost.
  virtual void SetDelegate(ResourceDispatcherHostDelegate* delegate) = 0;

  // Controls whether third-party sub-content can pop-up HTTP basic auth
  // dialog boxes.
  virtual void SetAllowCrossOriginAuthPrompt(bool value) = 0;

  // Initiates a download by explicit request of the renderer (e.g. due to
  // alt-clicking a link) or some other chrome subsystem.
  // |is_content_initiated| is used to indicate that the request was generated
  // from a web page, and hence may not be as trustworthy as a browser
  // generated request.  If |download_id| is invalid, a download id will be
  // automatically assigned to the request, otherwise the specified download id
  // will be used.  (Note that this will result in re-use of an existing
  // download item if the download id was already assigned.)  If the download
  // is started, |started_callback| will be called on the UI thread with the
  // DownloadItem; otherwise an interrupt reason will be returned.
  virtual DownloadInterruptReason BeginDownload(
      scoped_ptr<net::URLRequest> request,
      const Referrer& referrer,
      bool is_content_initiated,
      ResourceContext* context,
      int child_id,
      int route_id,
      bool prefer_cache,
      scoped_ptr<DownloadSaveInfo> save_info,
      uint32 download_id,
      const DownloadStartedCallback& started_callback) = 0;

  // Clears the ResourceDispatcherHostLoginDelegate associated with the request.
  virtual void ClearLoginDelegateForRequest(net::URLRequest* request) = 0;

  // Causes all new requests for the route identified by |child_id| and
  // |route_id| to be blocked (not being started) until
  // ResumeBlockedRequestsForRoute is called.
  virtual void BlockRequestsForRoute(int child_id, int route_id) = 0;

  // Resumes any blocked request for the specified route id.
  virtual void ResumeBlockedRequestsForRoute(int child_id, int route_id) = 0;

 protected:
  virtual ~ResourceDispatcherHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
