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

#include "core/style/StyleGeneratedImage.h"

#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/resolver/StyleResolver.h"

namespace blink {

StyleGeneratedImage::StyleGeneratedImage(const CSSImageGeneratorValue& value)
    : m_imageGeneratorValue(const_cast<CSSImageGeneratorValue*>(&value))
    , m_fixedSize(m_imageGeneratorValue->isFixedSize())
{
    m_isGeneratedImage = true;
    if (value.isPaintValue())
        m_isPaintImage = true;
}

CSSValue* StyleGeneratedImage::cssValue() const
{
    return m_imageGeneratorValue.get();
}

CSSValue* StyleGeneratedImage::computedCSSValue() const
{
    return m_imageGeneratorValue->valueWithURLsMadeAbsolute();
}

LayoutSize StyleGeneratedImage::imageSize(const LayoutObject& layoutObject, float multiplier, const LayoutSize& defaultObjectSize) const
{
    if (m_fixedSize) {
        FloatSize unzoomedDefaultObjectSize(defaultObjectSize);
        unzoomedDefaultObjectSize.scale(1 / multiplier);
        return applyZoom(LayoutSize(m_imageGeneratorValue->fixedSize(layoutObject, unzoomedDefaultObjectSize)), multiplier);
    }

    return defaultObjectSize;
}

void StyleGeneratedImage::addClient(LayoutObject* layoutObject)
{
    m_imageGeneratorValue->addClient(layoutObject, IntSize());
}

void StyleGeneratedImage::removeClient(LayoutObject* layoutObject)
{
    m_imageGeneratorValue->removeClient(layoutObject);
}

PassRefPtr<Image> StyleGeneratedImage::image(const LayoutObject& layoutObject, const IntSize& size, float) const
{
    return m_imageGeneratorValue->image(layoutObject, size);
}

bool StyleGeneratedImage::knownToBeOpaque(const LayoutObject& layoutObject) const
{
    return m_imageGeneratorValue->knownToBeOpaque(layoutObject);
}

DEFINE_TRACE(StyleGeneratedImage)
{
    visitor->trace(m_imageGeneratorValue);
    StyleImage::trace(visitor);
}

} // namespace blink
