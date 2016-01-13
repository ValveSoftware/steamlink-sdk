// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_H_
#define CC_RESOURCES_PICTURE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBBHFactory.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/record/SkRecording.h"
#include "ui/gfx/rect.h"

class SkPixelRef;

namespace base {
class Value;
}

namespace skia {
class AnalysisCanvas;
}

namespace cc {

class ContentLayerClient;

class CC_EXPORT Picture
    : public base::RefCountedThreadSafe<Picture> {
 public:
  typedef std::pair<int, int> PixelRefMapKey;
  typedef std::vector<SkPixelRef*> PixelRefs;
  typedef base::hash_map<PixelRefMapKey, PixelRefs> PixelRefMap;

  enum RecordingMode {
    RECORD_NORMALLY,
    RECORD_WITH_SK_NULL_CANVAS,
    RECORD_WITH_PAINTING_DISABLED,
    RECORD_WITH_SKRECORD,
    RECORDING_MODE_COUNT,  // Must be the last entry.
  };

  static scoped_refptr<Picture> Create(
      const gfx::Rect& layer_rect,
      ContentLayerClient* client,
      const SkTileGridFactory::TileGridInfo& tile_grid_info,
      bool gather_pixels_refs,
      int num_raster_threads,
      RecordingMode recording_mode);
  static scoped_refptr<Picture> CreateFromValue(const base::Value* value);
  static scoped_refptr<Picture> CreateFromSkpValue(const base::Value* value);

  gfx::Rect LayerRect() const { return layer_rect_; }
  gfx::Rect OpaqueRect() const { return opaque_rect_; }

  // Get thread-safe clone for rasterizing with on a specific thread.
  Picture* GetCloneForDrawingOnThread(unsigned thread_index);

  // Has Record() been called yet?
  bool HasRecording() const { return picture_.get() != NULL; }

  bool IsSuitableForGpuRasterization() const;

  // Apply this scale and raster the negated region into the canvas.
  // |negated_content_region| specifies the region to be clipped out of the
  // raster operation, i.e., the parts of the canvas which will not get drawn
  // to.
  int Raster(SkCanvas* canvas,
             SkDrawPictureCallback* callback,
             const Region& negated_content_region,
             float contents_scale);

  // Draw the picture directly into the given canvas, without applying any
  // clip/scale/layer transformations.
  void Replay(SkCanvas* canvas);

  scoped_ptr<base::Value> AsValue() const;

  // This iterator imprecisely returns the set of pixel refs that are needed to
  // raster this layer rect from this picture.  Internally, pixel refs are
  // clumped into tile grid buckets, so there may be false positives.
  class CC_EXPORT PixelRefIterator {
   public:
    PixelRefIterator();
    PixelRefIterator(const gfx::Rect& layer_rect, const Picture* picture);
    ~PixelRefIterator();

    SkPixelRef* operator->() const {
      DCHECK_LT(current_index_, current_pixel_refs_->size());
      return (*current_pixel_refs_)[current_index_];
    }

    SkPixelRef* operator*() const {
      DCHECK_LT(current_index_, current_pixel_refs_->size());
      return (*current_pixel_refs_)[current_index_];
    }

    PixelRefIterator& operator++();
    operator bool() const {
      return current_index_ < current_pixel_refs_->size();
    }

   private:
    static base::LazyInstance<PixelRefs> empty_pixel_refs_;
    const Picture* picture_;
    const PixelRefs* current_pixel_refs_;
    unsigned current_index_;

    gfx::Point min_point_;
    gfx::Point max_point_;
    int current_x_;
    int current_y_;
  };

  void EmitTraceSnapshot() const;
  void EmitTraceSnapshotAlias(Picture* original) const;

  bool WillPlayBackBitmaps() const { return picture_->willPlayBackBitmaps(); }

 private:
  explicit Picture(const gfx::Rect& layer_rect);
  // This constructor assumes SkPicture is already ref'd and transfers
  // ownership to this picture.
  Picture(const skia::RefPtr<SkPicture>&,
          const gfx::Rect& layer_rect,
          const gfx::Rect& opaque_rect,
          const PixelRefMap& pixel_refs);
  // This constructor will call AdoptRef on the SkPicture.
  Picture(SkPicture*,
          const gfx::Rect& layer_rect,
          const gfx::Rect& opaque_rect);
  ~Picture();

  // Make thread-safe clones for rasterizing with.
  void CloneForDrawing(int num_threads);

  // Record a paint operation. To be able to safely use this SkPicture for
  // playback on a different thread this can only be called once.
  void Record(ContentLayerClient* client,
              const SkTileGridFactory::TileGridInfo& tile_grid_info,
              RecordingMode recording_mode);

  // Gather pixel refs from recording.
  void GatherPixelRefs(const SkTileGridFactory::TileGridInfo& tile_grid_info);

  gfx::Rect layer_rect_;
  gfx::Rect opaque_rect_;
  skia::RefPtr<SkPicture> picture_;
  scoped_ptr<const EXPERIMENTAL::SkPlayback> playback_;

  typedef std::vector<scoped_refptr<Picture> > PictureVector;
  PictureVector clones_;

  PixelRefMap pixel_refs_;
  gfx::Point min_pixel_cell_;
  gfx::Point max_pixel_cell_;
  gfx::Size cell_size_;

  scoped_refptr<base::debug::ConvertableToTraceFormat>
    AsTraceableRasterData(float scale) const;
  scoped_refptr<base::debug::ConvertableToTraceFormat>
    AsTraceableRecordData() const;

  base::ThreadChecker raster_thread_checker_;

  friend class base::RefCountedThreadSafe<Picture>;
  friend class PixelRefIterator;
  DISALLOW_COPY_AND_ASSIGN(Picture);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_H_
