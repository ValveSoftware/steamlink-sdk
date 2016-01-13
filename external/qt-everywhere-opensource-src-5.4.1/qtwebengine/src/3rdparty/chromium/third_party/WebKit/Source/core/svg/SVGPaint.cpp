/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "config.h"
#include "core/svg/SVGPaint.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/css/RGBColor.h"
#include "core/css/parser/BisonCSSParser.h"

namespace WebCore {

SVGPaint::SVGPaint(const SVGPaintType& paintType, const String& uri)
    : CSSValue(SVGPaintClass)
    , m_paintType(paintType)
    , m_uri(uri)
{
}

String SVGPaint::customCSSText() const
{
    switch (m_paintType) {
    case SVG_PAINTTYPE_UNKNOWN:
        return String();
    case SVG_PAINTTYPE_RGBCOLOR:
    case SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR:
        return m_color.serializedAsCSSComponentValue();
    case SVG_PAINTTYPE_CURRENTCOLOR:
        return "currentColor";
    case SVG_PAINTTYPE_NONE:
        return "none";
    case SVG_PAINTTYPE_URI_NONE:
        return m_uri + " none";
    case SVG_PAINTTYPE_URI_CURRENTCOLOR:
        return "url(" + m_uri + ") currentColor";
    case SVG_PAINTTYPE_URI_RGBCOLOR:
    case SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR: {
        return "url(" + m_uri + ") " + m_color.serializedAsCSSComponentValue();
    }
    case SVG_PAINTTYPE_URI:
        return "url(" + m_uri + ')';
    };

    ASSERT_NOT_REACHED();
    return String();
}

SVGPaint::SVGPaint(const SVGPaint& cloneFrom)
    : CSSValue(SVGPaintClass, /*isCSSOMSafe*/ true)
    , m_paintType(cloneFrom.m_paintType)
    , m_color(cloneFrom.m_color)
    , m_uri(cloneFrom.m_uri)
{
}

PassRefPtrWillBeRawPtr<SVGPaint> SVGPaint::cloneForCSSOM() const
{
    return adoptRefWillBeNoop(new SVGPaint(*this));
}

bool SVGPaint::equals(const SVGPaint& other) const
{
    return m_paintType == other.m_paintType && m_uri == other.m_uri && m_color == other.m_color;
}

StyleColor SVGPaint::colorFromRGBColorString(const String& colorString)
{
    // FIXME: Rework css parser so it is more SVG aware.
    RGBA32 color;
    if (BisonCSSParser::parseColor(color, colorString.stripWhiteSpace()))
        return StyleColor(color);
    // FIXME: This branch catches the string currentColor, but we should error if we have an illegal color value.
    return StyleColor::currentColor();
}

}
