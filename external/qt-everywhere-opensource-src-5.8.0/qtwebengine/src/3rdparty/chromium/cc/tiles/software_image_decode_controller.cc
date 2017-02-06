// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/software_image_decode_controller.h"

#include <inttypes.h>
#include <stdint.h>

#include <algorithm>
#include <functional>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/raster/tile_task.h"
#include "cc/resources/resource_format_utils.h"
#include "cc/tiles/mipmap_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

// The largest single high quality image to try and process. Images above this
// size will drop down to medium quality.
const size_t kMaxHighQualityImageSizeBytes = 64 * 1024 * 1024;

// The number of entries to keep around in the cache. This limit can be breached
// if more items are locked. That is, locked items ignore this limit.
const size_t kMaxItemsInCache = 1000;

class AutoRemoveKeyFromTaskMap {
 public:
  AutoRemoveKeyFromTaskMap(
      std::unordered_map<SoftwareImageDecodeController::ImageKey,
                         scoped_refptr<TileTask>,
                         SoftwareImageDecodeController::ImageKeyHash>* task_map,
      const SoftwareImageDecodeController::ImageKey& key)
      : task_map_(task_map), key_(key) {}
  ~AutoRemoveKeyFromTaskMap() { task_map_->erase(key_); }

 private:
  std::unordered_map<SoftwareImageDecodeController::ImageKey,
                     scoped_refptr<TileTask>,
                     SoftwareImageDecodeController::ImageKeyHash>* task_map_;
  SoftwareImageDecodeController::ImageKey key_;
};

class ImageDecodeTaskImpl : public TileTask {
 public:
  ImageDecodeTaskImpl(SoftwareImageDecodeController* controller,
                      const SoftwareImageDecodeController::ImageKey& image_key,
                      const DrawImage& image,
                      const ImageDecodeController::TracingInfo& tracing_info)
      : TileTask(true),
        controller_(controller),
        image_key_(image_key),
        image_(image),
        tracing_info_(tracing_info) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageDecodeTaskImpl::RunOnWorkerThread", "mode",
                 "software", "source_prepare_tiles_id",
                 tracing_info_.prepare_tiles_id);
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        image_.image().get());
    controller_->DecodeImage(image_key_, image_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    controller_->RemovePendingTask(image_key_);
  }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  SoftwareImageDecodeController* controller_;
  SoftwareImageDecodeController::ImageKey image_key_;
  DrawImage image_;
  const ImageDecodeController::TracingInfo tracing_info_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

SkSize GetScaleAdjustment(const ImageDecodeControllerKey& key) {
  // If the requested filter quality did not require scale, then the adjustment
  // is identity.
  if (key.can_use_original_decode()) {
    return SkSize::Make(1.f, 1.f);
  } else if (key.filter_quality() == kMedium_SkFilterQuality) {
    return MipMapUtil::GetScaleAdjustmentForSize(key.src_rect().size(),
                                                 key.target_size());
  } else {
    float x_scale =
        key.target_size().width() / static_cast<float>(key.src_rect().width());
    float y_scale = key.target_size().height() /
                    static_cast<float>(key.src_rect().height());
    return SkSize::Make(x_scale, y_scale);
  }
}

SkFilterQuality GetDecodedFilterQuality(const ImageDecodeControllerKey& key) {
  return std::min(key.filter_quality(), kLow_SkFilterQuality);
}

SkImageInfo CreateImageInfo(size_t width,
                            size_t height,
                            ResourceFormat format) {
  return SkImageInfo::Make(width, height,
                           ResourceFormatToClosestSkColorType(format),
                           kPremul_SkAlphaType);
}

void RecordLockExistingCachedImageHistogram(TilePriority::PriorityBin bin,
                                            bool success) {
  switch (bin) {
    case TilePriority::NOW:
      UMA_HISTOGRAM_BOOLEAN("Renderer4.LockExistingCachedImage.Software.NOW",
                            success);
    case TilePriority::SOON:
      UMA_HISTOGRAM_BOOLEAN("Renderer4.LockExistingCachedImage.Software.SOON",
                            success);
    case TilePriority::EVENTUALLY:
      UMA_HISTOGRAM_BOOLEAN(
          "Renderer4.LockExistingCachedImage.Software.EVENTUALLY", success);
  }
}

}  // namespace

SoftwareImageDecodeController::SoftwareImageDecodeController(
    ResourceFormat format,
    size_t locked_memory_limit_bytes)
    : decoded_images_(ImageMRUCache::NO_AUTO_EVICT),
      at_raster_decoded_images_(ImageMRUCache::NO_AUTO_EVICT),
      locked_images_budget_(locked_memory_limit_bytes),
      format_(format) {
  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::SoftwareImageDecodeController",
        base::ThreadTaskRunnerHandle::Get());
  }
}

SoftwareImageDecodeController::~SoftwareImageDecodeController() {
  DCHECK_EQ(0u, decoded_images_ref_counts_.size());
  DCHECK_EQ(0u, at_raster_decoded_images_ref_counts_.size());

  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool SoftwareImageDecodeController::GetTaskForImageAndRef(
    const DrawImage& image,
    const TracingInfo& tracing_info,
    scoped_refptr<TileTask>* task) {
  // If the image already exists or if we're going to create a task for it, then
  // we'll likely need to ref this image (the exception is if we're prerolling
  // the image only). That means the image is or will be in the cache. When the
  // ref goes to 0, it will be unpinned but will remain in the cache. If the
  // image does not fit into the budget, then we don't ref this image, since it
  // will be decoded at raster time which is when it will be temporarily put in
  // the cache.
  ImageKey key = ImageKey::FromDrawImage(image);
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::GetTaskForImageAndRef", "key",
               key.ToString());

  // If the target size is empty, we can skip this image during draw (and thus
  // we don't need to decode it or ref it).
  if (key.target_size().IsEmpty()) {
    *task = nullptr;
    return false;
  }

  base::AutoLock lock(lock_);

  // If we already have the image in cache, then we can return it.
  auto decoded_it = decoded_images_.Get(key);
  bool new_image_fits_in_memory =
      locked_images_budget_.AvailableMemoryBytes() >= key.locked_bytes();
  if (decoded_it != decoded_images_.end()) {
    bool image_was_locked = decoded_it->second->is_locked();
    if (image_was_locked ||
        (new_image_fits_in_memory && decoded_it->second->Lock())) {
      RefImage(key);
      *task = nullptr;
      SanityCheckState(__LINE__, true);

      // If the image wasn't locked, then we just succeeded in locking it.
      if (!image_was_locked) {
        RecordLockExistingCachedImageHistogram(tracing_info.requesting_tile_bin,
                                               true);
      }
      return true;
    }

    // If the image fits in memory, then we at least tried to lock it and
    // failed. This means that it's not valid anymore.
    if (new_image_fits_in_memory) {
      RecordLockExistingCachedImageHistogram(tracing_info.requesting_tile_bin,
                                             false);
      decoded_images_.Erase(decoded_it);
    }
  }

  // If the task exists, return it.
  scoped_refptr<TileTask>& existing_task = pending_image_tasks_[key];
  if (existing_task) {
    RefImage(key);
    *task = existing_task;
    SanityCheckState(__LINE__, true);
    return true;
  }

  // At this point, we have to create a new image/task, so we need to abort if
  // it doesn't fit into memory and there are currently no raster tasks that
  // would have already accounted for memory. The latter part is possible if
  // there's a running raster task that could not be canceled, and still has a
  // ref to the image that is now being reffed for the new schedule.
  if (!new_image_fits_in_memory && (decoded_images_ref_counts_.find(key) ==
                                    decoded_images_ref_counts_.end())) {
    *task = nullptr;
    SanityCheckState(__LINE__, true);
    return false;
  }

  // Actually create the task. RefImage will account for memory on the first
  // ref.
  RefImage(key);
  existing_task = make_scoped_refptr(
      new ImageDecodeTaskImpl(this, key, image, tracing_info));
  *task = existing_task;
  SanityCheckState(__LINE__, true);
  return true;
}

void SoftwareImageDecodeController::RefImage(const ImageKey& key) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::RefImage", "key",
               key.ToString());
  lock_.AssertAcquired();
  int ref = ++decoded_images_ref_counts_[key];
  if (ref == 1) {
    DCHECK_GE(locked_images_budget_.AvailableMemoryBytes(), key.locked_bytes());
    locked_images_budget_.AddUsage(key.locked_bytes());
  }
}

void SoftwareImageDecodeController::UnrefImage(const DrawImage& image) {
  // When we unref the image, there are several situations we need to consider:
  // 1. The ref did not reach 0, which means we have to keep the image locked.
  // 2. The ref reached 0, we should unlock it.
  //   2a. The image isn't in the locked cache because we didn't get to decode
  //       it yet (or failed to decode it).
  //   2b. Unlock the image but keep it in list.
  const ImageKey& key = ImageKey::FromDrawImage(image);
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::UnrefImage", "key",
               key.ToString());

  base::AutoLock lock(lock_);
  auto ref_count_it = decoded_images_ref_counts_.find(key);
  DCHECK(ref_count_it != decoded_images_ref_counts_.end());

  --ref_count_it->second;
  if (ref_count_it->second == 0) {
    decoded_images_ref_counts_.erase(ref_count_it);
    locked_images_budget_.SubtractUsage(key.locked_bytes());

    auto decoded_image_it = decoded_images_.Peek(key);
    // If we've never decoded the image before ref reached 0, then we wouldn't
    // have it in our cache. This would happen if we canceled tasks.
    if (decoded_image_it == decoded_images_.end()) {
      SanityCheckState(__LINE__, true);
      return;
    }
    DCHECK(decoded_image_it->second->is_locked());
    decoded_image_it->second->Unlock();
  }
  SanityCheckState(__LINE__, true);
}

void SoftwareImageDecodeController::DecodeImage(const ImageKey& key,
                                                const DrawImage& image) {
  TRACE_EVENT1("cc", "SoftwareImageDecodeController::DecodeImage", "key",
               key.ToString());
  base::AutoLock lock(lock_);
  AutoRemoveKeyFromTaskMap remove_key_from_task_map(&pending_image_tasks_, key);

  // We could have finished all of the raster tasks (cancelled) while the task
  // was just starting to run. Since this task already started running, it
  // wasn't cancelled. So, if the ref count for the image is 0 then we can just
  // abort.
  if (decoded_images_ref_counts_.find(key) ==
      decoded_images_ref_counts_.end()) {
    return;
  }

  auto image_it = decoded_images_.Peek(key);
  if (image_it != decoded_images_.end()) {
    if (image_it->second->is_locked() || image_it->second->Lock())
      return;
    decoded_images_.Erase(image_it);
  }

  std::unique_ptr<DecodedImage> decoded_image;
  {
    base::AutoUnlock unlock(lock_);
    decoded_image = DecodeImageInternal(key, image);
  }

  // Abort if we failed to decode the image.
  if (!decoded_image)
    return;

  // At this point, it could have been the case that this image was decoded in
  // place by an already running raster task from a previous schedule. If that's
  // the case, then it would have already been placed into the cache (possibly
  // locked). Remove it if that was the case.
  image_it = decoded_images_.Peek(key);
  if (image_it != decoded_images_.end()) {
    if (image_it->second->is_locked() || image_it->second->Lock()) {
      // Make sure to unlock the decode we did in this function.
      decoded_image->Unlock();
      return;
    }
    decoded_images_.Erase(image_it);
  }

  // We could have finished all of the raster tasks (cancelled) while this image
  // decode task was running, which means that we now have a locked image but no
  // ref counts. Unlock it immediately in this case.
  if (decoded_images_ref_counts_.find(key) ==
      decoded_images_ref_counts_.end()) {
    decoded_image->Unlock();
  }

  decoded_images_.Put(key, std::move(decoded_image));
  SanityCheckState(__LINE__, true);
}

std::unique_ptr<SoftwareImageDecodeController::DecodedImage>
SoftwareImageDecodeController::DecodeImageInternal(
    const ImageKey& key,
    const DrawImage& draw_image) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::DecodeImageInternal", "key",
               key.ToString());
  sk_sp<const SkImage> image = draw_image.image();
  if (!image)
    return nullptr;

  switch (key.filter_quality()) {
    case kNone_SkFilterQuality:
    case kLow_SkFilterQuality:
      return GetOriginalImageDecode(key, std::move(image));
    case kMedium_SkFilterQuality:
    case kHigh_SkFilterQuality:
      return GetScaledImageDecode(key, std::move(image));
    default:
      NOTREACHED();
      return nullptr;
  }
}

DecodedDrawImage SoftwareImageDecodeController::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  ImageKey key = ImageKey::FromDrawImage(draw_image);
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::GetDecodedImageForDraw", "key",
               key.ToString());
  // If the target size is empty, we can skip this image draw.
  if (key.target_size().IsEmpty())
    return DecodedDrawImage(nullptr, kNone_SkFilterQuality);

  return GetDecodedImageForDrawInternal(key, draw_image);
}

DecodedDrawImage SoftwareImageDecodeController::GetDecodedImageForDrawInternal(
    const ImageKey& key,
    const DrawImage& draw_image) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::GetDecodedImageForDrawInternal",
               "key", key.ToString());
  base::AutoLock lock(lock_);
  auto decoded_images_it = decoded_images_.Get(key);
  // If we found the image and it's locked, then return it. If it's not locked,
  // erase it from the cache since it might be put into the at-raster cache.
  std::unique_ptr<DecodedImage> scoped_decoded_image;
  DecodedImage* decoded_image = nullptr;
  if (decoded_images_it != decoded_images_.end()) {
    decoded_image = decoded_images_it->second.get();
    if (decoded_image->is_locked()) {
      RefImage(key);
      decoded_image->mark_used();
      SanityCheckState(__LINE__, true);
      return DecodedDrawImage(
          decoded_image->image(), decoded_image->src_rect_offset(),
          GetScaleAdjustment(key), GetDecodedFilterQuality(key));
    } else {
      scoped_decoded_image = std::move(decoded_images_it->second);
      decoded_images_.Erase(decoded_images_it);
    }
  }

  // See if another thread already decoded this image at raster time. If so, we
  // can just use that result directly.
  auto at_raster_images_it = at_raster_decoded_images_.Get(key);
  if (at_raster_images_it != at_raster_decoded_images_.end()) {
    DCHECK(at_raster_images_it->second->is_locked());
    RefAtRasterImage(key);
    SanityCheckState(__LINE__, true);
    DecodedImage* at_raster_decoded_image = at_raster_images_it->second.get();
    at_raster_decoded_image->mark_used();
    auto decoded_draw_image =
        DecodedDrawImage(at_raster_decoded_image->image(),
                         at_raster_decoded_image->src_rect_offset(),
                         GetScaleAdjustment(key), GetDecodedFilterQuality(key));
    decoded_draw_image.set_at_raster_decode(true);
    return decoded_draw_image;
  }

  // Now we know that we don't have a locked image, and we seem to be the first
  // thread encountering this image (that might not be true, since other threads
  // might be decoding it already). This means that we need to decode the image
  // assuming we can't lock the one we found in the cache.
  bool check_at_raster_cache = false;
  if (!decoded_image || !decoded_image->Lock()) {
    // Note that we have to release the lock, since this lock is also accessed
    // on the compositor thread. This means holding on to the lock might stall
    // the compositor thread for the duration of the decode!
    base::AutoUnlock unlock(lock_);
    scoped_decoded_image = DecodeImageInternal(key, draw_image);
    decoded_image = scoped_decoded_image.get();

    // Skip the image if we couldn't decode it.
    if (!decoded_image)
      return DecodedDrawImage(nullptr, kNone_SkFilterQuality);
    check_at_raster_cache = true;
  }

  DCHECK(decoded_image == scoped_decoded_image.get());

  // While we unlocked the lock, it could be the case that another thread
  // already decoded this already and put it in the at-raster cache. Look it up
  // first.
  if (check_at_raster_cache) {
    at_raster_images_it = at_raster_decoded_images_.Get(key);
    if (at_raster_images_it != at_raster_decoded_images_.end()) {
      // We have to drop our decode, since the one in the cache is being used by
      // another thread.
      decoded_image->Unlock();
      decoded_image = at_raster_images_it->second.get();
      scoped_decoded_image = nullptr;
    }
  }

  // If we really are the first ones, or if the other thread already unlocked
  // the image, then put our work into at-raster time cache.
  if (scoped_decoded_image)
    at_raster_decoded_images_.Put(key, std::move(scoped_decoded_image));

  DCHECK(decoded_image);
  DCHECK(decoded_image->is_locked());
  RefAtRasterImage(key);
  SanityCheckState(__LINE__, true);
  decoded_image->mark_used();
  auto decoded_draw_image =
      DecodedDrawImage(decoded_image->image(), decoded_image->src_rect_offset(),
                       GetScaleAdjustment(key), GetDecodedFilterQuality(key));
  decoded_draw_image.set_at_raster_decode(true);
  return decoded_draw_image;
}

std::unique_ptr<SoftwareImageDecodeController::DecodedImage>
SoftwareImageDecodeController::GetOriginalImageDecode(
    const ImageKey& key,
    sk_sp<const SkImage> image) {
  SkImageInfo decoded_info =
      CreateImageInfo(image->width(), image->height(), format_);
  std::unique_ptr<base::DiscardableMemory> decoded_pixels;
  {
    TRACE_EVENT0("disabled-by-default-cc.debug",
                 "SoftwareImageDecodeController::GetOriginalImageDecode - "
                 "allocate decoded pixels");
    decoded_pixels =
        base::DiscardableMemoryAllocator::GetInstance()
            ->AllocateLockedDiscardableMemory(decoded_info.minRowBytes() *
                                              decoded_info.height());
  }
  {
    TRACE_EVENT0("disabled-by-default-cc.debug",
                 "SoftwareImageDecodeController::GetOriginalImageDecode - "
                 "read pixels");
    bool result = image->readPixels(decoded_info, decoded_pixels->data(),
                                    decoded_info.minRowBytes(), 0, 0,
                                    SkImage::kDisallow_CachingHint);

    if (!result) {
      decoded_pixels->Unlock();
      return nullptr;
    }
  }
  return base::WrapUnique(
      new DecodedImage(decoded_info, std::move(decoded_pixels),
                       SkSize::Make(0, 0), next_tracing_id_.GetNext()));
}

std::unique_ptr<SoftwareImageDecodeController::DecodedImage>
SoftwareImageDecodeController::GetScaledImageDecode(
    const ImageKey& key,
    sk_sp<const SkImage> image) {
  // Construct a key to use in GetDecodedImageForDrawInternal().
  // This allows us to reuse an image in any cache if available.
  gfx::Rect full_image_rect(image->width(), image->height());
  DrawImage original_size_draw_image(std::move(image),
                                     gfx::RectToSkIRect(full_image_rect),
                                     kNone_SkFilterQuality, SkMatrix::I());
  ImageKey original_size_key =
      ImageKey::FromDrawImage(original_size_draw_image);
  // Sanity checks.
  DCHECK(original_size_key.can_use_original_decode())
      << original_size_key.ToString();
  DCHECK(full_image_rect.size() == original_size_key.target_size());

  auto decoded_draw_image = GetDecodedImageForDrawInternal(
      original_size_key, original_size_draw_image);
  if (!decoded_draw_image.image()) {
    DrawWithImageFinished(original_size_draw_image, decoded_draw_image);
    return nullptr;
  }

  SkPixmap decoded_pixmap;
  bool result = decoded_draw_image.image()->peekPixels(&decoded_pixmap);
  DCHECK(result) << key.ToString();
  if (key.src_rect() != full_image_rect) {
    result = decoded_pixmap.extractSubset(&decoded_pixmap,
                                          gfx::RectToSkIRect(key.src_rect()));
    DCHECK(result) << key.ToString();
  }

  DCHECK(!key.target_size().IsEmpty());
  SkImageInfo scaled_info = CreateImageInfo(
      key.target_size().width(), key.target_size().height(), format_);
  std::unique_ptr<base::DiscardableMemory> scaled_pixels;
  {
    TRACE_EVENT0(
        "disabled-by-default-cc.debug",
        "SoftwareImageDecodeController::ScaleImage - allocate scaled pixels");
    scaled_pixels = base::DiscardableMemoryAllocator::GetInstance()
                        ->AllocateLockedDiscardableMemory(
                            scaled_info.minRowBytes() * scaled_info.height());
  }
  SkPixmap scaled_pixmap(scaled_info, scaled_pixels->data(),
                         scaled_info.minRowBytes());
  DCHECK(key.filter_quality() == kHigh_SkFilterQuality ||
         key.filter_quality() == kMedium_SkFilterQuality);
  {
    TRACE_EVENT0("disabled-by-default-cc.debug",
                 "SoftwareImageDecodeController::ScaleImage - scale pixels");
    bool result =
        decoded_pixmap.scalePixels(scaled_pixmap, key.filter_quality());
    DCHECK(result) << key.ToString();
  }

  // Release the original sized decode. Any other intermediate result to release
  // would be the subrect memory. However, that's in a scoped_ptr and will be
  // deleted automatically when we return.
  DrawWithImageFinished(original_size_draw_image, decoded_draw_image);

  return base::WrapUnique(
      new DecodedImage(scaled_info, std::move(scaled_pixels),
                       SkSize::Make(-key.src_rect().x(), -key.src_rect().y()),
                       next_tracing_id_.GetNext()));
}

void SoftwareImageDecodeController::DrawWithImageFinished(
    const DrawImage& image,
    const DecodedDrawImage& decoded_image) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::DrawWithImageFinished", "key",
               ImageKey::FromDrawImage(image).ToString());
  ImageKey key = ImageKey::FromDrawImage(image);
  if (!decoded_image.image())
    return;

  if (decoded_image.is_at_raster_decode())
    UnrefAtRasterImage(key);
  else
    UnrefImage(image);
  SanityCheckState(__LINE__, false);
}

void SoftwareImageDecodeController::RefAtRasterImage(const ImageKey& key) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::RefAtRasterImage", "key",
               key.ToString());
  DCHECK(at_raster_decoded_images_.Peek(key) !=
         at_raster_decoded_images_.end());
  ++at_raster_decoded_images_ref_counts_[key];
}

void SoftwareImageDecodeController::UnrefAtRasterImage(const ImageKey& key) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "SoftwareImageDecodeController::UnrefAtRasterImage", "key",
               key.ToString());
  base::AutoLock lock(lock_);

  auto ref_it = at_raster_decoded_images_ref_counts_.find(key);
  DCHECK(ref_it != at_raster_decoded_images_ref_counts_.end());
  --ref_it->second;
  if (ref_it->second == 0) {
    at_raster_decoded_images_ref_counts_.erase(ref_it);
    auto at_raster_image_it = at_raster_decoded_images_.Peek(key);
    DCHECK(at_raster_image_it != at_raster_decoded_images_.end());

    // The ref for our image reached 0 and it's still locked. We need to figure
    // out what the best thing to do with the image. There are several
    // situations:
    // 1. The image is not in the main cache and...
    //    1a. ... its ref count is 0: unlock our image and put it in the main
    //    cache.
    //    1b. ... ref count is not 0: keep the image locked and put it in the
    //    main cache.
    // 2. The image is in the main cache...
    //    2a. ... and is locked: unlock our image and discard it
    //    2b. ... and is unlocked and...
    //       2b1. ... its ref count is 0: unlock our image and replace the
    //       existing one with ours.
    //       2b2. ... its ref count is not 0: this shouldn't be possible.
    auto image_it = decoded_images_.Peek(key);
    if (image_it == decoded_images_.end()) {
      if (decoded_images_ref_counts_.find(key) ==
          decoded_images_ref_counts_.end()) {
        at_raster_image_it->second->Unlock();
      }
      decoded_images_.Put(key, std::move(at_raster_image_it->second));
    } else if (image_it->second->is_locked()) {
      at_raster_image_it->second->Unlock();
    } else {
      DCHECK(decoded_images_ref_counts_.find(key) ==
             decoded_images_ref_counts_.end());
      at_raster_image_it->second->Unlock();
      decoded_images_.Erase(image_it);
      decoded_images_.Put(key, std::move(at_raster_image_it->second));
    }
    at_raster_decoded_images_.Erase(at_raster_image_it);
  }
}

void SoftwareImageDecodeController::ReduceCacheUsage() {
  TRACE_EVENT0("cc", "SoftwareImageDecodeController::ReduceCacheUsage");
  base::AutoLock lock(lock_);
  size_t num_to_remove = (decoded_images_.size() > kMaxItemsInCache)
                             ? (decoded_images_.size() - kMaxItemsInCache)
                             : 0;
  for (auto it = decoded_images_.rbegin();
       num_to_remove != 0 && it != decoded_images_.rend();) {
    if (it->second->is_locked()) {
      ++it;
      continue;
    }

    it = decoded_images_.Erase(it);
    --num_to_remove;
  }
}

void SoftwareImageDecodeController::RemovePendingTask(const ImageKey& key) {
  base::AutoLock lock(lock_);
  pending_image_tasks_.erase(key);
}

bool SoftwareImageDecodeController::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);

  // Dump each of our caches.
  DumpImageMemoryForCache(decoded_images_, "cached", pmd);
  DumpImageMemoryForCache(at_raster_decoded_images_, "at_raster", pmd);

  // Memory dump can't fail, always return true.
  return true;
}

void SoftwareImageDecodeController::DumpImageMemoryForCache(
    const ImageMRUCache& cache,
    const char* cache_name,
    base::trace_event::ProcessMemoryDump* pmd) const {
  lock_.AssertAcquired();

  for (const auto& image_pair : cache) {
    std::string dump_name = base::StringPrintf(
        "cc/image_memory/controller_0x%" PRIXPTR "/%s/image_%" PRIu64 "_id_%d",
        reinterpret_cast<uintptr_t>(this), cache_name,
        image_pair.second->tracing_id(), image_pair.first.image_id());
    base::trace_event::MemoryAllocatorDump* dump =
        image_pair.second->memory()->CreateMemoryAllocatorDump(
            dump_name.c_str(), pmd);
    DCHECK(dump);
    if (image_pair.second->is_locked()) {
      dump->AddScalar("locked_size",
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      image_pair.first.locked_bytes());
    }
  }
}

void SoftwareImageDecodeController::SanityCheckState(int line,
                                                     bool lock_acquired) {
#if DCHECK_IS_ON()
  if (!lock_acquired) {
    base::AutoLock lock(lock_);
    SanityCheckState(line, true);
    return;
  }

  MemoryBudget budget(locked_images_budget_.total_limit_bytes());
  for (const auto& image_pair : decoded_images_) {
    const auto& key = image_pair.first;
    const auto& image = image_pair.second;

    auto ref_it = decoded_images_ref_counts_.find(key);
    if (image->is_locked()) {
      budget.AddUsage(key.locked_bytes());
      DCHECK(ref_it != decoded_images_ref_counts_.end()) << line;
    } else {
      DCHECK(ref_it == decoded_images_ref_counts_.end() ||
             pending_image_tasks_.find(key) != pending_image_tasks_.end())
          << line;
    }
  }
  DCHECK_GE(budget.AvailableMemoryBytes(),
            locked_images_budget_.AvailableMemoryBytes())
      << line;
#endif  // DCHECK_IS_ON()
}

// SoftwareImageDecodeControllerKey
ImageDecodeControllerKey ImageDecodeControllerKey::FromDrawImage(
    const DrawImage& image) {
  const SkSize& scale = image.scale();
  // If the src_rect falls outside of the image, we need to clip it since
  // otherwise we might end up with uninitialized memory in the decode process.
  // Note that the scale is still unchanged and the target size is now a
  // function of the new src_rect.
  gfx::Rect src_rect = gfx::IntersectRects(
      gfx::SkIRectToRect(image.src_rect()),
      gfx::Rect(image.image()->width(), image.image()->height()));

  gfx::Size target_size(
      SkScalarRoundToInt(std::abs(src_rect.width() * scale.width())),
      SkScalarRoundToInt(std::abs(src_rect.height() * scale.height())));

  // Start with the quality that was requested.
  SkFilterQuality quality = image.filter_quality();

  // If we're not going to do a scale, we can use low filter quality. Note that
  // checking if the sizes are the same is better than checking if scale is 1.f,
  // because even non-1 scale can result in the same (rounded) width/height.
  if (target_size.width() == src_rect.width() &&
      target_size.height() == src_rect.height()) {
    quality = std::min(quality, kLow_SkFilterQuality);
  }

  // Drop from high to medium if the the matrix we applied wasn't decomposable,
  // or if the scaled image will be too large.
  if (quality == kHigh_SkFilterQuality) {
    if (!image.matrix_is_decomposable()) {
      quality = kMedium_SkFilterQuality;
    } else {
      base::CheckedNumeric<size_t> size = 4u;
      size *= target_size.width();
      size *= target_size.height();
      if (size.ValueOrDefault(std::numeric_limits<size_t>::max()) >
          kMaxHighQualityImageSizeBytes) {
        quality = kMedium_SkFilterQuality;
      }
    }
  }

  // Drop from medium to low if the matrix we applied wasn't decomposable or if
  // we're enlarging the image in both dimensions.
  if (quality == kMedium_SkFilterQuality) {
    if (!image.matrix_is_decomposable() ||
        (scale.width() >= 1.f && scale.height() >= 1.f)) {
      quality = kLow_SkFilterQuality;
    }
  }

  bool can_use_original_decode =
      quality == kLow_SkFilterQuality || quality == kNone_SkFilterQuality;

  // If we're going to use the original decode, then the target size should be
  // the full image size, since that will allow for proper memory accounting.
  // Note we skip the decode if the target size is empty altogether, so don't
  // update the target size in that case.
  if (can_use_original_decode && !target_size.IsEmpty())
    target_size = gfx::Size(image.image()->width(), image.image()->height());

  if (quality == kMedium_SkFilterQuality && !target_size.IsEmpty()) {
    SkSize mip_target_size =
        MipMapUtil::GetScaleAdjustmentForSize(src_rect.size(), target_size);
    target_size.set_width(src_rect.width() * mip_target_size.width());
    target_size.set_height(src_rect.height() * mip_target_size.height());
  }

  return ImageDecodeControllerKey(image.image()->uniqueID(), src_rect,
                                  target_size, quality,
                                  can_use_original_decode);
}

ImageDecodeControllerKey::ImageDecodeControllerKey(
    uint32_t image_id,
    const gfx::Rect& src_rect,
    const gfx::Size& target_size,
    SkFilterQuality filter_quality,
    bool can_use_original_decode)
    : image_id_(image_id),
      src_rect_(src_rect),
      target_size_(target_size),
      filter_quality_(filter_quality),
      can_use_original_decode_(can_use_original_decode) {
  if (can_use_original_decode_) {
    hash_ = std::hash<uint32_t>()(image_id_);
  } else {
    // TODO(vmpstr): This is a mess. Maybe it's faster to just search the vector
    // always (forwards or backwards to account for LRU).
    uint64_t src_rect_hash = base::HashInts(
        static_cast<uint64_t>(base::HashInts(src_rect_.x(), src_rect_.y())),
        static_cast<uint64_t>(
            base::HashInts(src_rect_.width(), src_rect_.height())));

    uint64_t target_size_hash =
        base::HashInts(target_size_.width(), target_size_.height());

    hash_ = base::HashInts(base::HashInts(src_rect_hash, target_size_hash),
                           base::HashInts(image_id_, filter_quality_));
  }
}

ImageDecodeControllerKey::ImageDecodeControllerKey(
    const ImageDecodeControllerKey& other) = default;

std::string ImageDecodeControllerKey::ToString() const {
  std::ostringstream str;
  str << "id[" << image_id_ << "] src_rect[" << src_rect_.x() << ","
      << src_rect_.y() << " " << src_rect_.width() << "x" << src_rect_.height()
      << "] target_size[" << target_size_.width() << "x"
      << target_size_.height() << "] filter_quality[" << filter_quality_
      << "] can_use_original_decode [" << can_use_original_decode_ << "] hash ["
      << hash_ << "]";
  return str.str();
}

// DecodedImage
SoftwareImageDecodeController::DecodedImage::DecodedImage(
    const SkImageInfo& info,
    std::unique_ptr<base::DiscardableMemory> memory,
    const SkSize& src_rect_offset,
    uint64_t tracing_id)
    : locked_(true),
      image_info_(info),
      memory_(std::move(memory)),
      src_rect_offset_(src_rect_offset),
      tracing_id_(tracing_id) {
  SkPixmap pixmap(image_info_, memory_->data(), image_info_.minRowBytes());
  image_ = SkImage::MakeFromRaster(
      pixmap, [](const void* pixels, void* context) {}, nullptr);
}

SoftwareImageDecodeController::DecodedImage::~DecodedImage() {
  DCHECK(!locked_);
  // lock_count | used  | last lock failed | result state
  // ===========+=======+==================+==================
  //  1         | false | false            | WASTED
  //  1         | false | true             | WASTED
  //  1         | true  | false            | USED
  //  1         | true  | true             | USED_RELOCK_FAILED
  //  >1        | false | false            | WASTED_RELOCKED
  //  >1        | false | true             | WASTED_RELOCKED
  //  >1        | true  | false            | USED_RELOCKED
  //  >1        | true  | true             | USED_RELOCKED
  // Note that it's important not to reorder the following enums, since the
  // numerical values are used in the histogram code.
  enum State : int {
    DECODED_IMAGE_STATE_WASTED,
    DECODED_IMAGE_STATE_USED,
    DECODED_IMAGE_STATE_USED_RELOCK_FAILED,
    DECODED_IMAGE_STATE_WASTED_RELOCKED,
    DECODED_IMAGE_STATE_USED_RELOCKED,
    DECODED_IMAGE_STATE_COUNT
  } state = DECODED_IMAGE_STATE_WASTED;

  if (usage_stats_.lock_count == 1) {
    if (!usage_stats_.used)
      state = DECODED_IMAGE_STATE_WASTED;
    else if (usage_stats_.last_lock_failed)
      state = DECODED_IMAGE_STATE_USED_RELOCK_FAILED;
    else
      state = DECODED_IMAGE_STATE_USED;
  } else {
    if (usage_stats_.used)
      state = DECODED_IMAGE_STATE_USED_RELOCKED;
    else
      state = DECODED_IMAGE_STATE_WASTED_RELOCKED;
  }

  UMA_HISTOGRAM_ENUMERATION("Renderer4.SoftwareImageDecodeState", state,
                            DECODED_IMAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.SoftwareImageDecodeState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
}

bool SoftwareImageDecodeController::DecodedImage::Lock() {
  DCHECK(!locked_);
  bool success = memory_->Lock();
  if (!success) {
    usage_stats_.last_lock_failed = true;
    return false;
  }
  locked_ = true;
  ++usage_stats_.lock_count;
  return true;
}

void SoftwareImageDecodeController::DecodedImage::Unlock() {
  DCHECK(locked_);
  memory_->Unlock();
  locked_ = false;
  if (usage_stats_.lock_count == 1)
    usage_stats_.first_lock_wasted = !usage_stats_.used;
}

// MemoryBudget
SoftwareImageDecodeController::MemoryBudget::MemoryBudget(size_t limit_bytes)
    : limit_bytes_(limit_bytes), current_usage_bytes_(0u) {}

size_t SoftwareImageDecodeController::MemoryBudget::AvailableMemoryBytes()
    const {
  size_t usage = GetCurrentUsageSafe();
  return usage >= limit_bytes_ ? 0u : (limit_bytes_ - usage);
}

void SoftwareImageDecodeController::MemoryBudget::AddUsage(size_t usage) {
  current_usage_bytes_ += usage;
}

void SoftwareImageDecodeController::MemoryBudget::SubtractUsage(size_t usage) {
  DCHECK_GE(current_usage_bytes_.ValueOrDefault(0u), usage);
  current_usage_bytes_ -= usage;
}

void SoftwareImageDecodeController::MemoryBudget::ResetUsage() {
  current_usage_bytes_ = 0;
}

size_t SoftwareImageDecodeController::MemoryBudget::GetCurrentUsageSafe()
    const {
  return current_usage_bytes_.ValueOrDie();
}

}  // namespace cc
