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

#ifndef CSSImageSetValue_h
#define CSSImageSetValue_h

#include "core/css/CSSValueList.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/Allocator.h"

namespace blink {

class Document;
class StyleImage;

class CSSImageSetValue : public CSSValueList {
public:

    static CSSImageSetValue* create()
    {
        return new CSSImageSetValue();
    }
    ~CSSImageSetValue();

    bool isCachePending(float deviceScaleFactor) const;
    StyleImage* cachedImage(float deviceScaleFactor) const;
    StyleImage* cacheImage(Document*, float deviceScaleFactor, CrossOriginAttributeValue = CrossOriginAttributeNotSet);

    String customCSSText() const;

    struct ImageWithScale {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        String imageURL;
        Referrer referrer;
        float scaleFactor;
    };

    CSSImageSetValue* valueWithURLsMadeAbsolute();

    bool hasFailedOrCanceledSubresources() const;

    DECLARE_TRACE_AFTER_DISPATCH();

protected:
    ImageWithScale bestImageForScaleFactor(float scaleFactor);

private:
    CSSImageSetValue();

    void fillImageSet();
    static inline bool compareByScaleFactor(ImageWithScale first, ImageWithScale second) { return first.scaleFactor < second.scaleFactor; }

    float m_cachedScaleFactor;
    Member<StyleImage> m_cachedImage;

    Vector<ImageWithScale> m_imagesInSet;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSImageSetValue, isImageSetValue());

} // namespace blink

#endif // CSSImageSetValue_h
