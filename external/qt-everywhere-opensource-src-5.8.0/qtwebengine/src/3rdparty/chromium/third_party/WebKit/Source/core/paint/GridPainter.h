// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridPainter_h
#define GridPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutBox;
class LayoutGrid;

class GridPainter {
    STACK_ALLOCATED();
public:
    GridPainter(const LayoutGrid& layoutGrid) : m_layoutGrid(layoutGrid) { }

    void paintChildren(const PaintInfo&, const LayoutPoint&);

private:
    const LayoutGrid& m_layoutGrid;
};

} // namespace blink

#endif // GridPainter_h
