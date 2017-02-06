// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_RECORDING_SOURCE_H_
#define CC_PLAYBACK_RECORDING_SOURCE_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/base/invalidation_region.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

namespace proto {
class RecordingSource;
}  // namespace proto

class ClientPictureCache;
class ContentLayerClient;
class DisplayItemList;
class RasterSource;
class Region;

class CC_EXPORT RecordingSource {
 public:
  // TODO(schenney) Remove RECORD_WITH_SK_NULL_CANVAS when we no longer
  // support a non-Slimming Paint path.
  enum RecordingMode {
    RECORD_NORMALLY,
    RECORD_WITH_SK_NULL_CANVAS,
    RECORD_WITH_PAINTING_DISABLED,
    RECORD_WITH_CACHING_DISABLED,
    RECORD_WITH_CONSTRUCTION_DISABLED,
    RECORD_WITH_SUBSEQUENCE_CACHING_DISABLED,
    RECORDING_MODE_COUNT,  // Must be the last entry.
  };

  RecordingSource();
  virtual ~RecordingSource();

  void ToProtobuf(proto::RecordingSource* proto) const;
  void FromProtobuf(const proto::RecordingSource& proto,
                    ClientPictureCache* client_picture_cache,
                    std::vector<uint32_t>* used_engine_picture_ids);

  bool UpdateAndExpandInvalidation(ContentLayerClient* painter,
                                   Region* invalidation,
                                   const gfx::Size& layer_size,
                                   int frame_number,
                                   RecordingMode recording_mode);
  gfx::Size GetSize() const;
  void SetEmptyBounds();
  void SetSlowdownRasterScaleFactor(int factor);
  void SetGenerateDiscardableImagesMetadata(bool generate_metadata);
  void SetBackgroundColor(SkColor background_color);
  void SetRequiresClear(bool requires_clear);

  void SetNeedsDisplayRect(const gfx::Rect& layer_rect);

  // These functions are virtual for testing.
  virtual scoped_refptr<RasterSource> CreateRasterSource(
      bool can_use_lcd_text) const;
  virtual bool IsSuitableForGpuRasterization() const;

  gfx::Rect recorded_viewport() const { return recorded_viewport_; }

  const DisplayItemList* GetDisplayItemList();

 protected:
  void Clear();

  gfx::Rect recorded_viewport_;
  gfx::Size size_;
  int slow_down_raster_scale_factor_for_debug_;
  bool generate_discardable_images_metadata_;
  bool requires_clear_;
  bool is_solid_color_;
  bool clear_canvas_with_debug_color_;
  SkColor solid_color_;
  SkColor background_color_;

  scoped_refptr<DisplayItemList> display_list_;
  size_t painter_reported_memory_usage_;

 private:
  void UpdateInvalidationForNewViewport(const gfx::Rect& old_recorded_viewport,
                                        const gfx::Rect& new_recorded_viewport,
                                        Region* invalidation);
  void FinishDisplayItemListUpdate();

  friend class RasterSource;

  void DetermineIfSolidColor();

  InvalidationRegion invalidation_;

  DISALLOW_COPY_AND_ASSIGN(RecordingSource);
};

}  // namespace cc

#endif  // CC_PLAYBACK_RECORDING_SOURCE_H_
