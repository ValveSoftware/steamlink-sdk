/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#ifndef SVGFEImage_h
#define SVGFEImage_h

#include "core/dom/TreeScope.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace blink {

class Image;
class LayoutObject;

class FEImage final : public FilterEffect {
 public:
  static FEImage* createWithImage(Filter*,
                                  PassRefPtr<Image>,
                                  SVGPreserveAspectRatio*);
  static FEImage* createWithIRIReference(Filter*,
                                         TreeScope&,
                                         const String&,
                                         SVGPreserveAspectRatio*);

  // feImage does not perform color interpolation of any kind, so doesn't
  // depend on the value of color-interpolation-filters.
  void setOperatingColorSpace(ColorSpace) override {}

  TextStream& externalRepresentation(TextStream&, int indention) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  ~FEImage() override {}
  FEImage(Filter*, PassRefPtr<Image>, SVGPreserveAspectRatio*);
  FEImage(Filter*, TreeScope&, const String&, SVGPreserveAspectRatio*);
  LayoutObject* referencedLayoutObject() const;

  FilterEffectType getFilterEffectType() const override {
    return FilterEffectTypeImage;
  }

  FloatRect mapInputs(const FloatRect&) const override;

  sk_sp<SkImageFilter> createImageFilter() override;
  sk_sp<SkImageFilter> createImageFilterForLayoutObject(const LayoutObject&);

  RefPtr<Image> m_image;

  // m_treeScope will never be a dangling reference. See
  // https://bugs.webkit.org/show_bug.cgi?id=99243
  Member<TreeScope> m_treeScope;
  String m_href;
  Member<SVGPreserveAspectRatio> m_preserveAspectRatio;
};

}  // namespace blink

#endif  // SVGFEImage_h
