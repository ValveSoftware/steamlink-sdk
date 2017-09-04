// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_input_node.h"

#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_inline_box.h"
#include "core/layout/ng/ng_inline_layout_algorithm.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGLayoutAlgorithm* NGLayoutInputNode::AlgorithmForInputNode(
    NGLayoutInputNode* input_node,
    const NGConstraintSpace* constraint_space) {
  // At least for now, this should never be called on LegacyInline
  // children. However, there will be other kinds of input_node so
  // it makes sense to do this here.
  DCHECK(input_node->Type() == LegacyBlock);
  NGBox* block = toNGBox(input_node);

  if (block->HasInlineChildren())
    return new NGInlineLayoutAlgorithm(
        block->Style(), toNGInlineBox(block->FirstChild()),
        constraint_space->ChildSpace(block->Style()));
  return new NGBlockLayoutAlgorithm(
      block->Style(), toNGBox(block->FirstChild()),
      constraint_space->ChildSpace(block->Style()));
}
}
