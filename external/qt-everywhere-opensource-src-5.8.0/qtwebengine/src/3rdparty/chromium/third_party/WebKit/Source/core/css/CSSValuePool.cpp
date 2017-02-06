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

#include "core/css/CSSValuePool.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "core/style/ComputedStyle.h"
#include "platform/heap/Handle.h"
#include "wtf/Threading.h"

namespace blink {

CSSValuePool& cssValuePool()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<CSSValuePool>>, threadSpecificPool, new ThreadSpecific<Persistent<CSSValuePool>>());
    Persistent<CSSValuePool>& poolHandle = *threadSpecificPool;
    if (!poolHandle) {
        poolHandle = new CSSValuePool;
        poolHandle.registerAsStaticReference();
    }
    return *poolHandle;
}

CSSValuePool::CSSValuePool()
    : m_inheritedValue(new CSSInheritedValue)
    , m_implicitInitialValue(new CSSInitialValue(/* implicit */ true))
    , m_explicitInitialValue(new CSSInitialValue(/* implicit */ false))
    , m_unsetValue(new CSSUnsetValue)
    , m_colorTransparent(new CSSColorValue(Color::transparent))
    , m_colorWhite(new CSSColorValue(Color::white))
    , m_colorBlack(new CSSColorValue(Color::black))
{
    m_identifierValueCache.resize(numCSSValueKeywords);
    m_pixelValueCache.resize(maximumCacheableIntegerValue + 1);
    m_percentValueCache.resize(maximumCacheableIntegerValue + 1);
    m_numberValueCache.resize(maximumCacheableIntegerValue + 1);
}

DEFINE_TRACE(CSSValuePool)
{
    visitor->trace(m_inheritedValue);
    visitor->trace(m_implicitInitialValue);
    visitor->trace(m_explicitInitialValue);
    visitor->trace(m_unsetValue);
    visitor->trace(m_colorTransparent);
    visitor->trace(m_colorWhite);
    visitor->trace(m_colorBlack);
    visitor->trace(m_identifierValueCache);
    visitor->trace(m_pixelValueCache);
    visitor->trace(m_percentValueCache);
    visitor->trace(m_numberValueCache);
    visitor->trace(m_colorValueCache);
    visitor->trace(m_fontFaceValueCache);
    visitor->trace(m_fontFamilyValueCache);
}

} // namespace blink
