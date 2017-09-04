/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef FilterEffect_h
#define FilterEffect_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/ColorSpace.h"
#include "platform/heap/Handle.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class Filter;
class FilterEffect;
class TextStream;

typedef HeapVector<Member<FilterEffect>> FilterEffectVector;

enum FilterEffectType {
  FilterEffectTypeUnknown,
  FilterEffectTypeImage,
  FilterEffectTypeTile,
  FilterEffectTypeSourceInput
};

class PLATFORM_EXPORT FilterEffect
    : public GarbageCollectedFinalized<FilterEffect> {
  WTF_MAKE_NONCOPYABLE(FilterEffect);

 public:
  virtual ~FilterEffect();
  DECLARE_VIRTUAL_TRACE();

  void clearResult();

  FilterEffectVector& inputEffects() { return m_inputEffects; }
  FilterEffect* inputEffect(unsigned) const;
  unsigned numberOfEffectInputs() const { return m_inputEffects.size(); }

  inline bool hasImageFilter() const {
    return m_imageFilters[0] || m_imageFilters[1] || m_imageFilters[2] ||
           m_imageFilters[3];
  }

  // Clipped primitive subregion in the coordinate space of the target.
  FloatRect absoluteBounds() const;

  // Mapping a rect forwards to determine which which destination pixels a
  // given source rect would affect.
  FloatRect mapRect(const FloatRect&) const;

  virtual sk_sp<SkImageFilter> createImageFilter();
  virtual sk_sp<SkImageFilter> createImageFilterWithoutValidation();

  virtual FilterEffectType getFilterEffectType() const {
    return FilterEffectTypeUnknown;
  }

  virtual TextStream& externalRepresentation(TextStream&,
                                             int indention = 0) const;

  FloatRect filterPrimitiveSubregion() const {
    return m_filterPrimitiveSubregion;
  }
  void setFilterPrimitiveSubregion(const FloatRect& filterPrimitiveSubregion) {
    m_filterPrimitiveSubregion = filterPrimitiveSubregion;
  }

  Filter* getFilter() { return m_filter; }
  const Filter* getFilter() const { return m_filter; }

  bool clipsToBounds() const { return m_clipsToBounds; }
  void setClipsToBounds(bool value) { m_clipsToBounds = value; }

  ColorSpace operatingColorSpace() const { return m_operatingColorSpace; }
  virtual void setOperatingColorSpace(ColorSpace colorSpace) {
    m_operatingColorSpace = colorSpace;
  }

  virtual bool affectsTransparentPixels() const { return false; }

  // Return false if the filter will only operate correctly on valid RGBA
  // values, with alpha in [0,255] and each color component in [0, alpha].
  virtual bool mayProduceInvalidPreMultipliedPixels() { return false; }

  SkImageFilter* getImageFilter(ColorSpace,
                                bool requiresPMColorValidation) const;
  void setImageFilter(ColorSpace,
                      bool requiresPMColorValidation,
                      sk_sp<SkImageFilter>);

  bool originTainted() const { return m_originTainted; }
  void setOriginTainted() { m_originTainted = true; }

  bool inputsTaintOrigin() const;

 protected:
  FilterEffect(Filter*);

  // Determine the contribution from the filter effect's inputs.
  virtual FloatRect mapInputs(const FloatRect&) const;

  // Apply the contribution from the filter effect's itself. (Like
  // expanding with the blur radius etc.)
  virtual FloatRect mapEffect(const FloatRect&) const;

  // Apply the clip bounds and factor in the effect of
  // affectsTransparentPixels().
  FloatRect applyBounds(const FloatRect&) const;

  sk_sp<SkImageFilter> createTransparentBlack() const;

  Color adaptColorToOperatingColorSpace(const Color& deviceColor);

  SkImageFilter::CropRect getCropRect() const;

 private:
  FilterEffectVector m_inputEffects;

  Member<Filter> m_filter;

  // The following member variables are SVG specific and will move to
  // LayoutSVGResourceFilterPrimitive.
  // See bug https://bugs.webkit.org/show_bug.cgi?id=45614.

  // The subregion of a filter primitive according to the SVG Filter
  // specification in local coordinates.
  FloatRect m_filterPrimitiveSubregion;

  // Whether the effect should clip to its primitive region, or expand to use
  // the combined region of its inputs.
  bool m_clipsToBounds;

  bool m_originTainted;

  ColorSpace m_operatingColorSpace;

  sk_sp<SkImageFilter> m_imageFilters[4];
};

}  // namespace blink

#endif  // FilterEffect_h
