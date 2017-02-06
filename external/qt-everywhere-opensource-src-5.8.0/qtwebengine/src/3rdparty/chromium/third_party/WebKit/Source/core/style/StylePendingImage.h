/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef StylePendingImage_h
#define StylePendingImage_h

#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPaintValue.h"
#include "core/style/StyleImage.h"
#include "platform/graphics/Image.h"

namespace blink {

// StylePendingImage is a placeholder StyleImage that is entered into the ComputedStyle during
// style resolution, in order to avoid loading images that are not referenced by the final style.
// They should never exist in a ComputedStyle after it has been returned from the style selector.

class StylePendingImage final : public StyleImage {
public:
    static StylePendingImage* create(const CSSValue& value)
    {
        return new StylePendingImage(value);
    }

    WrappedImagePtr data() const override { return m_value.get(); }

    CSSValue* cssValue() const override { return m_value; }

    CSSValue* computedCSSValue() const override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    CSSImageValue* cssImageValue() const { return m_value->isImageValue() ? toCSSImageValue(m_value.get()) : 0; }
    CSSPaintValue* cssPaintValue() const { return m_value->isPaintValue() ? toCSSPaintValue(m_value.get()) : 0; }
    CSSImageGeneratorValue* cssImageGeneratorValue() const { return m_value->isImageGeneratorValue() ? toCSSImageGeneratorValue(m_value.get()) : 0; }
    CSSCursorImageValue* cssCursorImageValue() const { return m_value->isCursorImageValue() ? toCSSCursorImageValue(m_value.get()) : 0; }
    CSSImageSetValue* cssImageSetValue() const { return m_value->isImageSetValue() ? toCSSImageSetValue(m_value.get()) : 0; }

    LayoutSize imageSize(const LayoutObject&, float /*multiplier*/, const LayoutSize& /*defaultObjectSize*/) const override { return LayoutSize(); }
    bool imageHasRelativeSize() const override { return false; }
    bool usesImageContainerSize() const override { return false; }
    void addClient(LayoutObject*) override { }
    void removeClient(LayoutObject*) override { }
    PassRefPtr<Image> image(const LayoutObject&, const IntSize&, float) const override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    bool knownToBeOpaque(const LayoutObject&) const override { return false; }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_value);
        StyleImage::trace(visitor);
    }

private:
    explicit StylePendingImage(const CSSValue& value)
        : m_value(const_cast<CSSValue*>(&value))
    {
        m_isPendingImage = true;
    }

    // TODO(sashab): Replace this with <const CSSValue> once Member<>
    // supports const types.
    Member<CSSValue> m_value;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StylePendingImage, isPendingImage());

} // namespace blink
#endif
