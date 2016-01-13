// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/image_resource_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/crash_logging.h"
#include "content/child/image_decoder.h"
#include "content/public/renderer/resource_fetcher.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

using blink::WebFrame;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

ImageResourceFetcher::ImageResourceFetcher(
    const GURL& image_url,
    WebFrame* frame,
    int id,
    int image_size,
    WebURLRequest::TargetType target_type,
    const Callback& callback)
    : callback_(callback),
      id_(id),
      image_url_(image_url),
      image_size_(image_size) {
  fetcher_.reset(ResourceFetcher::Create(image_url));
  fetcher_->Start(frame, target_type,
                  base::Bind(&ImageResourceFetcher::OnURLFetchComplete,
                      base::Unretained(this)));

  // Set subresource URL for crash reporting.
  base::debug::SetCrashKeyValue("subresource_url", image_url.spec());
}

ImageResourceFetcher::~ImageResourceFetcher() {
}

void ImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  SkBitmap bitmap;
  if (!response.isNull() && response.httpStatusCode() == 200) {
    // Request succeeded, try to convert it to an image.
    ImageDecoder decoder(gfx::Size(image_size_, image_size_));
    bitmap = decoder.Decode(
        reinterpret_cast<const unsigned char*>(data.data()), data.size());
  } // else case:
    // If we get here, it means no image from server or couldn't decode the
    // response as an image. The delegate will see a null image, indicating
    // that an error occurred.

  // Take a reference to the callback as running the callback may lead to our
  // destruction.
  Callback callback = callback_;
  callback.Run(this, bitmap);
}

}  // namespace content
