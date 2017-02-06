// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_
#define CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/public/renderer/resource_fetcher.h"
#include "content/renderer/fetchers/web_url_loader_client_impl.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebURLLoaderOptions.h"

class GURL;

namespace blink {
class WebFrame;
class WebURLLoader;
enum class WebCachePolicy;
}

namespace content {

class ResourceFetcherImpl : public ResourceFetcher,
                            public WebURLLoaderClientImpl {
 public:
  // ResourceFetcher implementation:
  void SetMethod(const std::string& method) override;
  void SetBody(const std::string& body) override;
  void SetHeader(const std::string& header, const std::string& value) override;
  void SetSkipServiceWorker(
      blink::WebURLRequest::SkipServiceWorker skip_service_worker) override;
  void SetCachePolicy(blink::WebCachePolicy policy) override;
  void SetLoaderOptions(const blink::WebURLLoaderOptions& options) override;
  void Start(blink::WebFrame* frame,
             blink::WebURLRequest::RequestContext request_context,
             blink::WebURLRequest::FrameType frame_type,
             LoaderType loader_type,
             const Callback& callback) override;
  void SetTimeout(const base::TimeDelta& timeout) override;

 private:
  friend class ResourceFetcher;

  explicit ResourceFetcherImpl(const GURL& url);

  ~ResourceFetcherImpl() override;

  // Callback for timer that limits how long we wait for the server.  If this
  // timer fires and the request hasn't completed, we kill the request.
  void TimeoutFired();

  // WebURLLoaderClientImpl methods:
  void OnLoadComplete() override;
  void Cancel() override;

  std::unique_ptr<blink::WebURLLoader> loader_;

  // Options to send to the loader.
  blink::WebURLLoaderOptions options_;

  // Request to send.  Released once Start() is called.
  blink::WebURLRequest request_;

  // Callback when we're done.
  Callback callback_;

  // Limit how long to wait for the server.
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_
