// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_ITEM_LIST_H_
#define CC_PLAYBACK_DISPLAY_ITEM_LIST_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/cc_export.h"
#include "cc/base/contiguous_container.h"
#include "cc/playback/discardable_image_map.h"
#include "cc/playback/display_item.h"
#include "cc/playback/display_item_list_settings.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;
class SkPictureRecorder;

namespace cc {
class ClientPictureCache;
class DisplayItem;
class DrawingDisplayItem;

namespace proto {
class DisplayItemList;
}

class CC_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  // Creates a display item list. If picture caching is used, then layer_rect
  // specifies the cull rect of the display item list (the picture will not
  // exceed this rect). If picture caching is not used, then the given rect can
  // be empty.
  // TODO(vmpstr): Maybe this cull rect can be part of the settings instead.
  static scoped_refptr<DisplayItemList> Create(
      const gfx::Rect& layer_rect,
      const DisplayItemListSettings& settings);

  // Creates a DisplayItemList from a Protobuf.
  // TODO(dtrainor): Pass in a list of possible DisplayItems to reuse
  // (crbug.com/548434).
  static scoped_refptr<DisplayItemList> CreateFromProto(
      const proto::DisplayItemList& proto,
      ClientPictureCache* client_picture_cache,
      std::vector<uint32_t>* used_engine_picture_ids);

  // Creates a Protobuf representing the state of this DisplayItemList.
  void ToProtobuf(proto::DisplayItemList* proto);

  // TODO(trchen): Deprecated. Apply clip and scale on the canvas instead.
  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback,
              const gfx::Rect& canvas_target_playback_rect,
              float contents_scale) const;

  void Raster(SkCanvas* canvas, SkPicture::AbortCallback* callback) const;

  // This is a fast path for use only if canvas_ is set and
  // retain_individual_display_items_ is false. This method also updates
  // approximate_op_count_.
  void RasterIntoCanvas(const DisplayItem& display_item);

  // Because processing happens in this function, all the set up for
  // this item should be done via the args, which is why the return
  // type needs to be const, to prevent set-after-processing mistakes.
  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendItem(const gfx::Rect& visual_rect,
                                             Args&&... args) {
    visual_rects_.push_back(visual_rect);
    auto* item = &items_.AllocateAndConstruct<DisplayItemType>(
        std::forward<Args>(args)...);
    approximate_op_count_ += item->ApproximateOpCount();
    ProcessAppendedItem(item);
    return *item;
  }

  // Called after all items are appended, to process the items and, if
  // applicable, create an internally cached SkPicture.
  void Finalize();

  void SetIsSuitableForGpuRasterization(bool is_suitable) {
    is_suitable_for_gpu_rasterization_ = is_suitable;
  }
  bool IsSuitableForGpuRasterization() const;
  int ApproximateOpCount() const;
  size_t ApproximateMemoryUsage() const;
  bool ShouldBeAnalyzedForSolidColor() const;

  bool RetainsIndividualDisplayItems() const;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      bool include_items) const;

  void EmitTraceSnapshot() const;

  void GenerateDiscardableImagesMetadata();
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  float raster_scale,
                                  std::vector<DrawImage>* images);

  gfx::Rect VisualRectForTesting(int index) { return visual_rects_[index]; }

  ContiguousContainer<DisplayItem>::const_iterator begin() const {
    return items_.begin();
  }

  ContiguousContainer<DisplayItem>::const_iterator end() const {
    return items_.end();
  }

 private:
  DisplayItemList(gfx::Rect layer_rect,
                  const DisplayItemListSettings& display_list_settings,
                  bool retain_individual_display_items);
  ~DisplayItemList();

  void ProcessAppendedItem(const DisplayItem* item);

  ContiguousContainer<DisplayItem> items_;
  // The visual rects associated with each of the display items in the
  // display item list. There is one rect per display item, and the
  // position in |visual_rects_| matches the position of the item in
  // |items_| . These rects are intentionally kept separate
  // because they are not needed while walking the |items_| for raster.
  std::vector<gfx::Rect> visual_rects_;
  sk_sp<SkPicture> picture_;

  std::unique_ptr<SkPictureRecorder> recorder_;
  const DisplayItemListSettings settings_;
  bool retain_individual_display_items_;

  gfx::Rect layer_rect_;
  bool is_suitable_for_gpu_rasterization_;
  int approximate_op_count_;

  // Memory usage due to the cached SkPicture.
  size_t picture_memory_usage_;

  DiscardableImageMap image_map_;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, ApproximateMemoryUsage);
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_ITEM_LIST_H_
