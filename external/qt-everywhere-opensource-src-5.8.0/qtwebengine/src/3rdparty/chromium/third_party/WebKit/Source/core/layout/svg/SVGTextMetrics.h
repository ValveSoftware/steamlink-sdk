/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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
 */

#ifndef SVGTextMetrics_h
#define SVGTextMetrics_h

#include "wtf/Allocator.h"

namespace blink {

enum class FontOrientation;

class SVGTextMetrics {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    enum MetricsType {
        SkippedSpaceMetrics
    };

    SVGTextMetrics();
    SVGTextMetrics(MetricsType);
    SVGTextMetrics(unsigned length, float width, float height);

    bool isEmpty() const { return !m_width && !m_height && m_length <= 1; }

    float width() const { return m_width; }
    float height() const { return m_height; }

    // TODO(kojii): We should store logical width (advance) and height instead
    // of storing physical and calculate logical. crbug.com/544767
    float advance(FontOrientation) const;
    unsigned length() const { return m_length; }

private:
    float m_width;
    float m_height;
    unsigned m_length;
};

} // namespace blink

#endif
