// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_manager.h"

#include <algorithm>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

// The built-in set of protocol factories
namespace {

struct SchemeToFactory {
  const char* scheme;
  URLRequest::ProtocolFactory* factory;
};

}  // namespace

static const SchemeToFactory kBuiltinFactories[] = {
  { "http", URLRequestHttpJob::Factory },
  { "https", URLRequestHttpJob::Factory },

#if !defined(OS_IOS)
  { "ws", URLRequestHttpJob::Factory },
  { "wss", URLRequestHttpJob::Factory },
#endif  // !defined(OS_IOS)
};

// static
URLRequestJobManager* URLRequestJobManager::GetInstance() {
  return Singleton<URLRequestJobManager>::get();
}

URLRequestJob* URLRequestJobManager::CreateJob(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  DCHECK(IsAllowedThread());

  // If we are given an invalid URL, then don't even try to inspect the scheme.
  if (!request->url().is_valid())
    return new URLRequestErrorJob(request, network_delegate, ERR_INVALID_URL);

  // We do this here to avoid asking interceptors about unsupported schemes.
  const URLRequestJobFactory* job_factory = NULL;
  job_factory = request->context()->job_factory();

  const std::string& scheme = request->url().scheme();  // already lowercase
  if (!job_factory->IsHandledProtocol(scheme)) {
    return new URLRequestErrorJob(
        request, network_delegate, ERR_UNKNOWN_URL_SCHEME);
  }

  // THREAD-SAFETY NOTICE:
  //   We do not need to acquire the lock here since we are only reading our
  //   data structures.  They should only be modified on the current thread.

  // See if the request should be intercepted.
  //

  // TODO(pauljensen): Remove this when AppCacheInterceptor is a
  // ProtocolHandler, see crbug.com/161547.
  if (!(request->load_flags() & LOAD_DISABLE_INTERCEPT)) {
    InterceptorList::const_iterator i;
    for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
      URLRequestJob* job = (*i)->MaybeIntercept(request, network_delegate);
      if (job)
        return job;
    }
  }

  URLRequestJob* job = job_factory->MaybeCreateJobWithProtocolHandler(
      scheme, request, network_delegate);
  if (job)
    return job;

  // See if the request should be handled by a built-in protocol factory.
  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i) {
    if (scheme == kBuiltinFactories[i].scheme) {
      URLRequestJob* job = (kBuiltinFactories[i].factory)(
          request, network_delegate, scheme);
      DCHECK(job);  // The built-in factories are not expected to fail!
      return job;
    }
  }

  // If we reached here, then it means that a registered protocol factory
  // wasn't interested in handling the URL.  That is fairly unexpected, and we
  // don't have a specific error to report here :-(
  LOG(WARNING) << "Failed to map: " << request->url().spec();
  return new URLRequestErrorJob(request, network_delegate, ERR_FAILED);
}

URLRequestJob* URLRequestJobManager::MaybeInterceptRedirect(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const GURL& location) const {
  DCHECK(IsAllowedThread());
  if (!request->url().is_valid() ||
      request->load_flags() & LOAD_DISABLE_INTERCEPT ||
      request->status().status() == URLRequestStatus::CANCELED) {
    return NULL;
  }

  const URLRequestJobFactory* job_factory = NULL;
  job_factory = request->context()->job_factory();

  const std::string& scheme = request->url().scheme();  // already lowercase
  if (!job_factory->IsHandledProtocol(scheme))
    return NULL;

  InterceptorList::const_iterator i;
  for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
    URLRequestJob* job = (*i)->MaybeInterceptRedirect(request,
                                                      network_delegate,
                                                      location);
    if (job)
      return job;
  }
  return NULL;
}

URLRequestJob* URLRequestJobManager::MaybeInterceptResponse(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  DCHECK(IsAllowedThread());
  if (!request->url().is_valid() ||
      request->load_flags() & LOAD_DISABLE_INTERCEPT ||
      request->status().status() == URLRequestStatus::CANCELED) {
    return NULL;
  }

  const URLRequestJobFactory* job_factory = NULL;
  job_factory = request->context()->job_factory();

  const std::string& scheme = request->url().scheme();  // already lowercase
  if (!job_factory->IsHandledProtocol(scheme))
    return NULL;

  InterceptorList::const_iterator i;
  for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
    URLRequestJob* job = (*i)->MaybeInterceptResponse(request,
                                                      network_delegate);
    if (job)
      return job;
  }
  return NULL;
}

// static
bool URLRequestJobManager::SupportsScheme(const std::string& scheme) {
  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i) {
    if (LowerCaseEqualsASCII(scheme, kBuiltinFactories[i].scheme))
      return true;
  }

  return false;
}

void URLRequestJobManager::RegisterRequestInterceptor(
    URLRequest::Interceptor* interceptor) {
  DCHECK(IsAllowedThread());

  base::AutoLock locked(lock_);

  DCHECK(std::find(interceptors_.begin(), interceptors_.end(), interceptor) ==
         interceptors_.end());
  interceptors_.push_back(interceptor);
}

void URLRequestJobManager::UnregisterRequestInterceptor(
    URLRequest::Interceptor* interceptor) {
  DCHECK(IsAllowedThread());

  base::AutoLock locked(lock_);

  InterceptorList::iterator i =
      std::find(interceptors_.begin(), interceptors_.end(), interceptor);
  DCHECK(i != interceptors_.end());
  interceptors_.erase(i);
}

URLRequestJobManager::URLRequestJobManager()
    : allowed_thread_(0),
      allowed_thread_initialized_(false) {
}

URLRequestJobManager::~URLRequestJobManager() {}

}  // namespace net
