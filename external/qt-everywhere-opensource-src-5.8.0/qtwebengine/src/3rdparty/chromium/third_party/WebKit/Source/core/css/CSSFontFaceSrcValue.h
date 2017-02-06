/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSFontFaceSrcValue_h
#define CSSFontFaceSrcValue_h

#include "core/css/CSSValue.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ResourceOwner.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;

class CSSFontFaceSrcValue : public CSSValue {
public:
    static CSSFontFaceSrcValue* create(const String& specifiedResource, const String& absoluteResource, ContentSecurityPolicyDisposition shouldCheckContentSecurityPolicy)
    {
        return new CSSFontFaceSrcValue(specifiedResource, absoluteResource, false, shouldCheckContentSecurityPolicy);
    }
    static CSSFontFaceSrcValue* createLocal(const String& absoluteResource, ContentSecurityPolicyDisposition shouldCheckContentSecurityPolicy)
    {
        return new CSSFontFaceSrcValue(emptyString(), absoluteResource, true, shouldCheckContentSecurityPolicy);
    }

    const String& resource() const { return m_absoluteResource; }
    const String& format() const { return m_format; }
    bool isLocal() const { return m_isLocal; }

    void setFormat(const String& format) { m_format = format; }
    void setReferrer(const Referrer& referrer) { m_referrer = referrer; }

    bool isSupportedFormat() const;

    String customCSSText() const;

    bool hasFailedOrCanceledSubresources() const;

    FontResource* fetch(Document*) const;

    bool equals(const CSSFontFaceSrcValue&) const;

    DEFINE_INLINE_TRACE_AFTER_DISPATCH()
    {
        visitor->trace(m_fetched);
        CSSValue::traceAfterDispatch(visitor);
    }

private:
    CSSFontFaceSrcValue(const String& specifiedResource, const String& absoluteResource, bool local, ContentSecurityPolicyDisposition shouldCheckContentSecurityPolicy)
        : CSSValue(FontFaceSrcClass)
        , m_absoluteResource(absoluteResource)
        , m_specifiedResource(specifiedResource)
        , m_isLocal(local)
        , m_shouldCheckContentSecurityPolicy(shouldCheckContentSecurityPolicy)
    {
    }

    void restoreCachedResourceIfNeeded(Document*) const;

    String m_absoluteResource;
    String m_specifiedResource;
    String m_format;
    Referrer m_referrer;
    bool m_isLocal;
    ContentSecurityPolicyDisposition m_shouldCheckContentSecurityPolicy;


    class FontResourceHelper : public GarbageCollectedFinalized<FontResourceHelper>, public ResourceOwner<FontResource> {
        USING_GARBAGE_COLLECTED_MIXIN(FontResourceHelper);
    public:
        static FontResourceHelper* create(FontResource* resource)
        {
            return new FontResourceHelper(resource);
        }

        DEFINE_INLINE_VIRTUAL_TRACE() { ResourceOwner<FontResource>::trace(visitor); }

    private:
        FontResourceHelper(FontResource* resource)
        {
            setResource(resource);
        }

        String debugName() const override { return "CSSFontFaceSrcValue::FontResourceHelper"; }
    };
    mutable Member<FontResourceHelper> m_fetched;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFontFaceSrcValue, isFontFaceSrcValue());

} // namespace blink

#endif
