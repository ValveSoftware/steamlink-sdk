/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef FEDropShadow_h
#define FEDropShadow_h

#include "platform/graphics/Color.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace blink {

class PLATFORM_EXPORT FEDropShadow final : public FilterEffect {
 public:
  static FEDropShadow*
  create(Filter*, float, float, float, float, const Color&, float);

  // Compute which destination area will be affected when applying a drop
  // shadow effect with |stdDeviation| and |offset| to an area |rect|.
  static FloatRect mapEffect(const FloatSize& stdDeviation,
                             const FloatPoint& offset,
                             const FloatRect&);

  void setShadowColor(const Color& color) { m_shadowColor = color; }
  void setShadowOpacity(float opacity) { m_shadowOpacity = opacity; }

  TextStream& externalRepresentation(TextStream&, int indention) const override;

 private:
  FEDropShadow(Filter*, float, float, float, float, const Color&, float);

  FloatRect mapEffect(const FloatRect&) const override;

  sk_sp<SkImageFilter> createImageFilter() override;

  float m_stdX;
  float m_stdY;
  float m_dx;
  float m_dy;
  Color m_shadowColor;
  float m_shadowOpacity;
};

}  // namespace blink

#endif  // FEDropShadow_h
