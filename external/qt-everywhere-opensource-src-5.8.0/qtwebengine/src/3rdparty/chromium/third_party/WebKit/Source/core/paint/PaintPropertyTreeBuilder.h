// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreeBuilder_h
#define PaintPropertyTreeBuilder_h

#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/RefPtr.h"

namespace blink {

class FrameView;
class LayoutObject;

// The context for PaintPropertyTreeBuilder.
// It's responsible for bookkeeping tree state in other order, for example, the most recent
// position container seen.
struct PaintPropertyTreeBuilderContext {
    PaintPropertyTreeBuilderContext()
        : currentTransform(nullptr)
        , currentClip(nullptr)
        , transformForAbsolutePosition(nullptr)
        , containerForAbsolutePosition(nullptr)
        , clipForAbsolutePosition(nullptr)
        , transformForFixedPosition(nullptr)
        , clipForFixedPosition(nullptr)
        , currentEffect(nullptr) { }

    // The combination of a transform and paint offset describes a linear space.
    // When a layout object recur to its children, the main context is expected to refer
    // the object's border box, then the callee will derive its own border box by translating
    // the space with its own layout location.
    TransformPaintPropertyNode* currentTransform;
    LayoutPoint paintOffset;
    // The clip node describes the accumulated raster clip for the current subtree.
    // Note that the computed raster region in canvas space for a clip node is independent from
    // the transform and paint offset above. Also the actual raster region may be affected
    // by layerization and occlusion tracking.
    ClipPaintPropertyNode* currentClip;

    // Separate context for out-of-flow positioned and fixed positioned elements are needed
    // because they don't use DOM parent as their containing block.
    // These additional contexts normally pass through untouched, and are only copied from
    // the main context when the current element serves as the containing block of corresponding
    // positioned descendants.
    // Overflow clips are also inherited by containing block tree instead of DOM tree, thus they
    // are included in the additional context too.
    TransformPaintPropertyNode* transformForAbsolutePosition;
    LayoutPoint paintOffsetForAbsolutePosition;
    const LayoutObject* containerForAbsolutePosition;
    ClipPaintPropertyNode* clipForAbsolutePosition;

    TransformPaintPropertyNode* transformForFixedPosition;
    LayoutPoint paintOffsetForFixedPosition;
    ClipPaintPropertyNode* clipForFixedPosition;

    // The effect hierarchy is applied by the stacking context tree. It is guaranteed that every
    // DOM descendant is also a stacking context descendant. Therefore, we don't need extra
    // bookkeeping for effect nodes and can generate the effect tree from a DOM-order traversal.
    EffectPaintPropertyNode* currentEffect;
};

// Creates paint property tree nodes for special things in the layout tree.
// Special things include but not limit to: overflow clip, transform, fixed-pos, animation,
// mask, filter, ... etc.
// It expects to be invoked for each layout tree node in DOM order during InPrePaint phase.
class PaintPropertyTreeBuilder {
public:
    void buildTreeRootNodes(FrameView&, PaintPropertyTreeBuilderContext&);
    void buildTreeNodes(FrameView&, PaintPropertyTreeBuilderContext&);
    void buildTreeNodes(const LayoutObject&, PaintPropertyTreeBuilderContext&);

private:
    static void updatePaintOffsetTranslation(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updateTransform(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updateEffect(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updateCssClip(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updateLocalBorderBoxContext(const LayoutObject&, const PaintPropertyTreeBuilderContext&);
    static void updateScrollbarPaintOffset(const LayoutObject&, const PaintPropertyTreeBuilderContext&);
    static void updateOverflowClip(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updatePerspective(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updateSvgLocalToBorderBoxTransform(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updateScrollTranslation(const LayoutObject&, PaintPropertyTreeBuilderContext&);
    static void updateOutOfFlowContext(const LayoutObject&, PaintPropertyTreeBuilderContext&);

    // Holds references to root property nodes to keep them alive during tree walk.
    RefPtr<TransformPaintPropertyNode> transformRoot;
    RefPtr<ClipPaintPropertyNode> clipRoot;
    RefPtr<EffectPaintPropertyNode> effectRoot;
};

} // namespace blink

#endif // PaintPropertyTreeBuilder_h
