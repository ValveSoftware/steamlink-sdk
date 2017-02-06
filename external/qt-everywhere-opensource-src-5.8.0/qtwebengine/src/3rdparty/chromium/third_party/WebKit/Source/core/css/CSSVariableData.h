// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableData_h
#define CSSVariableData_h

#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSParserTokenRange;

class CSSVariableData : public RefCounted<CSSVariableData> {
    WTF_MAKE_NONCOPYABLE(CSSVariableData);
    USING_FAST_MALLOC(CSSVariableData);
public:
    static PassRefPtr<CSSVariableData> create(const CSSParserTokenRange& range, bool needsVariableResolution = true)
    {
        return adoptRef(new CSSVariableData(range, needsVariableResolution));
    }

    static PassRefPtr<CSSVariableData> createResolved(const Vector<CSSParserToken>& resolvedTokens, const CSSVariableData& unresolvedData)
    {
        return adoptRef(new CSSVariableData(resolvedTokens, unresolvedData.m_backingString));
    }

    CSSParserTokenRange tokenRange() { return m_tokens; }

    const Vector<CSSParserToken>& tokens() const { return m_tokens; }

    bool operator==(const CSSVariableData& other) const;

    bool needsVariableResolution() const { return m_needsVariableResolution; }

    StylePropertySet* propertySet();

private:
    CSSVariableData(const CSSParserTokenRange&, bool needsVariableResolution);

    // We can safely copy the tokens (which have raw pointers to substrings) because
    // StylePropertySets contain references to CSSCustomPropertyDeclarations, which
    // point to the unresolved CSSVariableData values that own the backing strings
    // this will potentially reference.
    CSSVariableData(const Vector<CSSParserToken>& resolvedTokens, String backingString)
        : m_backingString(backingString)
        , m_tokens(resolvedTokens)
        , m_needsVariableResolution(false)
        , m_cachedPropertySet(false)
    { }

    void consumeAndUpdateTokens(const CSSParserTokenRange&);
    template<typename CharacterType> void updateTokens(const CSSParserTokenRange&);

    String m_backingString;
    Vector<CSSParserToken> m_tokens;
    const bool m_needsVariableResolution;

    // Parsed representation for @apply
    bool m_cachedPropertySet;
    Persistent<StylePropertySet> m_propertySet;
};

} // namespace blink

#endif // CSSVariableData_h
