// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/LayoutAnalyzer.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutBlockFlow(element) {}

bool LayoutNGBlockFlow::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectNGBlockFlow || LayoutBlockFlow::isOfType(type);
}

void LayoutNGBlockFlow::layoutBlock(bool relayoutChildren) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  const auto* constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  // TODO(layout-dev): This should be created in the constructor once instead.
  // There is some internal state which needs to be cleared between layout
  // passes (probably FirstChild(), etc).
  m_box = new NGBox(this);

  NGFragmentBase* fragment;
  while (!m_box->Layout(constraint_space, &fragment))
    ;
  clearNeedsLayout();
}

}  // namespace blink
