// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDisplayItemList_h
#define WebDisplayItemList_h

#include "WebBlendMode.h"
#include "WebFloatPoint.h"
#include "WebFloatRect.h"
#include "WebRect.h"
#include "WebSize.h"
#include "WebVector.h"

#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkXfermode.h"

class SkColorFilter;
class SkImageFilter;
class SkMatrix44;
class SkPicture;
struct SkRect;
class SkRRect;

namespace cc {
class FilterOperations;
}

namespace blink {

// An ordered list of items representing content to be rendered (stored in
// 'drawing' items) and operations to be performed when rendering this content
// (stored in 'clip', 'transform', 'filter', etc...). For more details see:
// http://dev.chromium.org/blink/slimming-paint.
class WebDisplayItemList {
 public:
  virtual ~WebDisplayItemList() {}

  virtual void appendDrawingItem(const WebRect& visualRect,
                                 sk_sp<const SkPicture>) {}

  virtual void appendClipItem(const WebRect& clipRect,
                              const WebVector<SkRRect>& roundedClipRects) {}
  virtual void appendEndClipItem() {}
  virtual void appendClipPathItem(const SkPath&, SkRegion::Op, bool antialias) {
  }
  virtual void appendEndClipPathItem() {}
  virtual void appendFloatClipItem(const WebFloatRect& clipRect) {}
  virtual void appendEndFloatClipItem() {}
  virtual void appendTransformItem(const SkMatrix44&) {}
  virtual void appendEndTransformItem() {}
  virtual void appendCompositingItem(float opacity,
                                     SkXfermode::Mode,
                                     SkRect* bounds,
                                     SkColorFilter*) {}
  virtual void appendEndCompositingItem() {}

  // TODO(loyso): This should use CompositorFilterOperation. crbug.com/584551
  virtual void appendFilterItem(const cc::FilterOperations&,
                                const WebFloatRect& filter_bounds,
                                const WebFloatPoint& origin) {}
  virtual void appendEndFilterItem() {}

  // Scroll containers are identified by an opaque pointer.
  using ScrollContainerId = const void*;
  virtual void appendScrollItem(const WebSize& scrollOffset,
                                ScrollContainerId) {}
  virtual void appendEndScrollItem() {}

  virtual void setIsSuitableForGpuRasterization(bool isSuitable) {}
};

}  // namespace blink

#endif  // WebDisplayItemList_h
