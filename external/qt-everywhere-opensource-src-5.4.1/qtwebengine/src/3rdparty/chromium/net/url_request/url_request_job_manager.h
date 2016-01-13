// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H_

#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "net/url_request/url_request.h"

template <typename T> struct DefaultSingletonTraits;

namespace net {

// This class is responsible for managing the set of protocol factories and
// request interceptors that determine how an URLRequestJob gets created to
// handle an URLRequest.
//
// MULTI-THREADING NOTICE:
//   URLRequest is designed to have all consumers on a single thread, and
//   so no attempt is made to support Interceptor instances being
//   registered/unregistered or in any way poked on multiple threads.
class URLRequestJobManager {
 public:
  // Returns the singleton instance.
  static URLRequestJobManager* GetInstance();

  // Instantiate an URLRequestJob implementation based on the registered
  // interceptors and protocol factories.  This will always succeed in
  // returning a job unless we are--in the extreme case--out of memory.
  URLRequestJob* CreateJob(URLRequest* request,
                           NetworkDelegate* network_delegate) const;

  // Allows interceptors to hijack the request after examining the new location
  // of a redirect. Returns NULL if no interceptor intervenes.
  URLRequestJob* MaybeInterceptRedirect(URLRequest* request,
                                        NetworkDelegate* network_delegate,
                                        const GURL& location) const;

  // Allows interceptors to hijack the request after examining the response
  // status and headers. This is also called when there is no server response
  // at all to allow interception of failed requests due to network errors.
  // Returns NULL if no interceptor intervenes.
  URLRequestJob* MaybeInterceptResponse(
      URLRequest* request, NetworkDelegate* network_delegate) const;

  // Returns true if the manager has a built-in handler for |scheme|.
  static bool SupportsScheme(const std::string& scheme);

  // Register/unregister a request interceptor.
  void RegisterRequestInterceptor(URLRequest::Interceptor* interceptor);
  void UnregisterRequestInterceptor(URLRequest::Interceptor* interceptor);

 private:
  typedef std::vector<URLRequest::Interceptor*> InterceptorList;
  friend struct DefaultSingletonTraits<URLRequestJobManager>;

  URLRequestJobManager();
  ~URLRequestJobManager();

  // The first guy to call this function sets the allowed thread.  This way we
  // avoid needing to define that thread externally.  Since we expect all
  // callers to be on the same thread, we don't worry about threads racing to
  // set the allowed thread.
  bool IsAllowedThread() const {
#if 0
    if (!allowed_thread_initialized_) {
      allowed_thread_ = base::PlatformThread::CurrentId();
      allowed_thread_initialized_ = true;
    }
    return allowed_thread_ == base::PlatformThread::CurrentId();
#else
    // The previous version of this check used GetCurrentThread on Windows to
    // get thread handles to compare. Unfortunately, GetCurrentThread returns
    // a constant pseudo-handle (0xFFFFFFFE), and therefore IsAllowedThread
    // always returned true. The above code that's turned off is the correct
    // code, but causes the tree to turn red because some caller isn't
    // respecting our thread requirements. We're turning off the check for now;
    // bug http://b/issue?id=1338969 has been filed to fix things and turn the
    // check back on.
    return true;
  }

  // We use this to assert that CreateJob and the registration functions all
  // run on the same thread.
  mutable base::PlatformThreadId allowed_thread_;
  mutable bool allowed_thread_initialized_;
#endif

  mutable base::Lock lock_;
  InterceptorList interceptors_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJobManager);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H_
