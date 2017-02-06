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

#ifndef StyleFetchedImage_h
#define StyleFetchedImage_h

#include "core/fetch/ResourceClient.h"
#include "core/style/StyleImage.h"

namespace blink {

class Document;
class ImageResource;

class StyleFetchedImage final : public StyleImage, private ResourceClient {
    USING_PRE_FINALIZER(StyleFetchedImage, dispose);
public:
    static StyleFetchedImage* create(ImageResource* image, Document* document, const KURL& url)
    {
        return new StyleFetchedImage(image, document, url);
    }
    ~StyleFetchedImage() override;

    WrappedImagePtr data() const override;

    CSSValue* cssValue() const override;
    CSSValue* computedCSSValue() const override;

    bool canRender() const override;
    bool isLoaded() const override;
    bool errorOccurred() const override;
    LayoutSize imageSize(const LayoutObject&, float multiplier, const LayoutSize& defaultObjectSize) const override;
    bool imageHasRelativeSize() const override;
    bool usesImageContainerSize() const override;
    void addClient(LayoutObject*) override;
    void removeClient(LayoutObject*) override;
    void notifyFinished(Resource*) override;
    String debugName() const override { return "StyleFetchedImage"; }
    PassRefPtr<Image> image(const LayoutObject&, const IntSize&, float zoom) const override;
    bool knownToBeOpaque(const LayoutObject&) const override;
    ImageResource* cachedImage() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    StyleFetchedImage(ImageResource*, Document*, const KURL&);

    void dispose();

    Member<ImageResource> m_image;
    Member<Document> m_document;
    const KURL m_url;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StyleFetchedImage, isImageResource());

} // namespace blink
#endif
