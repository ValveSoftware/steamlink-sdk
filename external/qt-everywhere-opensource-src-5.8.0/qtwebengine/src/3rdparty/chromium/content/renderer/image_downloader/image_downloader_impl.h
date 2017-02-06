// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_
#define CONTENT_RENDERER_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "content/common/image_downloader/image_downloader.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace content {

class MultiResolutionImageResourceFetcher;
class RenderFrame;

class ImageDownloaderImpl : public content::mojom::ImageDownloader,
                            public RenderFrameObserver,
                            public RenderThreadObserver {
 public:
  static void CreateMojoService(
      RenderFrame* render_frame,
      mojo::InterfaceRequest<content::mojom::ImageDownloader> request);

  // RenderThreadObserver implementation.
  void OnRenderProcessShutdown() override;

 private:
  ImageDownloaderImpl(
      RenderFrame* render_frame,
      mojo::InterfaceRequest<content::mojom::ImageDownloader> request);
  ~ImageDownloaderImpl() override;

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // ImageDownloader methods:
  void DownloadImage(const GURL& url,
                     bool is_favicon,
                     uint32_t max_bitmap_size,
                     bool bypass_cache,
                     const DownloadImageCallback& callback) override;

  // Requests to fetch an image. When done, the ImageDownloaderImpl
  // is notified by way of DidFetchImage. Returns true if the
  // request was successfully started, false otherwise.
  // If the image is a favicon, cookies will not be
  // sent nor accepted during download. If the image has multiple frames, all
  // the frames whose size <= |max_image_size| are returned. If all of the
  // frames are larger than |max_image_size|, the smallest frame is resized to
  // |max_image_size| and is the only result. |max_image_size| == 0 is
  // interpreted as no max image size.
  bool FetchImage(const GURL& image_url,
                  bool is_favicon,
                  uint32_t max_image_size,
                  bool bypass_cache,
                  const DownloadImageCallback& callback);

  // This callback is triggered when FetchImage completes, either
  // succesfully or with a failure. See FetchImage for more
  // details.
  void DidFetchImage(uint32_t max_image_size,
                     const DownloadImageCallback& callback,
                     MultiResolutionImageResourceFetcher* fetcher,
                     const std::vector<SkBitmap>& images);

  // Reply download result
  void ReplyDownloadResult(
      int32_t http_status_code,
      const std::vector<SkBitmap>& result_images,
      const std::vector<gfx::Size>& result_original_image_sizes,
      const DownloadImageCallback& callback);

  // We use StrongBinding to ensure deletion of "this" when connection closed
  mojo::StrongBinding<ImageDownloader> binding_;

  typedef ScopedVector<MultiResolutionImageResourceFetcher>
      ImageResourceFetcherList;

  // ImageResourceFetchers schedule via FetchImage.
  ImageResourceFetcherList image_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(ImageDownloaderImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_
