/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGPathSegWithContext_h
#define SVGPathSegWithContext_h

#include "core/svg/SVGPathSeg.h"

namespace WebCore {

// FIXME: This should be deprecated.
class SVGPathSegWithContext : public SVGPathSeg {
public:
    // FIXME: remove second unused argument from all derived classes.
    SVGPathSegWithContext(SVGPathElement* contextElement, SVGPathSegRole)
        : SVGPathSeg(contextElement)
    {
    }
};

class SVGPathSegSingleCoordinate : public SVGPathSegWithContext {
public:
    float x() const { return m_x; }
    void setX(float x)
    {
        m_x = x;
        commitChange();
    }

    float y() const { return m_y; }
    void setY(float y)
    {
        m_y = y;
        commitChange();
    }

protected:
    SVGPathSegSingleCoordinate(SVGPathElement* element, SVGPathSegRole role, float x, float y)
        : SVGPathSegWithContext(element, role)
        , m_x(x)
        , m_y(y)
    {
    }

private:
    float m_x;
    float m_y;
};

} // namespace WebCore

#endif
