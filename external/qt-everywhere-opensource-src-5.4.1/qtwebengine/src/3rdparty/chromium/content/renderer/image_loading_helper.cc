// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/image_loading_helper.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/child/image_decoder.h"
#include "content/common/image_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/fetchers/multi_resolution_image_resource_fetcher.h"
#include "net/base/data_url.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "url/url_constants.h"

using blink::WebFrame;
using blink::WebVector;
using blink::WebURL;
using blink::WebURLRequest;

namespace {

//  Proportionally resizes the |image| to fit in a box of size
// |max_image_size|.
SkBitmap ResizeImage(const SkBitmap& image, uint32_t max_image_size) {
  if (max_image_size == 0)
    return image;
  uint32_t max_dimension = std::max(image.width(), image.height());
  if (max_dimension <= max_image_size)
    return image;
  // Proportionally resize the minimal image to fit in a box of size
  // max_image_size.
  return skia::ImageOperations::Resize(
      image,
      skia::ImageOperations::RESIZE_BEST,
      static_cast<uint64_t>(image.width()) * max_image_size / max_dimension,
      static_cast<uint64_t>(image.height()) * max_image_size / max_dimension);
}

// Filters the array of bitmaps, removing all images that do not fit in a box of
// size |max_image_size|. Returns the result if it is not empty. Otherwise,
// find the smallest image in the array and resize it proportionally to fit
// in a box of size |max_image_size|.
// Sets |original_image_sizes| to the sizes of |images| before resizing.
void FilterAndResizeImagesForMaximalSize(
    const std::vector<SkBitmap>& unfiltered,
    uint32_t max_image_size,
    std::vector<SkBitmap>* images,
    std::vector<gfx::Size>* original_image_sizes) {
  images->clear();
  original_image_sizes->clear();

  if (!unfiltered.size())
    return;

  if (max_image_size == 0)
    max_image_size = std::numeric_limits<uint32_t>::max();

  const SkBitmap* min_image = NULL;
  uint32_t min_image_size = std::numeric_limits<uint32_t>::max();
  // Filter the images by |max_image_size|, and also identify the smallest image
  // in case all the images are bigger than |max_image_size|.
  for (std::vector<SkBitmap>::const_iterator it = unfiltered.begin();
       it != unfiltered.end();
       ++it) {
    const SkBitmap& image = *it;
    uint32_t current_size = std::max(it->width(), it->height());
    if (current_size < min_image_size) {
      min_image = &image;
      min_image_size = current_size;
    }
    if (static_cast<uint32_t>(image.width()) <= max_image_size &&
        static_cast<uint32_t>(image.height()) <= max_image_size) {
      images->push_back(image);
      original_image_sizes->push_back(gfx::Size(image.width(), image.height()));
    }
  }
  DCHECK(min_image);
  if (images->size())
    return;
  // Proportionally resize the minimal image to fit in a box of size
  // |max_image_size|.
  images->push_back(ResizeImage(*min_image, max_image_size));
  original_image_sizes->push_back(
      gfx::Size(min_image->width(), min_image->height()));
}

}  // namespace

namespace content {

ImageLoadingHelper::ImageLoadingHelper(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

ImageLoadingHelper::~ImageLoadingHelper() {
}

void ImageLoadingHelper::OnDownloadImage(int id,
                                         const GURL& image_url,
                                         bool is_favicon,
                                         uint32_t max_image_size) {
  std::vector<SkBitmap> result_images;
  std::vector<gfx::Size> result_original_image_sizes;
  if (image_url.SchemeIs(url::kDataScheme)) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    if (!data_image.empty()) {
      result_images.push_back(ResizeImage(data_image, max_image_size));
      result_original_image_sizes.push_back(
          gfx::Size(data_image.width(), data_image.height()));
    }
  } else {
    if (DownloadImage(id, image_url, is_favicon, max_image_size)) {
      // Will complete asynchronously via ImageLoadingHelper::DidDownloadImage
      return;
    }
  }

  Send(new ImageHostMsg_DidDownloadImage(routing_id(),
                                         id,
                                         0,
                                         image_url,
                                         result_images,
                                         result_original_image_sizes));
}

bool ImageLoadingHelper::DownloadImage(int id,
                                       const GURL& image_url,
                                       bool is_favicon,
                                       uint32_t max_image_size) {
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(new MultiResolutionImageResourceFetcher(
      image_url,
      render_frame()->GetWebFrame(),
      id,
      is_favicon ? WebURLRequest::TargetIsFavicon :
                   WebURLRequest::TargetIsImage,
      base::Bind(&ImageLoadingHelper::DidDownloadImage,
                 base::Unretained(this),
                 max_image_size)));
  return true;
}

void ImageLoadingHelper::DidDownloadImage(
    uint32_t max_image_size,
    MultiResolutionImageResourceFetcher* fetcher,
    const std::vector<SkBitmap>& images) {
  std::vector<SkBitmap> result_images;
  std::vector<gfx::Size> result_original_image_sizes;
  FilterAndResizeImagesForMaximalSize(images, max_image_size, &result_images,
      &result_original_image_sizes);

  // Notify requester of image download status.
  Send(new ImageHostMsg_DidDownloadImage(
      routing_id(),
      fetcher->id(),
      fetcher->http_status_code(),
      fetcher->image_url(),
      result_images,
      result_original_image_sizes));

  // Remove the image fetcher from our pending list. We're in the callback from
  // MultiResolutionImageResourceFetcher, best to delay deletion.
  ImageResourceFetcherList::iterator iter =
      std::find(image_fetchers_.begin(), image_fetchers_.end(), fetcher);
  if (iter != image_fetchers_.end()) {
    image_fetchers_.weak_erase(iter);
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
  }
}

SkBitmap ImageLoadingHelper::ImageFromDataUrl(const GURL& url) const {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the image using WebKit's image decoder.
    ImageDecoder decoder(gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(&data[0]);

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}

bool ImageLoadingHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageLoadingHelper, message)
    IPC_MESSAGE_HANDLER(ImageMsg_DownloadImage, OnDownloadImage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

}  // namespace content
