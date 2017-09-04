/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/filters/FilterEffect.h"

#include "platform/graphics/filters/Filter.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"

namespace blink {

FilterEffect::FilterEffect(Filter* filter)
    : m_filter(filter),
      m_clipsToBounds(true),
      m_originTainted(false),
      m_operatingColorSpace(ColorSpaceLinearRGB) {
  ASSERT(m_filter);
}

FilterEffect::~FilterEffect() {}

DEFINE_TRACE(FilterEffect) {
  visitor->trace(m_inputEffects);
  visitor->trace(m_filter);
}

FloatRect FilterEffect::absoluteBounds() const {
  FloatRect computedBounds = getFilter()->filterRegion();
  if (!filterPrimitiveSubregion().isEmpty())
    computedBounds.intersect(filterPrimitiveSubregion());
  return getFilter()->mapLocalRectToAbsoluteRect(computedBounds);
}

FloatRect FilterEffect::mapInputs(const FloatRect& rect) const {
  if (!m_inputEffects.size()) {
    if (clipsToBounds())
      return absoluteBounds();
    return rect;
  }
  FloatRect inputUnion;
  for (const auto& effect : m_inputEffects)
    inputUnion.unite(effect->mapRect(rect));
  return inputUnion;
}

FloatRect FilterEffect::mapEffect(const FloatRect& rect) const {
  return rect;
}

FloatRect FilterEffect::applyBounds(const FloatRect& rect) const {
  // Filters in SVG clip to primitive subregion, while CSS doesn't.
  if (!clipsToBounds())
    return rect;
  FloatRect bounds = absoluteBounds();
  if (affectsTransparentPixels())
    return bounds;
  return intersection(rect, bounds);
}

FloatRect FilterEffect::mapRect(const FloatRect& rect) const {
  FloatRect result = mapInputs(rect);
  result = mapEffect(result);
  return applyBounds(result);
}

FilterEffect* FilterEffect::inputEffect(unsigned number) const {
  SECURITY_DCHECK(number < m_inputEffects.size());
  return m_inputEffects.at(number).get();
}

void FilterEffect::clearResult() {
  for (int i = 0; i < 4; i++)
    m_imageFilters[i] = nullptr;
}

Color FilterEffect::adaptColorToOperatingColorSpace(const Color& deviceColor) {
  // |deviceColor| is assumed to be DeviceRGB.
  return ColorSpaceUtilities::convertColor(deviceColor, operatingColorSpace());
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts, int) const {
  // FIXME: We should dump the subRegions of the filter primitives here later.
  // This isn't possible at the moment, because we need more detailed
  // information from the target object.
  return ts;
}

sk_sp<SkImageFilter> FilterEffect::createImageFilter() {
  return nullptr;
}

sk_sp<SkImageFilter> FilterEffect::createImageFilterWithoutValidation() {
  return createImageFilter();
}

bool FilterEffect::inputsTaintOrigin() const {
  for (const Member<FilterEffect>& effect : m_inputEffects) {
    if (effect->originTainted())
      return true;
  }
  return false;
}

sk_sp<SkImageFilter> FilterEffect::createTransparentBlack() const {
  SkImageFilter::CropRect rect = getCropRect();
  sk_sp<SkColorFilter> colorFilter =
      SkColorFilter::MakeModeFilter(0, SkBlendMode::kClear);
  return SkColorFilterImageFilter::Make(std::move(colorFilter), nullptr, &rect);
}

SkImageFilter::CropRect FilterEffect::getCropRect() const {
  if (!filterPrimitiveSubregion().isEmpty()) {
    FloatRect rect =
        getFilter()->mapLocalRectToAbsoluteRect(filterPrimitiveSubregion());
    return SkImageFilter::CropRect(rect);
  } else {
    return SkImageFilter::CropRect(SkRect::MakeEmpty(), 0);
  }
}

static int getImageFilterIndex(ColorSpace colorSpace,
                               bool requiresPMColorValidation) {
  // Map the (colorspace, bool) tuple to an integer index as follows:
  // 0 == linear colorspace, no PM validation
  // 1 == device colorspace, no PM validation
  // 2 == linear colorspace, PM validation
  // 3 == device colorspace, PM validation
  return (colorSpace == ColorSpaceLinearRGB ? 0x1 : 0x0) |
         (requiresPMColorValidation ? 0x2 : 0x0);
}

SkImageFilter* FilterEffect::getImageFilter(
    ColorSpace colorSpace,
    bool requiresPMColorValidation) const {
  int index = getImageFilterIndex(colorSpace, requiresPMColorValidation);
  return m_imageFilters[index].get();
}

void FilterEffect::setImageFilter(ColorSpace colorSpace,
                                  bool requiresPMColorValidation,
                                  sk_sp<SkImageFilter> imageFilter) {
  int index = getImageFilterIndex(colorSpace, requiresPMColorValidation);
  m_imageFilters[index] = std::move(imageFilter);
}

}  // namespace blink
