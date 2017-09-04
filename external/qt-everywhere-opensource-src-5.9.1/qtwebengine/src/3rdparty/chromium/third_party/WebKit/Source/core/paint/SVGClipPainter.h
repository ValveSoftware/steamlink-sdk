// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGClipPainter_h
#define SVGClipPainter_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "wtf/Allocator.h"

namespace blink {

class AffineTransform;
class GraphicsContext;
class LayoutObject;
class LayoutSVGResourceClipper;

enum class ClipperState;

class SVGClipPainter {
  STACK_ALLOCATED();

 public:
  SVGClipPainter(LayoutSVGResourceClipper& clip) : m_clip(clip) {}

  bool prepareEffect(const LayoutObject&,
                     const FloatRect&,
                     const FloatRect&,
                     const FloatPoint&,
                     GraphicsContext&,
                     ClipperState&);
  void finishEffect(const LayoutObject&, GraphicsContext&, ClipperState&);

 private:
  // Return false if there is a problem drawing the mask.
  bool drawClipAsMask(GraphicsContext&,
                      const LayoutObject&,
                      const FloatRect& targetBoundingBox,
                      const FloatRect& targetVisualRect,
                      const AffineTransform&,
                      const FloatPoint&);

  LayoutSVGResourceClipper& m_clip;
};

}  // namespace blink

#endif  // SVGClipPainter_h
