/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSValuePair_h
#define CSSValuePair_h

#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/style/ComputedStyle.h"
#include "platform/Length.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

class CORE_EXPORT CSSValuePair : public CSSValue {
public:
    enum IdenticalValuesPolicy { DropIdenticalValues, KeepIdenticalValues };

    static CSSValuePair* create(CSSValue* first, CSSValue* second,
        IdenticalValuesPolicy identicalValuesPolicy)
    {
        return new CSSValuePair(first, second, identicalValuesPolicy);
    }

    static CSSValuePair* create(const LengthSize& lengthSize, const ComputedStyle& style)
    {
        return new CSSValuePair(CSSPrimitiveValue::create(lengthSize.width(), style.effectiveZoom()), CSSPrimitiveValue::create(lengthSize.height(), style.effectiveZoom()), KeepIdenticalValues);
    }

    // TODO(sashab): Remove these non-const versions.
    CSSValue& first() { return *m_first; }
    CSSValue& second() { return *m_second; }
    const CSSValue& first() const { return *m_first; }
    const CSSValue& second() const { return *m_second; }

    String customCSSText() const
    {
        String first = m_first->cssText();
        String second = m_second->cssText();
        if (m_identicalValuesPolicy == DropIdenticalValues && first == second)
            return first;
        return first + ' ' + second;
    }

    bool equals(const CSSValuePair& other) const
    {
        ASSERT(m_identicalValuesPolicy == other.m_identicalValuesPolicy);
        return compareCSSValuePtr(m_first, other.m_first)
            && compareCSSValuePtr(m_second, other.m_second);
    }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSValuePair(CSSValue* first, CSSValue* second, IdenticalValuesPolicy identicalValuesPolicy)
        : CSSValue(ValuePairClass)
        , m_first(first)
        , m_second(second)
        , m_identicalValuesPolicy(identicalValuesPolicy)
    {
        ASSERT(m_first);
        ASSERT(m_second);
    }

    Member<CSSValue> m_first;
    Member<CSSValue> m_second;
    IdenticalValuesPolicy m_identicalValuesPolicy;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSValuePair, isValuePair());

} // namespace blink

#endif // CSSValuePair_h
