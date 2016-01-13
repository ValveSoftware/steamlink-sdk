// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "webkit/common/resource_type.h"

class GURL;
template <class T> class ScopedVector;

namespace appcache {
class AppCacheService;
}

namespace content {
class ResourceContext;
class ResourceThrottle;
class StreamHandle;
struct Referrer;
struct ResourceResponse;
}

namespace IPC {
class Sender;
}

namespace net {
class AuthChallengeInfo;
class URLRequest;
}

namespace content {

class ResourceDispatcherHostLoginDelegate;

// Interface that the embedder provides to ResourceDispatcherHost to allow
// observing and modifying requests.
class CONTENT_EXPORT ResourceDispatcherHostDelegate {
 public:
  // Called when a request begins. Return false to abort the request.
  virtual bool ShouldBeginRequest(
      int child_id,
      int route_id,
      const std::string& method,
      const GURL& url,
      ResourceType::Type resource_type,
      ResourceContext* resource_context);

  // Called after ShouldBeginRequest to allow the embedder to add resource
  // throttles.
  virtual void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      appcache::AppCacheService* appcache_service,
      ResourceType::Type resource_type,
      int child_id,
      int route_id,
      ScopedVector<ResourceThrottle>* throttles);

  // Allows an embedder to add additional resource handlers for a download.
  // |must_download| is set if the request must be handled as a download.
  virtual void DownloadStarting(
      net::URLRequest* request,
      ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      bool must_download,
      ScopedVector<ResourceThrottle>* throttles);

  // Creates a ResourceDispatcherHostLoginDelegate that asks the user for a
  // username and password.
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info, net::URLRequest* request);

  // Launches the url for the given tab. Returns true if an attempt to handle
  // the url was made, e.g. by launching an app. Note that this does not
  // guarantee that the app successfully handled it.
  virtual bool HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id);

  // Returns true if we should force the given resource to be downloaded.
  // Otherwise, the content layer decides.
  virtual bool ShouldForceDownloadResource(
      const GURL& url, const std::string& mime_type);

  // Returns true and sets |origin| if a Stream should be created for the
  // resource.
  // If true is returned, a new Stream will be created and OnStreamCreated()
  // will be called with
  // - a StreamHandle instance for the Stream. The handle contains the URL for
  //   reading the Stream etc.
  // The Stream's origin will be set to |origin|.
  //
  // If the stream will be rendered in a BrowserPlugin, |payload| will contain
  // the data that should be given to the old ResourceHandler to forward to the
  // renderer process.
  virtual bool ShouldInterceptResourceAsStream(
      net::URLRequest* request,
      const std::string& mime_type,
      GURL* origin,
      std::string* payload);

  // Informs the delegate that a Stream was created. The Stream can be read from
  // the blob URL of the Stream, but can only be read once.
  virtual void OnStreamCreated(
      net::URLRequest* request,
      scoped_ptr<content::StreamHandle> stream);

  // Informs the delegate that a response has started.
  virtual void OnResponseStarted(
      net::URLRequest* request,
      ResourceContext* resource_context,
      ResourceResponse* response,
      IPC::Sender* sender);

  // Informs the delegate that a request has been redirected.
  virtual void OnRequestRedirected(
      const GURL& redirect_url,
      net::URLRequest* request,
      ResourceContext* resource_context,
      ResourceResponse* response);

  // Notification that a request has completed.
  virtual void RequestComplete(net::URLRequest* url_request);

 protected:
  ResourceDispatcherHostDelegate();
  virtual ~ResourceDispatcherHostDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
