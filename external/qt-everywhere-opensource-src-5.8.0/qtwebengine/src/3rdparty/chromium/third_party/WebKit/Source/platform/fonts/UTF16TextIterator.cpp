/*
 * Copyright (C) 2003, 2006, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 *
 */

#include "platform/fonts/UTF16TextIterator.h"

using namespace WTF;
using namespace Unicode;

namespace blink {

UTF16TextIterator::UTF16TextIterator(const UChar* characters, int length)
    : m_characters(characters)
    , m_charactersEnd(characters + length)
    , m_offset(0)
    , m_endOffset(length)
    , m_currentGlyphLength(0)
{
}

UTF16TextIterator::UTF16TextIterator(const UChar* characters, int currentCharacter, int endOffset, int endCharacter)
    : m_characters(characters)
    , m_charactersEnd(characters + (endCharacter - currentCharacter))
    , m_offset(currentCharacter)
    , m_endOffset(endOffset)
    , m_currentGlyphLength(0)
{
}

bool UTF16TextIterator::isValidSurrogatePair(UChar32& character)
{
    // If we have a surrogate pair, make sure it starts with the high part.
    if (!U16_IS_SURROGATE_LEAD(character))
        return false;

    // Do we have a surrogate pair? If so, determine the full Unicode (32 bit)
    // code point before glyph lookup.
    // Make sure we have another character and it's a low surrogate.
    if (m_characters + 1 >= m_charactersEnd)
        return false;

    UChar low = m_characters[1];
    if (!U16_IS_TRAIL(low))
        return false;
    return true;
}

bool UTF16TextIterator::consumeSurrogatePair(UChar32& character)
{
    ASSERT(U16_IS_SURROGATE(character));

    if (!isValidSurrogatePair(character)) {
        character = replacementCharacter;
        return true;
    }

    UChar low = m_characters[1];
    character = U16_GET_SUPPLEMENTARY(character, low);
    m_currentGlyphLength = 2;
    return true;
}

} // namespace blink
