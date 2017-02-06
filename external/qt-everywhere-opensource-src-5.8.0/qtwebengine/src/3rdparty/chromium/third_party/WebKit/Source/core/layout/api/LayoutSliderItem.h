
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutSliderItem_h
#define LayoutSliderItem_h

#include "core/layout/LayoutSlider.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutSliderItem : public LayoutBlockItem {
public:
    explicit LayoutSliderItem(LayoutSlider* layoutSlider)
        : LayoutBlockItem(layoutSlider)
    {
    }

    explicit LayoutSliderItem(const LayoutBlockItem& item)
        : LayoutBlockItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isSlider());
    }

    explicit LayoutSliderItem(std::nullptr_t) : LayoutBlockItem(nullptr) { }

    LayoutSliderItem() { }

    bool inDragMode() const
    {
        return toSlider()->inDragMode();
    }

private:
    LayoutSlider* toSlider() { return toLayoutSlider(layoutObject()); }
    const LayoutSlider* toSlider() const { return toLayoutSlider(layoutObject()); }
};

} // namespace blink

#endif // LayoutSliderItem_h
