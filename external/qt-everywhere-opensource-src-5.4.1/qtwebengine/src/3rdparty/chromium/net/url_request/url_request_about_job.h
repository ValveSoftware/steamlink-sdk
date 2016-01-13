// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace net {

class NET_EXPORT URLRequestAboutJob : public URLRequestJob {
 public:
  URLRequestAboutJob(URLRequest* request, NetworkDelegate* network_delegate);

  // URLRequestJob:
  virtual void Start() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;

 private:
  virtual ~URLRequestAboutJob();

  void StartAsync();

  base::WeakPtrFactory<URLRequestAboutJob> weak_factory_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_
