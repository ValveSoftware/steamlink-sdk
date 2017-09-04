// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_LOADER_DELEGATE_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_LOADER_DELEGATE_H_

#include "content/common/content_export.h"

class GURL;

namespace net {
class AuthChallengeInfo;
class ClientCertStore;
}

namespace content {
class ResourceDispatcherHostLoginDelegate;
class ResourceLoader;
struct ResourceResponse;

class CONTENT_EXPORT ResourceLoaderDelegate {
 public:
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) = 0;

  virtual bool HandleExternalProtocol(ResourceLoader* loader,
                                      const GURL& url) = 0;

  virtual void DidStartRequest(ResourceLoader* loader) = 0;
  virtual void DidReceiveRedirect(ResourceLoader* loader,
                                  const GURL& new_url,
                                  ResourceResponse* response) = 0;
  virtual void DidReceiveResponse(ResourceLoader* loader) = 0;

  // This method informs the delegate that the loader is done, and the loader
  // expects to be destroyed as a side-effect of this call.
  virtual void DidFinishLoading(ResourceLoader* loader) = 0;

  // Get platform ClientCertStore. May return nullptr.
  virtual std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      ResourceLoader* loader) = 0;

 protected:
  virtual ~ResourceLoaderDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_LOADER_DELEGATE_H_
