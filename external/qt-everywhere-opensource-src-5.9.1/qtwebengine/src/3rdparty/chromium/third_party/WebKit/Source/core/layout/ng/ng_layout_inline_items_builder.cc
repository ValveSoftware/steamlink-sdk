// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_inline_items_builder.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "core/layout/ng/ng_inline_box.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGLayoutInlineItemsBuilder::~NGLayoutInlineItemsBuilder() {
  DCHECK_EQ(0u, exits_.size());
  DCHECK_EQ(text_.length(), items_->isEmpty() ? 0 : items_->last().EndOffset());
}

void NGLayoutInlineItemsBuilder::Append(const String& string,
                                        const ComputedStyle* style) {
  unsigned start_offset = text_.length();
  text_.append(string);
  items_->emplace_back(start_offset, text_.length(), style);
}

void NGLayoutInlineItemsBuilder::Append(UChar32 character,
                                        const ComputedStyle* style) {
  text_.append(character);
  unsigned end_offset = text_.length();
  items_->emplace_back(end_offset - 1, end_offset, style);
}

void NGLayoutInlineItemsBuilder::AppendBidiControl(const ComputedStyle* style,
                                                   UChar32 ltr,
                                                   UChar32 rtl) {
  Append(style->direction() == RTL ? rtl : ltr, nullptr);
}

void NGLayoutInlineItemsBuilder::EnterBlock(const ComputedStyle* style) {
  // Handle bidi-override on the block itself.
  // Isolate and embed values are enforced by default and redundant on the block
  // elements.
  // Plaintext and direction are handled as the paragraph level by
  // NGBidiParagraph::SetParagraph().
  if (style->unicodeBidi() == Override ||
      style->unicodeBidi() == IsolateOverride) {
    AppendBidiControl(style, leftToRightOverrideCharacter,
                      rightToLeftOverrideCharacter);
    Enter(nullptr, popDirectionalFormattingCharacter);
  }
}

void NGLayoutInlineItemsBuilder::EnterInline(LayoutObject* node) {
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  const ComputedStyle* style = node->style();
  switch (style->unicodeBidi()) {
    case UBNormal:
      break;
    case Embed:
      AppendBidiControl(style, leftToRightEmbedCharacter,
                        rightToLeftEmbedCharacter);
      Enter(node, popDirectionalFormattingCharacter);
      break;
    case Override:
      AppendBidiControl(style, leftToRightOverrideCharacter,
                        rightToLeftOverrideCharacter);
      Enter(node, popDirectionalFormattingCharacter);
      break;
    case Isolate:
      AppendBidiControl(style, leftToRightIsolateCharacter,
                        rightToLeftIsolateCharacter);
      Enter(node, popDirectionalIsolateCharacter);
      break;
    case Plaintext:
      Append(firstStrongIsolateCharacter, nullptr);
      Enter(node, popDirectionalIsolateCharacter);
      break;
    case IsolateOverride:
      Append(firstStrongIsolateCharacter, nullptr);
      AppendBidiControl(style, leftToRightOverrideCharacter,
                        rightToLeftOverrideCharacter);
      Enter(node, popDirectionalIsolateCharacter);
      Enter(node, popDirectionalFormattingCharacter);
      break;
  }
}

void NGLayoutInlineItemsBuilder::Enter(LayoutObject* node,
                                       UChar32 character_to_exit) {
  exits_.append(OnExitNode{node, character_to_exit});
}

void NGLayoutInlineItemsBuilder::ExitBlock() {
  Exit(nullptr);
}

void NGLayoutInlineItemsBuilder::ExitInline(LayoutObject* node) {
  DCHECK(node);
  Exit(node);
}

void NGLayoutInlineItemsBuilder::Exit(LayoutObject* node) {
  while (!exits_.isEmpty() && exits_.last().node == node) {
    Append(exits_.last().character, nullptr);
    exits_.pop_back();
  }
}

}  // namespace blink
