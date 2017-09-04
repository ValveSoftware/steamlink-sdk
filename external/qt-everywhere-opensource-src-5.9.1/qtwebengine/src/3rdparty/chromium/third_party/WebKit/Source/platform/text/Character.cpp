/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/text/Character.h"

#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <unicode/uobject.h>
#include <unicode/uscript.h>

#if defined(USING_SYSTEM_ICU)
#include "platform/text/CharacterPropertyDataGenerator.h"
#include <unicode/uniset.h>
#else
#define MUTEX_H  // Prevent compile failure of utrie2.h on Windows
#include <utrie2.h>
#endif

using namespace WTF;
using namespace Unicode;

namespace blink {

#if defined(USING_SYSTEM_ICU)
static icu::UnicodeSet* createUnicodeSet(const UChar32* characters,
                                         size_t charactersCount,
                                         const UChar32* ranges,
                                         size_t rangesCount) {
  icu::UnicodeSet* unicodeSet = new icu::UnicodeSet();
  for (size_t i = 0; i < charactersCount; i++)
    unicodeSet->add(characters[i]);
  for (size_t i = 0; i < rangesCount; i += 2)
    unicodeSet->add(ranges[i], ranges[i + 1]);
  unicodeSet->freeze();
  return unicodeSet;
}

#define CREATE_UNICODE_SET(name)                                             \
  createUnicodeSet(name##Array, WTF_ARRAY_LENGTH(name##Array), name##Ranges, \
                   WTF_ARRAY_LENGTH(name##Ranges))

#define RETURN_HAS_PROPERTY(c, name)            \
  static icu::UnicodeSet* unicodeSet = nullptr; \
  if (!unicodeSet)                              \
    unicodeSet = CREATE_UNICODE_SET(name);      \
  return unicodeSet->contains(c);
#else
// Freezed trie tree, see CharacterDataGenerator.cpp.
extern int32_t serializedCharacterDataSize;
extern uint8_t serializedCharacterData[];

static UTrie2* createTrie() {
  // Create a Trie from the value array.
  UErrorCode error = U_ZERO_ERROR;
  UTrie2* trie = utrie2_openFromSerialized(
      UTrie2ValueBits::UTRIE2_16_VALUE_BITS, serializedCharacterData,
      serializedCharacterDataSize, nullptr, &error);
  ASSERT(error == U_ZERO_ERROR);
  return trie;
}

static bool hasProperty(UChar32 c, CharacterProperty property) {
  static UTrie2* trie = nullptr;
  if (!trie)
    trie = createTrie();
  return UTRIE2_GET16(trie, c) & static_cast<CharacterPropertyType>(property);
}

#define RETURN_HAS_PROPERTY(c, name) \
  return hasProperty(c, CharacterProperty::name);
#endif

// Takes a flattened list of closed intervals
template <class T, size_t size>
bool valueInIntervalList(const T (&intervalList)[size], const T& value) {
  const T* bound =
      std::upper_bound(&intervalList[0], &intervalList[size], value);
  if ((bound - intervalList) % 2 == 1)
    return true;
  return bound > intervalList && *(bound - 1) == value;
}

bool Character::isUprightInMixedVertical(UChar32 character) {
  RETURN_HAS_PROPERTY(character, isUprightInMixedVertical)
}

bool Character::isCJKIdeographOrSymbol(UChar32 c) {
  // Likely common case
  if (c < 0x2C7)
    return false;

  RETURN_HAS_PROPERTY(c, isCJKIdeographOrSymbol)
}

bool Character::isPotentialCustomElementNameChar(UChar32 character) {
  RETURN_HAS_PROPERTY(character, isPotentialCustomElementNameChar);
}

unsigned Character::expansionOpportunityCount(const LChar* characters,
                                              size_t length,
                                              TextDirection direction,
                                              bool& isAfterExpansion,
                                              const TextJustify textJustify) {
  unsigned count = 0;
  if (textJustify == TextJustifyDistribute) {
    isAfterExpansion = true;
    return length;
  }

  if (direction == LTR) {
    for (size_t i = 0; i < length; ++i) {
      if (treatAsSpace(characters[i])) {
        count++;
        isAfterExpansion = true;
      } else {
        isAfterExpansion = false;
      }
    }
  } else {
    for (size_t i = length; i > 0; --i) {
      if (treatAsSpace(characters[i - 1])) {
        count++;
        isAfterExpansion = true;
      } else {
        isAfterExpansion = false;
      }
    }
  }

  return count;
}

unsigned Character::expansionOpportunityCount(const UChar* characters,
                                              size_t length,
                                              TextDirection direction,
                                              bool& isAfterExpansion,
                                              const TextJustify textJustify) {
  unsigned count = 0;
  if (direction == LTR) {
    for (size_t i = 0; i < length; ++i) {
      UChar32 character = characters[i];
      if (treatAsSpace(character)) {
        count++;
        isAfterExpansion = true;
        continue;
      }
      if (U16_IS_LEAD(character) && i + 1 < length &&
          U16_IS_TRAIL(characters[i + 1])) {
        character = U16_GET_SUPPLEMENTARY(character, characters[i + 1]);
        i++;
      }
      if (textJustify == TextJustify::TextJustifyAuto &&
          isCJKIdeographOrSymbol(character)) {
        if (!isAfterExpansion)
          count++;
        count++;
        isAfterExpansion = true;
        continue;
      }
      isAfterExpansion = false;
    }
  } else {
    for (size_t i = length; i > 0; --i) {
      UChar32 character = characters[i - 1];
      if (treatAsSpace(character)) {
        count++;
        isAfterExpansion = true;
        continue;
      }
      if (U16_IS_TRAIL(character) && i > 1 && U16_IS_LEAD(characters[i - 2])) {
        character = U16_GET_SUPPLEMENTARY(characters[i - 2], character);
        i--;
      }
      if (textJustify == TextJustify::TextJustifyAuto &&
          isCJKIdeographOrSymbol(character)) {
        if (!isAfterExpansion)
          count++;
        count++;
        isAfterExpansion = true;
        continue;
      }
      isAfterExpansion = false;
    }
  }
  return count;
}

bool Character::canReceiveTextEmphasis(UChar32 c) {
  CharCategory category = Unicode::category(c);
  if (category & (Separator_Space | Separator_Line | Separator_Paragraph |
                  Other_NotAssigned | Other_Control | Other_Format))
    return false;

  // Additional word-separator characters listed in CSS Text Level 3 Editor's
  // Draft 3 November 2010.
  if (c == ethiopicWordspaceCharacter ||
      c == aegeanWordSeparatorLineCharacter ||
      c == aegeanWordSeparatorDotCharacter ||
      c == ugariticWordDividerCharacter ||
      c == tibetanMarkIntersyllabicTshegCharacter ||
      c == tibetanMarkDelimiterTshegBstarCharacter)
    return false;

  return true;
}

template <typename CharacterType>
static inline String normalizeSpacesInternal(const CharacterType* characters,
                                             unsigned length) {
  StringBuilder normalized;
  normalized.reserveCapacity(length);

  for (unsigned i = 0; i < length; ++i)
    normalized.append(Character::normalizeSpaces(characters[i]));

  return normalized.toString();
}

String Character::normalizeSpaces(const LChar* characters, unsigned length) {
  return normalizeSpacesInternal(characters, length);
}

String Character::normalizeSpaces(const UChar* characters, unsigned length) {
  return normalizeSpacesInternal(characters, length);
}

bool Character::isCommonOrInheritedScript(UChar32 character) {
  UErrorCode status = U_ZERO_ERROR;
  UScriptCode script = uscript_getScript(character, &status);
  return U_SUCCESS(status) &&
         (script == USCRIPT_COMMON || script == USCRIPT_INHERITED);
}

}  // namespace blink
