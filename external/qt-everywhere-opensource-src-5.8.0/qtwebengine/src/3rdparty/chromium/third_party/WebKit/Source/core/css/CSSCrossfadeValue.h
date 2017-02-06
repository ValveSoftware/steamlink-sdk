/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSCrossfadeValue_h
#define CSSCrossfadeValue_h

#include "core/CoreExport.h"
#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ImageResourceObserver.h"
#include "platform/graphics/Image.h"

namespace blink {

class ImageResource;
class CrossfadeSubimageObserverProxy;
class LayoutObject;

class CORE_EXPORT CSSCrossfadeValue final : public CSSImageGeneratorValue {
    friend class CrossfadeSubimageObserverProxy;
    USING_PRE_FINALIZER(CSSCrossfadeValue, dispose);
public:
    static CSSCrossfadeValue* create(CSSValue* fromValue, CSSValue* toValue, CSSPrimitiveValue* percentageValue)
    {
        return new CSSCrossfadeValue(fromValue, toValue, percentageValue);
    }

    ~CSSCrossfadeValue();

    String customCSSText() const;

    PassRefPtr<Image> image(const LayoutObject&, const IntSize&);
    bool isFixedSize() const { return true; }
    IntSize fixedSize(const LayoutObject&, const FloatSize&);

    bool isPending() const;
    bool knownToBeOpaque(const LayoutObject&) const;

    void loadSubimages(Document*);

    bool hasFailedOrCanceledSubresources() const;

    bool equals(const CSSCrossfadeValue&) const;

    CSSCrossfadeValue* valueWithURLsMadeAbsolute();

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSCrossfadeValue(CSSValue* fromValue, CSSValue* toValue, CSSPrimitiveValue* percentageValue);

    void dispose();

    class CrossfadeSubimageObserverProxy final : public ImageResourceObserver {
        DISALLOW_NEW();
    public:
        explicit CrossfadeSubimageObserverProxy(CSSCrossfadeValue* ownerValue)
            : m_ownerValue(ownerValue)
            , m_ready(false) { }

        ~CrossfadeSubimageObserverProxy() override { }
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(m_ownerValue);
        }

        void imageChanged(ImageResource*, const IntRect* = nullptr) override;
        bool willRenderImage() override;
        String debugName() const override { return "CrossfadeSubimageObserverProxy"; }
        void setReady(bool ready) { m_ready = ready; }
    private:
        Member<CSSCrossfadeValue> m_ownerValue;
        bool m_ready;
    };

    bool willRenderImage() const;
    void crossfadeChanged(const IntRect&);

    Member<CSSValue> m_fromValue;
    Member<CSSValue> m_toValue;
    Member<CSSPrimitiveValue> m_percentageValue;

    Member<ImageResource> m_cachedFromImage;
    Member<ImageResource> m_cachedToImage;

    CrossfadeSubimageObserverProxy m_crossfadeSubimageObserver;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCrossfadeValue, isCrossfadeValue());

} // namespace blink

#endif // CSSCrossfadeValue_h
