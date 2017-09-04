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
  // Maps from a transform node that is a descendant of the ancestor to the
  // combined transform between the descendant's and the ancestor's coordinate
  // space.
  HashMap<const TransformPaintPropertyNode*, TransformationMatrix>
      toAncestorTransforms;

  // Maps from a descendant clip node to its equivalent "clip visual rect" in
  // the space of the ancestor. The clip visual rect is defined as the
  // intersection of all clips between the descendant and the ancestor (*not*
  // including the ancestor) in the clip tree, individually transformed from
  // their localTransformSpace into the ancestor's localTransformSpace.
  HashMap<const ClipPaintPropertyNode*, FloatRect> toAncestorClipRects;

  static std::unique_ptr<PrecomputedDataForAncestor> create() {
    return makeUnique<PrecomputedDataForAncestor>();
  }
};

// GeometryMapper is a helper class for fast computations of transformed and
// visual rects in different PropertyTreeStates. The design document has a
// number of details on use cases, algorithmic definitions, and running times.
//
// NOTE: A GeometryMapper object is only valid for property trees that do not
// change. If any mutation occurs, a new GeometryMapper object must be allocated
// corresponding to the new state.
//
// Design document: http://bit.ly/28P4FDA
//
// TODO(chrishtr): take effect tree into account.
class PLATFORM_EXPORT GeometryMapper {
 public:
  GeometryMapper() {}
  // The runtime of m calls among localToVisualRectInAncestorSpace,
  // localToAncestorRect or ancestorToLocalRect with the same |ancestorState|
  // parameter is guaranteed to be O(n + m), where n is the number of transform
  // and clip nodes in their respective property trees.

  // If the clips and transforms of |sourceState| are equal to or descendants of
  // those of |destinationState|, returns the same value as
  // localToVisualRectInAncestorSpace. Otherwise, maps the input rect to the
  // transform state which is the least common ancestor of
  // |sourceState.transform| and |destinationState.transform|, then multiplies
  // it by the the inverse transform mapping from the least common ancestor to
  // |destinationState.transform|.
  //
  // Sets |success| to whether that inverse transform is invertible. If it is
  // not, returns the input rect.
  FloatRect mapToVisualRectInDestinationSpace(
      const FloatRect&,
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      bool& success);

  // Same as mapToVisualRectInDestinationSpace() except that *no* clip is
  // applied.
  FloatRect mapRectToDestinationSpace(const FloatRect&,
                                      const PropertyTreeState& sourceState,
                                      const PropertyTreeState& destinationState,
                                      bool& success);

  // Maps from a rect in |localTransformSpace| to its visual rect in
  // |ancestorState|. This is computed by multiplying the rect by its combined
  // transform between |localTransformSpace| and |ancestorSpace|, then
  // flattening into 2D space, then intersecting by the "clip visual rect" for
  // |localTransformState|'s clips. See above for the definition of "clip visual
  // rect".
  //
  // Note that the clip of |ancestorState| is *not* applied.
  //
  // If any of the paint property tree nodes in |localTransformState| are not
  // equal to or a descendant of that in |ancestorState|, returns the passed-in
  // rect and sets |success| to false. Otherwise, sets |success| to true.
  FloatRect localToVisualRectInAncestorSpace(
      const FloatRect&,
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState,
      bool& success);

  // Maps from a rect in |localTransformSpace| to its transformed rect in
  // |ancestorSpace|. This is computed by multiplying the rect by the combined
  // transform between |localTransformState| and |ancestorState|, then
  // flattening into 2D space.
  //
  // If any of the paint property tree nodes in |localTransformState| are not
  // equal to or a descendant of that in |ancestorState|, returns the passed-in
  // rec and sets |success| to false. Otherwise, sets |success| to true.
  FloatRect localToAncestorRect(const FloatRect&,
                                const PropertyTreeState& localTransformState,
                                const PropertyTreeState& ancestorState,
                                bool& success);

  // Maps from a rect in |ancestorSpace| to its transformed rect in
  // |localTransformSpace|. This is computed by multiplying the rect by the
  // inverse combined transform between |localTransformState| and
  // |ancestorState|, if the transform is invertible.
  //
  // If any of the paint property tree nodes in |localTransformState| are not
  // equal to or a descendant of that in |ancestorState|, returns the passed-in
  // rect and sets |success| to false. Otherwise, sets |success| to true.
  FloatRect ancestorToLocalRect(const FloatRect&,
                                const PropertyTreeState& localTransformState,
                                const PropertyTreeState& ancestorState,
                                bool& success);

 private:
  // Used by mapToVisualRectInDestinationSpace() after fast mapping (assuming
  // destination is an ancestor of source) failed.
  FloatRect slowMapToVisualRectInDestinationSpace(
      const FloatRect&,
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      bool& success);

  // Used by mapRectToDestinationSpace() after fast mapping (assuming
  // destination is an ancestor of source) failed.
  FloatRect slowMapRectToDestinationSpace(
      const FloatRect&,
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      bool& success);

  // Returns the matrix used in |LocalToAncestorRect|. Sets |success| to false
  // iff |localTransformNode| is not equal to or a descendant of
  // |ancestorState.transform|.
  const TransformationMatrix& localToAncestorMatrix(
      const TransformPaintPropertyNode* localTransformNode,
      const PropertyTreeState& ancestorState,
      bool& success);

  // Returns the "clip visual rect" between |localTransformState| and
  // |ancestorState|. See above for the definition of "clip visual rect".
  FloatRect localToAncestorClipRect(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState,
      bool& success);

  // Returns the precomputed data if already set, or adds and memoizes a new
  // PrecomputedDataForAncestor otherwise.
  PrecomputedDataForAncestor& getPrecomputedDataForAncestor(
      const PropertyTreeState&);

  // Returns the least common ancestor in the transform tree.
  static const TransformPaintPropertyNode* leastCommonAncestor(
      const TransformPaintPropertyNode*,
      const TransformPaintPropertyNode*);

  friend class GeometryMapperTest;

  HashMap<const TransformPaintPropertyNode*,
          std::unique_ptr<PrecomputedDataForAncestor>>
      m_data;

  const TransformationMatrix m_identity;

  DISALLOW_COPY_AND_ASSIGN(GeometryMapper);
};

}  // namespace blink

#endif  // GeometryMapper_h
