// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewFragmentationContext_h
#define ViewFragmentationContext_h

#include "core/layout/FragmentationContext.h"

namespace blink {

class LayoutView;

class ViewFragmentationContext final : public FragmentationContext {
public:
    ViewFragmentationContext(LayoutView& view) : m_view(view) { }
    bool isFragmentainerLogicalHeightKnown() final;
    LayoutUnit fragmentainerLogicalHeightAt(LayoutUnit blockOffset) final;
    LayoutUnit remainingLogicalHeightAt(LayoutUnit blockOffset) final;

private:
    LayoutView& m_view;
};

} // namespace blink

#endif // ViewFragmentationContext_h
