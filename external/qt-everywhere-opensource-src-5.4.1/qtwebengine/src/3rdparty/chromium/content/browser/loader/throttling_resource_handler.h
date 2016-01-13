// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_THROTTLING_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_THROTTLING_RESOURCE_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/public/browser/resource_controller.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {

class ResourceThrottle;
struct ResourceResponse;

// Used to apply a list of ResourceThrottle instances to an URLRequest.
class ThrottlingResourceHandler : public LayeredResourceHandler,
                                  public ResourceController {
 public:
  // Takes ownership of the ResourceThrottle instances.
  ThrottlingResourceHandler(scoped_ptr<ResourceHandler> next_handler,
                            net::URLRequest* request,
                            ScopedVector<ResourceThrottle> throttles);
  virtual ~ThrottlingResourceHandler();

  // LayeredResourceHandler overrides:
  virtual bool OnRequestRedirected(const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(const GURL& url, bool* defer) OVERRIDE;
  virtual bool OnBeforeNetworkStart(const GURL& url, bool* defer) OVERRIDE;

  // ResourceController implementation:
  virtual void Cancel() OVERRIDE;
  virtual void CancelAndIgnore() OVERRIDE;
  virtual void CancelWithError(int error_code) OVERRIDE;
  virtual void Resume() OVERRIDE;

 private:
  void ResumeStart();
  void ResumeNetworkStart();
  void ResumeRedirect();
  void ResumeResponse();

  // Called when the throttle at |throttle_index| defers a request.  Logs the
  // name of the throttle that delayed the request.
  void OnRequestDefered(int throttle_index);

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_NETWORK_START,
    DEFERRED_REDIRECT,
    DEFERRED_RESPONSE
  };
  DeferredStage deferred_stage_;

  ScopedVector<ResourceThrottle> throttles_;
  size_t next_index_;

  GURL deferred_url_;
  scoped_refptr<ResourceResponse> deferred_response_;

  bool cancelled_by_resource_throttle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_THROTTLING_RESOURCE_HANDLER_H_
