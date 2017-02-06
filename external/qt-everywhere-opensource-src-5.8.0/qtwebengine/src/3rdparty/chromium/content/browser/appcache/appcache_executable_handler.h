// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_EXECUTABLE_HANDLER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_EXECUTABLE_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
class URLRequest;
}

namespace content {

// An interface that must be provided by the embedder to support this feature.
class CONTENT_EXPORT AppCacheExecutableHandler {
 public:
  // A handler can respond in one of 4 ways, if each of the GURL fields
  // in 'Response' are empty and use_network is false, an error response is
  // synthesized.
  struct Response {
    GURL cached_resource_url;
    GURL redirect_url;
    bool use_network;
    // TODO: blob + headers would be a good one to provide as well, as it would
    // make templating possible.
  };
  typedef base::Callback<void(const Response&)> ResponseCallback;

  // Deletion of the handler cancels all pending callbacks.
  virtual ~AppCacheExecutableHandler() {}

  virtual void HandleRequest(net::URLRequest* req,
                             ResponseCallback callback) = 0;
};

// A factory to produce instances.
class CONTENT_EXPORT AppCacheExecutableHandlerFactory {
 public:
  virtual std::unique_ptr<AppCacheExecutableHandler> CreateHandler(
      const GURL& handler_url,
      net::IOBuffer* handler_source) = 0;

 protected:
  virtual ~AppCacheExecutableHandlerFactory() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_EXECUTABLE_HANDLER_H_
