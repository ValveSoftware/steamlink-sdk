// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewPainter_h
#define ViewPainter_h

#include "wtf/Allocator.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class LayoutBox;
class LayoutView;

class ViewPainter {
    STACK_ALLOCATED();
public:
    ViewPainter(const LayoutView& layoutView) : m_layoutView(layoutView) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);
    void paintBoxDecorationBackground(const PaintInfo&);

private:
    const LayoutView& m_layoutView;
};

} // namespace blink

#endif // ViewPainter_h
