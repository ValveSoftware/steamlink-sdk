// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPendingSubstitutionValue_h
#define CSSPendingSubstitutionValue_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSVariableReferenceValue.h"

namespace blink {

class CSSPendingSubstitutionValue : public CSSValue {
public:
    static CSSPendingSubstitutionValue* create(CSSPropertyID shorthandPropertyId, CSSVariableReferenceValue* shorthandValue)
    {
        return new CSSPendingSubstitutionValue(shorthandPropertyId, shorthandValue);
    }

    CSSVariableReferenceValue* shorthandValue() const
    {
        return m_shorthandValue.get();
    }

    CSSPropertyID shorthandPropertyId() const
    {
        return m_shorthandPropertyId;
    }

    bool equals(const CSSPendingSubstitutionValue& other) const { return m_shorthandValue == other.m_shorthandValue; }
    String customCSSText() const;

    DECLARE_TRACE_AFTER_DISPATCH();
private:
    CSSPendingSubstitutionValue(CSSPropertyID shorthandPropertyId, CSSVariableReferenceValue* shorthandValue)
        : CSSValue(PendingSubstitutionValueClass)
        , m_shorthandPropertyId(shorthandPropertyId)
        , m_shorthandValue(shorthandValue)
    {
    }

    CSSPropertyID m_shorthandPropertyId;
    Member<CSSVariableReferenceValue> m_shorthandValue;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSPendingSubstitutionValue, isPendingSubstitutionValue());

} // namespace blink

#endif // CSSPendingSubstitutionValue_h
