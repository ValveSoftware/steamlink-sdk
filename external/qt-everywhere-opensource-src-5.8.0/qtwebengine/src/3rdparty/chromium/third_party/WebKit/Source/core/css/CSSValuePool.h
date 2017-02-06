/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSValuePool_h
#define CSSValuePool_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSUnsetValue.h"
#include "core/css/CSSValueList.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CORE_EXPORT CSSValuePool :  public GarbageCollectedFinalized<CSSValuePool> {
    WTF_MAKE_NONCOPYABLE(CSSValuePool);
public:
    // TODO(sashab): Make all the value pools store const CSSValues.
    static const int maximumCacheableIntegerValue = 255;
    using ColorValueCache = HeapHashMap<unsigned, Member<CSSColorValue>>;
    static const unsigned maximumColorCacheSize = 512;
    using FontFaceValueCache = HeapHashMap<AtomicString, Member<const CSSValueList>>;
    static const unsigned maximumFontFaceCacheSize = 128;
    using FontFamilyValueCache = HeapHashMap<String, Member<CSSFontFamilyValue>>;

    // Cached individual values.
    CSSColorValue* transparentColor() { return m_colorTransparent; }
    CSSColorValue* whiteColor() { return m_colorWhite; }
    CSSColorValue* blackColor() { return m_colorBlack; }
    CSSInheritedValue* inheritedValue() { return m_inheritedValue; }
    CSSInitialValue* implicitInitialValue() { return m_implicitInitialValue; }
    CSSInitialValue* explicitInitialValue() { return m_explicitInitialValue; }
    CSSUnsetValue* unsetValue() { return m_unsetValue; }

    // CSSPrimitiveValue vector caches.
    CSSPrimitiveValue* identifierCacheValue(CSSValueID ident) { return m_identifierValueCache[ident]; }
    CSSPrimitiveValue* setIdentifierCacheValue(CSSValueID ident, CSSPrimitiveValue* cssValue) { return m_identifierValueCache[ident] = cssValue; }
    CSSPrimitiveValue* pixelCacheValue(int intValue) { return m_pixelValueCache[intValue]; }
    CSSPrimitiveValue* setPixelCacheValue(int intValue, CSSPrimitiveValue* cssValue) { return m_pixelValueCache[intValue] = cssValue; }
    CSSPrimitiveValue* percentCacheValue(int intValue) { return m_percentValueCache[intValue]; }
    CSSPrimitiveValue* setPercentCacheValue(int intValue, CSSPrimitiveValue* cssValue) { return m_percentValueCache[intValue] = cssValue; }
    CSSPrimitiveValue* numberCacheValue(int intValue) { return m_numberValueCache[intValue]; }
    CSSPrimitiveValue* setNumberCacheValue(int intValue, CSSPrimitiveValue* cssValue) { return m_numberValueCache[intValue] = cssValue; }

    // Hash map caches.
    ColorValueCache::AddResult getColorCacheEntry(RGBA32 rgbValue)
    {
        // Just wipe out the cache and start rebuilding if it gets too big.
        if (m_colorValueCache.size() > maximumColorCacheSize)
            m_colorValueCache.clear();
        return m_colorValueCache.add(rgbValue, nullptr);
    }
    FontFamilyValueCache::AddResult getFontFamilyCacheEntry(const String& familyName)
    {
        return m_fontFamilyValueCache.add(familyName, nullptr);
    }
    FontFaceValueCache::AddResult getFontFaceCacheEntry(const AtomicString& string)
    {
        // Just wipe out the cache and start rebuilding if it gets too big.
        if (m_fontFaceValueCache.size() > maximumFontFaceCacheSize)
            m_fontFaceValueCache.clear();
        return m_fontFaceValueCache.add(string, nullptr);
    }

    DECLARE_TRACE();

private:
    CSSValuePool();

    // Cached individual values.
    Member<CSSInheritedValue> m_inheritedValue;
    Member<CSSInitialValue> m_implicitInitialValue;
    Member<CSSInitialValue> m_explicitInitialValue;
    Member<CSSUnsetValue> m_unsetValue;
    Member<CSSColorValue> m_colorTransparent;
    Member<CSSColorValue> m_colorWhite;
    Member<CSSColorValue> m_colorBlack;

    // CSSPrimitiveValue vector caches.
    HeapVector<Member<CSSPrimitiveValue>, numCSSValueKeywords> m_identifierValueCache;
    HeapVector<Member<CSSPrimitiveValue>, maximumCacheableIntegerValue + 1> m_pixelValueCache;
    HeapVector<Member<CSSPrimitiveValue>, maximumCacheableIntegerValue + 1> m_percentValueCache;
    HeapVector<Member<CSSPrimitiveValue>, maximumCacheableIntegerValue + 1> m_numberValueCache;

    // Hash map caches.
    ColorValueCache m_colorValueCache;
    FontFaceValueCache m_fontFaceValueCache;
    FontFamilyValueCache m_fontFamilyValueCache;

    friend CORE_EXPORT CSSValuePool& cssValuePool();
};

CORE_EXPORT CSSValuePool& cssValuePool();

} // namespace blink

#endif
