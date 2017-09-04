// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SymbolsIterator_h
#define SymbolsIterator_h

#include "platform/fonts/FontFallbackPriority.h"
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT SymbolsIterator {
  USING_FAST_MALLOC(SymbolsIterator);
  WTF_MAKE_NONCOPYABLE(SymbolsIterator);

 public:
  SymbolsIterator(const UChar* buffer, unsigned bufferSize);

  bool consume(unsigned* symbolsLimit, FontFallbackPriority*);

 private:
  FontFallbackPriority fontFallbackPriorityForCharacter(UChar32);

  std::unique_ptr<UTF16TextIterator> m_utf16Iterator;
  unsigned m_bufferSize;
  UChar32 m_nextChar;
  bool m_atEnd;

  FontFallbackPriority m_currentFontFallbackPriority;
  FontFallbackPriority m_previousFontFallbackPriority;
};

}  // namespace blink

#endif
