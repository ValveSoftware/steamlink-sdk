// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/AtomicHTMLToken.h"

namespace blink {

QualifiedName AtomicHTMLToken::nameForAttribute(
    const HTMLToken::Attribute& attribute) const {
  return QualifiedName(nullAtom, attribute.name(), nullAtom);
}

bool AtomicHTMLToken::usesName() const {
  return m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag ||
         m_type == HTMLToken::DOCTYPE;
}

bool AtomicHTMLToken::usesAttributes() const {
  return m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag;
}

#ifndef NDEBUG
const char* toString(HTMLToken::TokenType type) {
  switch (type) {
#define DEFINE_STRINGIFY(type) \
  case HTMLToken::type:        \
    return #type;
    DEFINE_STRINGIFY(Uninitialized);
    DEFINE_STRINGIFY(DOCTYPE);
    DEFINE_STRINGIFY(StartTag);
    DEFINE_STRINGIFY(EndTag);
    DEFINE_STRINGIFY(Comment);
    DEFINE_STRINGIFY(Character);
    DEFINE_STRINGIFY(EndOfFile);
#undef DEFINE_STRINGIFY
  }
  return "<unknown>";
}

void AtomicHTMLToken::show() const {
  printf("AtomicHTMLToken %s", toString(m_type));
  switch (m_type) {
    case HTMLToken::StartTag:
    case HTMLToken::EndTag:
      if (m_selfClosing)
        printf(" selfclosing");
    /* FALL THROUGH */
    case HTMLToken::DOCTYPE:
      printf(" name \"%s\"", m_name.getString().utf8().data());
      break;
    case HTMLToken::Comment:
    case HTMLToken::Character:
      printf(" data \"%s\"", m_data.utf8().data());
      break;
    default:
      break;
  }
  // TODO(kouhei): print m_attributes?
  printf("\n");
}
#endif

}  // namespace blink
