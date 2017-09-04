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
#include "cc/base/rtree.h"
#include "cc/playback/discardable_image_map.h"
#include "cc/playback/display_item.h"
#include "cc/playback/display_item_list_settings.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

class SkCanvas;

namespace cc {
class ClientPictureCache;
class DisplayItem;

namespace proto {
class DisplayItemList;
}

class CC_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  // Creates a display item list.
  static scoped_refptr<DisplayItemList> Create(
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


  // Because processing happens in these CreateAndAppend functions, all the set
  // up for the item should be done via the args, which is why the return type
  // needs to be const, to prevent set-after-processing mistakes.

  // Most paired begin item types default to an empty visual rect, which will
  // subsequently be grown as needed to encompass any contained items that draw
  // content, such as drawing or filter items.
  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendPairedBeginItem(Args&&... args) {
    return CreateAndAppendPairedBeginItemWithVisualRect<DisplayItemType>(
        gfx::Rect(), std::forward<Args>(args)...);
  }

  // This method variant is exposed to allow filters to specify their visual
  // rect since they may draw content despite containing no drawing items.
  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendPairedBeginItemWithVisualRect(
      const gfx::Rect& visual_rect,
      Args&&... args) {
    size_t item_index = inputs_.visual_rects.size();
    inputs_.visual_rects.push_back(visual_rect);
    inputs_.begin_item_indices.push_back(item_index);

    return AllocateAndConstruct<DisplayItemType>(std::forward<Args>(args)...);
  }

  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendPairedEndItem(Args&&... args) {
    DCHECK(!inputs_.begin_item_indices.empty());
    size_t last_begin_index = inputs_.begin_item_indices.back();
    inputs_.begin_item_indices.pop_back();

    // Note that we are doing two separate things below:
    //
    // 1. Appending a new rect to the |visual_rects| list associated with
    //    the newly-being-added paired end item, with that visual rect
    //    having same bounds as its paired begin item, referenced via
    //    |last_begin_index|. The paired begin item may or may not be the
    //    current last visual rect in |visual_rects|, and its bounds has
    //    potentially been grown via calls to CreateAndAppendDrawingItem().
    //
    // 2. If there is still a containing paired begin item after closing the
    //    pair ended in this method call, growing that item's visual rect to
    //    incorporate the bounds of the now-finished pair.
    //
    // Thus we're carefully pushing and growing by the visual rect of the
    // paired begin item we're closing in this method call, which is not
    // necessarily the same as |visual_rects.back()|, and given that the
    // |visual_rects| list is mutated in step 1 before step 2, we also can't
    // shorten the reference via a |const auto| reference. We could make a
    // copy of the rect before list mutation, but that would incur copy
    // overhead.

    // Ending bounds match the starting bounds.
    inputs_.visual_rects.push_back(inputs_.visual_rects[last_begin_index]);

    // The block that ended needs to be included in the bounds of the enclosing
    // block.
    GrowCurrentBeginItemVisualRect(inputs_.visual_rects[last_begin_index]);

    return AllocateAndConstruct<DisplayItemType>(std::forward<Args>(args)...);
  }

  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& CreateAndAppendDrawingItem(
      const gfx::Rect& visual_rect,
      Args&&... args) {
    inputs_.visual_rects.push_back(visual_rect);
    GrowCurrentBeginItemVisualRect(visual_rect);

    return AllocateAndConstruct<DisplayItemType>(std::forward<Args>(args)...);
  }

  // Called after all items are appended, to process the items and, if
  // applicable, create an internally cached SkPicture.
  void Finalize();

  void SetIsSuitableForGpuRasterization(bool is_suitable) {
    inputs_.all_items_are_suitable_for_gpu_rasterization = is_suitable;
  }
  bool IsSuitableForGpuRasterization() const;
  int ApproximateOpCount() const;
  size_t ApproximateMemoryUsage() const;
  bool ShouldBeAnalyzedForSolidColor() const;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      bool include_items) const;

  void EmitTraceSnapshot() const;

  void GenerateDiscardableImagesMetadata();
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  const gfx::SizeF& raster_scales,
                                  std::vector<DrawImage>* images);

  void SetRetainVisualRectsForTesting(bool retain) {
    retain_visual_rects_ = retain;
  }

  size_t size() const { return inputs_.items.size(); }

  gfx::Rect VisualRectForTesting(int index) {
    return inputs_.visual_rects[index];
  }

  ContiguousContainer<DisplayItem>::const_iterator begin() const {
    return inputs_.items.begin();
  }

  ContiguousContainer<DisplayItem>::const_iterator end() const {
    return inputs_.items.end();
  }

 private:
  explicit DisplayItemList(
      const DisplayItemListSettings& display_list_settings);
  ~DisplayItemList();

  RTree rtree_;
  // For testing purposes only. Whether to keep visual rects across calls to
  // Finalize().
  bool retain_visual_rects_ = false;

  // If we're currently within a paired display item block, unions the
  // given visual rect with the begin display item's visual rect.
  void GrowCurrentBeginItemVisualRect(const gfx::Rect& visual_rect);

  template <typename DisplayItemType, typename... Args>
  const DisplayItemType& AllocateAndConstruct(Args&&... args) {
    auto* item = &inputs_.items.AllocateAndConstruct<DisplayItemType>(
        std::forward<Args>(args)...);
    approximate_op_count_ += item->ApproximateOpCount();
    return *item;
  }

  int approximate_op_count_ = 0;

  DiscardableImageMap image_map_;

  struct Inputs {
    explicit Inputs(const DisplayItemListSettings& settings);
    ~Inputs();

    ContiguousContainer<DisplayItem> items;
    // The visual rects associated with each of the display items in the
    // display item list. There is one rect per display item, and the
    // position in |visual_rects| matches the position of the item in
    // |items| . These rects are intentionally kept separate
    // because they are not needed while walking the |items| for raster.
    std::vector<gfx::Rect> visual_rects;
    std::vector<size_t> begin_item_indices;
    const DisplayItemListSettings settings;
    bool all_items_are_suitable_for_gpu_rasterization = true;
  };

  Inputs inputs_;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, ApproximateMemoryUsage);
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_ITEM_LIST_H_
