// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintLayerPainter_h
#define PaintLayerPainter_h

#include "core/CoreExport.h"
#include "core/paint/PaintLayerFragment.h"
#include "core/paint/PaintLayerPaintingInfo.h"
#include "wtf/Allocator.h"

namespace blink {

class ClipRect;
class PaintLayer;
class GraphicsContext;
class LayoutPoint;

// This class is responsible for painting self-painting PaintLayer.
//
// See PainterLayer SELF-PAINTING LAYER section about what 'self-painting'
// means and how it impacts this class.
class CORE_EXPORT PaintLayerPainter {
    STACK_ALLOCATED();
public:
    enum FragmentPolicy { AllowMultipleFragments, ForceSingleFragment };

    // When adding new values, must update the number of bits of PaintLayer::m_previousPaintingResult.
    enum PaintResult {
        // The layer is fully painted. This includes cases that nothing needs painting
        // regardless of the paint rect.
        FullyPainted,
        // Some part of the layer is out of the paint rect and may be not fully painted.
        // The results cannot be cached because they may change when paint rect changes.
        MayBeClippedByPaintDirtyRect
    };

    PaintLayerPainter(PaintLayer& paintLayer) : m_paintLayer(paintLayer) { }

    // The paint() method paints the layers that intersect the damage rect from back to front.
    //  paint() assumes that the caller will clip to the bounds of damageRect if necessary.
    void paint(GraphicsContext&, const LayoutRect& damageRect, const GlobalPaintFlags = GlobalPaintNormalPhase, PaintLayerFlags = 0);
    // paintLayer() assumes that the caller will clip to the bounds of the painting dirty if necessary.
    PaintResult paintLayer(GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags);
    // paintLayerContents() assumes that the caller will clip to the bounds of the painting dirty rect if necessary.
    PaintResult paintLayerContents(GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);

    void paintOverlayScrollbars(GraphicsContext&, const LayoutRect& damageRect, const GlobalPaintFlags);

private:
    enum ClipState { HasNotClipped, HasClipped };

    PaintResult paintLayerContentsAndReflection(GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);
    PaintResult paintLayerWithTransform(GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags);
    PaintResult paintFragmentByApplyingTransform(GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& fragmentTranslation);

    PaintResult paintChildren(unsigned childrenToVisit, GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags);
    bool atLeastOneFragmentIntersectsDamageRect(PaintLayerFragments&, const PaintLayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& offsetFromRoot);
    void paintFragmentWithPhase(PaintPhase, const PaintLayerFragment&, GraphicsContext&, const ClipRect&, const PaintLayerPaintingInfo&, PaintLayerFlags, ClipState);
    void paintBackgroundForFragments(const PaintLayerFragments&, GraphicsContext&,
        const LayoutRect& transparencyPaintDirtyRect, const PaintLayerPaintingInfo&, PaintLayerFlags);
    void paintForegroundForFragments(const PaintLayerFragments&, GraphicsContext&,
        const LayoutRect& transparencyPaintDirtyRect, const PaintLayerPaintingInfo&, bool selectionOnly, PaintLayerFlags);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const PaintLayerFragments&, GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags, ClipState);
    void paintSelfOutlineForFragments(const PaintLayerFragments&, GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags);
    void paintOverflowControlsForFragments(const PaintLayerFragments&, GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags);
    void paintMaskForFragments(const PaintLayerFragments&, GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags);
    void paintChildClippingMaskForFragments(const PaintLayerFragments&, GraphicsContext&, const PaintLayerPaintingInfo&, PaintLayerFlags);

    static bool needsToClip(const PaintLayerPaintingInfo& localPaintingInfo, const ClipRect&);

    // Returns whether this layer should be painted during sofware painting (i.e., not via calls from CompositedLayerMapping to draw into composited
    // layers).
    bool shouldPaintLayerInSoftwareMode(const GlobalPaintFlags, PaintLayerFlags paintFlags);

    PaintLayer& m_paintLayer;
};

} // namespace blink

#endif // PaintLayerPainter_h
