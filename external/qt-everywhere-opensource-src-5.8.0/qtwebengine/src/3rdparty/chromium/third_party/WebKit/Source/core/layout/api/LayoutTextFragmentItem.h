// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTextFragmentItem_h
#define LayoutTextFragmentItem_h

#include "core/layout/LayoutTextFragment.h"
#include "core/layout/api/LayoutTextItem.h"

namespace blink {

class FirstLetterPseudoElement;

class LayoutTextFragmentItem : public LayoutTextItem {
public:
    explicit LayoutTextFragmentItem(LayoutTextFragment* layoutTextFragment)
        : LayoutTextItem(layoutTextFragment)
    {
    }

    explicit LayoutTextFragmentItem(const LayoutTextItem& item)
        : LayoutTextItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isTextFragment());
    }

    explicit LayoutTextFragmentItem(std::nullptr_t) : LayoutTextItem(nullptr) { }

    LayoutTextFragmentItem() { }

    void setTextFragment(PassRefPtr<StringImpl> text, unsigned start, unsigned length)
    {
        toTextFragment()->setTextFragment(text, start, length);
    }

    FirstLetterPseudoElement* firstLetterPseudoElement() const
    {
        return toTextFragment()->firstLetterPseudoElement();
    }

private:
    LayoutTextFragment* toTextFragment() { return toLayoutTextFragment(layoutObject()); }
    const LayoutTextFragment* toTextFragment() const { return toLayoutTextFragment(layoutObject()); }
};

} // namespace blink

#endif // LayoutTextFragmentItem_h
