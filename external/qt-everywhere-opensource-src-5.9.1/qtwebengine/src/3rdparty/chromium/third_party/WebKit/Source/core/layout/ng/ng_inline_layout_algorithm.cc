// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_layout_algorithm.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_box.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGInlineBox* first_child,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : style_(style),
      first_child_(first_child),
      constraint_space_(constraint_space),
      break_token_(break_token) {
  DCHECK(style_);
}

NGLayoutStatus NGInlineLayoutAlgorithm::Layout(
    NGFragmentBase*,
    NGPhysicalFragmentBase** fragment_out,
    NGLayoutAlgorithm**) {
  NGFragmentBuilder builder(NGPhysicalFragmentBase::FragmentBox);

  *fragment_out = builder.ToFragment();
  return NewFragment;
}

DEFINE_TRACE(NGInlineLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(first_child_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
}

}  // namespace blink
