/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright 2014 The Chromium Authors. All rights reserved.
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

#include "core/layout/svg/LayoutSVGResourcePattern.h"

#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGPatternElement.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

struct PatternData {
    USING_FAST_MALLOC(PatternData);
public:
    RefPtr<Pattern> pattern;
    AffineTransform transform;
};

LayoutSVGResourcePattern::LayoutSVGResourcePattern(SVGPatternElement* node)
    : LayoutSVGResourcePaintServer(node)
    , m_shouldCollectPatternAttributes(true)
    , m_attributesWrapper(PatternAttributesWrapper::create())
{
}

void LayoutSVGResourcePattern::removeAllClientsFromCache(bool markForInvalidation)
{
    m_patternMap.clear();
    m_shouldCollectPatternAttributes = true;
    markAllClientsForInvalidation(markForInvalidation ? PaintInvalidation : ParentOnlyInvalidation);
}

void LayoutSVGResourcePattern::removeClientFromCache(LayoutObject* client, bool markForInvalidation)
{
    ASSERT(client);
    m_patternMap.remove(client);
    markClientForInvalidation(client, markForInvalidation ? PaintInvalidation : ParentOnlyInvalidation);
}

PatternData* LayoutSVGResourcePattern::patternForLayoutObject(const LayoutObject& object)
{
    ASSERT(!m_shouldCollectPatternAttributes);

    // FIXME: the double hash lookup is needed to guard against paint-time invalidation
    // (painting animated images may trigger layout invals which delete our map entry).
    // Hopefully that will be addressed at some point, and then we can optimize the lookup.
    if (PatternData* currentData = m_patternMap.get(&object))
        return currentData;

    return m_patternMap.set(&object, buildPatternData(object)).storedValue->value.get();
}

std::unique_ptr<PatternData> LayoutSVGResourcePattern::buildPatternData(const LayoutObject& object)
{
    // If we couldn't determine the pattern content element root, stop here.
    const PatternAttributes& attributes = this->attributes();
    if (!attributes.patternContentElement())
        return nullptr;

    // An empty viewBox disables layout.
    if (attributes.hasViewBox() && attributes.viewBox().isEmpty())
        return nullptr;

    ASSERT(element());
    // Compute tile metrics.
    FloatRect clientBoundingBox = object.objectBoundingBox();
    FloatRect tileBounds = SVGLengthContext::resolveRectangle(element(),
        attributes.patternUnits(), clientBoundingBox,
        *attributes.x(), *attributes.y(), *attributes.width(), *attributes.height());
    if (tileBounds.isEmpty())
        return nullptr;

    AffineTransform tileTransform;
    if (attributes.hasViewBox()) {
        if (attributes.viewBox().isEmpty())
            return nullptr;
        tileTransform = SVGFitToViewBox::viewBoxToViewTransform(attributes.viewBox(),
            attributes.preserveAspectRatio(), tileBounds.width(), tileBounds.height());
    } else {
        // A viewbox overrides patternContentUnits, per spec.
        if (attributes.patternContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
            tileTransform.scale(clientBoundingBox.width(), clientBoundingBox.height());
    }

    std::unique_ptr<PatternData> patternData = wrapUnique(new PatternData);
    patternData->pattern = Pattern::createPicturePattern(asPicture(tileBounds, tileTransform));

    // Compute pattern space transformation.
    patternData->transform.translate(tileBounds.x(), tileBounds.y());
    patternData->transform.preMultiply(attributes.patternTransform());

    return patternData;
}

SVGPaintServer LayoutSVGResourcePattern::preparePaintServer(const LayoutObject& object)
{
    clearInvalidationMask();

    SVGPatternElement* patternElement = toSVGPatternElement(element());
    if (!patternElement)
        return SVGPaintServer::invalid();

    if (m_shouldCollectPatternAttributes) {
        patternElement->synchronizeAnimatedSVGAttribute(anyQName());

        m_attributesWrapper->set(PatternAttributes());
        patternElement->collectPatternAttributes(mutableAttributes());
        m_shouldCollectPatternAttributes = false;
    }

    // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
    // then the given effect (e.g. a gradient or a filter) will be ignored.
    FloatRect objectBoundingBox = object.objectBoundingBox();
    if (attributes().patternUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && objectBoundingBox.isEmpty())
        return SVGPaintServer::invalid();

    PatternData* patternData = patternForLayoutObject(object);
    if (!patternData || !patternData->pattern)
        return SVGPaintServer::invalid();

    return SVGPaintServer(patternData->pattern, patternData->transform);
}

const LayoutSVGResourceContainer* LayoutSVGResourcePattern::resolveContentElement() const
{
    ASSERT(attributes().patternContentElement());
    LayoutSVGResourceContainer* expectedLayoutObject =
        toLayoutSVGResourceContainer(attributes().patternContentElement()->layoutObject());
    // No content inheritance - avoid walking the inheritance chain.
    if (this == expectedLayoutObject)
        return this;
    // Walk the inheritance chain on the LayoutObject-side. If we reach the
    // expected LayoutObject, all is fine. If we don't, there's a cycle that
    // the cycle resolver did break, and the resource will be content-less.
    const LayoutSVGResourceContainer* contentLayoutObject = this;
    while (SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(contentLayoutObject)) {
        LayoutSVGResourceContainer* linkedResource = resources->linkedResource();
        if (!linkedResource)
            break;
        if (linkedResource == expectedLayoutObject)
            return expectedLayoutObject;
        contentLayoutObject = linkedResource;
    }
    // There was a cycle, just use this resource as the "content resource" even
    // though it will be empty (have no children).
    return this;
}

PassRefPtr<SkPicture> LayoutSVGResourcePattern::asPicture(const FloatRect& tileBounds,
    const AffineTransform& tileTransform) const
{
    ASSERT(!m_shouldCollectPatternAttributes);

    AffineTransform contentTransform;
    if (attributes().patternContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        contentTransform = tileTransform;

    FloatRect bounds(FloatPoint(), tileBounds.size());
    SkPictureBuilder pictureBuilder(bounds);

    const LayoutSVGResourceContainer* patternLayoutObject = resolveContentElement();
    ASSERT(patternLayoutObject && !patternLayoutObject->needsLayout());

    SubtreeContentTransformScope contentTransformScope(contentTransform);

    {
        TransformRecorder transformRecorder(pictureBuilder.context(), *patternLayoutObject, tileTransform);
        for (LayoutObject* child = patternLayoutObject->firstChild(); child; child = child->nextSibling())
            SVGPaintContext::paintSubtree(pictureBuilder.context(), child);
    }

    return pictureBuilder.endRecording();
}

} // namespace blink
