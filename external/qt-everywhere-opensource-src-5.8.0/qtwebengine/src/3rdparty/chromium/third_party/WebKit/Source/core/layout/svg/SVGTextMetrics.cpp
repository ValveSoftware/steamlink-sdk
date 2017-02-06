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

#include "core/layout/svg/SVGTextMetrics.h"

#include "platform/fonts/FontOrientation.h"

namespace blink {

SVGTextMetrics::SVGTextMetrics()
    : m_width(0)
    , m_height(0)
    , m_length(0)
{
}

SVGTextMetrics::SVGTextMetrics(unsigned length, float width, float height)
    : m_width(width)
    , m_height(height)
    , m_length(length)
{
}

SVGTextMetrics::SVGTextMetrics(SVGTextMetrics::MetricsType)
    : SVGTextMetrics(1, 0, 0)
{
}

float SVGTextMetrics::advance(FontOrientation orientation) const
{
    switch (orientation) {
    case FontOrientation::Horizontal:
    case FontOrientation::VerticalRotated:
        return width();
    case FontOrientation::VerticalUpright:
        return height();
    default:
        ASSERT_NOT_REACHED();
        return width();
    }
}

} // namespace blink
