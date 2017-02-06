// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_resource_dispatcher_host_delegate.h"

#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/net/connectivity_checker.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace chromecast {
namespace shell {

void CastResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request) {
  if (url_request->status().status() == net::URLRequestStatus::FAILED) {
    LOG(ERROR) << "Failed to load resource " << url_request->url()
               << "; status:" << url_request->status().status() << ", error:"
               << net::ErrorToShortString(url_request->status().error());
    CastBrowserProcess::GetInstance()->connectivity_checker()->Check();
  }
}

}  // namespace shell
}  // namespace chromecast
