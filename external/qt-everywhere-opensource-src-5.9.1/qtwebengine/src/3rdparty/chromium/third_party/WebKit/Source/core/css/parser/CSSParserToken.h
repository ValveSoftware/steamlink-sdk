// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserToken_h
#define CSSParserToken_h

#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/Allocator.h"
#include "wtf/text/StringView.h"

namespace blink {

enum CSSParserTokenType {
  IdentToken = 0,
  FunctionToken,
  AtKeywordToken,
  HashToken,
  UrlToken,
  BadUrlToken,
  DelimiterToken,
  NumberToken,
  PercentageToken,
  DimensionToken,
  IncludeMatchToken,
  DashMatchToken,
  PrefixMatchToken,
  SuffixMatchToken,
  SubstringMatchToken,
  ColumnToken,
  UnicodeRangeToken,
  WhitespaceToken,
  CDOToken,
  CDCToken,
  ColonToken,
  SemicolonToken,
  CommaToken,
  LeftParenthesisToken,
  RightParenthesisToken,
  LeftBracketToken,
  RightBracketToken,
  LeftBraceToken,
  RightBraceToken,
  StringToken,
  BadStringToken,
  EOFToken,
  CommentToken,
};

enum NumericSign {
  NoSign,
  PlusSign,
  MinusSign,
};

enum NumericValueType {
  IntegerValueType,
  NumberValueType,
};

enum HashTokenType {
  HashTokenId,
  HashTokenUnrestricted,
};

class CORE_EXPORT CSSParserToken {
  USING_FAST_MALLOC(CSSParserToken);

 public:
  enum BlockType {
    NotBlock,
    BlockStart,
    BlockEnd,
  };

  CSSParserToken(CSSParserTokenType, BlockType = NotBlock);
  CSSParserToken(CSSParserTokenType, StringView, BlockType = NotBlock);

  CSSParserToken(CSSParserTokenType, UChar);  // for DelimiterToken
  CSSParserToken(CSSParserTokenType,
                 double,
                 NumericValueType,
                 NumericSign);  // for NumberToken
  CSSParserToken(CSSParserTokenType,
                 UChar32,
                 UChar32);  // for UnicodeRangeToken

  CSSParserToken(HashTokenType, StringView);

  bool operator==(const CSSParserToken& other) const;
  bool operator!=(const CSSParserToken& other) const {
    return !(*this == other);
  }

  // Converts NumberToken to DimensionToken.
  void convertToDimensionWithUnit(StringView);

  // Converts NumberToken to PercentageToken.
  void convertToPercentage();

  CSSParserTokenType type() const {
    return static_cast<CSSParserTokenType>(m_type);
  }
  StringView value() const {
    if (m_valueIs8Bit)
      return StringView(reinterpret_cast<const LChar*>(m_valueDataCharRaw),
                        m_valueLength);
    return StringView(reinterpret_cast<const UChar*>(m_valueDataCharRaw),
                      m_valueLength);
  }

  UChar delimiter() const;
  NumericSign numericSign() const;
  NumericValueType numericValueType() const;
  double numericValue() const;
  HashTokenType getHashTokenType() const {
    ASSERT(m_type == HashToken);
    return m_hashTokenType;
  }
  BlockType getBlockType() const { return static_cast<BlockType>(m_blockType); }
  CSSPrimitiveValue::UnitType unitType() const {
    return static_cast<CSSPrimitiveValue::UnitType>(m_unit);
  }
  UChar32 unicodeRangeStart() const {
    ASSERT(m_type == UnicodeRangeToken);
    return m_unicodeRange.start;
  }
  UChar32 unicodeRangeEnd() const {
    ASSERT(m_type == UnicodeRangeToken);
    return m_unicodeRange.end;
  }
  CSSValueID id() const;
  CSSValueID functionId() const;

  bool hasStringBacking() const;

  CSSPropertyID parseAsUnresolvedCSSPropertyID() const;

  void serialize(StringBuilder&) const;

  CSSParserToken copyWithUpdatedString(const StringView&) const;

 private:
  void initValueFromStringView(StringView string) {
    m_valueLength = string.length();
    m_valueIs8Bit = string.is8Bit();
    m_valueDataCharRaw = string.bytes();
  }
  unsigned m_type : 6;              // CSSParserTokenType
  unsigned m_blockType : 2;         // BlockType
  unsigned m_numericValueType : 1;  // NumericValueType
  unsigned m_numericSign : 2;       // NumericSign
  unsigned m_unit : 7;              // CSSPrimitiveValue::UnitType

  bool valueDataCharRawEqual(const CSSParserToken& other) const;

  // m_value... is an unpacked StringView so that we can pack it
  // tightly with the rest of this object for a smaller object size.
  bool m_valueIs8Bit : 1;
  unsigned m_valueLength;
  const void* m_valueDataCharRaw;  // Either LChar* or UChar*.

  union {
    UChar m_delimiter;
    HashTokenType m_hashTokenType;
    double m_numericValue;
    mutable int m_id;

    struct {
      UChar32 start;
      UChar32 end;
    } m_unicodeRange;
  };
};

}  // namespace blink

#endif  // CSSSParserToken_h
