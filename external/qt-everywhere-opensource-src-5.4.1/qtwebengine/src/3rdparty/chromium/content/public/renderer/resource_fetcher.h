// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RESOURCE_FETCHER_H_
#define CONTENT_PUBLIC_RENDERER_RESOURCE_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

class GURL;

namespace base {
class TimeDelta;
}

namespace blink {
class WebFrame;
class WebURLResponse;
}

namespace content {

// Interface to download resources asynchronously.
class CONTENT_EXPORT ResourceFetcher {
 public:
  virtual ~ResourceFetcher() {}

  // This will be called asynchronously after the URL has been fetched,
  // successfully or not.  If there is a failure, response and data will both be
  // empty.  |response| and |data| are both valid until the URLFetcher instance
  // is destroyed.
  typedef base::Callback<void(const blink::WebURLResponse& response,
                              const std::string& data)> Callback;

  // Creates a ResourceFetcher for the specified resource.  Caller takes
  // ownership of the returned object.  Deleting the ResourceFetcher will cancel
  // the request, and the callback will never be run.
  static ResourceFetcher* Create(const GURL& url);

  // Set the corresponding parameters of the request.  Must be called before
  // Start.  By default, requests are GETs with no body.
  virtual void SetMethod(const std::string& method) = 0;
  virtual void SetBody(const std::string& body) = 0;
  virtual void SetHeader(const std::string& header,
                         const std::string& value) = 0;

  // Starts the request using the specified frame.  Calls |callback| when
  // done.
  virtual void Start(blink::WebFrame* frame,
                     blink::WebURLRequest::TargetType target_type,
                     const Callback& callback) = 0;

  // Sets how long to wait for the server to reply.  By default, there is no
  // timeout.  Must be called after a request is started.
  virtual void SetTimeout(const base::TimeDelta& timeout) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RESOURCE_FETCHER_H_
