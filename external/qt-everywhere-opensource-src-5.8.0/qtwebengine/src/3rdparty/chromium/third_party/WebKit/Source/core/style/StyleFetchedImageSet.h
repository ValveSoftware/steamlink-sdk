/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleFetchedImageSet_h
#define StyleFetchedImageSet_h

#include "core/fetch/ResourceClient.h"
#include "core/style/StyleImage.h"
#include "platform/geometry/LayoutSize.h"

namespace blink {

class ImageResource;
class CSSImageSetValue;

// This class keeps one cached image and has access to a set of alternatives.

class StyleFetchedImageSet final : public StyleImage, private ResourceClient {
    USING_PRE_FINALIZER(StyleFetchedImageSet, dispose);
public:
    static StyleFetchedImageSet* create(ImageResource* image, float imageScaleFactor, CSSImageSetValue* value, const KURL& url)
    {
        return new StyleFetchedImageSet(image, imageScaleFactor, value, url);
    }
    ~StyleFetchedImageSet() override;

    CSSValue* cssValue() const override;
    CSSValue* computedCSSValue() const override;

    // FIXME: This is used by StyleImage for equals comparison, but this implementation
    // only looks at the image from the set that we have loaded. I'm not sure if that is
    // meaningful enough or not.
    WrappedImagePtr data() const override;

    bool canRender() const override;
    bool isLoaded() const override;
    bool errorOccurred() const override;
    LayoutSize imageSize(const LayoutObject&, float multiplier, const LayoutSize& defaultObjectSize) const override;
    bool imageHasRelativeSize() const override;
    bool usesImageContainerSize() const override;
    void addClient(LayoutObject*) override;
    void removeClient(LayoutObject*) override;
    PassRefPtr<Image> image(const LayoutObject&, const IntSize&, float) const override;
    float imageScaleFactor() const override { return m_imageScaleFactor; }
    bool knownToBeOpaque(const LayoutObject&) const override;
    ImageResource* cachedImage() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    StyleFetchedImageSet(ImageResource*, float imageScaleFactor, CSSImageSetValue*, const KURL&);

    void dispose();

    String debugName() const override { return "StyleFetchedImageSet"; }

    Member<ImageResource> m_bestFitImage;
    float m_imageScaleFactor;

    Member<CSSImageSetValue> m_imageSetValue; // Not retained; it owns us.
    const KURL m_url;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StyleFetchedImageSet, isImageResourceSet());

} // namespace blink

#endif // StyleFetchedImageSet_h
