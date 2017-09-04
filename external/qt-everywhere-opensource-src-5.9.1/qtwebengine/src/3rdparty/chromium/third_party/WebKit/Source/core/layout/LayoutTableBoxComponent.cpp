// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableBoxComponent.h"

#include "core/layout/LayoutTable.h"
#include "core/style/ComputedStyle.h"

namespace blink {

bool LayoutTableBoxComponent::doCellsHaveDirtyWidth(
    const LayoutObject& tablePart,
    const LayoutTable& table,
    const StyleDifference& diff,
    const ComputedStyle& oldStyle) {
  // ComputedStyle::diffNeedsFullLayoutAndPaintInvalidation sets needsFullLayout
  // when border sizes change: checking diff.needsFullLayout() is an
  // optimization, not required for correctness.
  // TODO(dgrogan): Remove tablePart.needsLayout()? Perhaps it was an old
  // optimization but now it seems that diff.needsFullLayout() implies
  // tablePart.needsLayout().
  return diff.needsFullLayout() && tablePart.needsLayout() &&
         table.collapseBorders() &&
         !oldStyle.border().sizeEquals(tablePart.style()->border());
}

}  // namespace blink
