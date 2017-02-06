// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStylePropertyMap_h
#define ComputedStylePropertyMap_h

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/ImmutableStylePropertyMap.h"

namespace blink {

class CORE_EXPORT ComputedStylePropertyMap : public ImmutableStylePropertyMap {
    WTF_MAKE_NONCOPYABLE(ComputedStylePropertyMap);

public:
    static ComputedStylePropertyMap* create(CSSComputedStyleDeclaration* computedStyleDeclaration)
    {
        return new ComputedStylePropertyMap(computedStyleDeclaration);
    }

    Vector<String> getProperties() override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_computedStyleDeclaration);
        ImmutableStylePropertyMap::trace(visitor);
    }

protected:
    ComputedStylePropertyMap(CSSComputedStyleDeclaration* computedStyleDeclaration)
        : ImmutableStylePropertyMap()
        , m_computedStyleDeclaration(computedStyleDeclaration) { }

    CSSStyleValueVector getAllInternal(CSSPropertyID) override;
    CSSStyleValueVector getAllInternal(AtomicString customPropertyName) override;

    HeapVector<StylePropertyMapEntry> getIterationEntries() override { return HeapVector<StylePropertyMapEntry>(); }

    Member<CSSStyleDeclaration> m_computedStyleDeclaration;
};

} // namespace blink

#endif
