// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutRubyBase_h
#define LineLayoutRubyBase_h

#include "core/layout/LayoutRubyBase.h"
#include "core/layout/api/LineLayoutBlockFlow.h"

namespace blink {

class LineLayoutRubyBase : public LineLayoutBlockFlow {
public:
    explicit LineLayoutRubyBase(LayoutRubyBase* layoutRubyBase)
        : LineLayoutBlockFlow(layoutRubyBase)
    {
    }

    explicit LineLayoutRubyBase(const LineLayoutItem& item)
        : LineLayoutBlockFlow(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isRubyBase());
    }

    explicit LineLayoutRubyBase(std::nullptr_t) : LineLayoutBlockFlow(nullptr) { }

    LineLayoutRubyBase() { }


private:
    LayoutRubyBase* toRubyBase()
    {
        return toLayoutRubyBase(layoutObject());
    }

    const LayoutRubyBase* toRubyBase() const
    {
        return toLayoutRubyBase(layoutObject());
    }
};

} // namespace blink

#endif // LineLayoutRubyBase_h
