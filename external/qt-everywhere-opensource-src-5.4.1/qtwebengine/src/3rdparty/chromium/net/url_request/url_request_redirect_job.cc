// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_redirect_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_log.h"
#include "net/url_request/url_request.h"

namespace net {

URLRequestRedirectJob::URLRequestRedirectJob(URLRequest* request,
                                             NetworkDelegate* network_delegate,
                                             const GURL& redirect_destination,
                                             StatusCode http_status_code,
                                             const std::string& redirect_reason)
    : URLRequestJob(request, network_delegate),
      redirect_destination_(redirect_destination),
      http_status_code_(http_status_code),
      redirect_reason_(redirect_reason),
      weak_factory_(this) {
  DCHECK(!redirect_reason_.empty());
}

void URLRequestRedirectJob::Start() {
  request()->net_log().AddEvent(
      NetLog::TYPE_URL_REQUEST_REDIRECT_JOB,
      NetLog::StringCallback("reason", &redirect_reason_));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestRedirectJob::StartAsync,
                 weak_factory_.GetWeakPtr()));
}

bool URLRequestRedirectJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  *location = redirect_destination_;
  *http_status_code = http_status_code_;
  return true;
}

bool URLRequestRedirectJob::CopyFragmentOnRedirect(const GURL& location) const {
  // The instantiators have full control over the desired redirection target,
  // including the reference fragment part of the URL.
  return false;
}

URLRequestRedirectJob::~URLRequestRedirectJob() {}

void URLRequestRedirectJob::StartAsync() {
  receive_headers_end_ = base::TimeTicks::Now();
  NotifyHeadersComplete();
}

void URLRequestRedirectJob::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  // Set send_start and send_end to receive_headers_end_ to keep consistent
  // with network cache behavior.
  load_timing_info->send_start = receive_headers_end_;
  load_timing_info->send_end = receive_headers_end_;
  load_timing_info->receive_headers_end = receive_headers_end_;
}

}  // namespace net
