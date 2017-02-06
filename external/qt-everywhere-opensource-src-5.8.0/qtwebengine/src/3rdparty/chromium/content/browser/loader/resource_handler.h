// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the RenderProcessHosts, and dispatches them to URLRequests. It then
// fowards the messages from the URLRequests back to the correct process for
// handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_HANDLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"

class GURL;

namespace net {
class IOBuffer;
class URLRequest;
class URLRequestStatus;
struct RedirectInfo;
}  // namespace net

namespace content {
class ResourceController;
class ResourceMessageFilter;
class ResourceRequestInfoImpl;
struct ResourceResponse;

// The resource dispatcher host uses this interface to process network events
// for an URLRequest instance.  A ResourceHandler's lifetime is bound to its
// associated URLRequest.
class CONTENT_EXPORT ResourceHandler
    : public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  virtual ~ResourceHandler() {}

  // Sets the controller for this handler.
  virtual void SetController(ResourceController* controller);

  // The request was redirected to a new URL.  |*defer| has an initial value of
  // false.  Set |*defer| to true to defer the redirect.  The redirect may be
  // followed later on via ResourceDispatcherHost::FollowDeferredRedirect.  If
  // the handler returns false, then the request is cancelled.
  virtual bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                                   ResourceResponse* response,
                                   bool* defer) = 0;

  // Response headers and meta data are available.  If the handler returns
  // false, then the request is cancelled.  Set |*defer| to true to defer
  // processing of the response.  Call ResourceDispatcherHostImpl::
  // ResumeDeferredRequest to continue processing the response.
  virtual bool OnResponseStarted(ResourceResponse* response, bool* defer) = 0;

  // Called before the net::URLRequest (whose url is |url|) is to be started.
  // If the handler returns false, then the request is cancelled.  Otherwise if
  // the return value is true, the ResourceHandler can delay the request from
  // starting by setting |*defer = true|.  A deferred request will not have
  // called net::URLRequest::Start(), and will not resume until someone calls
  // ResourceDispatcherHost::StartDeferredRequest().
  virtual bool OnWillStart(const GURL& url, bool* defer) = 0;

  // Called before the net::URLRequest (whose url is |url|} uses the network for
  // the first time to load the resource. If the handler returns false, then the
  // request is cancelled. Otherwise if the return value is true, the
  // ResourceHandler can delay the request from starting by setting |*defer =
  // true|. Call controller()->Resume() to continue if deferred.
  virtual bool OnBeforeNetworkStart(const GURL& url, bool* defer) = 0;

  // Data will be read for the response.  Upon success, this method places the
  // size and address of the buffer where the data is to be written in its
  // out-params.  This call will be followed by either OnReadCompleted (on
  // successful read or EOF) or OnResponseCompleted (on error).  If
  // OnReadCompleted is called, the buffer may be recycled.  Otherwise, it may
  // not be recycled and may potentially outlive the handler.  If |min_size| is
  // not -1, it is the minimum size of the returned buffer.
  //
  // If the handler returns false, then the request is cancelled.  Otherwise,
  // once data is available, OnReadCompleted will be called.
  virtual bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                          int* buf_size,
                          int min_size) = 0;

  // Data (*bytes_read bytes) was written into the buffer provided by
  // OnWillRead.  A return value of false cancels the request, true continues
  // reading data.  Set |*defer| to true to defer reading more response data.
  // Call controller()->Resume() to continue reading response data.  A zero
  // |bytes_read| signals that no further data is available.
  virtual bool OnReadCompleted(int bytes_read, bool* defer) = 0;

  // The response is complete.  The final response status is given.  Set
  // |*defer| to true to defer destruction to a later time.  Otherwise, the
  // request will be destroyed upon return.
  virtual void OnResponseCompleted(const net::URLRequestStatus& status,
                                   const std::string& security_info,
                                   bool* defer) = 0;

  // This notification is synthesized by the RedirectToFileResourceHandler
  // to indicate progress of 'download_to_file' requests. OnReadCompleted
  // calls are consumed by the RedirectToFileResourceHandler and replaced
  // with OnDataDownloaded calls.
  virtual void OnDataDownloaded(int bytes_downloaded) = 0;

 protected:
  ResourceHandler(net::URLRequest* request);

  ResourceController* controller() const { return controller_; }
  net::URLRequest* request() const { return request_; }

  // Convenience functions.
  ResourceRequestInfoImpl* GetRequestInfo() const;
  int GetRequestID() const;
  ResourceMessageFilter* GetFilter() const;

 private:
  ResourceController* controller_;
  net::URLRequest* request_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_HANDLER_H_
