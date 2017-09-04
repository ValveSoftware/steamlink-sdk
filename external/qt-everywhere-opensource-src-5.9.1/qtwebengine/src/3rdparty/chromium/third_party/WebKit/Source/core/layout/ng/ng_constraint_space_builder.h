// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpaceBuilder_h
#define NGConstraintSpaceBuilder_h

#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

class CORE_EXPORT NGConstraintSpaceBuilder final
    : public GarbageCollected<NGConstraintSpaceBuilder> {
 public:
  NGConstraintSpaceBuilder(NGWritingMode writing_mode);

  NGConstraintSpaceBuilder& SetAvailableSize(NGLogicalSize available_size);

  NGConstraintSpaceBuilder& SetPercentageResolutionSize(
      NGLogicalSize percentage_resolution_size);

  NGConstraintSpaceBuilder& SetIsFixedSizeInline(bool is_fixed_size_inline);
  NGConstraintSpaceBuilder& SetIsFixedSizeBlock(bool is_fixed_size_block);

  NGConstraintSpaceBuilder& SetIsInlineDirectionTriggersScrollbar(
      bool is_inline_direction_triggers_scrollbar);
  NGConstraintSpaceBuilder& SetIsBlockDirectionTriggersScrollbar(
      bool is_block_direction_triggers_scrollbar);

  NGConstraintSpaceBuilder& SetFragmentationType(NGFragmentationType);
  NGConstraintSpaceBuilder& SetIsNewFormattingContext(bool is_new_fc);

  // Creates a new constraint space. This may be called multiple times, for
  // example the constraint space will be different for a child which:
  //  - Establishes a new formatting context.
  //  - Is within a fragmentation container and needs its fragmentation offset
  //    updated.
  //  - Has its size is determined by its parent layout (flex, abs-pos).
  NGPhysicalConstraintSpace* ToConstraintSpace();

  DEFINE_INLINE_TRACE() {}

 private:
  NGLogicalSize available_size_;
  NGLogicalSize percentage_resolution_size_;

  // const bit fields.
  const unsigned writing_mode_ : 2;

  // mutable bit fields.
  unsigned is_fixed_size_inline_ : 1;
  unsigned is_fixed_size_block_ : 1;
  unsigned is_inline_direction_triggers_scrollbar_ : 1;
  unsigned is_block_direction_triggers_scrollbar_ : 1;
  unsigned fragmentation_type_ : 2;
  unsigned is_new_fc_ : 1;
};

}  // namespace blink

#endif  // NGConstraintSpaceBuilder
