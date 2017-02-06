// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_SOFTWARE_IMAGE_DECODE_CONTROLLER_H_
#define CC_TILES_SOFTWARE_IMAGE_DECODE_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/atomic_sequence_num.h"
#include "base/containers/mru_cache.h"
#include "base/hash.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_math.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/base/cc_export.h"
#include "cc/playback/decoded_draw_image.h"
#include "cc/playback/draw_image.h"
#include "cc/resources/resource_format.h"
#include "cc/tiles/image_decode_controller.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

// ImageDecodeControllerKey is a class that gets a cache key out of a given draw
// image. That is, this key uniquely identifies an image in the cache. Note that
// it's insufficient to use SkImage's unique id, since the same image can appear
// in the cache multiple times at different scales and filter qualities.
class CC_EXPORT ImageDecodeControllerKey {
 public:
  static ImageDecodeControllerKey FromDrawImage(const DrawImage& image);

  ImageDecodeControllerKey(const ImageDecodeControllerKey& other);

  bool operator==(const ImageDecodeControllerKey& other) const {
    // The image_id always has to be the same. However, after that all original
    // decodes are the same, so if we can use the original decode, return true.
    // If not, then we have to compare every field.
    return image_id_ == other.image_id_ &&
           can_use_original_decode_ == other.can_use_original_decode_ &&
           (can_use_original_decode_ ||
            (src_rect_ == other.src_rect_ &&
             target_size_ == other.target_size_ &&
             filter_quality_ == other.filter_quality_));
  }

  bool operator!=(const ImageDecodeControllerKey& other) const {
    return !(*this == other);
  }

  uint32_t image_id() const { return image_id_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  gfx::Rect src_rect() const { return src_rect_; }
  gfx::Size target_size() const { return target_size_; }

  bool can_use_original_decode() const { return can_use_original_decode_; }
  size_t get_hash() const { return hash_; }

  // Helper to figure out how much memory the locked image represented by this
  // key would take.
  size_t locked_bytes() const {
    // TODO(vmpstr): Handle formats other than RGBA.
    base::CheckedNumeric<size_t> result = 4;
    result *= target_size_.width();
    result *= target_size_.height();
    return result.ValueOrDefault(std::numeric_limits<size_t>::max());
  }

  std::string ToString() const;

 private:
  ImageDecodeControllerKey(uint32_t image_id,
                           const gfx::Rect& src_rect,
                           const gfx::Size& size,
                           SkFilterQuality filter_quality,
                           bool can_use_original_decode);

  uint32_t image_id_;
  gfx::Rect src_rect_;
  gfx::Size target_size_;
  SkFilterQuality filter_quality_;
  bool can_use_original_decode_;
  size_t hash_;
};

// Hash function for the above ImageDecodeControllerKey.
struct ImageDecodeControllerKeyHash {
  size_t operator()(const ImageDecodeControllerKey& key) const {
    return key.get_hash();
  }
};

class CC_EXPORT SoftwareImageDecodeController
    : public ImageDecodeController,
      public base::trace_event::MemoryDumpProvider {
 public:
  using ImageKey = ImageDecodeControllerKey;
  using ImageKeyHash = ImageDecodeControllerKeyHash;

  SoftwareImageDecodeController(ResourceFormat format,
                                size_t locked_memory_limit_bytes);
  ~SoftwareImageDecodeController() override;

  // ImageDecodeController overrides.
  bool GetTaskForImageAndRef(const DrawImage& image,
                             const TracingInfo& tracing_info,
                             scoped_refptr<TileTask>* task) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  // Software doesn't keep outstanding images pinned, so this is a no-op.
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override {}

  // Decode the given image and store it in the cache. This is only called by an
  // image decode task from a worker thread.
  void DecodeImage(const ImageKey& key, const DrawImage& image);

  void RemovePendingTask(const ImageKey& key);

  // MemoryDumpProvider overrides.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // DecodedImage is a convenience storage for discardable memory. It can also
  // construct an image out of SkImageInfo and stored discardable memory.
  class DecodedImage {
   public:
    DecodedImage(const SkImageInfo& info,
                 std::unique_ptr<base::DiscardableMemory> memory,
                 const SkSize& src_rect_offset,
                 uint64_t tracing_id);
    ~DecodedImage();

    const sk_sp<SkImage>& image() const {
      DCHECK(locked_);
      return image_;
    }

    const SkSize& src_rect_offset() const { return src_rect_offset_; }

    bool is_locked() const { return locked_; }
    bool Lock();
    void Unlock();

    const base::DiscardableMemory* memory() const { return memory_.get(); }

    // An ID which uniquely identifies this DecodedImage within the image decode
    // controller. Used in memory tracing.
    uint64_t tracing_id() const { return tracing_id_; }
    // Mark this image as being used in either a draw or as a source for a
    // scaled image. Either case represents this decode as being valuable and
    // not wasted.
    void mark_used() { usage_stats_.used = true; }

   private:
    struct UsageStats {
      // We can only create a decoded image in a locked state, so the initial
      // lock count is 1.
      int lock_count = 1;
      bool used = false;
      bool last_lock_failed = false;
      bool first_lock_wasted = false;
    };

    bool locked_;
    SkImageInfo image_info_;
    std::unique_ptr<base::DiscardableMemory> memory_;
    sk_sp<SkImage> image_;
    SkSize src_rect_offset_;
    uint64_t tracing_id_;
    UsageStats usage_stats_;
  };

  // MemoryBudget is a convenience class for memory bookkeeping and ensuring
  // that we don't go over the limit when pre-decoding.
  class MemoryBudget {
   public:
    explicit MemoryBudget(size_t limit_bytes);

    size_t AvailableMemoryBytes() const;
    void AddUsage(size_t usage);
    void SubtractUsage(size_t usage);
    void ResetUsage();
    size_t total_limit_bytes() const { return limit_bytes_; }

   private:
    size_t GetCurrentUsageSafe() const;

    size_t limit_bytes_;
    base::CheckedNumeric<size_t> current_usage_bytes_;
  };

  using ImageMRUCache = base::HashingMRUCache<ImageKey,
                                              std::unique_ptr<DecodedImage>,
                                              ImageKeyHash>;

  // Looks for the key in the cache and returns true if it was found and was
  // successfully locked (or if it was already locked). Note that if this
  // function returns true, then a ref count is increased for the image.
  bool LockDecodedImageIfPossibleAndRef(const ImageKey& key);

  // Actually decode the image. Note that this function can (and should) be
  // called with no lock acquired, since it can do a lot of work. Note that it
  // can also return nullptr to indicate the decode failed.
  std::unique_ptr<DecodedImage> DecodeImageInternal(
      const ImageKey& key,
      const DrawImage& draw_image);

  // Get the decoded draw image for the given key and draw_image. Note that this
  // function has to be called with no lock acquired, since it will acquire its
  // own locks and might call DecodeImageInternal above. Also note that this
  // function will use the provided key, even if
  // ImageKey::FromDrawImage(draw_image) would return a different key.
  // Note that when used internally, we still require that
  // DrawWithImageFinished() is called afterwards.
  DecodedDrawImage GetDecodedImageForDrawInternal(const ImageKey& key,
                                                  const DrawImage& draw_image);

  // GetOriginalImageDecode is called by DecodeImageInternal when the quality
  // does not scale the image. Like DecodeImageInternal, it should be called
  // with no lock acquired and it returns nullptr if the decoding failed.
  std::unique_ptr<DecodedImage> GetOriginalImageDecode(
      const ImageKey& key,
      sk_sp<const SkImage> image);

  // GetScaledImageDecode is called by DecodeImageInternal when the quality
  // requires the image be scaled. Like DecodeImageInternal, it should be
  // called with no lock acquired and it returns nullptr if the decoding or
  // scaling failed.
  std::unique_ptr<DecodedImage> GetScaledImageDecode(
      const ImageKey& key,
      sk_sp<const SkImage> image);

  void SanityCheckState(int line, bool lock_acquired);
  void RefImage(const ImageKey& key);
  void RefAtRasterImage(const ImageKey& key);
  void UnrefAtRasterImage(const ImageKey& key);

  // Helper function which dumps all images in a specific ImageMRUCache.
  void DumpImageMemoryForCache(const ImageMRUCache& cache,
                               const char* cache_name,
                               base::trace_event::ProcessMemoryDump* pmd) const;

  std::unordered_map<ImageKey, scoped_refptr<TileTask>, ImageKeyHash>
      pending_image_tasks_;

  // The members below this comment can only be accessed if the lock is held to
  // ensure that they are safe to access on multiple threads.
  base::Lock lock_;

  // Decoded images and ref counts (predecode path).
  ImageMRUCache decoded_images_;
  std::unordered_map<ImageKey, int, ImageKeyHash> decoded_images_ref_counts_;

  // Decoded image and ref counts (at-raster decode path).
  ImageMRUCache at_raster_decoded_images_;
  std::unordered_map<ImageKey, int, ImageKeyHash>
      at_raster_decoded_images_ref_counts_;

  MemoryBudget locked_images_budget_;

  ResourceFormat format_;

  // Used to uniquely identify DecodedImages for memory traces.
  base::AtomicSequenceNumber next_tracing_id_;
};

}  // namespace cc

#endif  // CC_TILES_SOFTWARE_IMAGE_DECODE_CONTROLLER_H_
