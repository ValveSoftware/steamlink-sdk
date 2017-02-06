// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlinePainter_h
#define InlinePainter_h

#include "core/style/ComputedStyleConstants.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class Color;
class GraphicsContext;
class LayoutPoint;
class LayoutRect;
struct PaintInfo;
class LayoutInline;

class InlinePainter {
    STACK_ALLOCATED();
public:
    InlinePainter(const LayoutInline& layoutInline) : m_layoutInline(layoutInline) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);

private:
    const LayoutInline& m_layoutInline;
};

} // namespace blink

#endif // InlinePainter_h
