// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_url_loader.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationURLLoaderImplCore;
class NavigationData;
class ServiceWorkerNavigationHandle;
class StreamHandle;
struct ResourceResponse;
struct SSLStatus;

class NavigationURLLoaderImpl : public NavigationURLLoader {
 public:
  // The caller is responsible for ensuring that |delegate| outlives the loader.
  NavigationURLLoaderImpl(BrowserContext* browser_context,
                          std::unique_ptr<NavigationRequestInfo> request_info,
                          std::unique_ptr<NavigationUIData> navigation_ui_data,
                          ServiceWorkerNavigationHandle* service_worker_handle,
                          NavigationURLLoaderDelegate* delegate);
  ~NavigationURLLoaderImpl() override;

  // NavigationURLLoader implementation.
  void FollowRedirect() override;
  void ProceedWithResponse() override;

 private:
  friend class NavigationURLLoaderImplCore;

  // Notifies the delegate of a redirect.
  void NotifyRequestRedirected(const net::RedirectInfo& redirect_info,
                               const scoped_refptr<ResourceResponse>& response);

  // Notifies the delegate that the response has started.
  void NotifyResponseStarted(const scoped_refptr<ResourceResponse>& response,
                             std::unique_ptr<StreamHandle> body,
                             const SSLStatus& ssl_status,
                             std::unique_ptr<NavigationData> navigation_data);

  // Notifies the delegate the request failed to return a response.
  void NotifyRequestFailed(bool in_cache, int net_error);

  // Notifies the delegate the begin navigation request was handled and a
  // potential first network request is about to be made.
  void NotifyRequestStarted(base::TimeTicks timestamp);

  NavigationURLLoaderDelegate* delegate_;

  // |core_| is deleted on the IO thread in a subsequent task when the
  // NavigationURLLoaderImpl goes out of scope.
  NavigationURLLoaderImplCore* core_;

  base::WeakPtrFactory<NavigationURLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
