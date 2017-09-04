// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGUnits_h
#define NGUnits_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/text/TextDirection.h"
#include "wtf/text/WTFString.h"

namespace blink {

class LayoutUnit;
struct NGPhysicalOffset;
struct NGPhysicalSize;
struct NGBoxStrut;

struct MinAndMaxContentSizes {
  LayoutUnit min_content;
  LayoutUnit max_content;
  LayoutUnit ShrinkToFit(LayoutUnit available_size) const;
};

struct NGLogicalSize {
  NGLogicalSize() {}
  NGLogicalSize(LayoutUnit inline_size, LayoutUnit block_size)
      : inline_size(inline_size), block_size(block_size) {}

  LayoutUnit inline_size;
  LayoutUnit block_size;

  NGPhysicalSize ConvertToPhysical(NGWritingMode mode) const;
  bool operator==(const NGLogicalSize& other) const;

  bool IsEmpty() const {
    return inline_size == LayoutUnit() || block_size == LayoutUnit();
  }
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGLogicalSize& value) {
  return stream << value.inline_size << "x" << value.block_size;
}

// NGLogicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the logical coordinate system.
struct NGLogicalOffset {
  NGLogicalOffset() {}
  NGLogicalOffset(LayoutUnit inline_offset, LayoutUnit block_offset)
      : inline_offset(inline_offset), block_offset(block_offset) {}

  LayoutUnit inline_offset;
  LayoutUnit block_offset;

  // Converts a logical offset to a physical offset. See:
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  // @param outer_size the size of the rect (typically a fragment).
  // @param inner_size the size of the inner rect (typically a child fragment).
  CORE_EXPORT NGPhysicalOffset
  ConvertToPhysical(NGWritingMode,
                    TextDirection,
                    NGPhysicalSize outer_size,
                    NGPhysicalSize inner_size) const;
  bool operator==(const NGLogicalOffset& other) const;

  NGLogicalOffset operator+(const NGLogicalOffset& other) const;

  NGLogicalOffset& operator+=(const NGLogicalOffset& other);

  bool operator>(const NGLogicalOffset& other) const;
  bool operator>=(const NGLogicalOffset& other) const;

  bool operator<(const NGLogicalOffset& other) const;
  bool operator<=(const NGLogicalOffset& other) const;

  String ToString() const;
};

CORE_EXPORT inline std::ostream& operator<<(std::ostream& os,
                                            const NGLogicalOffset& value) {
  return os << value.ToString();
}

// NGPhysicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the physical coordinate system.
struct NGPhysicalOffset {
  NGPhysicalOffset() {}
  NGPhysicalOffset(LayoutUnit left, LayoutUnit top) : left(left), top(top) {}

  LayoutUnit left;
  LayoutUnit top;
};

struct NGPhysicalSize {
  NGPhysicalSize() {}
  NGPhysicalSize(LayoutUnit width, LayoutUnit height)
      : width(width), height(height) {}

  LayoutUnit width;
  LayoutUnit height;

  NGLogicalSize ConvertToLogical(NGWritingMode mode) const;

  String ToString() const {
    return String::format("%dx%d", width.toInt(), height.toInt());
  }
};

// NGPhysicalLocation is the position of a rect (typically a fragment) relative
// to the root document.
struct NGPhysicalLocation {
  LayoutUnit left;
  LayoutUnit top;
};

struct NGPhysicalRect {
  NGPhysicalOffset offset;
  NGPhysicalSize size;
};

// TODO(glebl): move to a separate file in layout/ng/units.
struct CORE_EXPORT NGLogicalRect {
  NGLogicalRect() {}
  NGLogicalRect(LayoutUnit inline_offset,
                LayoutUnit block_offset,
                LayoutUnit inline_size,
                LayoutUnit block_size)
      : offset(inline_offset, block_offset), size(inline_size, block_size) {}

  bool IsEmpty() const;

  // Whether this rectangle is contained by the provided rectangle.
  bool IsContained(const NGLogicalRect& other) const;

  String ToString() const;
  bool operator==(const NGLogicalRect& other) const;

  // Getters
  LayoutUnit InlineStartOffset() const { return offset.inline_offset; }

  LayoutUnit InlineEndOffset() const {
    return offset.inline_offset + size.inline_size;
  }

  LayoutUnit BlockStartOffset() const { return offset.block_offset; }

  LayoutUnit BlockEndOffset() const {
    return offset.block_offset + size.block_size;
  }

  LayoutUnit BlockSize() const { return size.block_size; }

  LayoutUnit InlineSize() const { return size.inline_size; }

  NGLogicalOffset offset;
  NGLogicalSize size;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGLogicalRect& value) {
  return stream << value.ToString();
}

// Struct that represents NG exclusion.
struct CORE_EXPORT NGExclusion {
  // Type of NG exclusion.
  enum Type {
    // Undefined exclusion type.
    // At this moment it's also used to represent CSS3 exclusion.
    NG_EXCLUSION_TYPE_UNDEFINED = 0,
    // Exclusion that is created by LEFT float.
    NG_FLOAT_LEFT = 1,
    // Exclusion that is created by RIGHT float.
    NG_FLOAT_RIGHT = 2
  };

  // Rectangle in logical coordinates the represents this exclusion.
  NGLogicalRect rect;

  // Type of this exclusion.
  Type type;
};

struct NGPixelSnappedPhysicalRect {
  int top;
  int left;
  int width;
  int height;
};

// Struct to store physical dimensions, independent of writing mode and
// direction.
// See https://drafts.csswg.org/css-writing-modes-3/#abstract-box
struct CORE_EXPORT NGPhysicalBoxStrut {
  LayoutUnit left;
  LayoutUnit right;
  LayoutUnit top;
  LayoutUnit bottom;
  NGBoxStrut ConvertToLogical(NGWritingMode, TextDirection) const;
};

// This struct is used for storing margins, borders or padding of a box on all
// four edges.
struct CORE_EXPORT NGBoxStrut {
  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit block_start;
  LayoutUnit block_end;

  LayoutUnit InlineSum() const { return inline_start + inline_end; }
  LayoutUnit BlockSum() const { return block_start + block_end; }

  bool IsEmpty() const;

  // The following two operators exist primarily to have an easy way to access
  // the sum of border and padding.
  NGBoxStrut& operator+=(const NGBoxStrut& other) {
    inline_start += other.inline_start;
    inline_end += other.inline_end;
    block_start += other.block_start;
    block_end += other.block_end;
    return *this;
  }

  NGBoxStrut operator+(const NGBoxStrut& other) {
    NGBoxStrut result(*this);
    result += other;
    return result;
  }

  bool operator==(const NGBoxStrut& other) const;
};

// This struct is used for the margin collapsing calculation.
struct CORE_EXPORT NGMarginStrut {
  LayoutUnit margin_block_start;
  LayoutUnit margin_block_end;

  LayoutUnit negative_margin_block_start;
  LayoutUnit negative_margin_block_end;

  LayoutUnit BlockEndSum() const;

  void AppendMarginBlockStart(const LayoutUnit& value);
  void AppendMarginBlockEnd(const LayoutUnit& value);
  void SetMarginBlockStart(const LayoutUnit& value);
  void SetMarginBlockEnd(const LayoutUnit& value);

  bool IsEmpty() const;

  String ToString() const;

  bool operator==(const NGMarginStrut& other) const;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGMarginStrut& value) {
  return stream << value.ToString();
}

// Struct to represent a simple edge that has start and end.
struct NGEdge {
  LayoutUnit start;
  LayoutUnit end;
};

}  // namespace blink

#endif  // NGUnits_h
