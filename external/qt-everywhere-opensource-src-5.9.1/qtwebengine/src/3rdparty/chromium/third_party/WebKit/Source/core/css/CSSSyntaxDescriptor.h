// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSyntaxDescriptor_h
#define CSSSyntaxDescriptor_h

#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

class CSSValue;

enum class CSSSyntaxType {
  TokenStream,
  Ident,
  Length,
  Number,
  Percentage,
  LengthPercentage,
  Color,
  Image,
  Url,
  Integer,
  Angle,
  Time,
  Resolution,
  TransformFunction,
  CustomIdent,
};

struct CSSSyntaxComponent {
  CSSSyntaxComponent(CSSSyntaxType type, const String& string, bool repeatable)
      : m_type(type), m_string(string), m_repeatable(repeatable) {}

  CSSSyntaxType m_type;
  String m_string;  // Only used when m_type is CSSSyntaxType::Ident
  bool m_repeatable;
};

class CSSSyntaxDescriptor {
 public:
  CSSSyntaxDescriptor(String syntax);

  const CSSValue* parse(CSSParserTokenRange, bool isAnimationTainted) const;
  bool isValid() const { return !m_syntaxComponents.isEmpty(); }
  bool isTokenStream() const {
    return m_syntaxComponents.size() == 1 &&
           m_syntaxComponents[0].m_type == CSSSyntaxType::TokenStream;
  }

 private:
  Vector<CSSSyntaxComponent> m_syntaxComponents;
};

}  // namespace blink

#endif  // CSSSyntaxDescriptor_h
