// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextSearcherICU_h
#define TextSearcherICU_h

#include "core/CoreExport.h"
#include "wtf/text/StringView.h"
#include "wtf/text/Unicode.h"

struct UStringSearch;

namespace blink {

struct CORE_EXPORT MatchResult {
  size_t start;
  size_t length;
};

class CORE_EXPORT TextSearcherICU {
  WTF_MAKE_NONCOPYABLE(TextSearcherICU);

 public:
  TextSearcherICU();
  ~TextSearcherICU();

  void setPattern(const StringView& pattern, bool sensitive);
  void setText(const UChar* text, size_t length);
  void setOffset(size_t);
  bool nextMatchResult(MatchResult&);

 private:
  void setPattern(const UChar* pattern, size_t length);
  void setCaseSensitivity(bool caseSensitive);

  UStringSearch* m_searcher = nullptr;
  size_t m_textLength = 0;
};

}  // namespace blink

#endif  // TextSearcherICU_h
