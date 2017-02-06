// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterListPropertyFunctions_h
#define FilterListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"
#include "wtf/Allocator.h"

namespace blink {

class FilterListPropertyFunctions {
    STATIC_ONLY(FilterListPropertyFunctions);
public:
    static const FilterOperations& getInitialFilterList(CSSPropertyID property)
    {
        return getFilterList(property, ComputedStyle::initialStyle());
    }

    static const FilterOperations& getFilterList(CSSPropertyID property, const ComputedStyle& style)
    {
        switch (property) {
        default:
            NOTREACHED();
            // Fall through.
        case CSSPropertyBackdropFilter:
            return style.backdropFilter();
        case CSSPropertyFilter:
            return style.filter();
        }
    }

    static void setFilterList(CSSPropertyID property, ComputedStyle& style, const FilterOperations& filterOperations)
    {
        switch (property) {
        case CSSPropertyBackdropFilter:
            style.setBackdropFilter(filterOperations);
            break;
        case CSSPropertyFilter:
            style.setFilter(filterOperations);
            break;
        default:
            NOTREACHED();
            break;
        }
    }
};

} // namespace blink

#endif // FilterListPropertyFunctions_h
