
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutProgressItem_h
#define LayoutProgressItem_h

#include "core/layout/LayoutProgress.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutProgressItem : public LayoutBlockItem {
public:
    explicit LayoutProgressItem(LayoutProgress* layoutProgress)
        : LayoutBlockItem(layoutProgress)
    {
    }

    explicit LayoutProgressItem(const LayoutBlockItem& item)
        : LayoutBlockItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isProgress());
    }

    explicit LayoutProgressItem(std::nullptr_t) : LayoutBlockItem(nullptr) { }

    LayoutProgressItem() { }

    bool isDeterminate() const
    {
        return toProgress()->isDeterminate();
    }

    void updateFromElement()
    {
        return toProgress()->updateFromElement();
    }

private:
    LayoutProgress* toProgress() { return toLayoutProgress(layoutObject()); }
    const LayoutProgress* toProgress() const { return toLayoutProgress(layoutObject()); }
};

} // namespace blink

#endif // LayoutProgressItem_h
