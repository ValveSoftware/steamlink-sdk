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
class CSSSyntaxDescriptor;

class CSSVariableData : public RefCounted<CSSVariableData> {
  WTF_MAKE_NONCOPYABLE(CSSVariableData);
  USING_FAST_MALLOC(CSSVariableData);

 public:
  static PassRefPtr<CSSVariableData> create(const CSSParserTokenRange& range,
                                            bool isAnimationTainted,
                                            bool needsVariableResolution) {
    return adoptRef(new CSSVariableData(range, isAnimationTainted,
                                        needsVariableResolution));
  }

  static PassRefPtr<CSSVariableData> createResolved(
      const Vector<CSSParserToken>& resolvedTokens,
      const CSSVariableData& unresolvedData,
      bool isAnimationTainted) {
    return adoptRef(new CSSVariableData(
        resolvedTokens, unresolvedData.m_backingString, isAnimationTainted));
  }

  CSSParserTokenRange tokenRange() const { return m_tokens; }

  const Vector<CSSParserToken>& tokens() const { return m_tokens; }

  bool operator==(const CSSVariableData& other) const;

  bool isAnimationTainted() const { return m_isAnimationTainted; }

  bool needsVariableResolution() const { return m_needsVariableResolution; }

  const CSSValue* parseForSyntax(const CSSSyntaxDescriptor&) const;

  StylePropertySet* propertySet();

 private:
  CSSVariableData(const CSSParserTokenRange&,
                  bool isAnimationTainted,
                  bool needsVariableResolution);

  // We can safely copy the tokens (which have raw pointers to substrings)
  // because StylePropertySets contain references to
  // CSSCustomPropertyDeclarations, which point to the unresolved
  // CSSVariableData values that own the backing strings this will potentially
  // reference.
  CSSVariableData(const Vector<CSSParserToken>& resolvedTokens,
                  String backingString,
                  bool isAnimationTainted)
      : m_backingString(backingString),
        m_tokens(resolvedTokens),
        m_isAnimationTainted(isAnimationTainted),
        m_needsVariableResolution(false),
        m_cachedPropertySet(false) {}

  void consumeAndUpdateTokens(const CSSParserTokenRange&);
  template <typename CharacterType>
  void updateTokens(const CSSParserTokenRange&);

  String m_backingString;
  Vector<CSSParserToken> m_tokens;
  const bool m_isAnimationTainted;
  const bool m_needsVariableResolution;

  // Parsed representation for @apply
  bool m_cachedPropertySet;
  Persistent<StylePropertySet> m_propertySet;
};

}  // namespace blink

#endif  // CSSVariableData_h
