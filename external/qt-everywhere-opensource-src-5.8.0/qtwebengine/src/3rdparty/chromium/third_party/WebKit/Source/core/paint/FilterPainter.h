// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterPainter_h
#define FilterPainter_h

#include "core/paint/PaintLayerPaintingInfo.h"
#include "wtf/Allocator.h"
#include <memory>

namespace blink {

class ClipRect;
class GraphicsContext;
class PaintLayer;
class LayerClipRecorder;

class FilterPainter {
    STACK_ALLOCATED();
public:
    FilterPainter(PaintLayer&, GraphicsContext&, const LayoutPoint& offsetFromRoot, const ClipRect&, PaintLayerPaintingInfo&, PaintLayerFlags paintFlags, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed);
    ~FilterPainter();

private:
    bool m_filterInProgress;
    GraphicsContext& m_context;
    std::unique_ptr<LayerClipRecorder> m_clipRecorder;
    LayoutObject* m_layoutObject;
};

} // namespace blink

#endif // FilterPainter_h
