// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_VIEW_HTTP_CACHE_JOB_FACTORY_H_
#define CONTENT_BROWSER_NET_VIEW_HTTP_CACHE_JOB_FACTORY_H_

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}  // namespace net

class GURL;

namespace content {

class ViewHttpCacheJobFactory {
 public:
  static bool IsSupportedURL(const GURL& url);
  static net::URLRequestJob* CreateJobForRequest(
      net::URLRequest* request, net::NetworkDelegate* network_delegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_VIEW_HTTP_CACHE_JOB_FACTORY_H_
