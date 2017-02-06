// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/FilteredComputedStylePropertyMap.h"

#include "core/css/parser/CSSVariableParser.h"

namespace blink {

FilteredComputedStylePropertyMap::FilteredComputedStylePropertyMap(CSSComputedStyleDeclaration* computedStyleDeclaration, const Vector<CSSPropertyID>& nativeProperties, const Vector<AtomicString>& customProperties)
    : ComputedStylePropertyMap(computedStyleDeclaration)
{
    for (const auto& nativeProperty : nativeProperties) {
        m_nativeProperties.add(nativeProperty);
    }

    for (const auto& customProperty: customProperties) {
        m_customProperties.add(customProperty);
    }
}


CSSStyleValue* FilteredComputedStylePropertyMap::get(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid && m_nativeProperties.contains(propertyID)) {
        CSSStyleValueVector styleVector = getAllInternal(propertyID);
        if (styleVector.isEmpty())
            return nullptr;

        return styleVector[0];
    }

    if (propertyID == CSSPropertyInvalid && CSSVariableParser::isValidVariableName(propertyName) && m_customProperties.contains(AtomicString(propertyName))) {
        CSSStyleValueVector styleVector = getAllInternal(AtomicString(propertyName));
        if (styleVector.isEmpty())
            return nullptr;

        return styleVector[0];
    }

    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return nullptr;
}

CSSStyleValueVector FilteredComputedStylePropertyMap::getAll(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid && m_nativeProperties.contains(propertyID))
        return getAllInternal(propertyID);

    if (propertyID == CSSPropertyInvalid && CSSVariableParser::isValidVariableName(propertyName) && m_customProperties.contains(AtomicString(propertyName)))
        return getAllInternal(AtomicString(propertyName));

    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return CSSStyleValueVector();
}

bool FilteredComputedStylePropertyMap::has(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid && m_nativeProperties.contains(propertyID))
        return !getAllInternal(propertyID).isEmpty();

    if (propertyID == CSSPropertyInvalid && CSSVariableParser::isValidVariableName(propertyName) && m_customProperties.contains(AtomicString(propertyName)))
        return !getAllInternal(AtomicString(propertyName)).isEmpty();

    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return false;
}

Vector<String> FilteredComputedStylePropertyMap::getProperties()
{
    Vector<String> result;
    for (const auto& nativeProperty : m_nativeProperties) {
        result.append(getPropertyNameString(nativeProperty));
    }

    for (const auto& customProperty : m_customProperties) {
        result.append(customProperty);
    }

    return result;
}

} // namespace blink
