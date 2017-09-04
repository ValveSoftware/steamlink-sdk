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
#include "third_party/WebKit/public/platform/WebURLRequest.h"

class GURL;

namespace blink {
class WebFrame;
class WebURLLoader;
}

namespace content {

class ResourceFetcherImpl : public ResourceFetcher {
 public:
  // ResourceFetcher implementation:
  void SetMethod(const std::string& method) override;
  void SetBody(const std::string& body) override;
  void SetHeader(const std::string& header, const std::string& value) override;
  void Start(blink::WebFrame* frame,
             blink::WebURLRequest::RequestContext request_context,
             blink::WebURLRequest::FrameType frame_type,
             const Callback& callback) override;
  void SetTimeout(const base::TimeDelta& timeout) override;

 private:
  friend class ResourceFetcher;

  class ClientImpl;

  explicit ResourceFetcherImpl(const GURL& url);

  ~ResourceFetcherImpl() override;

  void OnLoadComplete();
  void Cancel() override;

  std::unique_ptr<blink::WebURLLoader> loader_;
  std::unique_ptr<ClientImpl> client_;

  // Request to send.
  blink::WebURLRequest request_;

  // Limit how long to wait for the server.
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_
