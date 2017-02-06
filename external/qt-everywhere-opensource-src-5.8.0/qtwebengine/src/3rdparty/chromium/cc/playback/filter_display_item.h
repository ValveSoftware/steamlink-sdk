// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_
#define CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_

#include <stddef.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "cc/playback/display_item.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;

namespace cc {

class CC_EXPORT FilterDisplayItem : public DisplayItem {
 public:
  FilterDisplayItem(const FilterOperations& filters, const gfx::RectF& bounds);
  explicit FilterDisplayItem(const proto::DisplayItem& proto);
  ~FilterDisplayItem() override;

  void ToProtobuf(proto::DisplayItem* proto) const override;
  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const { return 1; }

 private:
  void SetNew(const FilterOperations& filters, const gfx::RectF& bounds);

  FilterOperations filters_;
  gfx::RectF bounds_;
};

class CC_EXPORT EndFilterDisplayItem : public DisplayItem {
 public:
  EndFilterDisplayItem();
  explicit EndFilterDisplayItem(const proto::DisplayItem& proto);
  ~EndFilterDisplayItem() override;

  static std::unique_ptr<EndFilterDisplayItem> Create() {
    return base::WrapUnique(new EndFilterDisplayItem());
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

#endif  // CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_
