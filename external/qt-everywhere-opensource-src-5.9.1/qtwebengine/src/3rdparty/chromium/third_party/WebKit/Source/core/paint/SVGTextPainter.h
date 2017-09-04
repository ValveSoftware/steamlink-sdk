// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTextPainter_h
#define SVGTextPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGText;

class SVGTextPainter {
  STACK_ALLOCATED();

 public:
  SVGTextPainter(const LayoutSVGText& layoutSVGText)
      : m_layoutSVGText(layoutSVGText) {}
  void paint(const PaintInfo&);

 private:
  const LayoutSVGText& m_layoutSVGText;
};

}  // namespace blink

#endif  // SVGTextPainter_h
