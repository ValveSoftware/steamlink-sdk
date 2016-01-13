// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_INTERCEPTING_JOB_FACTORY_H_
#define NET_URL_REQUEST_URL_REQUEST_INTERCEPTING_JOB_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;

namespace net {

class URLRequest;
class URLRequestJob;
class URLRequestInterceptor;

// This class acts as a wrapper for URLRequestJobFactory.  The
// URLRequestInteceptor is given the option of creating a URLRequestJob for each
// URLRequest. If the interceptor does not create a job, the URLRequest is
// forwarded to the wrapped URLRequestJobFactory instead.
//
// This class is only intended for use in intercepting requests before they
// are passed on to their default ProtocolHandler.  Each supported scheme should
// have its own ProtocolHandler.
class NET_EXPORT URLRequestInterceptingJobFactory
    : public URLRequestJobFactory {
 public:
  URLRequestInterceptingJobFactory(
      scoped_ptr<URLRequestJobFactory> job_factory,
      scoped_ptr<URLRequestInterceptor> interceptor);
  virtual ~URLRequestInterceptingJobFactory();

  // URLRequestJobFactory implementation
  virtual URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      URLRequest* request,
      NetworkDelegate* network_delegate) const OVERRIDE;
  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) const OVERRIDE;
  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE;

 private:
  scoped_ptr<URLRequestJobFactory> job_factory_;
  scoped_ptr<URLRequestInterceptor> interceptor_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestInterceptingJobFactory);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_INTERCEPTING_JOB_FACTORY_H_
