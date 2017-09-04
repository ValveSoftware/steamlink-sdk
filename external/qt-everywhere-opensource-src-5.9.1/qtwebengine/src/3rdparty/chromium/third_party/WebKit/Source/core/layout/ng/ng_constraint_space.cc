// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(NGWritingMode writing_mode,
                                     TextDirection direction,
                                     NGPhysicalConstraintSpace* physical_space)
    : physical_space_(physical_space),
      size_(physical_space->available_size_.ConvertToLogical(writing_mode)),
      writing_mode_(writing_mode),
      direction_(direction) {}

NGConstraintSpace* NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  bool fixed_inline = false, fixed_block = false;
  // XXX for orthogonal writing mode this is not right
  LayoutUnit available_logical_width =
      std::max(LayoutUnit(), box.containingBlockLogicalWidthForContent());
  LayoutUnit available_logical_height;
  if (!box.parent()) {
    available_logical_height = box.view()->viewLogicalHeightForPercentages();
  } else if (box.containingBlock()) {
    available_logical_height =
        box.containingBlock()->availableLogicalHeightForPercentageComputation();
  }
  // When we have an override size, the available_logical_{width,height} will be
  // used as the final size of the box, so it has to include border and
  // padding.
  if (box.hasOverrideLogicalContentWidth()) {
    available_logical_width =
        box.borderAndPaddingLogicalWidth() + box.overrideLogicalContentWidth();
    fixed_inline = true;
  }
  if (box.hasOverrideLogicalContentHeight()) {
    available_logical_height = box.borderAndPaddingLogicalHeight() +
                               box.overrideLogicalContentHeight();
    fixed_block = true;
  }

  bool is_new_fc =
      box.isLayoutBlock() && toLayoutBlock(box).createsNewFormattingContext();

  NGConstraintSpaceBuilder builder(
      FromPlatformWritingMode(box.styleRef().getWritingMode()));
  builder
      .SetAvailableSize(
          NGLogicalSize(available_logical_width, available_logical_height))
      .SetPercentageResolutionSize(
          NGLogicalSize(available_logical_width, available_logical_height))
      .SetIsInlineDirectionTriggersScrollbar(
          box.styleRef().overflowInlineDirection() == OverflowAuto)
      .SetIsBlockDirectionTriggersScrollbar(
          box.styleRef().overflowBlockDirection() == OverflowAuto)
      .SetIsFixedSizeInline(fixed_inline)
      .SetIsFixedSizeBlock(fixed_block)
      .SetIsNewFormattingContext(is_new_fc);

  return new NGConstraintSpace(
      FromPlatformWritingMode(box.styleRef().getWritingMode()),
      box.styleRef().direction(), builder.ToConstraintSpace());
}

void NGConstraintSpace::AddExclusion(const NGExclusion& exclusion) const {
  WRITING_MODE_IGNORED(
      "Exclusions are stored directly in physical constraint space.");
  MutablePhysicalSpace()->AddExclusion(exclusion);
}

const NGExclusion* NGConstraintSpace::LastLeftFloatExclusion() const {
  WRITING_MODE_IGNORED(
      "Exclusions are stored directly in physical constraint space.");
  return PhysicalSpace()->LastLeftFloatExclusion();
}

const NGExclusion* NGConstraintSpace::LastRightFloatExclusion() const {
  WRITING_MODE_IGNORED(
      "Exclusions are stored directly in physical constraint space.");
  return PhysicalSpace()->LastRightFloatExclusion();
}

NGLogicalSize NGConstraintSpace::PercentageResolutionSize() const {
  return physical_space_->percentage_resolution_size_.ConvertToLogical(
      static_cast<NGWritingMode>(writing_mode_));
}

NGLogicalSize NGConstraintSpace::AvailableSize() const {
  return physical_space_->available_size_.ConvertToLogical(
      static_cast<NGWritingMode>(writing_mode_));
}

bool NGConstraintSpace::IsNewFormattingContext() const {
  return physical_space_->is_new_fc_;
}

bool NGConstraintSpace::InlineTriggersScrollbar() const {
  return writing_mode_ == HorizontalTopBottom
             ? physical_space_->width_direction_triggers_scrollbar_
             : physical_space_->height_direction_triggers_scrollbar_;
}

bool NGConstraintSpace::BlockTriggersScrollbar() const {
  return writing_mode_ == HorizontalTopBottom
             ? physical_space_->height_direction_triggers_scrollbar_
             : physical_space_->width_direction_triggers_scrollbar_;
}

bool NGConstraintSpace::FixedInlineSize() const {
  return writing_mode_ == HorizontalTopBottom ? physical_space_->fixed_width_
                                              : physical_space_->fixed_height_;
}

bool NGConstraintSpace::FixedBlockSize() const {
  return writing_mode_ == HorizontalTopBottom ? physical_space_->fixed_height_
                                              : physical_space_->fixed_width_;
}

NGFragmentationType NGConstraintSpace::BlockFragmentationType() const {
  return static_cast<NGFragmentationType>(
      writing_mode_ == HorizontalTopBottom
          ? physical_space_->height_direction_fragmentation_type_
          : physical_space_->width_direction_fragmentation_type_);
}

void NGConstraintSpace::Subtract(const NGFragment*) {
  // TODO(layout-ng): Implement.
}

NGLayoutOpportunityIterator* NGConstraintSpace::LayoutOpportunities(
    unsigned clear,
    bool for_inline_or_bfc) {
  NGLayoutOpportunityIterator* iterator = new NGLayoutOpportunityIterator(this);
  return iterator;
}

void NGConstraintSpace::SetOverflowTriggersScrollbar(bool inline_triggers,
                                                     bool block_triggers) {
  if (writing_mode_ == HorizontalTopBottom) {
    physical_space_->width_direction_triggers_scrollbar_ = inline_triggers;
    physical_space_->height_direction_triggers_scrollbar_ = block_triggers;
  } else {
    physical_space_->width_direction_triggers_scrollbar_ = block_triggers;
    physical_space_->height_direction_triggers_scrollbar_ = inline_triggers;
  }
}

void NGConstraintSpace::SetFixedSize(bool inline_fixed, bool block_fixed) {
  if (writing_mode_ == HorizontalTopBottom) {
    physical_space_->fixed_width_ = inline_fixed;
    physical_space_->fixed_height_ = block_fixed;
  } else {
    physical_space_->fixed_width_ = block_fixed;
    physical_space_->fixed_height_ = inline_fixed;
  }
}

void NGConstraintSpace::SetFragmentationType(NGFragmentationType type) {
  if (writing_mode_ == HorizontalTopBottom) {
    DCHECK_EQ(static_cast<NGFragmentationType>(
                  physical_space_->width_direction_fragmentation_type_),
              FragmentNone);
    physical_space_->height_direction_fragmentation_type_ = type;
  } else {
    DCHECK_EQ(static_cast<NGFragmentationType>(
                  physical_space_->height_direction_fragmentation_type_),
              FragmentNone);
    physical_space_->width_direction_triggers_scrollbar_ = type;
  }
}

void NGConstraintSpace::SetIsNewFormattingContext(bool is_new_fc) {
  physical_space_->is_new_fc_ = is_new_fc;
}

NGConstraintSpace* NGConstraintSpace::ChildSpace(
    const ComputedStyle* style) const {
  return new NGConstraintSpace(FromPlatformWritingMode(style->getWritingMode()),
                               style->direction(), MutablePhysicalSpace());
}

String NGConstraintSpace::ToString() const {
  return String::format("%s,%s %sx%s",
                        offset_.inline_offset.toString().ascii().data(),
                        offset_.block_offset.toString().ascii().data(),
                        size_.inline_size.toString().ascii().data(),
                        size_.block_size.toString().ascii().data());
}

}  // namespace blink
