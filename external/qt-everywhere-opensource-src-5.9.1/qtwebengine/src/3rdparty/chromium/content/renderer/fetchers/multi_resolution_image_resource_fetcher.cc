// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/multi_resolution_image_resource_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/child/image_decoder.h"
#include "content/public/renderer/associated_resource_fetcher.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoaderOptions.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

using blink::WebFrame;
using blink::WebAssociatedURLLoaderOptions;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

MultiResolutionImageResourceFetcher::MultiResolutionImageResourceFetcher(
    const GURL& image_url,
    WebFrame* frame,
    int id,
    WebURLRequest::RequestContext request_context,
    blink::WebCachePolicy cache_policy,
    const Callback& callback)
    : callback_(callback),
      id_(id),
      http_status_code_(0),
      image_url_(image_url) {
  fetcher_.reset(AssociatedResourceFetcher::Create(image_url));

  WebAssociatedURLLoaderOptions options;
  options.allowCredentials = true;
  options.crossOriginRequestPolicy =
      WebAssociatedURLLoaderOptions::CrossOriginRequestPolicyAllow;
  fetcher_->SetLoaderOptions(options);

  // To prevent cache tainting, the favicon requests have to by-pass the service
  // workers. This should ideally not happen or at least not all the time.
  // See https://crbug.com/448427
  if (request_context == WebURLRequest::RequestContextFavicon)
    fetcher_->SetSkipServiceWorker(WebURLRequest::SkipServiceWorker::All);

  fetcher_->SetCachePolicy(cache_policy);

  fetcher_->Start(
      frame,
      request_context,
      WebURLRequest::FrameTypeNone,
      base::Bind(&MultiResolutionImageResourceFetcher::OnURLFetchComplete,
                 base::Unretained(this)));
}

MultiResolutionImageResourceFetcher::~MultiResolutionImageResourceFetcher() {
}

void MultiResolutionImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  std::vector<SkBitmap> bitmaps;
  if (!response.isNull()) {
    http_status_code_ = response.httpStatusCode();
    GURL url(response.url());
    if (http_status_code_ == 200 || url.SchemeIsFile()) {
      // Request succeeded, try to convert it to an image.
      bitmaps = ImageDecoder::DecodeAll(
          reinterpret_cast<const unsigned char*>(data.data()), data.size());
    }
  } // else case:
    // If we get here, it means no image from server or couldn't decode the
    // response as an image. The delegate will see an empty vector.

  // Take a reference to the callback as running the callback may lead to our
  // destruction.
  Callback callback = callback_;
  callback.Run(this, bitmaps);
}

}  // namespace content
