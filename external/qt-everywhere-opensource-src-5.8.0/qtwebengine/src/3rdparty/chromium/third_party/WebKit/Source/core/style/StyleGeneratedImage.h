/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef StyleGeneratedImage_h
#define StyleGeneratedImage_h

#include "core/CoreExport.h"
#include "core/style/StyleImage.h"

namespace blink {

class CSSValue;
class CSSImageGeneratorValue;

class CORE_EXPORT StyleGeneratedImage final : public StyleImage {
public:
    static StyleGeneratedImage* create(const CSSImageGeneratorValue& value)
    {
        return new StyleGeneratedImage(value);
    }

    WrappedImagePtr data() const override { return m_imageGeneratorValue.get(); }

    CSSValue* cssValue() const override;
    CSSValue* computedCSSValue() const override;

    LayoutSize imageSize(const LayoutObject&, float multiplier, const LayoutSize& defaultObjectSize) const override;
    bool imageHasRelativeSize() const override { return !m_fixedSize; }
    bool usesImageContainerSize() const override { return !m_fixedSize; }
    void addClient(LayoutObject*) override;
    void removeClient(LayoutObject*) override;
    PassRefPtr<Image> image(const LayoutObject&, const IntSize&, float) const override;
    bool knownToBeOpaque(const LayoutObject&) const override;

    DECLARE_VIRTUAL_TRACE();

private:
    StyleGeneratedImage(const CSSImageGeneratorValue&);

    // TODO(sashab): Replace this with <const CSSImageGeneratorValue> once Member<>
    // supports const types.
    Member<CSSImageGeneratorValue> m_imageGeneratorValue;
    const bool m_fixedSize;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StyleGeneratedImage, isGeneratedImage());

} // namespace blink
#endif
