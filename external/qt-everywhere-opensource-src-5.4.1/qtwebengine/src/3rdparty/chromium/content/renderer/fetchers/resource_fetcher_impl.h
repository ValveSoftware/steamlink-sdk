// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_
#define CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/public/renderer/resource_fetcher.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

class GURL;

namespace blink {
class WebFrame;
class WebURLLoader;
struct WebURLError;
}

namespace content {

class ResourceFetcherImpl : public ResourceFetcher,
                            public blink::WebURLLoaderClient {
 public:
  // ResourceFetcher implementation:
  virtual void SetMethod(const std::string& method) OVERRIDE;
  virtual void SetBody(const std::string& body) OVERRIDE;
  virtual void SetHeader(const std::string& header,
                         const std::string& value) OVERRIDE;
  virtual void Start(blink::WebFrame* frame,
                     blink::WebURLRequest::TargetType target_type,
                     const Callback& callback) OVERRIDE;
  virtual void SetTimeout(const base::TimeDelta& timeout) OVERRIDE;

 private:
  friend class ResourceFetcher;

  explicit ResourceFetcherImpl(const GURL& url);

  virtual ~ResourceFetcherImpl();

  void RunCallback(const blink::WebURLResponse& response,
                   const std::string& data);

  // Callback for timer that limits how long we wait for the server.  If this
  // timer fires and the request hasn't completed, we kill the request.
  void TimeoutFired();

  // WebURLLoaderClient methods:
  virtual void willSendRequest(
      blink::WebURLLoader* loader, blink::WebURLRequest& new_request,
      const blink::WebURLResponse& redirect_response);
  virtual void didSendData(
      blink::WebURLLoader* loader, unsigned long long bytes_sent,
      unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(
      blink::WebURLLoader* loader, const blink::WebURLResponse& response);
  virtual void didReceiveCachedMetadata(
      blink::WebURLLoader* loader, const char* data, int data_length);

  virtual void didReceiveData(
      blink::WebURLLoader* loader, const char* data, int data_length,
      int encoded_data_length);
  virtual void didFinishLoading(
      blink::WebURLLoader* loader, double finishTime,
      int64_t total_encoded_data_length);
  virtual void didFail(
      blink::WebURLLoader* loader, const blink::WebURLError& error);

  scoped_ptr<blink::WebURLLoader> loader_;

  // Request to send.  Released once Start() is called.
  blink::WebURLRequest request_;

  // Set to true once the request is complete.
  bool completed_;

  // Buffer to hold the content from the server.
  std::string data_;

  // A copy of the original resource response.
  blink::WebURLResponse response_;

  // Callback when we're done.
  Callback callback_;

  // Buffer to hold metadata from the cache.
  std::string metadata_;

  // Limit how long to wait for the server.
  base::OneShotTimer<ResourceFetcherImpl> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_
