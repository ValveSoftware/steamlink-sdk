// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/InlineStylePropertyMap.h"

#include "bindings/core/v8/Iterable.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

namespace {

CSSValue* styleValueToCSSValue(CSSPropertyID propertyID, const CSSStyleValue& styleValue)
{
    if (!CSSOMTypes::propertyCanTake(propertyID, styleValue))
        return nullptr;
    return styleValue.toCSSValueWithProperty(propertyID);
}

} // namespace

CSSStyleValueVector InlineStylePropertyMap::getAllInternal(CSSPropertyID propertyID)
{
    CSSValue* cssValue = m_ownerElement->ensureMutableInlineStyle().getPropertyCSSValue(propertyID);
    if (!cssValue)
        return CSSStyleValueVector();

    return StyleValueFactory::cssValueToStyleValueVector(propertyID, *cssValue);
}

CSSStyleValueVector InlineStylePropertyMap::getAllInternal(AtomicString customPropertyName)
{
    CSSValue* cssValue = m_ownerElement->ensureMutableInlineStyle().getPropertyCSSValue(customPropertyName);
    if (!cssValue)
        return CSSStyleValueVector();

    return StyleValueFactory::cssValueToStyleValueVector(CSSPropertyInvalid, *cssValue);
}

Vector<String> InlineStylePropertyMap::getProperties()
{
    Vector<String> result;
    StylePropertySet& inlineStyleSet = m_ownerElement->ensureMutableInlineStyle();
    for (unsigned i = 0; i < inlineStyleSet.propertyCount(); i++) {
        CSSPropertyID propertyID = inlineStyleSet.propertyAt(i).id();
        result.append(getPropertyNameString(propertyID));
    }
    return result;
}

void InlineStylePropertyMap::set(CSSPropertyID propertyID, CSSStyleValueOrCSSStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    if (item.isCSSStyleValue()) {
        CSSValue* cssValue = styleValueToCSSValue(propertyID, *item.getAsCSSStyleValue());
        if (!cssValue) {
            exceptionState.throwTypeError("Invalid type for property");
            return;
        }
        m_ownerElement->setInlineStyleProperty(propertyID, cssValue);
    } else if (item.isCSSStyleValueSequence()) {
        if (!CSSPropertyMetadata::propertySupportsMultiple(propertyID)) {
            exceptionState.throwTypeError("Property does not support multiple values");
            return;
        }

        // TODO(meade): This won't always work. Figure out what kind of CSSValueList to create properly.
        CSSValueList* valueList = CSSValueList::createSpaceSeparated();
        CSSStyleValueVector styleValueVector = item.getAsCSSStyleValueSequence();
        for (const Member<CSSStyleValue> value : styleValueVector) {
            CSSValue* cssValue = styleValueToCSSValue(propertyID, *value);
            if (!cssValue) {
                exceptionState.throwTypeError("Invalid type for property");
                return;
            }
            valueList->append(*cssValue);
        }

        m_ownerElement->setInlineStyleProperty(propertyID, valueList);
    } else {
        // Parse it.
        DCHECK(item.isString());
        // TODO(meade): Implement this.
        exceptionState.throwTypeError("Not implemented yet");
    }
}

void InlineStylePropertyMap::append(CSSPropertyID propertyID, CSSStyleValueOrCSSStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    if (!CSSPropertyMetadata::propertySupportsMultiple(propertyID)) {
        exceptionState.throwTypeError("Property does not support multiple values");
        return;
    }

    const CSSValue* cssValue = m_ownerElement->ensureMutableInlineStyle().getPropertyCSSValue(propertyID);
    CSSValueList* cssValueList = nullptr;
    if (cssValue->isValueList()) {
        cssValueList = toCSSValueList(cssValue)->copy();
    } else {
        // TODO(meade): Figure out what the correct behaviour here is.
        exceptionState.throwTypeError("Property is not already list valued");
        return;
    }

    if (item.isCSSStyleValue()) {
        CSSValue* cssValue = styleValueToCSSValue(propertyID, *item.getAsCSSStyleValue());
        if (!cssValue) {
            exceptionState.throwTypeError("Invalid type for property");
            return;
        }
        cssValueList->append(*cssValue);
    } else if (item.isCSSStyleValueSequence()) {
        for (CSSStyleValue* styleValue : item.getAsCSSStyleValueSequence()) {
            CSSValue* cssValue = styleValueToCSSValue(propertyID, *styleValue);
            if (!cssValue) {
                exceptionState.throwTypeError("Invalid type for property");
                return;
            }
            cssValueList->append(*cssValue);
        }
    } else {
        // Parse it.
        DCHECK(item.isString());
        // TODO(meade): Implement this.
        exceptionState.throwTypeError("Not implemented yet");
        return;
    }

    m_ownerElement->setInlineStyleProperty(propertyID, cssValueList);
}

void InlineStylePropertyMap::remove(CSSPropertyID propertyID, ExceptionState& exceptionState)
{
    m_ownerElement->removeInlineStyleProperty(propertyID);
}

HeapVector<StylePropertyMap::StylePropertyMapEntry> InlineStylePropertyMap::getIterationEntries()
{
    HeapVector<StylePropertyMap::StylePropertyMapEntry> result;
    StylePropertySet& inlineStyleSet = m_ownerElement->ensureMutableInlineStyle();
    for (unsigned i = 0; i < inlineStyleSet.propertyCount(); i++) {
        StylePropertySet::PropertyReference propertyReference = inlineStyleSet.propertyAt(i);
        CSSPropertyID propertyID = propertyReference.id();
        CSSStyleValueVector styleValueVector = StyleValueFactory::cssValueToStyleValueVector(propertyID, *propertyReference.value());
        CSSStyleValueOrCSSStyleValueSequence value;
        if (styleValueVector.size() == 1)
            value.setCSSStyleValue(styleValueVector[0]);
        else
            value.setCSSStyleValueSequence(styleValueVector);
        result.append(std::make_pair(getPropertyNameString(propertyID), value));
    }
    return result;
}

} // namespace blink

