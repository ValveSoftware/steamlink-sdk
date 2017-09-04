// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidator_h
#define PaintInvalidator_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "wtf/Vector.h"

namespace blink {

class FrameView;
class LayoutBoxModelObject;
class LayoutObject;
class PaintLayer;
struct PaintPropertyTreeBuilderContext;

struct PaintInvalidatorContext {
  PaintInvalidatorContext(
      const PaintPropertyTreeBuilderContext& treeBuilderContext)
      : treeBuilderContext(treeBuilderContext) {}

  PaintInvalidatorContext(
      const PaintPropertyTreeBuilderContext& treeBuilderContext,
      const PaintInvalidatorContext& parentContext)
      : treeBuilderContext(treeBuilderContext),
        forcedSubtreeInvalidationFlags(
            parentContext.forcedSubtreeInvalidationFlags),
        paintInvalidationContainer(parentContext.paintInvalidationContainer),
        paintInvalidationContainerForStackedContents(
            parentContext.paintInvalidationContainerForStackedContents),
        paintingLayer(parentContext.paintingLayer) {}

  // This method is temporary to adapt PaintInvalidatorContext and the legacy
  // PaintInvalidationState for code shared by old code and new code.
  virtual void mapLocalRectToPaintInvalidationBacking(const LayoutObject&,
                                                      LayoutRect&) const;

  const PaintPropertyTreeBuilderContext& treeBuilderContext;

  enum ForcedSubtreeInvalidationFlag {
    ForcedSubtreeInvalidationChecking = 1 << 0,
    ForcedSubtreeInvalidationRectUpdate = 1 << 1,
    ForcedSubtreeFullInvalidation = 1 << 2,
    ForcedSubtreeFullInvalidationForStackedContents = 1 << 3,
    ForcedSubtreeSVGResourceChange = 1 << 4,
    // TODO(crbug.com/637313): This is temporary before we support filters in
    // paint property tree.
    ForcedSubtreeSlowPathRect = 1 << 5,
  };
  unsigned forcedSubtreeInvalidationFlags = 0;

  // The following fields can be null only before
  // PaintInvalidator::updateContext().

  // The current paint invalidation container for normal flow objects.
  // It is the enclosing composited object.
  const LayoutBoxModelObject* paintInvalidationContainer = nullptr;

  // The current paint invalidation container for stacked contents (stacking
  // contexts or positioned objects).  It is the nearest ancestor composited
  // object which establishes a stacking context.  See
  // Source/core/paint/README.md ### PaintInvalidationState for details on how
  // stacked contents' paint invalidation containers differ.
  const LayoutBoxModelObject* paintInvalidationContainerForStackedContents =
      nullptr;

  PaintLayer* paintingLayer = nullptr;

  // Store the old and new visual rects in the paint invalidation backing's
  // coordinates. The rects do *not* account for composited scrolling.
  // See LayoutObject::adjustVisualRectForCompositedScrolling().
  LayoutRect oldVisualRect;
  LayoutRect newVisualRect;

  // Store the origin of the object's local coordinates in the paint
  // invalidation backing's coordinates. They are used to detect layoutObject
  // shifts that force a full invalidation and invalidation check in subtree.
  // The points do *not* account for composited scrolling. See
  // LayoutObject::adjustVisualRectForCompositedScrolling().
  LayoutPoint oldLocation;
  LayoutPoint newLocation;

  // Stores the old and new offsets to paint this object, relative to the
  // containing transform node. They are for SPv2 only.
  LayoutPoint oldPaintOffset;
  LayoutPoint newPaintOffset;
};

class PaintInvalidator {
 public:
  void invalidatePaintIfNeeded(FrameView&, PaintInvalidatorContext&);
  void invalidatePaintIfNeeded(const LayoutObject&, PaintInvalidatorContext&);

  // Process objects needing paint invalidation on the next frame.
  // See the definition of PaintInvalidationDelayedFull for more details.
  void processPendingDelayedPaintInvalidations();

 private:
  LayoutRect mapLocalRectToPaintInvalidationBacking(
      const LayoutObject&,
      const FloatRect&,
      const PaintInvalidatorContext&);
  LayoutRect computeVisualRectInBacking(const LayoutObject&,
                                        const PaintInvalidatorContext&);
  LayoutPoint computeLocationInBacking(const LayoutObject&,
                                       const PaintInvalidatorContext&);
  void updatePaintingLayer(const LayoutObject&, PaintInvalidatorContext&);
  void updateContext(const LayoutObject&, PaintInvalidatorContext&);

  Vector<const LayoutObject*> m_pendingDelayedPaintInvalidations;
  GeometryMapper m_geometryMapper;
};

}  // namespace blink

#endif  // PaintInvalidator_h
