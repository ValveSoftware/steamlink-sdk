// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"
#include "wtf/Optional.h"

namespace blink {
// TODO(layout-ng):
// - positioned and/or replaced calculations
// - Take scrollbars into account

bool NeedMinAndMaxContentSizes(const ComputedStyle& style) {
  // TODO(layout-ng): In the future we may pass a shrink-to-fit flag through the
  // constraint space; if so, this function needs to take a constraint space
  // as well to take that into account.
  // This check is technically too broad (fill-available does not need intrinsic
  // size computation) but that's a rare case and only affects performance, not
  // correctness.
  return style.logicalWidth().isIntrinsic() ||
         style.logicalMinWidth().isIntrinsic() ||
         style.logicalMaxWidth().isIntrinsic();
}

LayoutUnit ResolveInlineLength(
    const NGConstraintSpace& constraintSpace,
    const ComputedStyle& style,
    const WTF::Optional<MinAndMaxContentSizes>& min_and_max,
    const Length& length,
    LengthResolveType type) {
  // TODO(layout-ng): Handle min/max/fit-content
  DCHECK(!length.isMaxSizeNone());
  DCHECK_GE(constraintSpace.AvailableSize().inline_size, LayoutUnit());

  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();

  if (type == LengthResolveType::MarginBorderPaddingSize && length.isAuto())
    return LayoutUnit();

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::MarginBorderPaddingSize) {
    border_and_padding =
        ComputeBorders(style) + ComputePadding(constraintSpace, style);
  }
  switch (length.type()) {
    case Auto:
    case FillAvailable: {
      LayoutUnit content_size = constraintSpace.AvailableSize().inline_size;
      NGBoxStrut margins = ComputeMargins(
          constraintSpace, style,
          FromPlatformWritingMode(style.getWritingMode()), style.direction());
      return std::max(border_and_padding.InlineSum(),
                      content_size - margins.InlineSum());
    }
    case Percent:
    case Fixed:
    case Calculated: {
      LayoutUnit percentage_resolution_size =
          constraintSpace.PercentageResolutionSize().inline_size;
      LayoutUnit value = valueForLength(length, percentage_resolution_size);
      if (style.boxSizing() == BoxSizingContentBox) {
        value += border_and_padding.InlineSum();
      } else {
        value = std::max(border_and_padding.InlineSum(), value);
      }
      return value;
    }
    case MinContent:
    case MaxContent:
    case FitContent: {
      DCHECK(min_and_max.has_value());
      LayoutUnit available_size = constraintSpace.AvailableSize().inline_size;
      LayoutUnit value;
      if (length.isMinContent()) {
        value = min_and_max->min_content;
      } else if (length.isMaxContent() || available_size == LayoutUnit::max()) {
        // If the available space is infinite, fit-content resolves to
        // max-content. See css-sizing section 2.1.
        value = min_and_max->max_content;
      } else {
        NGBoxStrut margins = ComputeMargins(
            constraintSpace, style,
            FromPlatformWritingMode(style.getWritingMode()), style.direction());
        LayoutUnit fill_available =
            std::max(LayoutUnit(), available_size - margins.InlineSum() -
                                       border_and_padding.InlineSum());
        value = min_and_max->ShrinkToFit(fill_available);
      }
      return value + border_and_padding.InlineSum();
    }
    case DeviceWidth:
    case DeviceHeight:
    case ExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
    case MaxSizeNone:
    default:
      NOTREACHED();
      return border_and_padding.InlineSum();
  }
}

LayoutUnit ResolveBlockLength(const NGConstraintSpace& constraintSpace,
                              const ComputedStyle& style,
                              const Length& length,
                              LayoutUnit contentSize,
                              LengthResolveType type) {
  DCHECK(!length.isMaxSizeNone());
  DCHECK(type != LengthResolveType::MarginBorderPaddingSize);

  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();

  // Make sure that indefinite percentages resolve to NGSizeIndefinite, not to
  // a random negative number.
  if (length.isPercentOrCalc() &&
      constraintSpace.PercentageResolutionSize().block_size == NGSizeIndefinite)
    return contentSize;

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::MarginBorderPaddingSize) {
    border_and_padding =
        ComputeBorders(style) + ComputePadding(constraintSpace, style);
  }
  switch (length.type()) {
    case FillAvailable: {
      LayoutUnit content_size = constraintSpace.AvailableSize().block_size;
      NGBoxStrut margins = ComputeMargins(
          constraintSpace, style,
          FromPlatformWritingMode(style.getWritingMode()), style.direction());
      return std::max(border_and_padding.BlockSum(),
                      content_size - margins.BlockSum());
    }
    case Percent:
    case Fixed:
    case Calculated: {
      LayoutUnit percentage_resolution_size =
          constraintSpace.PercentageResolutionSize().block_size;
      LayoutUnit value = valueForLength(length, percentage_resolution_size);
      if (style.boxSizing() == BoxSizingContentBox) {
        value += border_and_padding.BlockSum();
      } else {
        value = std::max(border_and_padding.BlockSum(), value);
      }
      return value;
    }
    case Auto:
    case MinContent:
    case MaxContent:
    case FitContent:
      // Due to how contentSize is calculated, it should always include border
      // and padding.
      if (contentSize != LayoutUnit(-1))
        DCHECK_GE(contentSize, border_and_padding.BlockSum());
      return contentSize;
    case DeviceWidth:
    case DeviceHeight:
    case ExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
    case MaxSizeNone:
    default:
      NOTREACHED();
      return border_and_padding.BlockSum();
  }
}

LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace& constraintSpace,
    const ComputedStyle& style,
    const WTF::Optional<MinAndMaxContentSizes>& min_and_max) {
  if (constraintSpace.FixedInlineSize())
    return constraintSpace.AvailableSize().inline_size;

  LayoutUnit extent =
      ResolveInlineLength(constraintSpace, style, min_and_max,
                          style.logicalWidth(), LengthResolveType::ContentSize);

  Length maxLength = style.logicalMaxWidth();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max = ResolveInlineLength(constraintSpace, style, min_and_max,
                                         maxLength, LengthResolveType::MaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min =
      ResolveInlineLength(constraintSpace, style, min_and_max,
                          style.logicalMinWidth(), LengthResolveType::MinSize);
  extent = std::max(extent, min);
  return extent;
}

LayoutUnit ComputeBlockSizeForFragment(const NGConstraintSpace& constraintSpace,
                                       const ComputedStyle& style,
                                       LayoutUnit contentSize) {
  if (constraintSpace.FixedBlockSize())
    return constraintSpace.AvailableSize().block_size;

  LayoutUnit extent =
      ResolveBlockLength(constraintSpace, style, style.logicalHeight(),
                         contentSize, LengthResolveType::ContentSize);
  if (extent == NGSizeIndefinite) {
    DCHECK_EQ(contentSize, NGSizeIndefinite);
    return extent;
  }

  Length maxLength = style.logicalMaxHeight();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max =
        ResolveBlockLength(constraintSpace, style, maxLength, contentSize,
                           LengthResolveType::MaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min =
      ResolveBlockLength(constraintSpace, style, style.logicalMinHeight(),
                         contentSize, LengthResolveType::MinSize);
  extent = std::max(extent, min);
  return extent;
}

NGBoxStrut ComputeMargins(const NGConstraintSpace& constraintSpace,
                          const ComputedStyle& style,
                          const NGWritingMode writing_mode,
                          const TextDirection direction) {
  // We don't need these for margin computations
  MinAndMaxContentSizes empty_sizes;
  // Margins always get computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-margin-width
  NGPhysicalBoxStrut physical_dim;
  physical_dim.left = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.marginLeft(),
      LengthResolveType::MarginBorderPaddingSize);
  physical_dim.right = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.marginRight(),
      LengthResolveType::MarginBorderPaddingSize);
  physical_dim.top = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.marginTop(),
      LengthResolveType::MarginBorderPaddingSize);
  physical_dim.bottom = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.marginBottom(),
      LengthResolveType::MarginBorderPaddingSize);
  return physical_dim.ConvertToLogical(writing_mode, direction);
}

NGBoxStrut ComputeBorders(const ComputedStyle& style) {
  NGBoxStrut borders;
  borders.inline_start = LayoutUnit(style.borderStartWidth());
  borders.inline_end = LayoutUnit(style.borderEndWidth());
  borders.block_start = LayoutUnit(style.borderBeforeWidth());
  borders.block_end = LayoutUnit(style.borderAfterWidth());
  return borders;
}

NGBoxStrut ComputePadding(const NGConstraintSpace& constraintSpace,
                          const ComputedStyle& style) {
  // We don't need these for padding computations
  MinAndMaxContentSizes empty_sizes;
  // Padding always gets computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-padding-width
  NGBoxStrut padding;
  padding.inline_start = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.paddingStart(),
      LengthResolveType::MarginBorderPaddingSize);
  padding.inline_end = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.paddingEnd(),
      LengthResolveType::MarginBorderPaddingSize);
  padding.block_start = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.paddingBefore(),
      LengthResolveType::MarginBorderPaddingSize);
  padding.block_end = ResolveInlineLength(
      constraintSpace, style, empty_sizes, style.paddingAfter(),
      LengthResolveType::MarginBorderPaddingSize);
  return padding;
}

void ApplyAutoMargins(const NGConstraintSpace& constraint_space,
                      const ComputedStyle& style,
                      const NGFragmentBase& fragment,
                      NGBoxStrut* margins) {
  DCHECK(margins) << "Margins cannot be NULL here";
  const LayoutUnit used_space = fragment.InlineSize() + margins->InlineSum();
  const LayoutUnit available_space =
      constraint_space.AvailableSize().inline_size - used_space;
  if (available_space < LayoutUnit())
    return;
  if (style.marginStart().isAuto() && style.marginEnd().isAuto()) {
    margins->inline_start = available_space / 2;
    margins->inline_end = available_space - margins->inline_start;
  } else if (style.marginStart().isAuto()) {
    margins->inline_start = available_space;
  } else if (style.marginEnd().isAuto()) {
    margins->inline_end = available_space;
  }
}

}  // namespace blink
