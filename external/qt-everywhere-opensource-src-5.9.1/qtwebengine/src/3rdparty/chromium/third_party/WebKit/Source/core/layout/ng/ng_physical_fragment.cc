// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGPhysicalFragment::NGPhysicalFragment(
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    HeapVector<Member<const NGPhysicalFragmentBase>>& children,
    NGMarginStrut margin_strut)
    : NGPhysicalFragmentBase(size, overflow, FragmentBox),
      margin_strut_(margin_strut) {
  children_.swap(children);
}

DEFINE_TRACE_AFTER_DISPATCH(NGPhysicalFragment) {
  visitor->trace(children_);
  NGPhysicalFragmentBase::traceAfterDispatch(visitor);
}

}  // namespace blink
