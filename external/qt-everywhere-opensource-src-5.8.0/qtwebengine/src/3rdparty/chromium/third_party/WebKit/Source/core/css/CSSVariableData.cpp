// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSVariableData.h"

#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringView.h"

namespace blink {

StylePropertySet* CSSVariableData::propertySet()
{
    ASSERT(!m_needsVariableResolution);
    if (!m_cachedPropertySet) {
        m_propertySet = CSSParser::parseCustomPropertySet(m_tokens);
        m_cachedPropertySet = true;
    }
    return m_propertySet.get();
}

template<typename CharacterType> void CSSVariableData::updateTokens(const CSSParserTokenRange& range)
{
    const CharacterType* currentOffset = m_backingString.getCharacters<CharacterType>();
    for (const CSSParserToken& token : range) {
        if (token.hasStringBacking()) {
            unsigned length = token.value().length();
            StringView string(currentOffset, length);
            m_tokens.append(token.copyWithUpdatedString(string));
            currentOffset += length;
        } else {
            m_tokens.append(token);
        }
    }
    ASSERT(currentOffset == m_backingString.getCharacters<CharacterType>() + m_backingString.length());
}

bool CSSVariableData::operator==(const CSSVariableData& other) const
{
    return tokens() == other.tokens();
}

void CSSVariableData::consumeAndUpdateTokens(const CSSParserTokenRange& range)
{
    StringBuilder stringBuilder;
    CSSParserTokenRange localRange = range;

    while (!localRange.atEnd()) {
        CSSParserToken token = localRange.consume();
        if (token.hasStringBacking())
            stringBuilder.append(token.value());
    }
    m_backingString = stringBuilder.toString();
    if (m_backingString.is8Bit())
        updateTokens<LChar>(range);
    else
        updateTokens<UChar>(range);
}

CSSVariableData::CSSVariableData(const CSSParserTokenRange& range, bool needsVariableResolution)
    : m_needsVariableResolution(needsVariableResolution)
    , m_cachedPropertySet(false)
{
    ASSERT(!range.atEnd());
    consumeAndUpdateTokens(range);
}

} // namespace blink
