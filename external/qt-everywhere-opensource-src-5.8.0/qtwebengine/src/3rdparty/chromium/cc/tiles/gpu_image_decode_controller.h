// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_
#define CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/mru_cache.h"
#include "base/memory/discardable_memory.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource_format.h"
#include "cc/tiles/image_decode_controller.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImageTextureData;

namespace cc {

class ContextProvider;

// OVERVIEW:
//
// GpuImageDecodeController handles the decode and upload of images that will
// be used by Skia's GPU raster path. It also maintains a cache of these
// decoded/uploaded images for later re-use.
//
// Generally, when an image is required for raster, GpuImageDecodeController
// creates two tasks, one to decode the image, and one to upload the image to
// the GPU. These tasks are completed before the raster task which depends on
// the image. We need to seperate decode and upload tasks, as decode can occur
// simultaneously on multiple threads, while upload requires the GL context
// lock must happen on our non-concurrent raster thread.
//
// Decoded and Uploaded image data share a single cache entry. Depending on how
// far we've progressed, this cache entry may contain CPU-side decoded data,
// GPU-side uploaded data, or both. Because CPU-side decoded data is stored in
// discardable memory, and is only locked for short periods of time (until the
// upload completes), this memory is not counted against our sized cache
// limits. Uploaded GPU memory, being non-discardable, always counts against
// our limits.
//
// In cases where the number of images needed exceeds our cache limits, we
// operate in an "at-raster" mode. In this mode, there are no decode/upload
// tasks, and images are decoded/uploaded as needed, immediately before being
// used in raster. Cache entries for at-raster tasks are marked as such, which
// prevents future tasks from taking a dependency on them and extending their
// lifetime longer than is necessary.
//
// RASTER-SCALE CACHING:
//
// In order to save memory, images which are going to be scaled may be uploaded
// at lower than original resolution. In these cases, we may later need to
// re-upload the image at a higher resolution. To handle multiple images of
// different scales being in use at the same time, we have a two-part caching
// system.
//
// The first cache, |persistent_cache_|, stores one ImageData per image id.
// These ImageDatas are not necessarily associated with a given DrawImage, and
// are saved (persisted) even when their ref-count reaches zero (assuming they
// fit in the current memory budget). This allows for future re-use of image
// resources.
//
// The second cache, |in_use_cache_|, stores one image data per DrawImage -
// this may be the same ImageData that is in the persistent_cache_.  These
// cache entries are more transient and are deleted as soon as all refs to the
// given DrawImage are released (the image is no longer in-use).
//
// For examples of raster-scale caching, see https://goo.gl/0zCd9Z
//
// REF COUNTING:
//
// In dealing with the two caches in GpuImageDecodeController, there are three
// ref-counting concepts in use:
//   1) ImageData upload/decode ref-counts.
//      These ref-counts represent the overall number of references to the
//      upload or decode portion of an ImageData. These ref-counts control
//      both whether the upload/decode data can be freed, as well as whether an
//      ImageData can be removed from the |persistent_cache_|. ImageDatas are
//      only removed from the |persistent_cache_| if their upload/decode
//      ref-counts are zero or if they are orphaned and replaced by a new entry.
//   2) InUseCacheEntry ref-counts.
//      These ref-counts represent the number of references to an
//      InUseCacheEntry from a specific DrawImage. When the InUseCacheEntry's
//      ref-count reaches 0 it will be deleted.
//   3) scoped_refptr ref-counts.
//      Because both the persistent_cache_ and the in_use_cache_ point at the
//      same ImageDatas (and may need to keep these ImageDatas alive independent
//      of each other), they hold ImageDatas by scoped_refptr. The scoped_refptr
//      keeps an ImageData alive while it is present in either the
//      |persistent_cache_| or |in_use_cache_|.
//
class CC_EXPORT GpuImageDecodeController
    : public ImageDecodeController,
      public base::trace_event::MemoryDumpProvider {
 public:
  explicit GpuImageDecodeController(ContextProvider* context,
                                    ResourceFormat decode_format,
                                    size_t max_gpu_image_bytes);
  ~GpuImageDecodeController() override;

  // ImageDecodeController overrides.

  // Finds the existing uploaded image for the provided DrawImage. Creates an
  // upload task to upload the image if an exsiting image does not exist.
  bool GetTaskForImageAndRef(const DrawImage& image,
                             const TracingInfo& tracing_info,
                             scoped_refptr<TileTask>* task) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& draw_image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override;

  // MemoryDumpProvider overrides.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Called by Decode / Upload tasks.
  void DecodeImage(const DrawImage& image);
  void UploadImage(const DrawImage& image);

  // Called by Decode / Upload tasks when tasks are finished.
  void OnImageDecodeTaskCompleted(const DrawImage& image);
  void OnImageUploadTaskCompleted(const DrawImage& image);

  // For testing only.
  void SetCachedItemLimitForTesting(size_t limit) {
    cached_items_limit_ = limit;
  }
  void SetCachedBytesLimitForTesting(size_t limit) {
    cached_bytes_limit_ = limit;
  }
  size_t GetBytesUsedForTesting() const { return bytes_used_; }
  size_t GetDrawImageSizeForTesting(const DrawImage& image);
  void SetImageDecodingFailedForTesting(const DrawImage& image);
  bool DiscardableIsLockedForTesting(const DrawImage& image);

 private:
  enum class DecodedDataMode { GPU, CPU };

  // Stores the CPU-side decoded bits of an image and supporting fields.
  struct DecodedImageData {
    DecodedImageData();
    ~DecodedImageData();

    bool is_locked() const { return is_locked_; }
    bool Lock();
    void Unlock();
    void SetLockedData(std::unique_ptr<base::DiscardableMemory> data);
    void ResetData();
    base::DiscardableMemory* data() const { return data_.get(); }
    void mark_used() { usage_stats_.used = true; }

    uint32_t ref_count = 0;
    // Set to true if the image was corrupt and could not be decoded.
    bool decode_failure = false;
    // If non-null, this is the pending decode task for this image.
    scoped_refptr<TileTask> task;

   private:
    struct UsageStats {
      int lock_count = 1;
      bool used = false;
      bool first_lock_wasted = false;
    };

    void ReportUsageStats() const;

    std::unique_ptr<base::DiscardableMemory> data_;
    bool is_locked_ = false;
    UsageStats usage_stats_;
  };

  // Stores the GPU-side image and supporting fields.
  struct UploadedImageData {
    UploadedImageData();
    ~UploadedImageData();

    void SetImage(sk_sp<SkImage> image);
    const sk_sp<SkImage>& image() const { return image_; }

    void mark_used() { usage_stats_.used = true; }
    void notify_ref_reached_zero() {
      if (++usage_stats_.ref_reached_zero_count == 1)
        usage_stats_.first_ref_wasted = !usage_stats_.used;
    }

    // True if the image is counting against our memory limits.
    bool budgeted = false;
    uint32_t ref_count = 0;
    // If non-null, this is the pending upload task for this image.
    scoped_refptr<TileTask> task;

   private:
    struct UsageStats {
      bool used = false;
      bool first_ref_wasted = false;
      int ref_reached_zero_count = 0;
    };

    void ReportUsageStats() const;

    // May be null if image not yet uploaded / prepared.
    sk_sp<SkImage> image_;
    UsageStats usage_stats_;
  };

  struct ImageData : public base::RefCounted<ImageData> {
    ImageData(DecodedDataMode mode,
              size_t size,
              int upload_scale_mip_level,
              SkFilterQuality upload_scale_filter_quality);

    const DecodedDataMode mode;
    const size_t size;
    bool is_at_raster = false;

    // Variables used to identify/track multiple scale levels of a single image.
    int upload_scale_mip_level = 0;
    SkFilterQuality upload_scale_filter_quality = kNone_SkFilterQuality;
    // If true, this image is no longer in our |persistent_cache_| and will be
    // deleted as soon as its ref count reaches zero.
    bool is_orphaned = false;

    DecodedImageData decode;
    UploadedImageData upload;

   private:
    friend class base::RefCounted<ImageData>;
    ~ImageData();
  };

  // A ref-count and ImageData, used to associate the ImageData with a specific
  // DrawImage in the |in_use_cache_|.
  struct InUseCacheEntry {
    explicit InUseCacheEntry(scoped_refptr<ImageData> image_data);
    InUseCacheEntry(const InUseCacheEntry& other);
    InUseCacheEntry(InUseCacheEntry&& other);
    ~InUseCacheEntry();

    uint32_t ref_count = 0;
    scoped_refptr<ImageData> image_data;
  };

  // Uniquely identifies (without collisions) a specific DrawImage for use in
  // the |in_use_cache_|.
  using InUseCacheKey = uint64_t;

  // All private functions should only be called while holding |lock_|. Some
  // functions also require the |context_| lock. These are indicated by
  // additional comments.

  // Similar to GetTaskForImageAndRef, but gets the dependent decode task
  // rather than the upload task, if necessary.
  scoped_refptr<TileTask> GetImageDecodeTaskAndRef(
      const DrawImage& image,
      const TracingInfo& tracing_info);

  void RefImageDecode(const DrawImage& draw_image);
  void UnrefImageDecode(const DrawImage& draw_image);
  void RefImage(const DrawImage& draw_image);
  void UnrefImageInternal(const DrawImage& draw_image);

  // Called any time the ownership of an object changed. This includes changes
  // to ref-count or to orphaned status.
  void OwnershipChanged(ImageData* image_data);

  // Ensures that the cache can hold an element of |required_size|, freeing
  // unreferenced cache entries if necessary to make room.
  bool EnsureCapacity(size_t required_size);
  bool CanFitSize(size_t size) const;
  bool ExceedsPreferredCount() const;

  void DecodeImageIfNecessary(const DrawImage& draw_image,
                              ImageData* image_data);

  scoped_refptr<GpuImageDecodeController::ImageData> CreateImageData(
      const DrawImage& image);
  SkImageInfo CreateImageInfoForDrawImage(const DrawImage& draw_image,
                                          int upload_scale_mip_level) const;

  // Finds the ImageData that should be used for the given DrawImage. Looks
  // first in the |in_use_cache_|, and then in the |persistent_cache_|.
  ImageData* GetImageDataForDrawImage(const DrawImage& image);

  // Returns true if the given ImageData can be used to draw the specified
  // DrawImage.
  bool IsCompatible(const ImageData* image_data,
                    const DrawImage& draw_image) const;

  // The following two functions also require the |context_| lock to be held.
  void UploadImageIfNecessary(const DrawImage& draw_image,
                              ImageData* image_data);
  void DeletePendingImages();

  const ResourceFormat format_;
  ContextProvider* context_;
  sk_sp<GrContextThreadSafeProxy> context_threadsafe_proxy_;

  // All members below this point must only be accessed while holding |lock_|.
  base::Lock lock_;

  // |persistent_cache_| represents the long-lived cache, keeping a certain
  // budget of ImageDatas alive even when their ref count reaches zero.
  using PersistentCache = base::MRUCache<uint32_t, scoped_refptr<ImageData>>;
  PersistentCache persistent_cache_;

  // |in_use_cache_| represents the in-use (short-lived) cache. Entries are
  // cleaned up as soon as their ref count reaches zero.
  using InUseCache = std::unordered_map<InUseCacheKey, InUseCacheEntry>;
  InUseCache in_use_cache_;

  size_t cached_items_limit_;
  size_t cached_bytes_limit_;
  size_t bytes_used_;
  const size_t max_gpu_image_bytes_;

  // We can't release GPU backed SkImages without holding the context lock,
  // so we add them to this list and defer deletion until the next time the lock
  // is held.
  std::vector<sk_sp<SkImage>> images_pending_deletion_;
};

}  // namespace cc

#endif  // CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_
