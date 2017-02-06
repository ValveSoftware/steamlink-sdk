// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrePaintTreeWalk_h
#define PrePaintTreeWalk_h

#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

class FrameView;
class LayoutObject;
struct PrePaintTreeWalkContext;

// This class walks the whole layout tree, beginning from the root FrameView, across
// frame boundaries. Helper classes are called for each tree node to perform actual actions.
// It expects to be invoked in InPrePaint phase.
class PrePaintTreeWalk {
public:
    void walk(FrameView& rootFrame);

private:
    void walk(FrameView&, const PrePaintTreeWalkContext&);
    void walk(const LayoutObject&, const PrePaintTreeWalkContext&);

    PaintPropertyTreeBuilder m_propertyTreeBuilder;
    PaintInvalidator m_paintInvalidator;
};

} // namespace blink

#endif // PrePaintTreeWalk_h
