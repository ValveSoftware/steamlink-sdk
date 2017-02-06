// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace {

// Processes the bitmap data returned from the FaviconService as part of a
// LargeIconService request.
class LargeIconWorker : public base::RefCountedThreadSafe<LargeIconWorker> {
 public:
  LargeIconWorker(int min_source_size_in_pixel,
                  int desired_size_in_pixel,
                  favicon_base::LargeIconCallback callback,
                  scoped_refptr<base::TaskRunner> background_task_runner,
                  base::CancelableTaskTracker* tracker);

  // Must run on the owner (UI) thread in production.
  // Intermediate callback for GetLargeIconOrFallbackStyle(). Invokes
  // ProcessIconOnBackgroundThread() so we do not perform complex image
  // operations on the UI thread.
  void OnIconLookupComplete(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

 private:
  friend class base::RefCountedThreadSafe<LargeIconWorker>;

  ~LargeIconWorker();

  // Must run on a background thread in production.
  // Tries to resize |bitmap_result_| and pass the output to |callback_|. If
  // that does not work, computes the icon fallback style and uses it to
  // invoke |callback_|. This must be run on a background thread because image
  // resizing and dominant color extraction can be expensive.
  void ProcessIconOnBackgroundThread();

  // Must run on a background thread in production.
  // If |bitmap_result_| is square and large enough (>= |min_source_in_pixel_|),
  // resizes it to |desired_size_in_pixel_| (but if |desired_size_in_pixel_| is
  // 0 then don't resize). If successful, stores the resulting bitmap data
  // into |resized_bitmap_result| and returns true.
  bool ResizeLargeIconOnBackgroundThreadIfValid(
      favicon_base::FaviconRawBitmapResult* resized_bitmap_result);

  // Must run on the owner (UI) thread in production.
  // Invoked when ProcessIconOnBackgroundThread() is done.
  void OnIconProcessingComplete();

  int min_source_size_in_pixel_;
  int desired_size_in_pixel_;
  favicon_base::LargeIconCallback callback_;
  scoped_refptr<base::TaskRunner> background_task_runner_;
  base::CancelableTaskTracker* tracker_;
  favicon_base::FaviconRawBitmapResult bitmap_result_;
  std::unique_ptr<favicon_base::LargeIconResult> result_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconWorker);
};

LargeIconWorker::LargeIconWorker(
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback callback,
    scoped_refptr<base::TaskRunner> background_task_runner,
    base::CancelableTaskTracker* tracker)
    : min_source_size_in_pixel_(min_source_size_in_pixel),
      desired_size_in_pixel_(desired_size_in_pixel),
      callback_(callback),
      background_task_runner_(background_task_runner),
      tracker_(tracker) {
}

LargeIconWorker::~LargeIconWorker() {
}

void LargeIconWorker::OnIconLookupComplete(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  bitmap_result_ = bitmap_result;
  tracker_->PostTaskAndReply(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&LargeIconWorker::ProcessIconOnBackgroundThread, this),
      base::Bind(&LargeIconWorker::OnIconProcessingComplete, this));
}

void LargeIconWorker::ProcessIconOnBackgroundThread() {
  favicon_base::FaviconRawBitmapResult resized_bitmap_result;
  if (ResizeLargeIconOnBackgroundThreadIfValid(&resized_bitmap_result)) {
    result_.reset(
        new favicon_base::LargeIconResult(resized_bitmap_result));
  } else {
    // Failed to resize |bitmap_result_|, so compute fallback icon style.
    std::unique_ptr<favicon_base::FallbackIconStyle> fallback_icon_style(
        new favicon_base::FallbackIconStyle());
    if (bitmap_result_.is_valid()) {
      favicon_base::SetDominantColorAsBackground(
          bitmap_result_.bitmap_data, fallback_icon_style.get());
    }
    result_.reset(
        new favicon_base::LargeIconResult(fallback_icon_style.release()));
  }
}

bool LargeIconWorker::ResizeLargeIconOnBackgroundThreadIfValid(
    favicon_base::FaviconRawBitmapResult* resized_bitmap_result) {
  // Require bitmap to be valid and square.
  if (!bitmap_result_.is_valid() ||
      bitmap_result_.pixel_size.width() != bitmap_result_.pixel_size.height())
    return false;

  // Require bitmap to be large enough. It's square, so just check width.
  if (bitmap_result_.pixel_size.width() < min_source_size_in_pixel_)
    return false;

  *resized_bitmap_result = bitmap_result_;

  // Special case: Can use |bitmap_result_| as is.
  if (desired_size_in_pixel_ == 0 ||
      bitmap_result_.pixel_size.width() == desired_size_in_pixel_)
    return true;

  // Resize bitmap: decode PNG, resize, and re-encode PNG.
  SkBitmap decoded_bitmap;
  if (!gfx::PNGCodec::Decode(bitmap_result_.bitmap_data->front(),
          bitmap_result_.bitmap_data->size(), &decoded_bitmap))
    return false;

  SkBitmap resized_bitmap = skia::ImageOperations::Resize(
      decoded_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
      desired_size_in_pixel_, desired_size_in_pixel_);

  std::vector<unsigned char> bitmap_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(resized_bitmap, false, &bitmap_data))
    return false;

  resized_bitmap_result->pixel_size =
      gfx::Size(desired_size_in_pixel_, desired_size_in_pixel_);
  resized_bitmap_result->bitmap_data =
      base::RefCountedBytes::TakeVector(&bitmap_data);
  return true;
}

void LargeIconWorker::OnIconProcessingComplete() {
  callback_.Run(*result_);
}

}  // namespace

namespace favicon {

LargeIconService::LargeIconService(
    FaviconService* favicon_service,
    const scoped_refptr<base::TaskRunner>& background_task_runner)
    : favicon_service_(favicon_service),
      background_task_runner_(background_task_runner) {
  large_icon_types_.push_back(favicon_base::IconType::FAVICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_ICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_PRECOMPOSED_ICON);
}

LargeIconService::~LargeIconService() {
}

base::CancelableTaskTracker::TaskId
    LargeIconService::GetLargeIconOrFallbackStyle(
        const GURL& page_url,
        int min_source_size_in_pixel,
        int desired_size_in_pixel,
        const favicon_base::LargeIconCallback& callback,
        base::CancelableTaskTracker* tracker) {
  DCHECK_LE(1, min_source_size_in_pixel);
  DCHECK_LE(0, desired_size_in_pixel);

  scoped_refptr<LargeIconWorker> worker =
      new LargeIconWorker(min_source_size_in_pixel, desired_size_in_pixel,
                          callback, background_task_runner_, tracker);

  // TODO(beaudoin): For now this is just a wrapper around
  //   GetLargestRawFaviconForPageURL. Add the logic required to select the best
  //   possible large icon. Also add logic to fetch-on-demand when the URL of
  //   a large icon is known but its bitmap is not available.
  return favicon_service_->GetLargestRawFaviconForPageURL(
      page_url, large_icon_types_, min_source_size_in_pixel,
      base::Bind(&LargeIconWorker::OnIconLookupComplete, worker),
      tracker);
}

}  // namespace favicon
