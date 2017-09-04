// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space_builder.h"

namespace blink {

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(NGWritingMode writing_mode)
    : writing_mode_(writing_mode),
      is_fixed_size_inline_(false),
      is_fixed_size_block_(false),
      is_inline_direction_triggers_scrollbar_(false),
      is_block_direction_triggers_scrollbar_(false),
      fragmentation_type_(NGFragmentationType::FragmentNone),
      is_new_fc_(false) {}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetAvailableSize(
    NGLogicalSize available_size) {
  available_size_ = available_size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetPercentageResolutionSize(
    NGLogicalSize percentage_resolution_size) {
  percentage_resolution_size_ = percentage_resolution_size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsFixedSizeInline(
    bool is_fixed_size_inline) {
  is_fixed_size_inline_ = is_fixed_size_inline;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsFixedSizeBlock(
    bool is_fixed_size_block) {
  is_fixed_size_block_ = is_fixed_size_block;
  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetIsInlineDirectionTriggersScrollbar(
    bool is_inline_direction_triggers_scrollbar) {
  is_inline_direction_triggers_scrollbar_ =
      is_inline_direction_triggers_scrollbar;
  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetIsBlockDirectionTriggersScrollbar(
    bool is_block_direction_triggers_scrollbar) {
  is_block_direction_triggers_scrollbar_ =
      is_block_direction_triggers_scrollbar;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFragmentationType(
    NGFragmentationType fragmentation_type) {
  fragmentation_type_ = fragmentation_type;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsNewFormattingContext(
    bool is_new_fc) {
  is_new_fc_ = is_new_fc;
  return *this;
}

NGPhysicalConstraintSpace* NGConstraintSpaceBuilder::ToConstraintSpace() {
  NGPhysicalSize available_size = available_size_.ConvertToPhysical(
      static_cast<NGWritingMode>(writing_mode_));
  NGPhysicalSize percentage_resolution_size =
      percentage_resolution_size_.ConvertToPhysical(
          static_cast<NGWritingMode>(writing_mode_));

  if (writing_mode_ == HorizontalTopBottom) {
    return new NGPhysicalConstraintSpace(
        available_size, percentage_resolution_size, is_fixed_size_inline_,
        is_fixed_size_block_, is_inline_direction_triggers_scrollbar_,
        is_block_direction_triggers_scrollbar_, FragmentNone,
        static_cast<NGFragmentationType>(fragmentation_type_), is_new_fc_);
  } else {
    return new NGPhysicalConstraintSpace(
        available_size, percentage_resolution_size, is_fixed_size_block_,
        is_fixed_size_inline_, is_block_direction_triggers_scrollbar_,
        is_inline_direction_triggers_scrollbar_,
        static_cast<NGFragmentationType>(fragmentation_type_), FragmentNone,
        is_new_fc_);
  }
}

}  // namespace blink
