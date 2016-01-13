// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_IMAGE_RESOURCE_FETCHER_H_
#define CONTENT_RENDERER_FETCHERS_IMAGE_RESOURCE_FETCHER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

class SkBitmap;

namespace blink {
class WebFrame;
class WebURLResponse;
}

namespace content {

class ResourceFetcher;

// ImageResourceFetcher handles downloading an image for a webview. Once
// downloading is done the supplied callback is notified. ImageResourceFetcher
// is used to download the favicon and images for web apps.
class ImageResourceFetcher {
 public:
  typedef base::Callback<void(ImageResourceFetcher*, const SkBitmap&)> Callback;

  ImageResourceFetcher(
      const GURL& image_url,
      blink::WebFrame* frame,
      int id,
      int image_size,
      blink::WebURLRequest::TargetType target_type,
      const Callback& callback);

  virtual ~ImageResourceFetcher();

  // URL of the image we're downloading.
  const GURL& image_url() const { return image_url_; }

  // Unique identifier for the request.
  int id() const { return id_; }

 private:
  // ResourceFetcher::Callback. Decodes the image and invokes callback_.
  void OnURLFetchComplete(const blink::WebURLResponse& response,
                          const std::string& data);

  Callback callback_;

  // Unique identifier for the request.
  const int id_;

  // URL of the image.
  const GURL image_url_;

  // The size of the image. This is only a hint that is used if the image
  // contains multiple sizes. A value of 0 results in using the first frame
  // of the image.
  const int image_size_;

  // Does the actual download.
  scoped_ptr<ResourceFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ImageResourceFetcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_IMAGE_RESOURCE_FETCHER_H_
