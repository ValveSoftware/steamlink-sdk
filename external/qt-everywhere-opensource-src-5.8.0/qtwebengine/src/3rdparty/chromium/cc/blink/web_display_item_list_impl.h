// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
#define CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/playback/display_item_list.h"
#include "third_party/WebKit/public/platform/WebDisplayItemList.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/point_f.h"

class SkColorFilter;
class SkImageFilter;
class SkMatrix44;
class SkPath;
class SkPicture;
class SkRRect;

namespace blink {
struct WebFloatRect;
struct WebRect;
}

namespace cc {
class DisplayItemListSettings;
class FilterOperations;
}

namespace cc_blink {

class WebDisplayItemListImpl : public blink::WebDisplayItemList {
 public:
  CC_BLINK_EXPORT WebDisplayItemListImpl();
  CC_BLINK_EXPORT explicit WebDisplayItemListImpl(
      cc::DisplayItemList* display_list);
  ~WebDisplayItemListImpl() override;

  // blink::WebDisplayItemList implementation.
  void appendDrawingItem(const blink::WebRect&,
                         sk_sp<const SkPicture>) override;
  void appendClipItem(
      const blink::WebRect& visual_rect,
      const blink::WebRect& clip_rect,
      const blink::WebVector<SkRRect>& rounded_clip_rects) override;
  void appendEndClipItem(const blink::WebRect& visual_rect) override;
  void appendClipPathItem(const blink::WebRect& visual_rect,
                          const SkPath& clip_path,
                          SkRegion::Op clip_op,
                          bool antialias) override;
  void appendEndClipPathItem(const blink::WebRect& visual_rect) override;
  void appendFloatClipItem(const blink::WebRect& visual_rect,
                           const blink::WebFloatRect& clip_rect) override;
  void appendEndFloatClipItem(const blink::WebRect& visual_rect) override;
  void appendTransformItem(const blink::WebRect& visual_rect,
                           const SkMatrix44& matrix) override;
  void appendEndTransformItem(const blink::WebRect& visual_rect) override;
  void appendCompositingItem(const blink::WebRect& visual_rect,
                             float opacity,
                             SkXfermode::Mode,
                             SkRect* bounds,
                             SkColorFilter*) override;
  void appendEndCompositingItem(const blink::WebRect& visual_rect) override;
  void appendFilterItem(const blink::WebRect& visual_rect,
                        const cc::FilterOperations& filters,
                        const blink::WebFloatRect& bounds) override;
  void appendEndFilterItem(const blink::WebRect& visual_rect) override;
  void appendScrollItem(const blink::WebRect& visual_rect,
                        const blink::WebSize& scrollOffset,
                        ScrollContainerId) override;
  void appendEndScrollItem(const blink::WebRect& visual_rect) override;

  void setIsSuitableForGpuRasterization(bool isSuitable) override;

 private:
  scoped_refptr<cc::DisplayItemList> display_item_list_;

  DISALLOW_COPY_AND_ASSIGN(WebDisplayItemListImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
