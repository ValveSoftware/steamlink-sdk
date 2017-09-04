// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImagePainter_h
#define ImagePainter_h

#include "wtf/Allocator.h"

namespace blink {

class GraphicsContext;
struct PaintInfo;
class LayoutPoint;
class LayoutRect;
class LayoutImage;

class ImagePainter {
  STACK_ALLOCATED();

 public:
  ImagePainter(const LayoutImage& layoutImage) : m_layoutImage(layoutImage) {}

  void paint(const PaintInfo&, const LayoutPoint& paintOffset);
  void paintReplaced(const PaintInfo&, const LayoutPoint& paintOffset);

  // Paint the image into |destRect|, after clipping by |contentRect|. Both
  // |destRect| and |contentRect| should be in local coordinates plus the paint
  // offset.
  void paintIntoRect(GraphicsContext&,
                     const LayoutRect& destRect,
                     const LayoutRect& contentRect);

 private:
  void paintAreaElementFocusRing(const PaintInfo&,
                                 const LayoutPoint& paintOffset);

  const LayoutImage& m_layoutImage;
};

}  // namespace blink

#endif  // ImagePainter_h
