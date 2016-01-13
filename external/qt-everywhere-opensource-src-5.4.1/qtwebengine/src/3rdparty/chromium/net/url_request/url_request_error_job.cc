// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_error_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"

namespace net {

URLRequestErrorJob::URLRequestErrorJob(
    URLRequest* request, NetworkDelegate* network_delegate, int error)
    : URLRequestJob(request, network_delegate),
      error_(error),
      weak_factory_(this) {}

URLRequestErrorJob::~URLRequestErrorJob() {}

void URLRequestErrorJob::Start() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestErrorJob::StartAsync, weak_factory_.GetWeakPtr()));
}

void URLRequestErrorJob::StartAsync() {
  NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, error_));
}

}  // namespace net
