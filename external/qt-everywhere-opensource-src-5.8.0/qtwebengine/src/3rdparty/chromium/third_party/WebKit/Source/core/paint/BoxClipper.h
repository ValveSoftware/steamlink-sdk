// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxClipper_h
#define BoxClipper_h

#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "wtf/Allocator.h"
#include "wtf/Optional.h"

namespace blink {

class LayoutBox;
struct PaintInfo;

enum ContentsClipBehavior { ForceContentsClip, SkipContentsClipIfPossible };

class BoxClipper {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    BoxClipper(const LayoutBox&, const PaintInfo&, const LayoutPoint& accumulatedOffset, ContentsClipBehavior);
    ~BoxClipper();
private:
    const LayoutBox& m_box;
    const PaintInfo& m_paintInfo;
    DisplayItem::Type m_clipType;

    Optional<ScopedPaintChunkProperties> m_scopedClipProperty;
};

} // namespace blink

#endif // BoxClipper_h
