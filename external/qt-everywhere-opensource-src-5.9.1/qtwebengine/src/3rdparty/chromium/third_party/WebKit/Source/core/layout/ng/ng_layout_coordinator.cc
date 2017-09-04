// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_coordinator.h"

#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/ng_physical_fragment_base.h"

namespace blink {

NGLayoutCoordinator::NGLayoutCoordinator(
    NGLayoutInputNode* input_node,
    const NGConstraintSpace* constraint_space) {
  layout_algorithms_.append(
      NGLayoutInputNode::AlgorithmForInputNode(input_node, constraint_space));
}

bool NGLayoutCoordinator::Tick(NGPhysicalFragmentBase** fragment) {
  NGLayoutAlgorithm* child_algorithm;

  // Tick should never be called without a layout algorithm on the stack.
  DCHECK(layout_algorithms_.size());

  // TODO(layout-dev): store box from last tick and pass it into Layout here.
  switch (
      layout_algorithms_.last()->Layout(nullptr, fragment, &child_algorithm)) {
    case NotFinished:
      return false;
    case NewFragment:
      layout_algorithms_.pop_back();
      return (layout_algorithms_.size() == 0);
    case ChildAlgorithmRequired:
      layout_algorithms_.append(child_algorithm);
      return false;
  }

  NOTREACHED();
  return false;
}

DEFINE_TRACE(NGLayoutCoordinator) {
  visitor->trace(layout_algorithms_);
}
}
