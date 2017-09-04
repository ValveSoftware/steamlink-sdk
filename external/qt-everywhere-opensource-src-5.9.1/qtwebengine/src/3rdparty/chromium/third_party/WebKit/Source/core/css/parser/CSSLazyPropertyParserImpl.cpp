// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyPropertyParserImpl.h"

#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSParserImpl.h"

namespace blink {

CSSLazyPropertyParserImpl::CSSLazyPropertyParserImpl(CSSParserTokenRange block,
                                                     CSSLazyParsingState* state)
    : CSSLazyPropertyParser(), m_lazyState(state) {
  // Reserve capacity to minimize heap bloat.
  size_t length = block.end() - block.begin();
  m_tokens.reserveCapacity(length);
  m_tokens.append(block.begin(), length);
}

StylePropertySet* CSSLazyPropertyParserImpl::parseProperties() {
  return CSSParserImpl::parseDeclarationListForLazyStyle(
      m_tokens, m_lazyState->context());
}

}  // namespace blink
