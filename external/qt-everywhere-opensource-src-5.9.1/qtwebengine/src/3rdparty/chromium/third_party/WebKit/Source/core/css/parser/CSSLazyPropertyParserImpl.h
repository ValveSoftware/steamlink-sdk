// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLazyPropertyParserImpl_h
#define CSSLazyPropertyParserImpl_h

#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"
#include "wtf/Vector.h"

namespace blink {

class CSSLazyParsingState;

// This class is responsible for lazily parsing a single CSS declaration list.
class CSSLazyPropertyParserImpl : public CSSLazyPropertyParser {
 public:
  CSSLazyPropertyParserImpl(CSSParserTokenRange block, CSSLazyParsingState*);

  // CSSLazyPropertyParser:
  StylePropertySet* parseProperties() override;

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_lazyState);
    CSSLazyPropertyParser::trace(visitor);
  }

 private:
  Vector<CSSParserToken> m_tokens;
  Member<CSSLazyParsingState> m_lazyState;
};

}  // namespace blink

#endif  // CSSLazyPropertyParserImpl_h
