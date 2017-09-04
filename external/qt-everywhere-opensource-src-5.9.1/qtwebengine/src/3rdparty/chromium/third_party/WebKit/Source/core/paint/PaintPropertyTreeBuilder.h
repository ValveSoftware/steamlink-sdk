// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreeBuilder_h
#define PaintPropertyTreeBuilder_h

#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/RefPtr.h"

namespace blink {

class FrameView;
class LayoutObject;

// The context for PaintPropertyTreeBuilder.
// It's responsible for bookkeeping tree state in other order, for example, the
// most recent position container seen.
struct PaintPropertyTreeBuilderContext {
  // State that propagates on the containing block chain (and so is adjusted
  // when an absolute or fixed position object is encountered).
  struct ContainingBlockContext {
    // The combination of a transform and paint offset describes a linear space.
    // When a layout object recur to its children, the main context is expected
    // to refer the object's border box, then the callee will derive its own
    // border box by translating the space with its own layout location.
    const TransformPaintPropertyNode* transform = nullptr;
    LayoutPoint paintOffset;
    // Whether newly created children should flatten their inherited transform
    // (equivalently, draw into the plane of their parent). Should generally
    // be updated whenever |transform| is; flattening only needs to happen
    // to immediate children.
    bool shouldFlattenInheritedTransform = false;
    // Rendering context for 3D sorting. See
    // TransformPaintPropertyNode::renderingContextID.
    unsigned renderingContextID = 0;
    // The clip node describes the accumulated raster clip for the current
    // subtree.  Note that the computed raster region in canvas space for a clip
    // node is independent from the transform and paint offset above. Also the
    // actual raster region may be affected by layerization and occlusion
    // tracking.
    const ClipPaintPropertyNode* clip = nullptr;
    // The scroll node contains information for scrolling such as the parent
    // scroll space, the extent that can be scrolled, etc. Because scroll nodes
    // reference a scroll offset transform, scroll nodes should be updated if
    // the transform tree changes.
    ScrollPaintPropertyNode* scroll = nullptr;
  };

  ContainingBlockContext current;

  // Separate context for out-of-flow positioned and fixed positioned elements
  // are needed because they don't use DOM parent as their containing block.
  // These additional contexts normally pass through untouched, and are only
  // copied from the main context when the current element serves as the
  // containing block of corresponding positioned descendants.  Overflow clips
  // are also inherited by containing block tree instead of DOM tree, thus they
  // are included in the additional context too.
  ContainingBlockContext absolutePosition;
  const LayoutObject* containerForAbsolutePosition = nullptr;

  ContainingBlockContext fixedPosition;

  // The effect hierarchy is applied by the stacking context tree. It is
  // guaranteed that every DOM descendant is also a stacking context descendant.
  // Therefore, we don't need extra bookkeeping for effect nodes and can
  // generate the effect tree from a DOM-order traversal.
  const EffectPaintPropertyNode* currentEffect = nullptr;

  bool isUnderMultiColumnSpanner = false;
};

// Creates paint property tree nodes for special things in the layout tree.
// Special things include but not limit to: overflow clip, transform, fixed-pos,
// animation, mask, filter, ... etc.
// It expects to be invoked for each layout tree node in DOM order during
// InPrePaint phase.
class PaintPropertyTreeBuilder {
 public:
  PaintPropertyTreeBuilderContext setupInitialContext();
  // Update the paint properties for a frame and ensure the context is up to
  // date.
  void updateProperties(FrameView&, PaintPropertyTreeBuilderContext&);

  // Update the paint properties that affect this object (e.g., properties like
  // paint offset translation) and ensure the context is up to date.
  void updatePropertiesForSelf(const LayoutObject&,
                               PaintPropertyTreeBuilderContext&);
  // Update the paint properties that affect children of this object (e.g.,
  // scroll offset transform) and ensure the context is up to date.
  void updatePropertiesForChildren(const LayoutObject&,
                                   PaintPropertyTreeBuilderContext&);

 private:
  static void updatePaintOffsetTranslation(const LayoutObject&,
                                           PaintPropertyTreeBuilderContext&);
  static void updateTransform(const LayoutObject&,
                              PaintPropertyTreeBuilderContext&);
  static void updateTransformForNonRootSVG(const LayoutObject&,
                                           PaintPropertyTreeBuilderContext&);
  static void updateEffect(const LayoutObject&,
                           PaintPropertyTreeBuilderContext&);
  static void updateCssClip(const LayoutObject&,
                            PaintPropertyTreeBuilderContext&);
  static void updateLocalBorderBoxContext(const LayoutObject&,
                                          PaintPropertyTreeBuilderContext&);
  static void updateScrollbarPaintOffset(
      const LayoutObject&,
      const PaintPropertyTreeBuilderContext&);
  static void updateMainThreadScrollingReasons(
      const LayoutObject&,
      PaintPropertyTreeBuilderContext&);
  static void updateOverflowClip(const LayoutObject&,
                                 PaintPropertyTreeBuilderContext&);
  static void updatePerspective(const LayoutObject&,
                                PaintPropertyTreeBuilderContext&);
  static void updateSvgLocalToBorderBoxTransform(
      const LayoutObject&,
      PaintPropertyTreeBuilderContext&);
  static void updateScrollAndScrollTranslation(
      const LayoutObject&,
      PaintPropertyTreeBuilderContext&);
  static void updateOutOfFlowContext(const LayoutObject&,
                                     PaintPropertyTreeBuilderContext&);
};

}  // namespace blink

#endif  // PaintPropertyTreeBuilder_h
