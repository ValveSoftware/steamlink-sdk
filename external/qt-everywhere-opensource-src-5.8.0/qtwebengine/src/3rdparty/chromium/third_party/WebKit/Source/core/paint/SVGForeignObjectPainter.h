// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGForeignObjectPainter_h
#define SVGForeignObjectPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGForeignObject;

class SVGForeignObjectPainter {
    STACK_ALLOCATED();
public:
    SVGForeignObjectPainter(const LayoutSVGForeignObject& layoutSVGForeignObject) : m_layoutSVGForeignObject(layoutSVGForeignObject) { }
    void paint(const PaintInfo&);

private:
    const LayoutSVGForeignObject& m_layoutSVGForeignObject;
};

} // namespace blink

#endif // SVGForeignObjectPainter_h
