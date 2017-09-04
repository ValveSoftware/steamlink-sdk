/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "core/layout/svg/LayoutSVGRoot.h"

#include "core/frame/LocalFrame.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGRootPainter.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/LengthFunctions.h"

namespace blink {

LayoutSVGRoot::LayoutSVGRoot(SVGElement* node)
    : LayoutReplaced(node),
      m_objectBoundingBoxValid(false),
      m_isLayoutSizeChanged(false),
      m_didScreenScaleFactorChange(false),
      m_needsBoundariesOrTransformUpdate(true),
      m_hasBoxDecorationBackground(false),
      m_hasNonIsolatedBlendingDescendants(false),
      m_hasNonIsolatedBlendingDescendantsDirty(false) {
  SVGSVGElement* svg = toSVGSVGElement(node);
  ASSERT(svg);

  LayoutSize intrinsicSize(svg->intrinsicWidth(), svg->intrinsicHeight());
  if (!svg->hasIntrinsicWidth())
    intrinsicSize.setWidth(LayoutUnit(defaultWidth));
  if (!svg->hasIntrinsicHeight())
    intrinsicSize.setHeight(LayoutUnit(defaultHeight));
  setIntrinsicSize(intrinsicSize);
}

LayoutSVGRoot::~LayoutSVGRoot() {}

void LayoutSVGRoot::computeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsicSizingInfo) const {
  // https://www.w3.org/TR/SVG/coords.html#IntrinsicSizing

  SVGSVGElement* svg = toSVGSVGElement(node());
  ASSERT(svg);

  intrinsicSizingInfo.size =
      FloatSize(svg->intrinsicWidth(), svg->intrinsicHeight());
  intrinsicSizingInfo.hasWidth = svg->hasIntrinsicWidth();
  intrinsicSizingInfo.hasHeight = svg->hasIntrinsicHeight();

  if (!intrinsicSizingInfo.size.isEmpty()) {
    intrinsicSizingInfo.aspectRatio = intrinsicSizingInfo.size;
  } else {
    FloatSize viewBoxSize = svg->viewBox()->currentValue()->value().size();
    if (!viewBoxSize.isEmpty()) {
      // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
      intrinsicSizingInfo.aspectRatio = viewBoxSize;
    }
  }

  if (!isHorizontalWritingMode())
    intrinsicSizingInfo.transpose();
}

bool LayoutSVGRoot::isEmbeddedThroughSVGImage() const {
  return SVGImage::isInSVGImage(toSVGSVGElement(node()));
}

bool LayoutSVGRoot::isEmbeddedThroughFrameContainingSVGDocument() const {
  if (!node())
    return false;

  LocalFrame* frame = node()->document().frame();
  if (!frame)
    return false;

  // If our frame has an owner layoutObject, we're embedded through eg.
  // object/embed/iframe, but we only negotiate if we're in an SVG document
  // inside a embedded object (object/embed).
  if (frame->ownerLayoutItem().isNull() ||
      !frame->ownerLayoutItem().isEmbeddedObject())
    return false;
  return frame->document()->isSVGDocument();
}

LayoutUnit LayoutSVGRoot::computeReplacedLogicalWidth(
    ShouldComputePreferred shouldComputePreferred) const {
  // When we're embedded through SVGImage
  // (border-image/background-image/<html:img>/...) we're forced to resize to a
  // specific size.
  if (!m_containerSize.isEmpty())
    return LayoutUnit(m_containerSize.width());

  if (isEmbeddedThroughFrameContainingSVGDocument())
    return containingBlock()->availableLogicalWidth();

  return LayoutReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
}

LayoutUnit LayoutSVGRoot::computeReplacedLogicalHeight(
    LayoutUnit estimatedUsedWidth) const {
  // When we're embedded through SVGImage
  // (border-image/background-image/<html:img>/...) we're forced to resize to a
  // specific size.
  if (!m_containerSize.isEmpty())
    return LayoutUnit(m_containerSize.height());

  if (isEmbeddedThroughFrameContainingSVGDocument())
    return containingBlock()->availableLogicalHeight(
        IncludeMarginBorderPadding);

  return LayoutReplaced::computeReplacedLogicalHeight(estimatedUsedWidth);
}

void LayoutSVGRoot::layout() {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  LayoutSize oldSize = size();
  updateLogicalWidth();
  updateLogicalHeight();

  buildLocalToBorderBoxTransform();
  // TODO(fs): Temporarily, needing a layout implies that the local transform
  // has changed. This should be updated to be more precise and factor in the
  // actual (relevant) changes to the computed user-space transform.
  m_didScreenScaleFactorChange = selfNeedsLayout();

  SVGLayoutSupport::layoutResourcesIfNeeded(this);

  // selfNeedsLayout() will cover changes to one (or more) of viewBox,
  // current{Scale,Translate} and decorations.
  const bool viewportMayHaveChanged = selfNeedsLayout() || oldSize != size();

  // The scale of one or more of the SVG elements may have changed, or new
  // content may have been exposed, so mark the entire subtree as needing paint
  // invalidation checking. (It is only somewhat by coincidence that this
  // condition happens to be the same as the one for viewport changes.)
  if (viewportMayHaveChanged)
    setMayNeedPaintInvalidationSubtree();

  SVGSVGElement* svg = toSVGSVGElement(node());
  ASSERT(svg);
  // When hasRelativeLengths() is false, no descendants have relative lengths
  // (hence no one is interested in viewport size changes).
  m_isLayoutSizeChanged = viewportMayHaveChanged && svg->hasRelativeLengths();

  SVGLayoutSupport::layoutChildren(
      firstChild(), false, m_didScreenScaleFactorChange, m_isLayoutSizeChanged);

  if (m_needsBoundariesOrTransformUpdate) {
    updateCachedBoundaries();
    m_needsBoundariesOrTransformUpdate = false;
  }

  m_overflow.reset();
  addVisualEffectOverflow();

  if (!shouldApplyViewportClip()) {
    FloatRect contentVisualRect = visualRectInLocalSVGCoordinates();
    contentVisualRect = m_localToBorderBoxTransform.mapRect(contentVisualRect);
    addContentsVisualOverflow(enclosingLayoutRect(contentVisualRect));
  }

  updateLayerTransformAfterLayout();
  m_hasBoxDecorationBackground = isDocumentElement()
                                     ? styleRef().hasBoxDecorationBackground()
                                     : hasBoxDecorationBackground();
  invalidateBackgroundObscurationStatus();

  clearNeedsLayout();
}

bool LayoutSVGRoot::shouldApplyViewportClip() const {
  // the outermost svg is clipped if auto, and svg document roots are always
  // clipped. When the svg is stand-alone (isDocumentElement() == true) the
  // viewport clipping should always be applied, noting that the window
  // scrollbars should be hidden if overflow=hidden.
  return style()->overflowX() == OverflowHidden ||
         style()->overflowX() == OverflowAuto ||
         style()->overflowX() == OverflowScroll || this->isDocumentElement();
}

LayoutRect LayoutSVGRoot::visualOverflowRect() const {
  LayoutRect rect = LayoutReplaced::selfVisualOverflowRect();
  if (!shouldApplyViewportClip())
    rect.unite(contentsVisualOverflowRect());
  return rect;
}

LayoutRect LayoutSVGRoot::overflowClipRect(const LayoutPoint& location,
                                           OverlayScrollbarClipBehavior) const {
  return LayoutRect(pixelSnappedIntRect(
      LayoutReplaced::overflowClipRect(location, IgnoreOverlayScrollbarSize)));
}

void LayoutSVGRoot::paintReplaced(const PaintInfo& paintInfo,
                                  const LayoutPoint& paintOffset) const {
  SVGRootPainter(*this).paintReplaced(paintInfo, paintOffset);
}

void LayoutSVGRoot::willBeDestroyed() {
  SVGResourcesCache::clientDestroyed(this);
  LayoutReplaced::willBeDestroyed();
}

void LayoutSVGRoot::styleDidChange(StyleDifference diff,
                                   const ComputedStyle* oldStyle) {
  if (diff.needsFullLayout())
    setNeedsBoundariesUpdate();
  if (diff.needsPaintInvalidation()) {
    // Box decorations may have appeared/disappeared - recompute status.
    m_hasBoxDecorationBackground = styleRef().hasBoxDecorationBackground();
  }

  LayoutReplaced::styleDidChange(diff, oldStyle);
  SVGResourcesCache::clientStyleChanged(this, diff, styleRef());
}

bool LayoutSVGRoot::isChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  return child->isSVG() &&
         !(child->isSVGInline() || child->isSVGInlineText() ||
           child->isSVGGradientStop());
}

void LayoutSVGRoot::addChild(LayoutObject* child, LayoutObject* beforeChild) {
  LayoutReplaced::addChild(child, beforeChild);
  SVGResourcesCache::clientWasAddedToTree(child, child->styleRef());

  bool shouldIsolateDescendants =
      (child->isBlendingAllowed() && child->style()->hasBlendMode()) ||
      child->hasNonIsolatedBlendingDescendants();
  if (shouldIsolateDescendants)
    descendantIsolationRequirementsChanged(DescendantIsolationRequired);
}

void LayoutSVGRoot::removeChild(LayoutObject* child) {
  SVGResourcesCache::clientWillBeRemovedFromTree(child);
  LayoutReplaced::removeChild(child);

  bool hadNonIsolatedDescendants =
      (child->isBlendingAllowed() && child->style()->hasBlendMode()) ||
      child->hasNonIsolatedBlendingDescendants();
  if (hadNonIsolatedDescendants)
    descendantIsolationRequirementsChanged(DescendantIsolationNeedsUpdate);
}

bool LayoutSVGRoot::hasNonIsolatedBlendingDescendants() const {
  if (m_hasNonIsolatedBlendingDescendantsDirty) {
    m_hasNonIsolatedBlendingDescendants =
        SVGLayoutSupport::computeHasNonIsolatedBlendingDescendants(this);
    m_hasNonIsolatedBlendingDescendantsDirty = false;
  }
  return m_hasNonIsolatedBlendingDescendants;
}

void LayoutSVGRoot::descendantIsolationRequirementsChanged(
    DescendantIsolationState state) {
  switch (state) {
    case DescendantIsolationRequired:
      m_hasNonIsolatedBlendingDescendants = true;
      m_hasNonIsolatedBlendingDescendantsDirty = false;
      break;
    case DescendantIsolationNeedsUpdate:
      m_hasNonIsolatedBlendingDescendantsDirty = true;
      break;
  }
}

void LayoutSVGRoot::insertedIntoTree() {
  LayoutReplaced::insertedIntoTree();
  SVGResourcesCache::clientWasAddedToTree(this, styleRef());
}

void LayoutSVGRoot::willBeRemovedFromTree() {
  SVGResourcesCache::clientWillBeRemovedFromTree(this);
  LayoutReplaced::willBeRemovedFromTree();
}

PositionWithAffinity LayoutSVGRoot::positionForPoint(const LayoutPoint& point) {
  FloatPoint absolutePoint = FloatPoint(point);
  absolutePoint = m_localToBorderBoxTransform.inverse().mapPoint(absolutePoint);
  LayoutObject* closestDescendant =
      SVGLayoutSupport::findClosestLayoutSVGText(this, absolutePoint);

  if (!closestDescendant)
    return LayoutReplaced::positionForPoint(point);

  LayoutObject* layoutObject = closestDescendant;
  AffineTransform transform = layoutObject->localToSVGParentTransform();
  transform.translate(toLayoutSVGText(layoutObject)->location().x(),
                      toLayoutSVGText(layoutObject)->location().y());
  while (layoutObject) {
    layoutObject = layoutObject->parent();
    if (layoutObject->isSVGRoot())
      break;
    transform = layoutObject->localToSVGParentTransform() * transform;
  }

  absolutePoint = transform.inverse().mapPoint(absolutePoint);

  return closestDescendant->positionForPoint(LayoutPoint(absolutePoint));
}

// LayoutBox methods will expect coordinates w/o any transforms in coordinates
// relative to our borderBox origin.  This method gives us exactly that.
void LayoutSVGRoot::buildLocalToBorderBoxTransform() {
  SVGSVGElement* svg = toSVGSVGElement(node());
  ASSERT(svg);
  float scale = style()->effectiveZoom();
  FloatPoint translate = svg->currentTranslate();
  LayoutSize borderAndPadding(borderLeft() + paddingLeft(),
                              borderTop() + paddingTop());
  m_localToBorderBoxTransform = svg->viewBoxToViewTransform(
      contentWidth() / scale, contentHeight() / scale);

  AffineTransform viewToBorderBoxTransform(
      scale, 0, 0, scale, borderAndPadding.width() + translate.x(),
      borderAndPadding.height() + translate.y());
  viewToBorderBoxTransform.scale(svg->currentScale());
  m_localToBorderBoxTransform.preMultiply(viewToBorderBoxTransform);
}

const AffineTransform& LayoutSVGRoot::localToSVGParentTransform() const {
  // Slightly optimized version of m_localToParentTransform =
  // AffineTransform::translation(x(), y()) * m_localToBorderBoxTransform;
  m_localToParentTransform = m_localToBorderBoxTransform;
  if (location().x())
    m_localToParentTransform.setE(m_localToParentTransform.e() +
                                  roundToInt(location().x()));
  if (location().y())
    m_localToParentTransform.setF(m_localToParentTransform.f() +
                                  roundToInt(location().y()));
  return m_localToParentTransform;
}

LayoutRect LayoutSVGRoot::localVisualRect() const {
  // This is an open-coded aggregate of SVGLayoutSupport::localVisualRect
  // and LayoutReplaced::localVisualRect. The reason for this is to optimize/
  // minimize the visual rect when the box is not "decorated" (does not have
  // background/border/etc., see
  // LayoutSVGRootTest.VisualRectMappingWithViewportClipWithoutBorder).

  // Return early for any cases where we don't actually paint.
  if (style()->visibility() != EVisibility::Visible &&
      !enclosingLayer()->hasVisibleContent())
    return LayoutRect();

  // Compute the visual rect of the content of the SVG in the border-box
  // coordinate space.
  FloatRect contentVisualRect = visualRectInLocalSVGCoordinates();
  contentVisualRect = m_localToBorderBoxTransform.mapRect(contentVisualRect);

  // Apply initial viewport clip, overflow:visible content is added to
  // visualOverflow but the most common case is that overflow is hidden, so
  // always intersect.
  contentVisualRect.intersect(pixelSnappedBorderBoxRect());

  LayoutRect visualRect = enclosingLayoutRect(contentVisualRect);
  // If the box is decorated or is overflowing, extend it to include the
  // border-box and overflow.
  if (m_hasBoxDecorationBackground || hasOverflowModel()) {
    // The selectionRect can project outside of the overflowRect, so take their
    // union for paint invalidation to avoid selection painting glitches.
    LayoutRect decoratedVisualRect =
        unionRect(localSelectionRect(), visualOverflowRect());
    visualRect.unite(decoratedVisualRect);
  }

  return LayoutRect(enclosingIntRect(visualRect));
}

// This method expects local CSS box coordinates.
// Callers with local SVG viewport coordinates should first apply the
// localToBorderBoxTransform to convert from SVG viewport coordinates to local
// CSS box coordinates.
void LayoutSVGRoot::mapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                       TransformState& transformState,
                                       MapCoordinatesFlags mode) const {
  LayoutReplaced::mapLocalToAncestor(ancestor, transformState,
                                     mode | ApplyContainerFlip);
}

const LayoutObject* LayoutSVGRoot::pushMappingToContainer(
    const LayoutBoxModelObject* ancestorToStopAt,
    LayoutGeometryMap& geometryMap) const {
  return LayoutReplaced::pushMappingToContainer(ancestorToStopAt, geometryMap);
}

void LayoutSVGRoot::updateCachedBoundaries() {
  SVGLayoutSupport::computeContainerBoundingBoxes(
      this, m_objectBoundingBox, m_objectBoundingBoxValid, m_strokeBoundingBox,
      m_visualRectInLocalSVGCoordinates);
}

bool LayoutSVGRoot::nodeAtPoint(HitTestResult& result,
                                const HitTestLocation& locationInContainer,
                                const LayoutPoint& accumulatedOffset,
                                HitTestAction hitTestAction) {
  LayoutPoint pointInParent =
      locationInContainer.point() - toLayoutSize(accumulatedOffset);
  LayoutPoint pointInBorderBox = pointInParent - toLayoutSize(location());

  // Only test SVG content if the point is in our content box, or in case we
  // don't clip to the viewport, the visual overflow rect.
  // FIXME: This should be an intersection when rect-based hit tests are
  // supported by nodeAtFloatPoint.
  if (contentBoxRect().contains(pointInBorderBox) ||
      (!shouldApplyViewportClip() &&
       visualOverflowRect().contains(pointInBorderBox))) {
    const AffineTransform& localToParentTransform =
        this->localToSVGParentTransform();
    if (localToParentTransform.isInvertible()) {
      FloatPoint localPoint =
          localToParentTransform.inverse().mapPoint(FloatPoint(pointInParent));

      for (LayoutObject* child = lastChild(); child;
           child = child->previousSibling()) {
        // FIXME: nodeAtFloatPoint() doesn't handle rect-based hit tests yet.
        if (child->nodeAtFloatPoint(result, localPoint, hitTestAction)) {
          updateHitTestResult(result, pointInBorderBox);
          if (result.addNodeToListBasedTestResult(
                  child->node(), locationInContainer) == StopHitTesting)
            return true;
        }
      }
    }
  }

  // If we didn't early exit above, we've just hit the container <svg> element.
  // Unlike SVG 1.1, 2nd Edition allows container elements to be hit.
  if ((hitTestAction == HitTestBlockBackground ||
       hitTestAction == HitTestChildBlockBackground) &&
      visibleToHitTestRequest(result.hitTestRequest())) {
    // Only return true here, if the last hit testing phase 'BlockBackground'
    // (or 'ChildBlockBackground' - depending on context) is executed.
    // If we'd return true in the 'Foreground' phase, hit testing would stop
    // immediately. For SVG only trees this doesn't matter.
    // Though when we have a <foreignObject> subtree we need to be able to
    // detect hits on the background of a <div> element.
    // If we'd return true here in the 'Foreground' phase, we are not able to
    // detect these hits anymore.
    LayoutRect boundsRect(accumulatedOffset + location(), size());
    if (locationInContainer.intersects(boundsRect)) {
      updateHitTestResult(result, pointInBorderBox);
      if (result.addNodeToListBasedTestResult(node(), locationInContainer,
                                              boundsRect) == StopHitTesting)
        return true;
    }
  }

  return false;
}

}  // namespace blink
