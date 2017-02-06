// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBR_h
#define LineLayoutBR_h

#include "core/layout/LayoutBR.h"
#include "core/layout/api/LineLayoutText.h"

namespace blink {

class LineLayoutBR : public LineLayoutText {
public:
    explicit LineLayoutBR(LayoutBR* layoutBR)
        : LineLayoutText(layoutBR)
    {
    }

    explicit LineLayoutBR(const LineLayoutItem& item)
        : LineLayoutText(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isBR());
    }

    explicit LineLayoutBR(std::nullptr_t) : LineLayoutText(nullptr) { }

    LineLayoutBR() { }

    int lineHeight(bool firstLine) const
    {
        return toBR()->lineHeight(firstLine);
    }

private:
    LayoutBR* toBR()
    {
        return toLayoutBR(layoutObject());
    }

    const LayoutBR* toBR() const
    {
        return toLayoutBR(layoutObject());
    }
};

} // namespace blink

#endif // LineLayoutBR_h
