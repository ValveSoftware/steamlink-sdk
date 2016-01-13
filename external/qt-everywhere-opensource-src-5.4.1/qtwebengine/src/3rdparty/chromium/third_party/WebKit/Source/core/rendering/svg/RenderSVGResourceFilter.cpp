/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "config.h"

#include "core/rendering/svg/RenderSVGResourceFilter.h"

#include "core/frame/Settings.h"
#include "core/rendering/svg/RenderSVGResourceFilterPrimitive.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SourceAlpha.h"
#include "platform/graphics/filters/SourceGraphic.h"

using namespace std;

namespace WebCore {

const RenderSVGResourceType RenderSVGResourceFilter::s_resourceType = FilterResourceType;

RenderSVGResourceFilter::RenderSVGResourceFilter(SVGFilterElement* node)
    : RenderSVGResourceContainer(node)
{
}

RenderSVGResourceFilter::~RenderSVGResourceFilter()
{
    m_filter.clear();
}

bool RenderSVGResourceFilter::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->isSVGResourceFilterPrimitive();
}

void RenderSVGResourceFilter::removeAllClientsFromCache(bool markForInvalidation)
{
    m_filter.clear();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceFilter::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);

    if (FilterData* filterData = m_filter.get(client)) {
        if (filterData->savedContext)
            filterData->state = FilterData::MarkedForRemoval;
        else
            m_filter.remove(client);
    }

    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

PassRefPtr<SVGFilterBuilder> RenderSVGResourceFilter::buildPrimitives(SVGFilter* filter)
{
    SVGFilterElement* filterElement = toSVGFilterElement(element());
    FloatRect targetBoundingBox = filter->targetBoundingBox();

    // Add effects to the builder
    RefPtr<SVGFilterBuilder> builder = SVGFilterBuilder::create(SourceGraphic::create(filter), SourceAlpha::create(filter));
    for (SVGElement* element = Traversal<SVGElement>::firstChild(*filterElement); element; element = Traversal<SVGElement>::nextSibling(*element)) {
        if (!element->isFilterEffect() || !element->renderer())
            continue;

        SVGFilterPrimitiveStandardAttributes* effectElement = static_cast<SVGFilterPrimitiveStandardAttributes*>(element);
        RefPtr<FilterEffect> effect = effectElement->build(builder.get(), filter);
        if (!effect) {
            builder->clearEffects();
            return nullptr;
        }
        builder->appendEffectToEffectReferences(effect, effectElement->renderer());
        effectElement->setStandardAttributes(effect.get());
        effect->setEffectBoundaries(SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(effectElement, filterElement->primitiveUnits()->currentValue()->enumValue(), targetBoundingBox));
        effect->setOperatingColorSpace(
            effectElement->renderer()->style()->svgStyle()->colorInterpolationFilters() == CI_LINEARRGB ? ColorSpaceLinearRGB : ColorSpaceDeviceRGB);
        builder->add(AtomicString(effectElement->result()->currentValue()->value()), effect);
    }
    return builder.release();
}

void RenderSVGResourceFilter::adjustScaleForMaximumImageSize(const FloatSize& size, FloatSize& filterScale)
{
    FloatSize scaledSize(size);
    scaledSize.scale(filterScale.width(), filterScale.height());
    float scaledArea = scaledSize.width() * scaledSize.height();

    if (scaledArea <= FilterEffect::maxFilterArea())
        return;

    // If area of scaled size is bigger than the upper limit, adjust the scale
    // to fit.
    filterScale.scale(sqrt(FilterEffect::maxFilterArea() / scaledArea));
}

static bool createImageBuffer(const Filter* filter, OwnPtr<ImageBuffer>& imageBuffer)
{
    IntRect paintRect = filter->sourceImageRect();
    // Don't create empty ImageBuffers.
    if (paintRect.isEmpty())
        return false;

    OwnPtr<ImageBufferSurface> surface = adoptPtr(new UnacceleratedImageBufferSurface(paintRect.size()));
    if (!surface->isValid())
        return false;
    OwnPtr<ImageBuffer> image = ImageBuffer::create(surface.release());

    GraphicsContext* imageContext = image->context();
    ASSERT(imageContext);

    imageContext->translate(-paintRect.x(), -paintRect.y());
    imageContext->concatCTM(filter->absoluteTransform());
    imageBuffer = image.release();
    return true;
}

static void beginDeferredFilter(GraphicsContext* context, FilterData* filterData, SVGFilterElement* filterElement)
{
    SkiaImageFilterBuilder builder(context);
    RefPtr<ImageFilter> imageFilter = builder.build(filterData->builder->lastEffect(), ColorSpaceDeviceRGB);
    // FIXME: Remove the cache when impl-size painting is enabled on every platform and the non impl-side painting path is removed
    if (!context->isRecordingCanvas()) // Recording canvases do not use the cache
        filterData->filter->enableCache();
    FloatRect boundaries = enclosingIntRect(filterData->boundaries);
    context->save();

    FloatSize deviceSize = context->getCTM().mapSize(boundaries.size());
    float scaledArea = deviceSize.width() * deviceSize.height();

    // If area of scaled size is bigger than the upper limit, adjust the scale
    // to fit. Note that this only really matters in the non-impl-side painting
    // case, since the impl-side case never allocates a full-sized backing
    // store, only tile-sized.
    // FIXME: remove this once all platforms are using impl-side painting.
    // crbug.com/169282.
    if (scaledArea > FilterEffect::maxFilterArea()) {
        float scale = sqrtf(FilterEffect::maxFilterArea() / scaledArea);
        context->scale(scale, scale);
    }
    // Clip drawing of filtered image to primitive boundaries.
    context->clipRect(boundaries);
    if (filterElement->hasAttribute(SVGNames::filterResAttr)) {
        // Get boundaries in device coords.
        // FIXME: See crbug.com/382491. Is the use of getCTM OK here, given it does not include device
        // zoom or High DPI adjustments?
        FloatSize size = context->getCTM().mapSize(boundaries.size());
        // Compute the scale amount required so that the resulting offscreen is exactly filterResX by filterResY pixels.
        float filterResScaleX = filterElement->filterResX()->currentValue()->value() / size.width();
        float filterResScaleY = filterElement->filterResY()->currentValue()->value() / size.height();
        // Scale the CTM so the primitive is drawn to filterRes.
        context->scale(filterResScaleX, filterResScaleY);
        // Create a resize filter with the inverse scale.
        AffineTransform resizeMatrix;
        resizeMatrix.scale(1 / filterResScaleX, 1 / filterResScaleY);
        imageFilter = builder.buildTransform(resizeMatrix, imageFilter.get());
    }
    // If the CTM contains rotation or shearing, apply the filter to
    // the unsheared/unrotated matrix, and do the shearing/rotation
    // as a final pass.
    AffineTransform ctm = context->getCTM();
    if (ctm.b() || ctm.c()) {
        AffineTransform scaleAndTranslate;
        scaleAndTranslate.translate(ctm.e(), ctm.f());
        scaleAndTranslate.scale(ctm.xScale(), ctm.yScale());
        ASSERT(scaleAndTranslate.isInvertible());
        AffineTransform shearAndRotate = scaleAndTranslate.inverse();
        shearAndRotate.multiply(ctm);
        context->setCTM(scaleAndTranslate);
        imageFilter = builder.buildTransform(shearAndRotate, imageFilter.get());
    }
    context->beginLayer(1, CompositeSourceOver, &boundaries, ColorFilterNone, imageFilter.get());
}

static void endDeferredFilter(GraphicsContext* context, FilterData* filterData)
{
    context->endLayer();
    context->restore();
    // FIXME: Remove the cache when impl-size painting is enabled on every platform and the non impl-side painting path is removed
    if (!context->isRecordingCanvas()) // Recording canvases do not use the cache
        filterData->filter->disableCache();
}

bool RenderSVGResourceFilter::applyResource(RenderObject* object, RenderStyle*, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);

    clearInvalidationMask();

    bool deferredFiltersEnabled = object->document().settings()->deferredFiltersEnabled();
    if (m_filter.contains(object)) {
        FilterData* filterData = m_filter.get(object);
        if (filterData->state == FilterData::PaintingSource || filterData->state == FilterData::Applying)
            filterData->state = FilterData::CycleDetected;
        if (deferredFiltersEnabled && filterData->state == FilterData::Built) {
            SVGFilterElement* filterElement = toSVGFilterElement(element());
            beginDeferredFilter(context, filterData, filterElement);
            return true;
        }
        return false; // Already built, or we're in a cycle, or we're marked for removal. Regardless, just do nothing more now.
    }

    OwnPtr<FilterData> filterData(adoptPtr(new FilterData));
    FloatRect targetBoundingBox = object->objectBoundingBox();

    SVGFilterElement* filterElement = toSVGFilterElement(element());
    filterData->boundaries = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement, filterElement->filterUnits()->currentValue()->enumValue(), targetBoundingBox);
    if (filterData->boundaries.isEmpty())
        return false;

    // Determine absolute transformation matrix for filter.
    AffineTransform absoluteTransform;
    SVGRenderingContext::calculateDeviceSpaceTransformation(object, absoluteTransform);
    if (!absoluteTransform.isInvertible())
        return false;

    // Filters cannot handle a full transformation, only scales in each direction.
    FloatSize filterScale;

    // Calculate the scale factor for the filter.
    // Also see http://www.w3.org/TR/SVG/filters.html#FilterEffectsRegion
    if (filterElement->hasAttribute(SVGNames::filterResAttr)) {
        //  If resolution is specified, scale to match it.
        filterScale = FloatSize(
            filterElement->filterResX()->currentValue()->value() / filterData->boundaries.width(),
            filterElement->filterResY()->currentValue()->value() / filterData->boundaries.height());
    } else {
        // Otherwise, use the scale of the absolute transform.
        filterScale = FloatSize(absoluteTransform.xScale(), absoluteTransform.yScale());
    }
    // The size of the scaled filter boundaries shouldn't be bigger than kMaxFilterSize.
    // Intermediate filters are limited by the filter boundaries so they can't be bigger than this.
    adjustScaleForMaximumImageSize(filterData->boundaries.size(), filterScale);

    filterData->drawingRegion = object->strokeBoundingBox();
    filterData->drawingRegion.intersect(filterData->boundaries);
    FloatRect absoluteDrawingRegion = filterData->drawingRegion;
    if (!deferredFiltersEnabled)
        absoluteDrawingRegion.scale(filterScale.width(), filterScale.height());

    IntRect intDrawingRegion = enclosingIntRect(absoluteDrawingRegion);

    // Create the SVGFilter object.
    bool primitiveBoundingBoxMode = filterElement->primitiveUnits()->currentValue()->enumValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    filterData->shearFreeAbsoluteTransform = AffineTransform();
    if (!deferredFiltersEnabled)
        filterData->shearFreeAbsoluteTransform.scale(filterScale.width(), filterScale.height());
    filterData->filter = SVGFilter::create(filterData->shearFreeAbsoluteTransform, intDrawingRegion, targetBoundingBox, filterData->boundaries, primitiveBoundingBoxMode);

    // Create all relevant filter primitives.
    filterData->builder = buildPrimitives(filterData->filter.get());
    if (!filterData->builder)
        return false;

    FilterEffect* lastEffect = filterData->builder->lastEffect();
    if (!lastEffect)
        return false;

    lastEffect->determineFilterPrimitiveSubregion(ClipToFilterRegion);

    if (deferredFiltersEnabled) {
        FilterData* data = filterData.get();
        m_filter.set(object, filterData.release());
        beginDeferredFilter(context, data, filterElement);
        return true;
    }

    // If the drawingRegion is empty, we have something like <g filter=".."/>.
    // Even if the target objectBoundingBox() is empty, we still have to draw the last effect result image in postApplyResource.
    if (filterData->drawingRegion.isEmpty()) {
        ASSERT(!m_filter.contains(object));
        filterData->savedContext = context;
        m_filter.set(object, filterData.release());
        return false;
    }

    OwnPtr<ImageBuffer> sourceGraphic;
    if (!createImageBuffer(filterData->filter.get(), sourceGraphic)) {
        ASSERT(!m_filter.contains(object));
        filterData->savedContext = context;
        m_filter.set(object, filterData.release());
        return false;
    }

    GraphicsContext* sourceGraphicContext = sourceGraphic->context();
    ASSERT(sourceGraphicContext);

    filterData->sourceGraphicBuffer = sourceGraphic.release();
    filterData->savedContext = context;

    context = sourceGraphicContext;

    ASSERT(!m_filter.contains(object));
    m_filter.set(object, filterData.release());

    return true;
}

void RenderSVGResourceFilter::postApplyResource(RenderObject* object, GraphicsContext*& context, unsigned short resourceMode, const Path*, const RenderSVGShape*)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);

    FilterData* filterData = m_filter.get(object);
    if (!filterData)
        return;

    if (object->document().settings()->deferredFiltersEnabled() && (filterData->state == FilterData::PaintingSource || filterData->state == FilterData::Built)) {
        endDeferredFilter(context, filterData);
        filterData->state = FilterData::Built;
        return;
    }

    switch (filterData->state) {
    case FilterData::MarkedForRemoval:
        m_filter.remove(object);
        return;

    case FilterData::CycleDetected:
    case FilterData::Applying:
        // We have a cycle if we are already applying the data.
        // This can occur due to FeImage referencing a source that makes use of the FEImage itself.
        // This is the first place we've hit the cycle, so set the state back to PaintingSource so the return stack
        // will continue correctly.
        filterData->state = FilterData::PaintingSource;
        return;

    case FilterData::PaintingSource:
        if (!filterData->savedContext) {
            removeClientFromCache(object);
            return;
        }

        context = filterData->savedContext;
        filterData->savedContext = 0;
        break;

    case FilterData::Built: { } // Empty
    }

    FilterEffect* lastEffect = filterData->builder->lastEffect();

    if (lastEffect && !filterData->boundaries.isEmpty() && !lastEffect->filterPrimitiveSubregion().isEmpty()) {
        // This is the real filtering of the object. It just needs to be called on the
        // initial filtering process. We just take the stored filter result on a
        // second drawing.
        if (filterData->state != FilterData::Built)
            filterData->filter->setSourceImage(filterData->sourceGraphicBuffer.release());

        // Always true if filterData is just built (filterData->state == FilterData::Built).
        if (!lastEffect->hasResult()) {
            filterData->state = FilterData::Applying;
            lastEffect->apply();
            lastEffect->correctFilterResultIfNeeded();
            lastEffect->transformResultColorSpace(ColorSpaceDeviceRGB);
        }
        filterData->state = FilterData::Built;

        ImageBuffer* resultImage = lastEffect->asImageBuffer();
        if (resultImage) {
            context->drawImageBuffer(resultImage, filterData->filter->mapAbsoluteRectToLocalRect(lastEffect->absolutePaintRect()));
        }
    }
    filterData->sourceGraphicBuffer.clear();
}

FloatRect RenderSVGResourceFilter::resourceBoundingBox(const RenderObject* object)
{
    if (SVGFilterElement* element = toSVGFilterElement(this->element()))
        return SVGLengthContext::resolveRectangle<SVGFilterElement>(element, element->filterUnits()->currentValue()->enumValue(), object->objectBoundingBox());

    return FloatRect();
}

void RenderSVGResourceFilter::primitiveAttributeChanged(RenderObject* object, const QualifiedName& attribute)
{
    FilterMap::iterator it = m_filter.begin();
    FilterMap::iterator end = m_filter.end();
    SVGFilterPrimitiveStandardAttributes* primitve = static_cast<SVGFilterPrimitiveStandardAttributes*>(object->node());

    for (; it != end; ++it) {
        FilterData* filterData = it->value.get();
        if (filterData->state != FilterData::Built)
            continue;

        SVGFilterBuilder* builder = filterData->builder.get();
        FilterEffect* effect = builder->effectByRenderer(object);
        if (!effect)
            continue;
        // Since all effects shares the same attribute value, all
        // or none of them will be changed.
        if (!primitve->setFilterEffectAttribute(effect, attribute))
            return;
        builder->clearResultsRecursive(effect);

        // Repaint the image on the screen.
        markClientForInvalidation(it->key, RepaintInvalidation);
    }
    markAllClientLayersForInvalidation();
}

FloatRect RenderSVGResourceFilter::drawingRegion(RenderObject* object) const
{
    FilterData* filterData = m_filter.get(object);
    return filterData ? filterData->drawingRegion : FloatRect();
}

}
