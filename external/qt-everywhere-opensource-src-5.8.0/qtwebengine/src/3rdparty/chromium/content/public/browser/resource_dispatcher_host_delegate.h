// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "ui/base/page_transition_types.h"

class GURL;
template <class T> class ScopedVector;

namespace IPC {
class Sender;
}

namespace net {
class AuthChallengeInfo;
class ClientCertStore;
class URLRequest;
}

namespace content {

class AppCacheService;
class NavigationData;
class ResourceContext;
class ResourceDispatcherHostLoginDelegate;
class ResourceThrottle;
struct Referrer;
struct ResourceResponse;
struct StreamInfo;

// Interface that the embedder provides to ResourceDispatcherHost to allow
// observing and modifying requests.
class CONTENT_EXPORT ResourceDispatcherHostDelegate {
 public:
  // Called when a request begins. Return false to abort the request.
  virtual bool ShouldBeginRequest(const std::string& method,
                                  const GURL& url,
                                  ResourceType resource_type,
                                  ResourceContext* resource_context);

  // Called after ShouldBeginRequest to allow the embedder to add resource
  // throttles.
  virtual void RequestBeginning(net::URLRequest* request,
                                ResourceContext* resource_context,
                                AppCacheService* appcache_service,
                                ResourceType resource_type,
                                ScopedVector<ResourceThrottle>* throttles);

  // Allows an embedder to add additional resource handlers for a download.
  // |must_download| is set if the request must be handled as a download.
  virtual void DownloadStarting(net::URLRequest* request,
                                ResourceContext* resource_context,
                                int child_id,
                                int route_id,
                                bool is_content_initiated,
                                bool must_download,
                                ScopedVector<ResourceThrottle>* throttles);

  // Creates a ResourceDispatcherHostLoginDelegate that asks the user for a
  // username and password.
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      net::URLRequest* request);

  // Launches the url for the given tab. Returns true if an attempt to handle
  // the url was made, e.g. by launching an app. Note that this does not
  // guarantee that the app successfully handled it.
  virtual bool HandleExternalProtocol(
      const GURL& url,
      int child_id,
      const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      ResourceContext* resource_context);

  // Returns true if we should force the given resource to be downloaded.
  // Otherwise, the content layer decides.
  virtual bool ShouldForceDownloadResource(const GURL& url,
                                           const std::string& mime_type);

  // Returns true and sets |origin| if a Stream should be created for the
  // resource. |plugin_path| is the plugin which will be used to handle the
  // request (if the stream will be rendered in a BrowserPlugin). It may be
  // empty. If true is returned, a new Stream will be created and
  // OnStreamCreated() will be called with a StreamHandle instance for the
  // Stream. The handle contains the URL for reading the Stream etc. The
  // Stream's origin will be set to |origin|.
  //
  // If the stream will be rendered in a BrowserPlugin, |payload| will contain
  // the data that should be given to the old ResourceHandler to forward to the
  // renderer process.
  virtual bool ShouldInterceptResourceAsStream(
      net::URLRequest* request,
      const base::FilePath& plugin_path,
      const std::string& mime_type,
      GURL* origin,
      std::string* payload);

  // Informs the delegate that a Stream was created. The Stream can be read from
  // the blob URL of the Stream, but can only be read once.
  virtual void OnStreamCreated(net::URLRequest* request,
                               std::unique_ptr<content::StreamInfo> stream);

  // Informs the delegate that a response has started.
  virtual void OnResponseStarted(net::URLRequest* request,
                                 ResourceContext* resource_context,
                                 ResourceResponse* response,
                                 IPC::Sender* sender);

  // Informs the delegate that a request has been redirected.
  virtual void OnRequestRedirected(const GURL& redirect_url,
                                   net::URLRequest* request,
                                   ResourceContext* resource_context,
                                   ResourceResponse* response);

  // Notification that a request has completed.
  virtual void RequestComplete(net::URLRequest* url_request);

  // Asks the embedder if Lo-Fi mode should be enabled for the given request. It
  // is only called for requests with an unspecified Lo-Fi value.
  virtual bool ShouldEnableLoFiMode(const net::URLRequest& url_request,
                                    content::ResourceContext* resource_context);

  // Asks the embedder for NavigationData related to this request. It is only
  // called for navigation requests.
  virtual NavigationData* GetNavigationData(net::URLRequest* request) const;

  // Get platform ClientCertStore. May return nullptr.
  virtual std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      ResourceContext* resource_context);

 protected:
  virtual ~ResourceDispatcherHostDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
