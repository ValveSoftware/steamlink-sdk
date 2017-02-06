/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "core/svg/graphics/filters/SVGFilterBuilder.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/ElementTraversal.h"
#include "core/layout/LayoutObject.h"
#include "core/svg/SVGFilterElement.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/PaintFilterEffect.h"
#include "platform/graphics/filters/SourceAlpha.h"
#include "platform/graphics/filters/SourceGraphic.h"

namespace blink {

namespace {

class FilterInputKeywords {
public:
    static const AtomicString& getSourceGraphic()
    {
        DEFINE_STATIC_LOCAL(const AtomicString, s_sourceGraphicName, ("SourceGraphic"));
        return s_sourceGraphicName;
    }

    static const AtomicString& sourceAlpha()
    {
        DEFINE_STATIC_LOCAL(const AtomicString, s_sourceAlphaName, ("SourceAlpha"));
        return s_sourceAlphaName;
    }

    static const AtomicString& fillPaint()
    {
        DEFINE_STATIC_LOCAL(const AtomicString, s_fillPaintName, ("FillPaint"));
        return s_fillPaintName;
    }

    static const AtomicString& strokePaint()
    {
        DEFINE_STATIC_LOCAL(const AtomicString, s_strokePaintName, ("StrokePaint"));
        return s_strokePaintName;
    }
};

} // namespace

SVGFilterGraphNodeMap::SVGFilterGraphNodeMap()
{
}

void SVGFilterGraphNodeMap::addBuiltinEffect(FilterEffect* effect)
{
    m_effectReferences.add(effect, FilterEffectSet());
}

void SVGFilterGraphNodeMap::addPrimitive(LayoutObject* object, FilterEffect* effect)
{
    // The effect must be a newly created filter effect.
    ASSERT(!m_effectReferences.contains(effect));
    ASSERT(!object || !m_effectRenderer.contains(object));
    m_effectReferences.add(effect, FilterEffectSet());

    unsigned numberOfInputEffects = effect->inputEffects().size();

    // Add references from the inputs of this effect to the effect itself, to
    // allow determining what effects needs to be invalidated when a certain
    // effect changes.
    for (unsigned i = 0; i < numberOfInputEffects; ++i)
        effectReferences(effect->inputEffect(i)).add(effect);

    // If object is null, that means the element isn't attached for some
    // reason, which in turn mean that certain types of invalidation will not
    // work (the LayoutObject -> FilterEffect mapping will not be defined).
    if (object)
        m_effectRenderer.add(object, effect);
}

void SVGFilterGraphNodeMap::invalidateDependentEffects(FilterEffect* effect)
{
    if (!effect->hasImageFilter())
        return;

    effect->clearResult();

    FilterEffectSet& effectReferences = this->effectReferences(effect);
    for (FilterEffect* effectReference : effectReferences)
        invalidateDependentEffects(effectReference);
}

DEFINE_TRACE(SVGFilterGraphNodeMap)
{
    visitor->trace(m_effectRenderer);
    visitor->trace(m_effectReferences);
}

SVGFilterBuilder::SVGFilterBuilder(
    FilterEffect* sourceGraphic,
    SVGFilterGraphNodeMap* nodeMap,
    const SkPaint* fillPaint,
    const SkPaint* strokePaint)
    : m_nodeMap(nodeMap)
{
    FilterEffect* sourceGraphicRef = sourceGraphic;
    m_builtinEffects.add(FilterInputKeywords::getSourceGraphic(), sourceGraphicRef);
    m_builtinEffects.add(FilterInputKeywords::sourceAlpha(), SourceAlpha::create(sourceGraphicRef));
    if (fillPaint)
        m_builtinEffects.add(FilterInputKeywords::fillPaint(), PaintFilterEffect::create(sourceGraphicRef->getFilter(), *fillPaint));
    if (strokePaint)
        m_builtinEffects.add(FilterInputKeywords::strokePaint(), PaintFilterEffect::create(sourceGraphicRef->getFilter(), *strokePaint));
    addBuiltinEffects();
}

void SVGFilterBuilder::addBuiltinEffects()
{
    if (!m_nodeMap)
        return;
    for (const auto& entry : m_builtinEffects)
        m_nodeMap->addBuiltinEffect(entry.value.get());
}

// Returns the color-interpolation-filters property of the element.
static EColorInterpolation colorInterpolationForElement(SVGElement& element, EColorInterpolation parentColorInterpolation)
{
    if (const LayoutObject* layoutObject = element.layoutObject())
        return layoutObject->styleRef().svgStyle().colorInterpolationFilters();

    // No layout has been performed, try to determine the property value
    // "manually" (used by external SVG files.)
    if (const StylePropertySet* propertySet = element.presentationAttributeStyle()) {
        CSSValue* cssValue = propertySet->getPropertyCSSValue(CSSPropertyColorInterpolationFilters);
        if (cssValue && cssValue->isPrimitiveValue()) {
            const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(*cssValue);
            return primitiveValue.convertTo<EColorInterpolation>();
        }
    }
    // 'auto' is the default (per Filter Effects), but since the property is
    // inherited, propagate the parent's value.
    return parentColorInterpolation;
}

void SVGFilterBuilder::buildGraph(Filter* filter, SVGFilterElement& filterElement, const FloatRect& referenceBox)
{
    EColorInterpolation filterColorInterpolation = colorInterpolationForElement(filterElement, CI_AUTO);

    for (SVGElement* element = Traversal<SVGElement>::firstChild(filterElement); element; element = Traversal<SVGElement>::nextSibling(*element)) {
        if (!element->isFilterEffect())
            continue;

        SVGFilterPrimitiveStandardAttributes* effectElement = static_cast<SVGFilterPrimitiveStandardAttributes*>(element);
        FilterEffect* effect = effectElement->build(this, filter);
        if (!effect)
            continue;

        if (m_nodeMap)
            m_nodeMap->addPrimitive(effectElement->layoutObject(), effect);

        effectElement->setStandardAttributes(effect);
        effect->setEffectBoundaries(SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(effectElement, filterElement.primitiveUnits()->currentValue()->enumValue(), referenceBox));
        EColorInterpolation colorInterpolation = colorInterpolationForElement(*effectElement, filterColorInterpolation);
        effect->setOperatingColorSpace(colorInterpolation == CI_LINEARRGB ? ColorSpaceLinearRGB : ColorSpaceDeviceRGB);
        if (effectElement->taintsOrigin(effect->inputsTaintOrigin()))
            effect->setOriginTainted();

        add(AtomicString(effectElement->result()->currentValue()->value()), effect);
    }
}

void SVGFilterBuilder::add(const AtomicString& id, FilterEffect* effect)
{
    if (id.isEmpty()) {
        m_lastEffect = effect;
        return;
    }

    if (m_builtinEffects.contains(id))
        return;

    m_lastEffect = effect;
    m_namedEffects.set(id, m_lastEffect);
}

FilterEffect* SVGFilterBuilder::getEffectById(const AtomicString& id) const
{
    if (!id.isEmpty()) {
        if (FilterEffect* builtinEffect = m_builtinEffects.get(id))
            return builtinEffect;

        if (FilterEffect* namedEffect = m_namedEffects.get(id))
            return namedEffect;
    }

    if (m_lastEffect)
        return m_lastEffect.get();

    return m_builtinEffects.get(FilterInputKeywords::getSourceGraphic());
}

} // namespace blink
