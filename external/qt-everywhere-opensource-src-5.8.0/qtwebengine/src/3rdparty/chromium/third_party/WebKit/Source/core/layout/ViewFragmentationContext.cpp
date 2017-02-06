// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ViewFragmentationContext.h"

#include "core/layout/LayoutView.h"

namespace blink {

bool ViewFragmentationContext::isFragmentainerLogicalHeightKnown()
{
    ASSERT(m_view.pageLogicalHeight());
    return true;
}

LayoutUnit ViewFragmentationContext::fragmentainerLogicalHeightAt(LayoutUnit)
{
    ASSERT(m_view.pageLogicalHeight());
    return m_view.pageLogicalHeight();
}

LayoutUnit ViewFragmentationContext::remainingLogicalHeightAt(LayoutUnit blockOffset)
{
    LayoutUnit pageLogicalHeight = m_view.pageLogicalHeight();
    return pageLogicalHeight - intMod(blockOffset, pageLogicalHeight);
}

} // namespace blink
