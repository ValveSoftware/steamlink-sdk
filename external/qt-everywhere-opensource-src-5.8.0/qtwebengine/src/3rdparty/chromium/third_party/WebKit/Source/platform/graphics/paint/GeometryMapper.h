// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapper_h
#define GeometryMapper_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/HashMap.h"

namespace blink {

struct PrecomputedDataForAncestor {
    // Maps from a transform node that is a descendant of the ancestor to the combined
    // transform between the descendant's and the ancestor's coordinate space.
    HashMap<const TransformPaintPropertyNode*, TransformationMatrix> toAncestorTransforms;

    // Maps from a descendant clip node to its equivalent "clip visual rect" in the space of the ancestor.
    // The clip visual rect is defined as the intersection of all clips between the descendant
    // and the ancestor (*not* including the ancestor) in the clip tree, individually transformed from their
    // localTransformSpace into the ancestor's localTransformSpace.
    HashMap<const ClipPaintPropertyNode*, FloatRect> toAncestorClipRects;

    static std::unique_ptr<PrecomputedDataForAncestor> create()
    {
        return wrapUnique(new PrecomputedDataForAncestor());
    }
};

// GeometryMapper is a helper class for fast computations of transformed and visual rects in different
// PropertyTreeStates. The design document has a number of details on use cases, algorithmic definitions,
// and running times.
//
// Design document: http://bit.ly/28P4FDA
//
// TODO(chrishtr): take effect and scroll trees into account.
class PLATFORM_EXPORT GeometryMapper {
public:
    GeometryMapper() {}

    // The runtime of m calls among LocalToVisualRectInAncestorSpace, LocalToAncestorRect or AncestorToLocalRect
    // with the same |ancestorState| parameter is guaranteed to be O(n + m),  where n is the number of transform and clip
    // nodes in their respective property trees.

    // Maps from a rect in |localTransformSpace| to its visual rect in |ancestorState|. This is computed
    // by multiplying the rect by its combined transform between |localTransformSpace| and |ancestorSpace|,
    // then flattening into 2D space, then intersecting by the "clip visual rect" for |localTransformState|'s clips.
    // See above for the definition of "clip visual rect".
    //
    // Note that the clip of |ancestorState| is *not* applied.
    //
    // It is an error to call this method if any of the paint property tree nodes in |localTransformState| are not equal
    // to or a descendant of that in |ancestorState|.
    FloatRect LocalToVisualRectInAncestorSpace(const FloatRect&,
        const PropertyTreeState& localTransformState,
        const PropertyTreeState& ancestorState);

    // Maps from a rect in |localTransformSpace| to its transformed rect in |ancestorSpace|. This is computed
    // by multiplying the rect by the combined transform between |localTransformState| and |ancestorState|,
    // then flattening into 2D space.
    //
    // It is an error to call this method if any of the paint property tree nodes in |localTransformState| are not equal
    // to or a descendant of that in |ancestorState|.
    FloatRect LocalToAncestorRect(const FloatRect&,
        const PropertyTreeState& localTransformState,
        const PropertyTreeState& ancestorState);

    // Maps from a rect in |ancestorSpace| to its transformed rect in |localTransformSpace|. This is computed
    // by multiplying the rect by the inverse combined transform between |localTransformState| and |ancestorState|,
    // if the transform is invertible. If is invertible, also sets |*success| to true. Otherwise sets |*success| to false.
    //
    // It is an error to call this method if any of the paint property tree nodes in |localTransformState| are not equal
    // to or a descendant of that in |ancestorState|.
    FloatRect AncestorToLocalRect(const FloatRect&,
        const PropertyTreeState& localTransformState,
        const PropertyTreeState& ancestorState, bool* success);

private:
    // Returns the matrix used in |LocalToAncestorRect|.
    const TransformationMatrix& LocalToAncestorMatrix(
        const TransformPaintPropertyNode* localTransformState,
        const PropertyTreeState& ancestorState);

    // Returns the "clip visual rect" between |localTransformState| and |ancestorState|. See above for the definition
    // of "clip visual rect".
    const FloatRect& LocalToAncestorClipRect(
        const PropertyTreeState& localTransformState,
        const PropertyTreeState& ancestorState);

    // Returns the precomputed data if already set, or adds and memoizes a new PrecomputedDataForAncestor otherwise.
    PrecomputedDataForAncestor& GetPrecomputedDataForAncestor(const PropertyTreeState&);

    friend class GeometryMapperTest;

    HashMap<const TransformPaintPropertyNode*, std::unique_ptr<PrecomputedDataForAncestor>> m_data;

    DISALLOW_COPY_AND_ASSIGN(GeometryMapper);
};

} // namespace blink

#endif // GeometryMapper_h
