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

#ifndef StyleImage_h
#define StyleImage_h

#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/Image.h"
#include "wtf/Forward.h"

namespace blink {

class ImageResource;
class CSSValue;
class LayoutObject;
class SVGImage;

typedef void* WrappedImagePtr;

class CORE_EXPORT StyleImage : public GarbageCollectedFinalized<StyleImage> {
public:
    virtual ~StyleImage() { }

    bool operator==(const StyleImage& other) const
    {
        return data() == other.data();
    }

    virtual CSSValue* cssValue() const = 0;
    virtual CSSValue* computedCSSValue() const = 0;

    virtual bool canRender() const { return true; }
    virtual bool isLoaded() const { return true; }
    virtual bool errorOccurred() const { return false; }
    // Note that the defaultObjectSize is assumed to be in the
    // effective zoom level given by multiplier, i.e. if multiplier is
    // the constant 1 the defaultObjectSize should be unzoomed.
    virtual LayoutSize imageSize(const LayoutObject&, float multiplier, const LayoutSize& defaultObjectSize) const = 0;
    virtual bool imageHasRelativeSize() const = 0;
    virtual bool usesImageContainerSize() const = 0;
    virtual void addClient(LayoutObject*) = 0;
    virtual void removeClient(LayoutObject*) = 0;
    // Note that the containerSize is assumed to be in the effective
    // zoom level given by multiplier, i.e if the multiplier is the
    // constant 1 the containerSize should be unzoomed.
    virtual PassRefPtr<Image> image(const LayoutObject&, const IntSize& containerSize, float multiplier) const = 0;
    virtual WrappedImagePtr data() const = 0;
    virtual float imageScaleFactor() const { return 1; }
    virtual bool knownToBeOpaque(const LayoutObject&) const = 0;
    virtual ImageResource* cachedImage() const { return 0; }

    ALWAYS_INLINE bool isImageResource() const { return m_isImageResource; }
    ALWAYS_INLINE bool isPendingImage() const { return m_isPendingImage; }
    ALWAYS_INLINE bool isGeneratedImage() const { return m_isGeneratedImage; }
    ALWAYS_INLINE bool isImageResourceSet() const { return m_isImageResourceSet; }
    ALWAYS_INLINE bool isInvalidImage() const { return m_isInvalidImage; }
    ALWAYS_INLINE bool isPaintImage() const { return m_isPaintImage; }

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    StyleImage()
        : m_isImageResource(false)
        , m_isPendingImage(false)
        , m_isGeneratedImage(false)
        , m_isImageResourceSet(false)
        , m_isInvalidImage(false)
        , m_isPaintImage(false)
    {
    }
    bool m_isImageResource:1;
    bool m_isPendingImage:1;
    bool m_isGeneratedImage:1;
    bool m_isImageResourceSet:1;
    bool m_isInvalidImage:1;
    bool m_isPaintImage:1;

    static LayoutSize applyZoom(const LayoutSize&, float multiplier);
    LayoutSize imageSizeForSVGImage(SVGImage*, float multiplier, const LayoutSize& defaultObjectSize) const;
};

#define DEFINE_STYLE_IMAGE_TYPE_CASTS(thisType, function) \
    DEFINE_TYPE_CASTS(thisType, StyleImage, styleImage, styleImage->function, styleImage.function); \
    inline thisType* to##thisType(const Member<StyleImage>& styleImage) { return to##thisType(styleImage.get()); } \
    typedef int NeedsSemiColonAfterDefineStyleImageTypeCasts

} // namespace blink
#endif
