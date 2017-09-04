// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutInlineItemsBuilder_h
#define NGLayoutInlineItemsBuilder_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class NGLayoutInlineItem;

// AppendBidiControlForBlockIfNeeded builds a string and a list of
// NGLayoutInlineItem from inlines.
//
// By calling EnterInline/ExitInline, it inserts bidirectional control
// characters as defined in:
// https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
class CORE_EXPORT NGLayoutInlineItemsBuilder {
  STACK_ALLOCATED();

 public:
  explicit NGLayoutInlineItemsBuilder(Vector<NGLayoutInlineItem>* items)
      : items_(items) {}
  ~NGLayoutInlineItemsBuilder();

  String ToString() { return text_.toString(); }

  void Append(const String&, const ComputedStyle*);
  void Append(UChar32, const ComputedStyle*);
  void AppendBidiControl(const ComputedStyle*, UChar32 ltr, UChar32 rtl);

  void EnterBlock(const ComputedStyle*);
  void ExitBlock();
  void EnterInline(LayoutObject*);
  void ExitInline(LayoutObject*);

 private:
  Vector<NGLayoutInlineItem>* items_;
  StringBuilder text_;

  typedef struct OnExitNode {
    LayoutObject* node;
    UChar32 character;
  } OnExitNode;
  Vector<OnExitNode> exits_;

  void Enter(LayoutObject*, UChar32);
  void Exit(LayoutObject*);
};

}  // namespace blink

#endif  // NGLayoutInlineItemsBuilder_h
