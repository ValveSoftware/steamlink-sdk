/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef Filter_h
#define Filter_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class SourceGraphic;
class FilterEffect;

class PLATFORM_EXPORT Filter final : public GarbageCollected<Filter> {
  WTF_MAKE_NONCOPYABLE(Filter);

 public:
  enum UnitScaling { UserSpace, BoundingBox };

  static Filter* create(const FloatRect& referenceBox,
                        const FloatRect& filterRegion,
                        float scale,
                        UnitScaling);
  static Filter* create(float scale);

  DECLARE_TRACE();

  float scale() const { return m_scale; }
  FloatRect mapLocalRectToAbsoluteRect(const FloatRect&) const;
  FloatRect mapAbsoluteRectToLocalRect(const FloatRect&) const;

  float applyHorizontalScale(float value) const;
  float applyVerticalScale(float value) const;

  FloatPoint3D resolve3dPoint(const FloatPoint3D&) const;

  FloatRect absoluteFilterRegion() const {
    return mapLocalRectToAbsoluteRect(m_filterRegion);
  }

  const FloatRect& filterRegion() const { return m_filterRegion; }
  const FloatRect& referenceBox() const { return m_referenceBox; }

  void setLastEffect(FilterEffect*);
  FilterEffect* lastEffect() const { return m_lastEffect.get(); }

  SourceGraphic* getSourceGraphic() const { return m_sourceGraphic.get(); }

 private:
  Filter(const FloatRect& referenceBox,
         const FloatRect& filterRegion,
         float scale,
         UnitScaling);

  FloatRect m_referenceBox;
  FloatRect m_filterRegion;
  float m_scale;
  UnitScaling m_unitScaling;

  Member<SourceGraphic> m_sourceGraphic;
  Member<FilterEffect> m_lastEffect;
};

}  // namespace blink

#endif  // Filter_h
