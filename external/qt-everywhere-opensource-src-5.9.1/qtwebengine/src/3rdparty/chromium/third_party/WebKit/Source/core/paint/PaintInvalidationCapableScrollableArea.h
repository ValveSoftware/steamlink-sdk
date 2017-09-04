// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidationCapableScrollableArea_h
#define PaintInvalidationCapableScrollableArea_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class LayoutScrollbarPart;
class PaintInvalidationState;
struct PaintInvalidatorContext;

// Base class of FrameView and PaintLayerScrollableArea to share paint
// invalidation code.
// TODO(wangxianzhu): Combine this into PaintLayerScrollableArea when
// root-layer-scrolls launches.
class CORE_EXPORT PaintInvalidationCapableScrollableArea
    : public ScrollableArea {
 public:
  PaintInvalidationCapableScrollableArea()
      : m_horizontalScrollbarPreviouslyWasOverlay(false),
        m_verticalScrollbarPreviouslyWasOverlay(false) {}

  void willRemoveScrollbar(Scrollbar&, ScrollbarOrientation) override;

  void invalidatePaintOfScrollControlsIfNeeded(const PaintInvalidationState&);
  void invalidatePaintOfScrollControlsIfNeeded(const PaintInvalidatorContext&);

  // Should be called when the previous visual rects are no longer valid.
  void clearPreviousVisualRects();

  virtual IntRect scrollCornerAndResizerRect() const {
    return scrollCornerRect();
  }

  LayoutRect visualRectForScrollbarParts() const override;

 private:
  virtual LayoutScrollbarPart* scrollCorner() const = 0;
  virtual LayoutScrollbarPart* resizer() const = 0;

  void scrollControlWasSetNeedsPaintInvalidation() override;

  bool m_horizontalScrollbarPreviouslyWasOverlay;
  bool m_verticalScrollbarPreviouslyWasOverlay;
  LayoutRect m_horizontalScrollbarPreviousVisualRect;
  LayoutRect m_verticalScrollbarPreviousVisualRect;
  LayoutRect m_scrollCornerAndResizerPreviousVisualRect;
};

}  // namespace blink

#endif  // PaintInvalidationCapableScrollableArea_h
