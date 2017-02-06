// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationData;
class StreamHandle;
struct ResourceResponse;

// PlzNavigate: The delegate interface to NavigationURLLoader.
class CONTENT_EXPORT NavigationURLLoaderDelegate {
 public:
  // Called when the request is redirected. Call FollowRedirect to continue
  // processing the request.
  virtual void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<ResourceResponse>& response) = 0;

  // Called when the request receives its response. No further calls will be
  // made to the delegate. The response body is returned as a stream in
  // |body_stream|. |navigation_data| is passed to the NavigationHandle.
  virtual void OnResponseStarted(
      const scoped_refptr<ResourceResponse>& response,
      std::unique_ptr<StreamHandle> body_stream,
      std::unique_ptr<NavigationData> navigation_data) = 0;

  // Called if the request fails before receving a response. |net_error| is a
  // network error code for the failure. |has_stale_copy_in_cache| is true if
  // there is a stale copy of the unreachable page in cache.
  virtual void OnRequestFailed(bool has_stale_copy_in_cache, int net_error) = 0;

  // Called after the network request has begun on the IO thread at time
  // |timestamp|. This is just a thread hop but is used to compare timing
  // against the pre-PlzNavigate codepath which didn't start the network request
  // until after the renderer was initialized.
  virtual void OnRequestStarted(base::TimeTicks timestamp) = 0;

  // Called when a ServiceWorker was found for the navigation.
  virtual void OnServiceWorkerEncountered() = 0;

 protected:
  NavigationURLLoaderDelegate() {}
  virtual ~NavigationURLLoaderDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
