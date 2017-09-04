/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 *
 */

#ifndef LayoutImageResourceStyleImage_h
#define LayoutImageResourceStyleImage_h

#include "core/layout/LayoutImageResource.h"
#include "core/style/StyleImage.h"
#include "wtf/RefPtr.h"

namespace blink {

class LayoutObject;

class LayoutImageResourceStyleImage final : public LayoutImageResource {
 public:
  ~LayoutImageResourceStyleImage() override;

  static LayoutImageResource* create(StyleImage* styleImage) {
    return new LayoutImageResourceStyleImage(styleImage);
  }
  void initialize(LayoutObject*) override;
  void shutdown() override;

  bool hasImage() const override { return true; }
  PassRefPtr<Image> image(const IntSize&, float) const override;
  bool errorOccurred() const override { return m_styleImage->errorOccurred(); }

  bool imageHasRelativeSize() const override {
    return m_styleImage->imageHasRelativeSize();
  }
  LayoutSize imageSize(float multiplier) const override;
  WrappedImagePtr imagePtr() const override { return m_styleImage->data(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit LayoutImageResourceStyleImage(StyleImage*);
  Member<StyleImage> m_styleImage;
};

}  // namespace blink

#endif  // LayoutImageStyleImage_h
