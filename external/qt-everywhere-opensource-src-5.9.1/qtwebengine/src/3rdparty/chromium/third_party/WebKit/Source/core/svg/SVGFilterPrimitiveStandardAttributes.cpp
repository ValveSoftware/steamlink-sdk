/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"

#include "core/SVGNames.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/LayoutSVGResourceFilterPrimitive.h"
#include "core/svg/SVGLength.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace blink {

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(
    const QualifiedName& tagName,
    Document& document)
    : SVGElement(tagName, document),
      m_x(SVGAnimatedLength::create(this,
                                    SVGNames::xAttr,
                                    SVGLength::create(SVGLengthMode::Width))),
      m_y(SVGAnimatedLength::create(this,
                                    SVGNames::yAttr,
                                    SVGLength::create(SVGLengthMode::Height))),
      m_width(
          SVGAnimatedLength::create(this,
                                    SVGNames::widthAttr,
                                    SVGLength::create(SVGLengthMode::Width))),
      m_height(
          SVGAnimatedLength::create(this,
                                    SVGNames::heightAttr,
                                    SVGLength::create(SVGLengthMode::Height))),
      m_result(SVGAnimatedString::create(this, SVGNames::resultAttr)) {
  // Spec: If the x/y attribute is not specified, the effect is as if a value of
  // "0%" were specified.
  m_x->setDefaultValueAsString("0%");
  m_y->setDefaultValueAsString("0%");

  // Spec: If the width/height attribute is not specified, the effect is as if a
  // value of "100%" were specified.
  m_width->setDefaultValueAsString("100%");
  m_height->setDefaultValueAsString("100%");

  addToPropertyMap(m_x);
  addToPropertyMap(m_y);
  addToPropertyMap(m_width);
  addToPropertyMap(m_height);
  addToPropertyMap(m_result);
}

DEFINE_TRACE(SVGFilterPrimitiveStandardAttributes) {
  visitor->trace(m_x);
  visitor->trace(m_y);
  visitor->trace(m_width);
  visitor->trace(m_height);
  visitor->trace(m_result);
  SVGElement::trace(visitor);
}

bool SVGFilterPrimitiveStandardAttributes::setFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attrName) {
  DCHECK(attrName == SVGNames::color_interpolation_filtersAttr);
  DCHECK(layoutObject());
  EColorInterpolation colorInterpolation =
      layoutObject()->styleRef().svgStyle().colorInterpolationFilters();
  ColorSpace resolvedColorSpace =
      SVGFilterBuilder::resolveColorSpace(colorInterpolation);
  if (resolvedColorSpace == effect->operatingColorSpace())
    return false;
  effect->setOperatingColorSpace(resolvedColorSpace);
  return true;
}

void SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(
    const QualifiedName& attrName) {
  if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
      attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr ||
      attrName == SVGNames::resultAttr) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    invalidate();
    return;
  }

  SVGElement::svgAttributeChanged(attrName);
}

void SVGFilterPrimitiveStandardAttributes::childrenChanged(
    const ChildrenChange& change) {
  SVGElement::childrenChanged(change);

  if (!change.byParser)
    invalidate();
}

static FloatRect defaultFilterPrimitiveSubregion(FilterEffect* filterEffect) {
  // https://drafts.fxtf.org/filters/#FilterPrimitiveSubRegion
  DCHECK(filterEffect->getFilter());

  // <feTurbulence>, <feFlood> and <feImage> don't have input effects, so use
  // the filter region as default subregion. <feTile> does have an input
  // reference, but due to its function (and special-cases) its default
  // resolves to the filter region.
  if (filterEffect->getFilterEffectType() == FilterEffectTypeTile ||
      !filterEffect->numberOfEffectInputs())
    return filterEffect->getFilter()->filterRegion();

  // "x, y, width and height default to the union (i.e., tightest fitting
  // bounding box) of the subregions defined for all referenced nodes."
  FloatRect subregionUnion;
  for (const auto& inputEffect : filterEffect->inputEffects()) {
    // "If ... one or more of the referenced nodes is a standard input
    // ... the default subregion is 0%, 0%, 100%, 100%, where as a
    // special-case the percentages are relative to the dimensions of the
    // filter region..."
    if (inputEffect->getFilterEffectType() == FilterEffectTypeSourceInput)
      return filterEffect->getFilter()->filterRegion();
    subregionUnion.unite(inputEffect->filterPrimitiveSubregion());
  }
  return subregionUnion;
}

void SVGFilterPrimitiveStandardAttributes::setStandardAttributes(
    FilterEffect* filterEffect,
    SVGUnitTypes::SVGUnitType primitiveUnits,
    const FloatRect& referenceBox) const {
  DCHECK(filterEffect);

  FloatRect subregion = defaultFilterPrimitiveSubregion(filterEffect);
  FloatRect primitiveBoundaries =
      SVGLengthContext::resolveRectangle(this, primitiveUnits, referenceBox);

  if (x()->isSpecified())
    subregion.setX(primitiveBoundaries.x());
  if (y()->isSpecified())
    subregion.setY(primitiveBoundaries.y());
  if (width()->isSpecified())
    subregion.setWidth(primitiveBoundaries.width());
  if (height()->isSpecified())
    subregion.setHeight(primitiveBoundaries.height());

  filterEffect->setFilterPrimitiveSubregion(subregion);
}

LayoutObject* SVGFilterPrimitiveStandardAttributes::createLayoutObject(
    const ComputedStyle&) {
  return new LayoutSVGResourceFilterPrimitive(this);
}

bool SVGFilterPrimitiveStandardAttributes::layoutObjectIsNeeded(
    const ComputedStyle& style) {
  if (isSVGFilterElement(parentNode()))
    return SVGElement::layoutObjectIsNeeded(style);

  return false;
}

void SVGFilterPrimitiveStandardAttributes::invalidate() {
  if (LayoutObject* primitiveLayoutObject = layoutObject())
    markForLayoutAndParentResourceInvalidation(primitiveLayoutObject);
}

void SVGFilterPrimitiveStandardAttributes::primitiveAttributeChanged(
    const QualifiedName& attribute) {
  if (LayoutObject* primitiveLayoutObject = layoutObject())
    static_cast<LayoutSVGResourceFilterPrimitive*>(primitiveLayoutObject)
        ->primitiveAttributeChanged(attribute);
}

void invalidateFilterPrimitiveParent(SVGElement* element) {
  if (!element)
    return;

  ContainerNode* parent = element->parentNode();

  if (!parent)
    return;

  LayoutObject* layoutObject = parent->layoutObject();
  if (!layoutObject || !layoutObject->isSVGResourceFilterPrimitive())
    return;

  LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(
      layoutObject, false);
}

}  // namespace blink
