// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReplacedPainter_h
#define ReplacedPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutReplaced;

class ReplacedPainter {
    STACK_ALLOCATED();
public:
    ReplacedPainter(const LayoutReplaced& layoutReplaced) : m_layoutReplaced(layoutReplaced) { }

    void paint(const PaintInfo&, const LayoutPoint&);

    // The adjustedPaintOffset should include the location (offset) of the object itself.
    bool shouldPaint(const PaintInfo&, const LayoutPoint& adjustedPaintOffset) const;

private:
    const LayoutReplaced& m_layoutReplaced;
};

} // namespace blink

#endif // ReplacedPainter_h
