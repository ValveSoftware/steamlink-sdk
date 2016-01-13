// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_

#include <vector>

class GURL;

namespace content {

class ResourceController;

// A ResourceThrottle gets notified at various points during the process of
// loading a resource.  At each stage, it has the opportunity to defer the
// resource load.  The ResourceController interface may be used to resume a
// deferred resource load, or it may be used to cancel a resource load at any
// time.
class ResourceThrottle {
 public:
  virtual ~ResourceThrottle() {}

  virtual void WillStartRequest(bool* defer) {}
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) {}
  virtual void WillProcessResponse(bool* defer) {}
  virtual void OnBeforeNetworkStart(bool* defer) {}

  // Returns the name of the throttle, as a UTF-8 C-string, for logging
  // purposes.  NULL is not allowed.  Caller does *not* take ownership of the
  // returned string.
  virtual const char* GetNameForLogging() const = 0;

  void set_controller_for_testing(ResourceController* c) {
    controller_ = c;
  }

 protected:
  ResourceThrottle() : controller_(NULL) {}
  ResourceController* controller() { return controller_; }

 private:
  friend class ThrottlingResourceHandler;
  void set_controller(ResourceController* c) { controller_ = c; }

  ResourceController* controller_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_
