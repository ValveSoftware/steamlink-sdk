
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTextItem_h
#define LayoutTextItem_h

#include "core/layout/LayoutText.h"
#include "core/layout/api/LayoutItem.h"

namespace blink {

class ComputedStyle;

class LayoutTextItem : public LayoutItem {
public:
    explicit LayoutTextItem(LayoutText* layoutText)
        : LayoutItem(layoutText)
    {
    }

    explicit LayoutTextItem(const LayoutItem& item)
        : LayoutItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isText());
    }

    explicit LayoutTextItem(std::nullptr_t) : LayoutItem(nullptr) { }

    LayoutTextItem() { }

    void setStyle(PassRefPtr<ComputedStyle> style)
    {
        toText()->setStyle(style);
    }

    void setText(PassRefPtr<StringImpl> text, bool force = false)
    {
        toText()->setText(text, force);
    }

    bool isTextFragment() const
    {
        return toText()->isTextFragment();
    }

    void dirtyLineBoxes()
    {
        toText()->dirtyLineBoxes();
    }

private:
    LayoutText* toText() { return toLayoutText(layoutObject()); }
    const LayoutText* toText() const { return toLayoutText(layoutObject()); }
};

} // namespace blink

#endif // LayoutTextItem_h
