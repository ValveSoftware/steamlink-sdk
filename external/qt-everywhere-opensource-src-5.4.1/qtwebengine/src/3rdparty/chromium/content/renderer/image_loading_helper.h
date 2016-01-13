// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IMAGE_LOADING_HELPER_H_
#define CONTENT_RENDERER_IMAGE_LOADING_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "content/public/renderer/render_frame_observer.h"
#include "url/gurl.h"

class SkBitmap;

namespace content {

class MultiResolutionImageResourceFetcher;

// This class deals with image downloading.
// One instance of ImageLoadingHelper is owned by RenderFrame.
class ImageLoadingHelper : public RenderFrameObserver {
 public:
  explicit ImageLoadingHelper(RenderFrame* render_frame);

 private:
  virtual ~ImageLoadingHelper();

  // Message handler.
  void OnDownloadImage(int id,
                       const GURL& image_url,
                       bool is_favicon,
                       uint32_t max_image_size);

  // Requests to download an image. When done, the ImageLoadingHelper
  // is notified by way of DidDownloadImage. Returns true if the
  // request was successfully started, false otherwise. id is used to
  // uniquely identify the request and passed back to the
  // DidDownloadImage method. If the image is a favicon, cookies will not be
  // sent nor accepted during download. If the image has multiple frames, all
  // the frames whose size <= |max_image_size| are returned. If all of the
  // frames are larger than |max_image_size|, the smallest frame is resized to
  // |max_image_size| and is the only result. |max_image_size| == 0 is
  // interpreted as no max image size.
  bool DownloadImage(int id,
                     const GURL& image_url,
                     bool is_favicon,
                     uint32_t max_image_size);

  // This callback is triggered when DownloadImage completes, either
  // succesfully or with a failure. See DownloadImage for more
  // details.
  void DidDownloadImage(
      uint32_t max_image_size,
      MultiResolutionImageResourceFetcher* fetcher,
      const std::vector<SkBitmap>& images);

  // Decodes a data: URL image or returns an empty image in case of failure.
  SkBitmap ImageFromDataUrl(const GURL&) const;

  // RenderFrameObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  typedef ScopedVector<MultiResolutionImageResourceFetcher>
      ImageResourceFetcherList;

  // ImageResourceFetchers schedule via DownloadImage.
  ImageResourceFetcherList image_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoadingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_IMAGE_LOADING_HELPER_H_

