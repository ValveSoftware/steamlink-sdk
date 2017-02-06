// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/ComputedStylePropertyMap.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

CSSStyleValueVector ComputedStylePropertyMap::getAllInternal(CSSPropertyID propertyID)
{
    const CSSValue* cssValue = m_computedStyleDeclaration->getPropertyCSSValueInternal(propertyID);
    if (!cssValue)
        return CSSStyleValueVector();
    return StyleValueFactory::cssValueToStyleValueVector(propertyID, *cssValue);
}

CSSStyleValueVector ComputedStylePropertyMap::getAllInternal(AtomicString customPropertyName)
{
    const CSSValue* cssValue = m_computedStyleDeclaration->getPropertyCSSValueInternal(customPropertyName);
    if (!cssValue)
        return CSSStyleValueVector();
    return StyleValueFactory::cssValueToStyleValueVector(CSSPropertyInvalid, *cssValue);
}

Vector<String> ComputedStylePropertyMap::getProperties()
{
    Vector<String> result;
    for (unsigned i = 0; i < m_computedStyleDeclaration->length(); i++) {
        result.append(m_computedStyleDeclaration->item(i));
    }
    return result;
}

} // namespace blink
