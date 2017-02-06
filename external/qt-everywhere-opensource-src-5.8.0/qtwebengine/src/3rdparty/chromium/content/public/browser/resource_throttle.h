// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_

namespace net {
struct RedirectInfo;
}

namespace content {

class AsyncRevalidationDriver;
class ResourceController;
class ThrottlingResourceHandler;

// A ResourceThrottle gets notified at various points during the process of
// loading a resource.  At each stage, it has the opportunity to defer the
// resource load.  The ResourceController interface may be used to resume a
// deferred resource load, or it may be used to cancel a resource load at any
// time.
class ResourceThrottle {
 public:
  virtual ~ResourceThrottle() {}

  // Called before the resource request is started.
  virtual void WillStartRequest(bool* defer) {}

  // Called before the resource request uses the network for the first time.
  virtual void WillStartUsingNetwork(bool* defer) {}

  // Called when the request was redirected.  |redirect_info| contains the
  // redirect responses's HTTP status code and some information about the new
  // request that will be sent if the redirect is followed, including the new
  // URL and new method.
  virtual void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                                   bool* defer) {}

  // Called when the response headers and meta data are available.
  virtual void WillProcessResponse(bool* defer) {}

  // Returns the name of the throttle, as a UTF-8 C-string, for logging
  // purposes.  nullptr is not allowed.  Caller does *not* take ownership of the
  // returned string.
  virtual const char* GetNameForLogging() const = 0;

  void set_controller_for_testing(ResourceController* c) {
    controller_ = c;
  }

 protected:
  ResourceThrottle() : controller_(nullptr) {}
  ResourceController* controller() { return controller_; }

 private:
  friend class AsyncRevalidationDriver;
  friend class ThrottlingResourceHandler;
  void set_controller(ResourceController* c) { controller_ = c; }

  ResourceController* controller_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_
