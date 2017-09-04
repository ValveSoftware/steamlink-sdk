// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_layout_algorithm.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"

namespace blink {

NGTextLayoutAlgorithm::NGTextLayoutAlgorithm(
    NGInlineBox* inline_box,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : inline_box_(inline_box),
      constraint_space_(constraint_space),
      break_token_(break_token) {
  DCHECK(inline_box_);
}

NGLayoutStatus NGTextLayoutAlgorithm::Layout(
    NGFragmentBase*,
    NGPhysicalFragmentBase** fragment_out,
    NGLayoutAlgorithm**) {
  // TODO(layout-dev): implement.
  *fragment_out = nullptr;
  return NewFragment;
}

DEFINE_TRACE(NGTextLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(inline_box_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
}

}  // namespace blink
