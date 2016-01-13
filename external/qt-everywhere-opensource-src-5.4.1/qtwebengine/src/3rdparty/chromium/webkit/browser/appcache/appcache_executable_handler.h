// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_EXECUTABLE_HANDLER_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_EXECUTABLE_HANDLER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace net {
class IOBuffer;
class URLRequest;
}

namespace appcache {

// An interface that must be provided by the embedder to support this feature.
class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheExecutableHandler {
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
class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheExecutableHandlerFactory {
 public:
  virtual scoped_ptr<AppCacheExecutableHandler> CreateHandler(
      const GURL& handler_url, net::IOBuffer* handler_source) = 0;

 protected:
  virtual ~AppCacheExecutableHandlerFactory() {}
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_EXECUTABLE_HANDLER_H_
