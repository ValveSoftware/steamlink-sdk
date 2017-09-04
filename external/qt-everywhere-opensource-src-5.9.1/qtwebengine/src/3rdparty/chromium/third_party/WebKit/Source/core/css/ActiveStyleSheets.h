// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ActiveStyleSheets_h
#define ActiveStyleSheets_h

#include "core/CoreExport.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class CSSStyleSheet;
class RuleSet;
class StyleEngine;
class TreeScope;

using ActiveStyleSheet = std::pair<Member<CSSStyleSheet>, Member<RuleSet>>;
using ActiveStyleSheetVector = HeapVector<ActiveStyleSheet>;

enum ActiveSheetsChange {
  NoActiveSheetsChanged,  // Nothing changed.
  ActiveSheetsChanged,    // Sheets were added and/or inserted.
  ActiveSheetsAppended    // Only additions, and all appended.
};

CORE_EXPORT ActiveSheetsChange
compareActiveStyleSheets(const ActiveStyleSheetVector& oldStyleSheets,
                         const ActiveStyleSheetVector& newStyleSheets,
                         HeapVector<Member<RuleSet>>& changedRuleSets);

}  // namespace blink

#endif  // ActiveStyleSheets_h
