// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutImageItem_h
#define LayoutImageItem_h

#include "core/layout/LayoutImage.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutImageItem : public LayoutBoxItem {
public:
    explicit LayoutImageItem(LayoutImage* layoutImage)
        : LayoutBoxItem(layoutImage)
    {
    }

    explicit LayoutImageItem(const LayoutItem& item)
        : LayoutBoxItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isImage());
    }

    explicit LayoutImageItem(std::nullptr_t) : LayoutBoxItem(nullptr) { }

    LayoutImageItem() { }

    void setImageDevicePixelRatio(float factor)
    {
        toImage()->setImageDevicePixelRatio(factor);
    }

private:
    LayoutImage* toImage()
    {
        return toLayoutImage(layoutObject());
    }

    const LayoutImage* toImage() const
    {
        return toLayoutImage(layoutObject());
    }
};

} // namespace blink

#endif // LayoutImageItem_h
