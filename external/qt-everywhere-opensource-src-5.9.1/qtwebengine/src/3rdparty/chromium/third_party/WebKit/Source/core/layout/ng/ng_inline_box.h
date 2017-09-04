// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineBox_h
#define NGInlineBox_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "platform/fonts/FontFallbackPriority.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextDirection.h"
#include "wtf/text/WTFString.h"
#include <unicode/ubidi.h>
#include <unicode/uscript.h>

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGConstraintSpace;
class NGFragmentBase;
class NGLayoutAlgorithm;
class NGLayoutInlineItem;
class NGLayoutInlineItemsBuilder;
class NGPhysicalFragment;
struct MinAndMaxContentSizes;

// Represents an inline node to be laid out.
// TODO(layout-dev): Make this and NGBox inherit from a common class.
class CORE_EXPORT NGInlineBox : public NGLayoutInputNode {
 public:
  NGInlineBox(LayoutObject* start_inline, ComputedStyle* block_style);
  ~NGInlineBox() override;

  bool Layout(const NGConstraintSpace*, NGFragmentBase**) override;
  NGInlineBox* NextSibling() override;

  // Prepare inline and text content for layout. Must be called before
  // calling the Layout method.
  void PrepareLayout();

  String Text(unsigned start_offset, unsigned end_offset) const {
    return text_content_.substring(start_offset, end_offset);
  }

  DECLARE_VIRTUAL_TRACE();

 protected:
  NGInlineBox();  // This constructor is only for testing.
  void CollectInlines(LayoutObject* start, LayoutObject* last);
  void CollectInlines(LayoutObject* start,
                      LayoutObject* last,
                      NGLayoutInlineItemsBuilder*);
  void CollapseWhiteSpace();
  void SegmentText();
  void ShapeText();

  LayoutObject* start_inline_;
  LayoutObject* last_inline_;
  RefPtr<ComputedStyle> block_style_;

  Member<NGInlineBox> next_sibling_;
  Member<NGLayoutAlgorithm> layout_algorithm_;

  // Text content for all inline items represented by a single NGInlineBox
  // instance. Encoded either as UTF-16 or latin-1 depending on content.
  String text_content_;
  Vector<NGLayoutInlineItem> items_;
};

// Class representing a single text node or styled inline element with text
// content segmented by style, text direction, sideways rotation, font fallback
// priority (text, symbol, emoji, etc) and script (but not by font).
// In this representation TextNodes are merged up into their parent inline
// element where possible.
class NGLayoutInlineItem {
 public:
  NGLayoutInlineItem(unsigned start, unsigned end, const ComputedStyle* style)
      : start_offset_(start),
        end_offset_(end),
        bidi_level_(UBIDI_LTR),
        script_(USCRIPT_INVALID_CODE),
        fallback_priority_(FontFallbackPriority::Invalid),
        rotate_sideways_(false),
        style_(style) {
    DCHECK(end >= start);
  }

  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }
  TextDirection Direction() const { return bidi_level_ & 1 ? RTL : LTR; }
  UScriptCode Script() const { return script_; }
  const ComputedStyle* Style() const { return style_; }

  static void Split(Vector<NGLayoutInlineItem>&,
                    unsigned index,
                    unsigned offset);
  static unsigned SetBidiLevel(Vector<NGLayoutInlineItem>&,
                               unsigned index,
                               unsigned end_offset,
                               UBiDiLevel);

 private:
  unsigned start_offset_;
  unsigned end_offset_;
  UBiDiLevel bidi_level_;
  UScriptCode script_;
  FontFallbackPriority fallback_priority_;
  bool rotate_sideways_;
  const ComputedStyle* style_;
  Vector<RefPtr<const ShapeResult>> shape_results_;

  friend class NGInlineBox;
};

DEFINE_TYPE_CASTS(NGInlineBox,
                  NGLayoutInputNode,
                  node,
                  node->Type() == NGLayoutInputNode::LegacyInline,
                  node.Type() == NGLayoutInputNode::LegacyInline);

}  // namespace blink

#endif  // NGInlineBox_h
