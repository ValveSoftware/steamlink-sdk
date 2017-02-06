// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableBoxComponent.h"

#include "core/style/ComputedStyle.h"

namespace blink {

void LayoutTableBoxComponent::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutBox::styleDidChange(diff, oldStyle);

    if (parent() && oldStyle) {
        if (resolveColor(*oldStyle, CSSPropertyBackgroundColor) != resolveColor(CSSPropertyBackgroundColor)
            || oldStyle->backgroundLayers() != styleRef().backgroundLayers())
            m_backgroundChangedSinceLastPaintInvalidation = true;
    }
}

void LayoutTableBoxComponent::imageChanged(WrappedImagePtr, const IntRect*)
{
    setShouldDoFullPaintInvalidation();
    m_backgroundChangedSinceLastPaintInvalidation = true;
}

} // namespace blink
