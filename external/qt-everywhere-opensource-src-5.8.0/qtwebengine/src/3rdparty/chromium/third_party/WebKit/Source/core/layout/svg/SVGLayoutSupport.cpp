/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "core/layout/svg/SVGLayoutSupport.h"

#include "core/frame/FrameHost.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/LayoutSVGResourceFilter.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/LayoutSVGShape.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/LayoutSVGTransformableContainer.h"
#include "core/layout/svg/LayoutSVGViewportContainer.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGElement.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/StrokeData.h"

namespace blink {

struct SearchCandidate {
    SearchCandidate()
        : candidateLayoutObject(nullptr)
        , candidateDistance(std::numeric_limits<float>::max())
    {
    }
    SearchCandidate(LayoutObject* layoutObject, float distance)
        : candidateLayoutObject(layoutObject)
        , candidateDistance(distance)
    {
    }
    LayoutObject* candidateLayoutObject;
    float candidateDistance;
};

FloatRect SVGLayoutSupport::localOverflowRectForPaintInvalidation(const LayoutObject& object)
{
    // This doesn't apply to LayoutSVGRoot. Use LayoutSVGRoot::localOverflowRectForPaintInvalidation() instead.
    ASSERT(!object.isSVGRoot());

    // Return early for any cases where we don't actually paint
    if (object.styleRef().visibility() != VISIBLE && !object.enclosingLayer()->hasVisibleContent())
        return FloatRect();

    FloatRect paintInvalidationRect = object.paintInvalidationRectInLocalSVGCoordinates();
    if (int outlineOutset = object.styleRef().outlineOutsetExtent())
        paintInvalidationRect.inflate(outlineOutset);
    return paintInvalidationRect;
}

LayoutRect SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(const LayoutObject& object, const LayoutBoxModelObject& paintInvalidationContainer)
{
    LayoutRect rect;
    mapToVisualRectInAncestorSpace(object, &paintInvalidationContainer, localOverflowRectForPaintInvalidation(object), rect);
    return rect;
}

LayoutRect SVGLayoutSupport::transformPaintInvalidationRect(const LayoutObject& object, const AffineTransform& rootTransform, const FloatRect& localRect)
{
    FloatRect adjustedRect = rootTransform.mapRect(localRect);

    if (object.isSVGShape() && object.styleRef().svgStyle().hasStroke()) {
        if (float strokeWidthForHairlinePadding = toLayoutSVGShape(object).strokeWidth()) {
            // For hairline strokes (stroke-width < 1 in device space), Skia rasterizes up to 0.4(9) off
            // the stroke center. That means enclosingIntRect is not enough - we must also pad to 0.5.
            // This is still fragile as it misses out on CC/DSF CTM components.
            const FloatSize strokeSize = rootTransform.mapSize(
                FloatSize(strokeWidthForHairlinePadding, strokeWidthForHairlinePadding));
            if (strokeSize.width() < 1 || strokeSize.height() < 1) {
                const float pad = 0.5f - std::min(strokeSize.width(), strokeSize.height()) / 2;
                ASSERT(pad > 0);
                adjustedRect.inflate(pad);
            }
        }
    }

    if (adjustedRect.isEmpty())
        return LayoutRect();

    return LayoutRect(enclosingIntRect(adjustedRect));
}

static const LayoutSVGRoot& computeTransformToSVGRoot(const LayoutObject& object, AffineTransform& rootBorderBoxTransform)
{
    ASSERT(object.isSVG() && !object.isSVGRoot());

    const LayoutObject* parent;
    for (parent = &object; !parent->isSVGRoot(); parent = parent->parent())
        rootBorderBoxTransform.preMultiply(parent->localToSVGParentTransform());

    const LayoutSVGRoot& svgRoot = toLayoutSVGRoot(*parent);
    rootBorderBoxTransform.preMultiply(svgRoot.localToBorderBoxTransform());
    return svgRoot;
}

bool SVGLayoutSupport::mapToVisualRectInAncestorSpace(const LayoutObject& object, const LayoutBoxModelObject* ancestor, const FloatRect& localPaintInvalidationRect, LayoutRect& resultRect, VisualRectFlags visualRectFlags)
{
    AffineTransform rootBorderBoxTransform;
    const LayoutSVGRoot& svgRoot = computeTransformToSVGRoot(object, rootBorderBoxTransform);
    resultRect = transformPaintInvalidationRect(object, rootBorderBoxTransform, localPaintInvalidationRect);

    // Apply initial viewport clip.
    if (svgRoot.shouldApplyViewportClip()) {
        LayoutRect clipRect(svgRoot.pixelSnappedBorderBoxRect());
        if (visualRectFlags & EdgeInclusive) {
            if (!resultRect.inclusiveIntersect(clipRect))
                return false;
        } else {
            resultRect.intersect(clipRect);
        }
    }
    return svgRoot.mapToVisualRectInAncestorSpace(ancestor, resultRect, visualRectFlags);
}

void SVGLayoutSupport::mapLocalToAncestor(const LayoutObject* object, const LayoutBoxModelObject* ancestor, TransformState& transformState)
{
    transformState.applyTransform(object->localToSVGParentTransform());

    LayoutObject* parent = object->parent();

    // At the SVG/HTML boundary (aka LayoutSVGRoot), we apply the localToBorderBoxTransform
    // to map an element from SVG viewport coordinates to CSS box coordinates.
    // LayoutSVGRoot's mapLocalToAncestor method expects CSS box coordinates.
    if (parent->isSVGRoot())
        transformState.applyTransform(toLayoutSVGRoot(parent)->localToBorderBoxTransform());

    MapCoordinatesFlags mode = UseTransforms;
    parent->mapLocalToAncestor(ancestor, transformState, mode);
}

void SVGLayoutSupport::mapAncestorToLocal(const LayoutObject& object, const LayoutBoxModelObject* ancestor, TransformState& transformState)
{
    // |object| is either a LayoutSVGModelObject or a LayoutSVGBlock here. In
    // the former case, |object| can never be an ancestor while in the latter
    // the caller is responsible for doing the ancestor check. Because of this,
    // computing the transform to the SVG root is always what we want to do here.
    ASSERT(ancestor != &object);
    ASSERT(object.isSVGContainer() || object.isSVGShape() || object.isSVGImage() || object.isSVGText() || object.isSVGForeignObject());
    AffineTransform localToSVGRoot;
    const LayoutSVGRoot& svgRoot = computeTransformToSVGRoot(object, localToSVGRoot);

    MapCoordinatesFlags mode = UseTransforms | ApplyContainerFlip;
    svgRoot.mapAncestorToLocal(ancestor, transformState, mode);

    transformState.applyTransform(localToSVGRoot);
}

const LayoutObject* SVGLayoutSupport::pushMappingToContainer(const LayoutObject* object, const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap& geometryMap)
{
    ASSERT_UNUSED(ancestorToStopAt, ancestorToStopAt != object);

    LayoutObject* parent = object->parent();

    // At the SVG/HTML boundary (aka LayoutSVGRoot), we apply the localToBorderBoxTransform
    // to map an element from SVG viewport coordinates to CSS box coordinates.
    // LayoutSVGRoot's mapLocalToAncestor method expects CSS box coordinates.
    if (parent->isSVGRoot()) {
        TransformationMatrix matrix(object->localToSVGParentTransform());
        matrix.multiply(toLayoutSVGRoot(parent)->localToBorderBoxTransform());
        geometryMap.push(object, matrix);
    } else {
        geometryMap.push(object, object->localToSVGParentTransform());
    }

    return parent;
}

// Update a bounding box taking into account the validity of the other bounding box.
inline void SVGLayoutSupport::updateObjectBoundingBox(FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, LayoutObject* other, FloatRect otherBoundingBox)
{
    bool otherValid = other->isSVGContainer() ? toLayoutSVGContainer(other)->isObjectBoundingBoxValid() : true;
    if (!otherValid)
        return;

    if (!objectBoundingBoxValid) {
        objectBoundingBox = otherBoundingBox;
        objectBoundingBoxValid = true;
        return;
    }

    objectBoundingBox.uniteEvenIfEmpty(otherBoundingBox);
}

void SVGLayoutSupport::computeContainerBoundingBoxes(const LayoutObject* container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& paintInvalidationBoundingBox)
{
    objectBoundingBox = FloatRect();
    objectBoundingBoxValid = false;
    strokeBoundingBox = FloatRect();

    // When computing the strokeBoundingBox, we use the paintInvalidationRects of the container's children so that the container's stroke includes
    // the resources applied to the children (such as clips and filters). This allows filters applied to containers to correctly bound
    // the children, and also improves inlining of SVG content, as the stroke bound is used in that situation also.
    for (LayoutObject* current = container->slowFirstChild(); current; current = current->nextSibling()) {
        if (current->isSVGHiddenContainer())
            continue;

        // Don't include elements in the union that do not layout.
        if (current->isSVGShape() && toLayoutSVGShape(current)->isShapeEmpty())
            continue;

        if (current->isSVGText() && !toLayoutSVGText(current)->isObjectBoundingBoxValid())
            continue;

        const AffineTransform& transform = current->localToSVGParentTransform();
        updateObjectBoundingBox(objectBoundingBox, objectBoundingBoxValid, current,
            transform.mapRect(current->objectBoundingBox()));
        strokeBoundingBox.unite(transform.mapRect(current->paintInvalidationRectInLocalSVGCoordinates()));
    }

    paintInvalidationBoundingBox = strokeBoundingBox;
}

const LayoutSVGRoot* SVGLayoutSupport::findTreeRootObject(const LayoutObject* start)
{
    while (start && !start->isSVGRoot())
        start = start->parent();

    ASSERT(start);
    ASSERT(start->isSVGRoot());
    return toLayoutSVGRoot(start);
}

bool SVGLayoutSupport::layoutSizeOfNearestViewportChanged(const LayoutObject* start)
{
    for (; start; start = start->parent()) {
        if (start->isSVGRoot())
            return toLayoutSVGRoot(start)->isLayoutSizeChanged();
        if (start->isSVGViewportContainer())
            return toLayoutSVGViewportContainer(start)->isLayoutSizeChanged();
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool SVGLayoutSupport::screenScaleFactorChanged(const LayoutObject* ancestor)
{
    for (; ancestor; ancestor = ancestor->parent()) {
        if (ancestor->isSVGRoot())
            return toLayoutSVGRoot(ancestor)->didScreenScaleFactorChange();
        if (ancestor->isSVGTransformableContainer())
            return toLayoutSVGTransformableContainer(ancestor)->didScreenScaleFactorChange();
        if (ancestor->isSVGViewportContainer())
            return toLayoutSVGViewportContainer(ancestor)->didScreenScaleFactorChange();
    }
    ASSERT_NOT_REACHED();
    return false;
}

void SVGLayoutSupport::layoutChildren(
    LayoutObject* firstChild, bool forceLayout, bool screenScalingFactorChanged, bool layoutSizeChanged)
{
    for (LayoutObject* child = firstChild; child; child = child->nextSibling()) {
        bool forceChildLayout = forceLayout;

        if (screenScalingFactorChanged) {
            // If the screen scaling factor changed we need to update the text
            // metrics (note: this also happens for layoutSizeChanged=true).
            if (child->isSVGText())
                toLayoutSVGText(child)->setNeedsTextMetricsUpdate();
            forceChildLayout = true;
        }

        if (layoutSizeChanged) {
            // When selfNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths
            if (SVGElement* element = child->node()->isSVGElement() ? toSVGElement(child->node()) : 0) {
                if (element->hasRelativeLengths()) {
                    // FIXME: this should be done on invalidation, not during layout.
                    // When the layout size changed and when using relative values tell the LayoutSVGShape to update its shape object
                    if (child->isSVGShape()) {
                        toLayoutSVGShape(child)->setNeedsShapeUpdate();
                    } else if (child->isSVGText()) {
                        toLayoutSVGText(child)->setNeedsTextMetricsUpdate();
                        toLayoutSVGText(child)->setNeedsPositioningValuesUpdate();
                    }

                    forceChildLayout = true;
                }
            }
        }

        // Resource containers are nasty: they can invalidate clients outside the current SubtreeLayoutScope.
        // Since they only care about viewport size changes (to resolve their relative lengths), we trigger
        // their invalidation directly from SVGSVGElement::svgAttributeChange() or at a higher
        // SubtreeLayoutScope (in LayoutView::layout()). We do not create a SubtreeLayoutScope for
        // resources because their ability to reference each other leads to circular layout. We protect
        // against that within the layout code for resources, but it causes assertions if we use a
        // SubTreeLayoutScope for them.
        if (child->isSVGResourceContainer()) {
            // Lay out any referenced resources before the child.
            layoutResourcesIfNeeded(child);
            child->layoutIfNeeded();
        } else {
            SubtreeLayoutScope layoutScope(*child);
            if (forceChildLayout)
                layoutScope.setNeedsLayout(child, LayoutInvalidationReason::SvgChanged);

            // Lay out any referenced resources before the child.
            layoutResourcesIfNeeded(child);
            child->layoutIfNeeded();
        }
    }
}

void SVGLayoutSupport::layoutResourcesIfNeeded(const LayoutObject* object)
{
    ASSERT(object);

    SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(object);
    if (resources)
        resources->layoutIfNeeded();
}

bool SVGLayoutSupport::isOverflowHidden(const LayoutObject* object)
{
    // LayoutSVGRoot should never query for overflow state - it should always clip itself to the initial viewport size.
    ASSERT(!object->isDocumentElement());

    return object->style()->overflowX() == OverflowHidden || object->style()->overflowX() == OverflowScroll;
}

void SVGLayoutSupport::intersectPaintInvalidationRectWithResources(const LayoutObject* layoutObject, FloatRect& paintInvalidationRect)
{
    ASSERT(layoutObject);

    SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(layoutObject);
    if (!resources)
        return;

    if (LayoutSVGResourceFilter* filter = resources->filter())
        paintInvalidationRect = filter->resourceBoundingBox(layoutObject);

    if (LayoutSVGResourceClipper* clipper = resources->clipper())
        paintInvalidationRect.intersect(clipper->resourceBoundingBox(layoutObject));

    if (LayoutSVGResourceMasker* masker = resources->masker())
        paintInvalidationRect.intersect(masker->resourceBoundingBox(layoutObject));
}

bool SVGLayoutSupport::hasFilterResource(const LayoutObject& object)
{
    SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(&object);
    return resources && resources->filter();
}

bool SVGLayoutSupport::pointInClippingArea(const LayoutObject* object, const FloatPoint& point)
{
    ASSERT(object);

    // We just take clippers into account to determine if a point is on the node. The Specification may
    // change later and we also need to check maskers.
    SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(object);
    if (!resources)
        return true;

    if (LayoutSVGResourceClipper* clipper = resources->clipper())
        return clipper->hitTestClipContent(object->objectBoundingBox(), point);

    return true;
}

bool SVGLayoutSupport::transformToUserSpaceAndCheckClipping(const LayoutObject* object, const AffineTransform& localTransform, const FloatPoint& pointInParent, FloatPoint& localPoint)
{
    if (!localTransform.isInvertible())
        return false;
    localPoint = localTransform.inverse().mapPoint(pointInParent);
    return pointInClippingArea(object, localPoint);
}

DashArray SVGLayoutSupport::resolveSVGDashArray(const SVGDashArray& svgDashArray, const ComputedStyle& style, const SVGLengthContext& lengthContext)
{
    DashArray dashArray;
    for (const Length& dashLength : svgDashArray.vector())
        dashArray.append(lengthContext.valueForLength(dashLength, style));
    return dashArray;
}

void SVGLayoutSupport::applyStrokeStyleToStrokeData(StrokeData& strokeData, const ComputedStyle& style, const LayoutObject& object, float dashScaleFactor)
{
    ASSERT(object.node());
    ASSERT(object.node()->isSVGElement());

    const SVGComputedStyle& svgStyle = style.svgStyle();

    SVGLengthContext lengthContext(toSVGElement(object.node()));
    strokeData.setThickness(lengthContext.valueForLength(svgStyle.strokeWidth()));
    strokeData.setLineCap(svgStyle.capStyle());
    strokeData.setLineJoin(svgStyle.joinStyle());
    strokeData.setMiterLimit(svgStyle.strokeMiterLimit());

    DashArray dashArray = resolveSVGDashArray(*svgStyle.strokeDashArray(), style, lengthContext);
    float dashOffset = lengthContext.valueForLength(svgStyle.strokeDashOffset(), style);
    // Apply scaling from 'pathLength'.
    if (dashScaleFactor != 1) {
        ASSERT(dashScaleFactor >= 0);
        dashOffset *= dashScaleFactor;
        for (auto& dashItem : dashArray)
            dashItem *= dashScaleFactor;
    }
    strokeData.setLineDash(dashArray, dashOffset);
}

bool SVGLayoutSupport::isLayoutableTextNode(const LayoutObject* object)
{
    ASSERT(object->isText());
    // <br> is marked as text, but is not handled by the SVG layout code-path.
    return object->isSVGInlineText() && !toLayoutSVGInlineText(object)->hasEmptyText();
}

bool SVGLayoutSupport::willIsolateBlendingDescendantsForStyle(const ComputedStyle& style)
{
    const SVGComputedStyle& svgStyle = style.svgStyle();

    return style.hasIsolation() || style.opacity() < 1 || style.hasBlendMode()
        || style.hasFilter() || svgStyle.hasMasker() || svgStyle.hasClipper();
}

bool SVGLayoutSupport::willIsolateBlendingDescendantsForObject(const LayoutObject* object)
{
    if (object->isSVGHiddenContainer())
        return false;
    if (!object->isSVGRoot() && !object->isSVGContainer())
        return false;
    return willIsolateBlendingDescendantsForStyle(object->styleRef());
}

bool SVGLayoutSupport::isIsolationRequired(const LayoutObject* object)
{
    return willIsolateBlendingDescendantsForObject(object) && object->hasNonIsolatedBlendingDescendants();
}

AffineTransform::Transform SubtreeContentTransformScope::s_currentContentTransformation = IDENTITY_TRANSFORM;

SubtreeContentTransformScope::SubtreeContentTransformScope(const AffineTransform& subtreeContentTransformation)
    : m_savedContentTransformation(s_currentContentTransformation)
{
    AffineTransform contentTransformation = subtreeContentTransformation * AffineTransform(s_currentContentTransformation);
    contentTransformation.copyTransformTo(s_currentContentTransformation);
}

SubtreeContentTransformScope::~SubtreeContentTransformScope()
{
    m_savedContentTransformation.copyTransformTo(s_currentContentTransformation);
}

AffineTransform SVGLayoutSupport::deprecatedCalculateTransformToLayer(const LayoutObject* layoutObject)
{
    AffineTransform transform;
    while (layoutObject) {
        transform = layoutObject->localToSVGParentTransform() * transform;
        if (layoutObject->isSVGRoot())
            break;
        layoutObject = layoutObject->parent();
    }

    // Continue walking up the layer tree, accumulating CSS transforms.
    // FIXME: this queries layer compositing state - which is not
    // supported during layout. Hence, the result may not include all CSS transforms.
    PaintLayer* layer = layoutObject ? layoutObject->enclosingLayer() : 0;
    while (layer && layer->isAllowedToQueryCompositingState()) {
        // We can stop at compositing layers, to match the backing resolution.
        // FIXME: should we be computing the transform to the nearest composited layer,
        // or the nearest composited layer that does not paint into its ancestor?
        // I think this is the nearest composited ancestor since we will inherit its
        // transforms in the composited layer tree.
        if (layer->compositingState() != NotComposited)
            break;

        if (TransformationMatrix* layerTransform = layer->transform())
            transform = layerTransform->toAffineTransform() * transform;

        layer = layer->parent();
    }

    return transform;
}

float SVGLayoutSupport::calculateScreenFontSizeScalingFactor(const LayoutObject* layoutObject)
{
    ASSERT(layoutObject);

    // FIXME: trying to compute a device space transform at record time is wrong. All clients
    // should be updated to avoid relying on this information, and the method should be removed.
    AffineTransform ctm = deprecatedCalculateTransformToLayer(layoutObject) * SubtreeContentTransformScope::currentContentTransformation();
    ctm.scale(layoutObject->document().frameHost()->deviceScaleFactorDeprecated());

    return narrowPrecisionToFloat(sqrt((ctm.xScaleSquared() + ctm.yScaleSquared()) / 2));
}

static inline bool compareCandidateDistance(const SearchCandidate& r1, const SearchCandidate& r2)
{
    return r1.candidateDistance < r2.candidateDistance;
}

static inline float distanceToChildLayoutObject(LayoutObject* child, const FloatPoint& point)
{
    const AffineTransform& localToParentTransform = child->localToSVGParentTransform();
    if (!localToParentTransform.isInvertible())
        return std::numeric_limits<float>::max();
    FloatPoint childLocalPoint = localToParentTransform.inverse().mapPoint(point);
    return child->objectBoundingBox().squaredDistanceTo(childLocalPoint);
}

static SearchCandidate searchTreeForFindClosestLayoutSVGText(LayoutObject* layoutObject, const FloatPoint& point)
{
    // Try to find the closest LayoutSVGText.
    SearchCandidate closestText;
    Vector<SearchCandidate> candidates;

    // Find the closest LayoutSVGText on this tree level, and also collect any
    // containers that could contain LayoutSVGTexts that are closer.
    for (LayoutObject* child = layoutObject->slowLastChild(); child; child = child->previousSibling()) {
        if (child->isSVGText()) {
            float distance = distanceToChildLayoutObject(child, point);
            if (distance >= closestText.candidateDistance)
                continue;
            candidates.clear();
            closestText.candidateLayoutObject = child;
            closestText.candidateDistance = distance;
            continue;
        }

        if (child->isSVGContainer() && !layoutObject->isSVGHiddenContainer()) {
            float distance = distanceToChildLayoutObject(child, point);
            if (distance > closestText.candidateDistance)
                continue;
            candidates.append(SearchCandidate(child, distance));
        }
    }

    // If a LayoutSVGText was found and there are no potentially closer sub-trees,
    // just return |closestText|.
    if (closestText.candidateLayoutObject && candidates.isEmpty())
        return closestText;

    std::stable_sort(candidates.begin(), candidates.end(), compareCandidateDistance);

    // Find the closest LayoutSVGText in the sub-trees in |candidates|.
    // If a LayoutSVGText is found that is strictly closer than any previous candidate, then end the search.
    for (const SearchCandidate& searchCandidate : candidates) {
        if (closestText.candidateDistance < searchCandidate.candidateDistance)
            break;
        LayoutObject* candidateLayoutObject = searchCandidate.candidateLayoutObject;
        FloatPoint candidateLocalPoint = candidateLayoutObject->localToSVGParentTransform().inverse().mapPoint(point);

        SearchCandidate candidateText = searchTreeForFindClosestLayoutSVGText(candidateLayoutObject, candidateLocalPoint);

        if (candidateText.candidateDistance < closestText.candidateDistance)
            closestText = candidateText;
    }

    return closestText;
}

LayoutObject* SVGLayoutSupport::findClosestLayoutSVGText(LayoutObject* layoutObject, const FloatPoint& point)
{
    return searchTreeForFindClosestLayoutSVGText(layoutObject, point).candidateLayoutObject;
}

} // namespace blink
