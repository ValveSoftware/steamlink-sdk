/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "core/layout/svg/LayoutSVGImage.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGImagePainter.h"
#include "core/svg/SVGImageElement.h"
#include "platform/LengthFunctions.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

LayoutSVGImage::LayoutSVGImage(SVGImageElement* impl)
    : LayoutSVGModelObject(impl)
    , m_needsBoundariesUpdate(true)
    , m_needsTransformUpdate(true)
    , m_imageResource(LayoutImageResource::create())
{
    m_imageResource->initialize(this);
}

LayoutSVGImage::~LayoutSVGImage()
{
}

void LayoutSVGImage::willBeDestroyed()
{
    ImageQualityController::remove(*this);
    m_imageResource->shutdown();
    LayoutSVGModelObject::willBeDestroyed();
}

void LayoutSVGImage::updateBoundingBox()
{
    FloatRect oldBoundaries = m_objectBoundingBox;

    SVGLengthContext lengthContext(element());
    m_objectBoundingBox = FloatRect(
        lengthContext.valueForLength(styleRef().svgStyle().x(), styleRef(), SVGLengthMode::Width),
        lengthContext.valueForLength(styleRef().svgStyle().y(), styleRef(), SVGLengthMode::Height),
        lengthContext.valueForLength(styleRef().width(), styleRef(), SVGLengthMode::Width),
        lengthContext.valueForLength(styleRef().height(), styleRef(), SVGLengthMode::Height));
    m_needsBoundariesUpdate |= oldBoundaries != m_objectBoundingBox;
}

void LayoutSVGImage::layout()
{
    ASSERT(needsLayout());
    LayoutAnalyzer::Scope analyzer(*this);

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    updateBoundingBox();

    bool updateParentBoundaries = false;
    if (m_needsTransformUpdate) {
        m_localTransform = toSVGImageElement(element())->calculateAnimatedLocalTransform();
        m_needsTransformUpdate = false;
        updateParentBoundaries = true;
    }

    if (m_needsBoundariesUpdate) {
        m_paintInvalidationBoundingBox = m_objectBoundingBox;
        SVGLayoutSupport::intersectPaintInvalidationRectWithResources(this, m_paintInvalidationBoundingBox);
        m_needsBoundariesUpdate = false;
        updateParentBoundaries = true;
    }

    // If our bounds changed, notify the parents.
    if (updateParentBoundaries)
        LayoutSVGModelObject::setNeedsBoundariesUpdate();

    ASSERT(!m_needsBoundariesUpdate);
    ASSERT(!m_needsTransformUpdate);
    clearNeedsLayout();
}

void LayoutSVGImage::paint(const PaintInfo& paintInfo, const LayoutPoint&) const
{
    SVGImagePainter(*this).paint(paintInfo);
}

bool LayoutSVGImage::nodeAtFloatPoint(HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_IMAGE_HITTESTING, result.hitTestRequest(), style()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        FloatPoint localPoint;
        if (!SVGLayoutSupport::transformToUserSpaceAndCheckClipping(this, localToSVGParentTransform(), pointInParent, localPoint))
            return false;

        if (hitRules.canHitFill || hitRules.canHitBoundingBox) {
            if (m_objectBoundingBox.contains(localPoint)) {
                const LayoutPoint& localLayoutPoint = roundedLayoutPoint(localPoint);
                updateHitTestResult(result, localLayoutPoint);
                if (result.addNodeToListBasedTestResult(element(), localLayoutPoint) == StopHitTesting)
                    return true;
            }
        }
    }

    return false;
}

void LayoutSVGImage::imageChanged(WrappedImagePtr, const IntRect*)
{
    // Notify parent resources that we've changed. This also invalidates
    // references from resources (filters) that may have a cached
    // representation of this image/layout object.
    LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(this, false);

    setShouldDoFullPaintInvalidation();
}

void LayoutSVGImage::addOutlineRects(Vector<LayoutRect>& rects, const LayoutPoint&, IncludeBlockVisualOverflowOrNot) const
{
    // this is called from paint() after the localTransform has already been applied
    rects.append(LayoutRect(paintInvalidationRectInLocalSVGCoordinates()));
}

} // namespace blink
