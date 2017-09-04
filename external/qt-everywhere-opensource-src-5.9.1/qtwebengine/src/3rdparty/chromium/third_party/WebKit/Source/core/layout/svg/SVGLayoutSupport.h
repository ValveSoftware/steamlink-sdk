/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGLayoutSupport_h
#define SVGLayoutSupport_h

#include "core/layout/LayoutObject.h"
#include "core/style/SVGComputedStyleDefs.h"
#include "platform/graphics/DashArray.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Allocator.h"

namespace blink {

class AffineTransform;
class FloatPoint;
class FloatRect;
class LayoutRect;
class LayoutGeometryMap;
class LayoutBoxModelObject;
class LayoutObject;
class ComputedStyle;
class LayoutSVGRoot;
class SVGLengthContext;
class StrokeData;
class TransformState;

class CORE_EXPORT SVGLayoutSupport {
  STATIC_ONLY(SVGLayoutSupport);

 public:
  // Shares child layouting code between
  // LayoutSVGRoot/LayoutSVG(Hidden)Container
  static void layoutChildren(LayoutObject*,
                             bool forceLayout,
                             bool screenScalingFactorChanged,
                             bool layoutSizeChanged);

  // Layout resources used by this node.
  static void layoutResourcesIfNeeded(const LayoutObject*);

  // Helper function determining whether overflow is hidden.
  static bool isOverflowHidden(const LayoutObject*);

  // Adjusts the visualRect in combination with filter, clipper and masker
  // in local coordinates.
  static void adjustVisualRectWithResources(const LayoutObject*, FloatRect&);

  // Determine if the LayoutObject references a filter resource object.
  static bool hasFilterResource(const LayoutObject&);

  // Determines whether the passed point lies in a clipping area
  static bool pointInClippingArea(const LayoutObject&, const FloatPoint&);

  // Transform |pointInParent| to |object|'s user-space and check if it is
  // within the clipping area. Returns false if the transform is singular or
  // the point is outside the clipping area.
  static bool transformToUserSpaceAndCheckClipping(
      const LayoutObject&,
      const AffineTransform& localTransform,
      const FloatPoint& pointInParent,
      FloatPoint& localPoint);

  static void computeContainerBoundingBoxes(const LayoutObject* container,
                                            FloatRect& objectBoundingBox,
                                            bool& objectBoundingBoxValid,
                                            FloatRect& strokeBoundingBox,
                                            FloatRect& localVisualRect);

  // Important functions used by nearly all SVG layoutObjects centralizing
  // coordinate transformations / visual rect calculations
  static FloatRect localVisualRect(const LayoutObject&);
  static LayoutRect visualRectInAncestorSpace(
      const LayoutObject&,
      const LayoutBoxModelObject& ancestor);
  static LayoutRect transformVisualRect(const LayoutObject&,
                                        const AffineTransform&,
                                        const FloatRect&);
  static bool mapToVisualRectInAncestorSpace(
      const LayoutObject&,
      const LayoutBoxModelObject* ancestor,
      const FloatRect& localVisualRect,
      LayoutRect& resultRect,
      VisualRectFlags = DefaultVisualRectFlags);
  static void mapLocalToAncestor(const LayoutObject*,
                                 const LayoutBoxModelObject* ancestor,
                                 TransformState&,
                                 MapCoordinatesFlags);
  static void mapAncestorToLocal(const LayoutObject&,
                                 const LayoutBoxModelObject* ancestor,
                                 TransformState&,
                                 MapCoordinatesFlags);
  static const LayoutObject* pushMappingToContainer(
      const LayoutObject*,
      const LayoutBoxModelObject* ancestorToStopAt,
      LayoutGeometryMap&);

  // Shared between SVG layoutObjects and resources.
  static void applyStrokeStyleToStrokeData(StrokeData&,
                                           const ComputedStyle&,
                                           const LayoutObject&,
                                           float dashScaleFactor);

  static DashArray resolveSVGDashArray(const SVGDashArray&,
                                       const ComputedStyle&,
                                       const SVGLengthContext&);

  // Determines if any ancestor has adjusted the scale factor.
  static bool screenScaleFactorChanged(const LayoutObject*);

  // Determines if any ancestor's layout size has changed.
  static bool layoutSizeOfNearestViewportChanged(const LayoutObject*);

  // FIXME: These methods do not belong here.
  static const LayoutSVGRoot* findTreeRootObject(const LayoutObject*);

  // Helper method for determining if a LayoutObject marked as text (isText()==
  // true) can/will be laid out as part of a <text>.
  static bool isLayoutableTextNode(const LayoutObject*);

  // Determines whether a svg node should isolate or not based on ComputedStyle.
  static bool willIsolateBlendingDescendantsForStyle(const ComputedStyle&);
  static bool willIsolateBlendingDescendantsForObject(const LayoutObject*);
  template <typename LayoutObjectType>
  static bool computeHasNonIsolatedBlendingDescendants(const LayoutObjectType*);
  static bool isIsolationRequired(const LayoutObject*);

  static AffineTransform deprecatedCalculateTransformToLayer(
      const LayoutObject*);
  static float calculateScreenFontSizeScalingFactor(const LayoutObject*);

  static LayoutObject* findClosestLayoutSVGText(LayoutObject*,
                                                const FloatPoint&);

 private:
  static void updateObjectBoundingBox(FloatRect& objectBoundingBox,
                                      bool& objectBoundingBoxValid,
                                      LayoutObject* other,
                                      FloatRect otherBoundingBox);
};

class SubtreeContentTransformScope {
  STACK_ALLOCATED();

 public:
  SubtreeContentTransformScope(const AffineTransform&);
  ~SubtreeContentTransformScope();

  static AffineTransform currentContentTransformation() {
    return AffineTransform(s_currentContentTransformation);
  }

 private:
  static AffineTransform::Transform s_currentContentTransformation;
  AffineTransform m_savedContentTransformation;
};

// The following enumeration is used to optimize cases where the scale is known
// to be invariant (see: LayoutSVGContainer::layout and LayoutSVGroot). The
// value 'Full' can be used in the general case when the scale change is
// unknown, or known to change.
enum class SVGTransformChange {
  None,
  ScaleInvariant,
  Full,
};

// Helper for computing ("classifying") a change to a transform using the
// categoies defined above.
class SVGTransformChangeDetector {
  STACK_ALLOCATED();

 public:
  explicit SVGTransformChangeDetector(const AffineTransform& previous)
      : m_previousTransform(previous) {}

  SVGTransformChange computeChange(const AffineTransform& current) {
    if (m_previousTransform == current)
      return SVGTransformChange::None;
    if (scaleReference(m_previousTransform) == scaleReference(current))
      return SVGTransformChange::ScaleInvariant;
    return SVGTransformChange::Full;
  }

 private:
  static std::pair<double, double> scaleReference(
      const AffineTransform& transform) {
    return std::make_pair(transform.xScaleSquared(), transform.yScaleSquared());
  }
  AffineTransform m_previousTransform;
};

template <typename LayoutObjectType>
bool SVGLayoutSupport::computeHasNonIsolatedBlendingDescendants(
    const LayoutObjectType* object) {
  for (LayoutObject* child = object->firstChild(); child;
       child = child->nextSibling()) {
    if (child->isBlendingAllowed() && child->style()->hasBlendMode())
      return true;
    if (child->hasNonIsolatedBlendingDescendants() &&
        !willIsolateBlendingDescendantsForObject(child))
      return true;
  }
  return false;
}

}  // namespace blink

#endif  // SVGLayoutSupport_h
