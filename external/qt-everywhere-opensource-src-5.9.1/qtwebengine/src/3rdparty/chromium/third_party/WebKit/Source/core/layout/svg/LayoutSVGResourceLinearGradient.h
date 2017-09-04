/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef LayoutSVGResourceLinearGradient_h
#define LayoutSVGResourceLinearGradient_h

#include "core/layout/svg/LayoutSVGResourceGradient.h"
#include "core/svg/LinearGradientAttributes.h"

namespace blink {

class SVGLinearGradientElement;

class LayoutSVGResourceLinearGradient final : public LayoutSVGResourceGradient {
 public:
  explicit LayoutSVGResourceLinearGradient(SVGLinearGradientElement*);
  ~LayoutSVGResourceLinearGradient() override;

  const char* name() const override {
    return "LayoutSVGResourceLinearGradient";
  }

  static const LayoutSVGResourceType s_resourceType =
      LinearGradientResourceType;
  LayoutSVGResourceType resourceType() const override { return s_resourceType; }

  SVGUnitTypes::SVGUnitType gradientUnits() const override {
    return attributes().gradientUnits();
  }
  AffineTransform calculateGradientTransform() const override {
    return attributes().gradientTransform();
  }
  bool collectGradientAttributes(SVGGradientElement*) override;
  PassRefPtr<Gradient> buildGradient() const override;

  FloatPoint startPoint(const LinearGradientAttributes&) const;
  FloatPoint endPoint(const LinearGradientAttributes&) const;

 private:
  Persistent<LinearGradientAttributesWrapper> m_attributesWrapper;

  LinearGradientAttributes& mutableAttributes() {
    return m_attributesWrapper->attributes();
  }
  const LinearGradientAttributes& attributes() const {
    return m_attributesWrapper->attributes();
  }
};

}  // namespace blink

#endif  // LayoutSVGResourceLinearGradient_h
