// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_CLIP_DISPLAY_ITEM_H_
#define CC_PLAYBACK_CLIP_DISPLAY_ITEM_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/playback/display_item.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;

namespace cc {

class CC_EXPORT ClipDisplayItem : public DisplayItem {
 public:
  ClipDisplayItem(const gfx::Rect& clip_rect,
                  const std::vector<SkRRect>& rounded_clip_rects,
                  bool antialias);
  explicit ClipDisplayItem(const proto::DisplayItem& proto);
  ~ClipDisplayItem() override;

  void ToProtobuf(proto::DisplayItem* proto) const override;
  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const { return 1; }

 private:
  void SetNew(const gfx::Rect& clip_rect,
              const std::vector<SkRRect>& rounded_clip_rects,
              bool antialias);

  gfx::Rect clip_rect_;
  std::vector<SkRRect> rounded_clip_rects_;
  bool antialias_;
};

class CC_EXPORT EndClipDisplayItem : public DisplayItem {
 public:
  EndClipDisplayItem();
  explicit EndClipDisplayItem(const proto::DisplayItem& proto);
  ~EndClipDisplayItem() override;

  void ToProtobuf(proto::DisplayItem* proto) const override;
  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PLAYBACK_CLIP_DISPLAY_ITEM_H_
