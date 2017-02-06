// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_FLOAT_CLIP_DISPLAY_ITEM_H_
#define CC_PLAYBACK_FLOAT_CLIP_DISPLAY_ITEM_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/playback/display_item.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;

namespace cc {

class CC_EXPORT FloatClipDisplayItem : public DisplayItem {
 public:
  explicit FloatClipDisplayItem(const gfx::RectF& clip_rect);
  explicit FloatClipDisplayItem(const proto::DisplayItem& proto);
  ~FloatClipDisplayItem() override;

  void ToProtobuf(proto::DisplayItem* proto) const override;
  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const { return 1; }

 private:
  void SetNew(const gfx::RectF& clip_rect);

  gfx::RectF clip_rect_;
};

class CC_EXPORT EndFloatClipDisplayItem : public DisplayItem {
 public:
  EndFloatClipDisplayItem();
  explicit EndFloatClipDisplayItem(const proto::DisplayItem& proto);
  ~EndFloatClipDisplayItem() override;

  static std::unique_ptr<EndFloatClipDisplayItem> Create() {
    return base::WrapUnique(new EndFloatClipDisplayItem());
  }

  void ToProtobuf(proto::DisplayItem* proto) const override;
  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PLAYBACK_FLOAT_CLIP_DISPLAY_ITEM_H_
