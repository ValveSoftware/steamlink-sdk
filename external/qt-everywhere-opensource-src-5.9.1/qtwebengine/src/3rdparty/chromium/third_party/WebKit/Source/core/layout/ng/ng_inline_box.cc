// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_box.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/style/ComputedStyle.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_bidi_paragraph.h"
#include "core/layout/ng/ng_layout_inline_items_builder.h"
#include "core/layout/ng/ng_text_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/text/TextDirection.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/fonts/shaping/CachingWordShapeIterator.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

NGInlineBox::NGInlineBox(LayoutObject* start_inline, ComputedStyle* block_style)
    : NGLayoutInputNode(NGLayoutInputNodeType::LegacyInline),
      start_inline_(start_inline),
      last_inline_(nullptr),
      block_style_(block_style) {
  DCHECK(start_inline);
  PrepareLayout();  // TODO(layout-dev): Shouldn't be called here.
}

NGInlineBox::NGInlineBox()
    : NGLayoutInputNode(NGLayoutInputNodeType::LegacyInline),
      start_inline_(nullptr),
      last_inline_(nullptr),
      block_style_(nullptr) {}

NGInlineBox::~NGInlineBox() {}

void NGInlineBox::PrepareLayout() {
  // Scan list of siblings collecting all in-flow non-atomic inlines. A single
  // NGInlineBox represent a collection of adjacent non-atomic inlines.
  last_inline_ = start_inline_;
  for (LayoutObject* curr = start_inline_; curr; curr = curr->nextSibling())
    last_inline_ = curr;

  CollectInlines(start_inline_, last_inline_);
  CollapseWhiteSpace();
  SegmentText();
  ShapeText();
}

// Depth-first-scan of all LayoutInline and LayoutText nodes that make up this
// NGInlineBox object. Collects LayoutText items, merging them up into the
// parent LayoutInline where possible, and joining all text content in a single
// string to allow bidi resolution and shaping of the entire block.
void NGInlineBox::CollectInlines(LayoutObject* start, LayoutObject* last) {
  NGLayoutInlineItemsBuilder builder(&items_);
  builder.EnterBlock(block_style_.get());
  CollectInlines(start, last, &builder);
  builder.ExitBlock();
  text_content_ = builder.ToString();
}

void NGInlineBox::CollectInlines(LayoutObject* start,
                                 LayoutObject* last,
                                 NGLayoutInlineItemsBuilder* builder) {
  LayoutObject* node = start;
  while (node) {
    if (node->isText()) {
      builder->Append(toLayoutText(node)->text(), node->style());
    } else if (node->isFloating() || node->isOutOfFlowPositioned()) {
      // Skip positioned objects.
    } else if (!node->isInline()) {
      // TODO(kojii): Implement when inline has block children.
    } else {
      builder->EnterInline(node);

      // For atomic inlines add a unicode "object replacement character" to
      // signal the presence of a non-text object to the unicode bidi algorithm.
      if (node->isAtomicInlineLevel()) {
        builder->Append(objectReplacementCharacter, nullptr);
      }

      // Otherwise traverse to children if they exist.
      else if (LayoutObject* child = node->slowFirstChild()) {
        node = child;
        continue;
      }

      builder->ExitInline(node);
    }

    while (true) {
      if (LayoutObject* next = node->nextSibling()) {
        node = next;
        break;
      }
      node = node->parent();
      builder->ExitInline(node);
      if (node == start || node == start->parent())
        return;
    }
  }
}

void NGInlineBox::CollapseWhiteSpace() {
  // TODO(eae): Implement. This needs to adjust the offsets for each item as it
  // collapses whitespace.
}

void NGInlineBox::SegmentText() {
  if (text_content_.isEmpty())
    return;
  // TODO(kojii): Move this to caller, this will be used again after line break.
  NGBidiParagraph bidi;
  text_content_.ensure16Bit();
  if (!bidi.SetParagraph(text_content_, block_style_.get())) {
    // On failure, give up bidi resolving and reordering.
    NOTREACHED();
    return;
  }
  UBiDiDirection direction = bidi.Direction();
  if (direction != UBIDI_MIXED) {
    // TODO(kojii): Only LTR or RTL, we can have an optimized code path.
  }
  unsigned item_index = 0;
  for (unsigned start = 0; start < text_content_.length();) {
    UBiDiLevel level;
    unsigned end = bidi.GetLogicalRun(start, &level);
    DCHECK_EQ(items_[item_index].start_offset_, start);
    item_index =
        NGLayoutInlineItem::SetBidiLevel(items_, item_index, end, level);
    start = end;
  }
  DCHECK_EQ(item_index, items_.size());
}

// Set bidi level to a list of NGLayoutInlineItem from |index| to the item that
// ends with |end_offset|.
// If |end_offset| is mid of an item, the item is split to ensure each item has
// one bidi level.
// @param items The list of NGLayoutInlineItem.
// @param index The first index of the list to set.
// @param end_offset The exclusive end offset to set.
// @param level The level to set.
// @return The index of the next item.
unsigned NGLayoutInlineItem::SetBidiLevel(Vector<NGLayoutInlineItem>& items,
                                          unsigned index,
                                          unsigned end_offset,
                                          UBiDiLevel level) {
  for (; items[index].end_offset_ < end_offset; index++)
    items[index].bidi_level_ = level;
  items[index].bidi_level_ = level;
  if (items[index].end_offset_ > end_offset)
    Split(items, index, end_offset);
  return index + 1;
}

// Split |items[index]| to 2 items at |offset|.
// All properties other than offsets are copied to the new item and it is
// inserted at |items[index + 1]|.
// @param items The list of NGLayoutInlineItem.
// @param index The index to split.
// @param offset The offset to split at.
void NGLayoutInlineItem::Split(Vector<NGLayoutInlineItem>& items,
                               unsigned index,
                               unsigned offset) {
  DCHECK_GT(offset, items[index].start_offset_);
  DCHECK_LT(offset, items[index].end_offset_);
  items.insert(index + 1, items[index]);
  items[index].end_offset_ = offset;
  items[index + 1].start_offset_ = offset;
}

void NGInlineBox::ShapeText() {
  // TODO(layout-dev): Should pass the entire range to the shaper as context
  // and then shape each item based on the relevant font.
  for (auto& item : items_) {
    // Skip object replacement characters and bidi control characters.
    if (!item.style_)
      continue;
    StringView item_text(text_content_, item.start_offset_,
                         item.end_offset_ - item.start_offset_);
    const Font& item_font = item.style_->font();
    ShapeCache* shape_cache = item_font.shapeCache();

    TextRun item_run(item_text);
    CachingWordShapeIterator iterator(shape_cache, item_run, &item_font);
    RefPtr<const ShapeResult> word_result;
    while (iterator.next(&word_result)) {
      item.shape_results_.append(word_result.get());
    };
  }
}

bool NGInlineBox::Layout(const NGConstraintSpace* constraint_space,
                         NGFragmentBase** out) {
  // TODO(layout-dev): Perform pre-layout text step.

  // NOTE: We don't need to change the coordinate system here as we are an
  // inline.
  NGConstraintSpace* child_constraint_space = new NGConstraintSpace(
      constraint_space->WritingMode(), constraint_space->Direction(),
      constraint_space->MutablePhysicalSpace());

  if (!layout_algorithm_)
    // TODO(layout-dev): If an atomic inline run the appropriate algorithm.
    layout_algorithm_ = new NGTextLayoutAlgorithm(this, child_constraint_space);

  NGPhysicalFragmentBase* fragment = nullptr;
  if (!layout_algorithm_->Layout(nullptr, &fragment, nullptr))
    return false;

  // TODO(layout-dev): Implement copying of fragment data to LayoutObject tree.

  *out = new NGTextFragment(constraint_space->WritingMode(),
                            constraint_space->Direction(),
                            toNGPhysicalTextFragment(fragment));

  // Reset algorithm for future use
  layout_algorithm_ = nullptr;
  return true;
}

NGInlineBox* NGInlineBox::NextSibling() {
  if (!next_sibling_) {
    LayoutObject* next_sibling =
        last_inline_ ? last_inline_->nextSibling() : nullptr;
    next_sibling_ = next_sibling
                        ? new NGInlineBox(next_sibling, block_style_.get())
                        : nullptr;
  }
  return next_sibling_;
}

DEFINE_TRACE(NGInlineBox) {
  visitor->trace(next_sibling_);
  visitor->trace(layout_algorithm_);
  NGLayoutInputNode::trace(visitor);
}

}  // namespace blink
