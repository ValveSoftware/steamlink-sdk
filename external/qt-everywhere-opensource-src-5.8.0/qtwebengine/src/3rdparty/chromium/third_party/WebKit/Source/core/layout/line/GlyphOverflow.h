/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef GlyphOverflow_h
#define GlyphOverflow_h

#include "platform/geometry/FloatRect.h"
#include "wtf/Allocator.h"
#include <algorithm>

namespace blink {

struct GlyphOverflow {
    GlyphOverflow()
        : left(0)
        , right(0)
        , top(0)
        , bottom(0)
    {
    }

    bool isApproximatelyZero() const
    {
        // Overflow can be expensive so we try to avoid it. Small amounts of
        // overflow is imperceptible and is typically masked by pixel snapping.
        static const float kApproximatelyNoOverflow = 0.0625f;
        return std::fabs(left) < kApproximatelyNoOverflow
            && std::fabs(right) < kApproximatelyNoOverflow
            && std::fabs(top) < kApproximatelyNoOverflow
            && std::fabs(bottom) < kApproximatelyNoOverflow;
    }

    void setFromBounds(const FloatRect& bounds, float ascent, float descent, float textWidth)
    {
        top = std::max(0.0f, -bounds.y() - ascent);
        bottom = std::max(0.0f, bounds.maxY() - descent);
        left = std::max(0.0f, -bounds.x());
        right = std::max(0.0f, bounds.maxX() - textWidth);
    }

    // Top and bottom are the amounts of glyph overflows exceeding the font metrics' ascent and descent, respectively.
    // Left and right are the amounts of glyph overflows exceeding the left and right edge of normal layout boundary, respectively.
    float left;
    float right;
    float top;
    float bottom;
};

} // namespace blink

#endif // GlyphOverflow_h
