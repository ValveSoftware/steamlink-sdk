// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilteredComputedStylePropertyMap_h
#define FilteredComputedStylePropertyMap_h

#include "core/CoreExport.h"
#include "core/css/CSSPropertyIDTemplates.h"
#include "core/css/cssom/ComputedStylePropertyMap.h"

namespace blink {

class CORE_EXPORT FilteredComputedStylePropertyMap : public ComputedStylePropertyMap {
    WTF_MAKE_NONCOPYABLE(FilteredComputedStylePropertyMap);
public:
    static FilteredComputedStylePropertyMap* create(CSSComputedStyleDeclaration* computedStyleDeclaration, const Vector<CSSPropertyID>& nativeProperties, const Vector<AtomicString>& customProperties)
    {
        return new FilteredComputedStylePropertyMap(computedStyleDeclaration, nativeProperties, customProperties);
    }

    CSSStyleValue* get(const String& propertyName, ExceptionState&) override;
    CSSStyleValueVector getAll(const String& propertyName, ExceptionState&) override;
    bool has(const String& propertyName, ExceptionState&) override;

    Vector<String> getProperties() override;

private:
    FilteredComputedStylePropertyMap(CSSComputedStyleDeclaration*, const Vector<CSSPropertyID>& nativeProperties, const Vector<AtomicString>& customProperties);

    HashSet<CSSPropertyID> m_nativeProperties;
    HashSet<AtomicString> m_customProperties;
};

} // namespace blink

#endif // FilteredComputedStylePropertyMap_h
