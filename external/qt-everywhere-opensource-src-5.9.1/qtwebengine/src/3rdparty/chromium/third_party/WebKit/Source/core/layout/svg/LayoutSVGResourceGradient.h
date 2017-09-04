/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef LayoutSVGResourceGradient_h
#define LayoutSVGResourceGradient_h

#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "core/svg/SVGGradientElement.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/Gradient.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/HashMap.h"
#include <memory>

namespace blink {

struct GradientData {
  USING_FAST_MALLOC(GradientData);

 public:
  RefPtr<Gradient> gradient;
  AffineTransform userspaceTransform;
};

class LayoutSVGResourceGradient : public LayoutSVGResourcePaintServer {
 public:
  explicit LayoutSVGResourceGradient(SVGGradientElement*);

  void removeAllClientsFromCache(bool markForInvalidation = true) final;
  void removeClientFromCache(LayoutObject*,
                             bool markForInvalidation = true) final;

  SVGPaintServer preparePaintServer(const LayoutObject&) final;

  bool isChildAllowed(LayoutObject* child, const ComputedStyle&) const final;

 protected:
  void addStops(Gradient&, const Vector<Gradient::ColorStop>&) const;

  virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0;
  virtual AffineTransform calculateGradientTransform() const = 0;
  virtual bool collectGradientAttributes(SVGGradientElement*) = 0;
  virtual PassRefPtr<Gradient> buildGradient() const = 0;

  static GradientSpreadMethod platformSpreadMethodFromSVGType(
      SVGSpreadMethodType);

 private:
  bool m_shouldCollectGradientAttributes : 1;
  HashMap<const LayoutObject*, std::unique_ptr<GradientData>> m_gradientMap;
};

}  // namespace blink

#endif
