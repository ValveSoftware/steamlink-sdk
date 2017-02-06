// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

namespace {

class StylePropertyMapIterationSource final : public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::IterationSource {
public:
    explicit StylePropertyMapIterationSource(HeapVector<StylePropertyMap::StylePropertyMapEntry> values)
        : m_index(0)
        , m_values(values)
    {
    }

    bool next(ScriptState*, String& key, CSSStyleValueOrCSSStyleValueSequence& value, ExceptionState&) override
    {
        if (m_index >= m_values.size())
            return false;

        const StylePropertyMap::StylePropertyMapEntry& pair = m_values.at(m_index++);
        key = pair.first;
        value = pair.second;
        return true;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_values);
        PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::IterationSource::trace(visitor);
    }

private:
    size_t m_index;
    const HeapVector<StylePropertyMap::StylePropertyMapEntry> m_values;
};

} // namespace

CSSStyleValue* StylePropertyMap::get(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID == CSSPropertyInvalid) {
        // TODO(meade): Handle custom properties here.
        exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
        return nullptr;
    }

    CSSStyleValueVector styleVector = getAllInternal(propertyID);
    if (styleVector.isEmpty())
        return nullptr;

    return styleVector[0];
}

CSSStyleValueVector StylePropertyMap::getAll(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return getAllInternal(propertyID);

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return CSSStyleValueVector();
}

bool StylePropertyMap::has(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return !getAllInternal(propertyID).isEmpty();

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return false;
}

void StylePropertyMap::set(const String& propertyName, CSSStyleValueOrCSSStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        set(propertyID, item, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::append(const String& propertyName, CSSStyleValueOrCSSStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        append(propertyID, item, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::remove(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        remove(propertyID, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

StylePropertyMap::IterationSource* StylePropertyMap::startIteration(ScriptState*, ExceptionState&)
{
    return new StylePropertyMapIterationSource(getIterationEntries());
}

} // namespace blink
