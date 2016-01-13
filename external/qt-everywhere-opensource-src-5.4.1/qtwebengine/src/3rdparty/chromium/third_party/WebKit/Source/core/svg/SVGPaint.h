/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmial.com>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGPaint_h
#define SVGPaint_h

#include "core/css/CSSValue.h"
#include "core/css/StyleColor.h"
#include "platform/graphics/Color.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class SVGPaint : public CSSValue {
public:
    enum SVGPaintType {
        SVG_PAINTTYPE_UNKNOWN,
        SVG_PAINTTYPE_RGBCOLOR,
        SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR,
        SVG_PAINTTYPE_NONE,
        SVG_PAINTTYPE_CURRENTCOLOR,
        SVG_PAINTTYPE_URI_NONE,
        SVG_PAINTTYPE_URI_CURRENTCOLOR,
        SVG_PAINTTYPE_URI_RGBCOLOR,
        SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR,
        SVG_PAINTTYPE_URI
    };

    static PassRefPtrWillBeRawPtr<SVGPaint> createUnknown()
    {
        return adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_UNKNOWN));
    }

    static PassRefPtrWillBeRawPtr<SVGPaint> createNone()
    {
        return adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_NONE));
    }

    static PassRefPtrWillBeRawPtr<SVGPaint> createCurrentColor()
    {
        return adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_CURRENTCOLOR));
    }

    static PassRefPtrWillBeRawPtr<SVGPaint> createColor(const Color& color)
    {
        RefPtrWillBeRawPtr<SVGPaint> paint = adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_RGBCOLOR));
        paint->m_color = color;
        return paint.release();
    }

    static PassRefPtrWillBeRawPtr<SVGPaint> createURI(const String& uri)
    {
        RefPtrWillBeRawPtr<SVGPaint> paint = adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_URI, uri));
        return paint.release();
    }

    static PassRefPtrWillBeRawPtr<SVGPaint> createURIAndColor(const String& uri, const Color& color)
    {
        RefPtrWillBeRawPtr<SVGPaint> paint = adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_URI_RGBCOLOR, uri));
        paint->m_color = color;
        return paint.release();
    }

    static PassRefPtrWillBeRawPtr<SVGPaint> createURIAndNone(const String& uri)
    {
        RefPtrWillBeRawPtr<SVGPaint> paint = adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_URI_NONE, uri));
        return paint.release();
    }

    static PassRefPtrWillBeRawPtr<SVGPaint> createURIAndCurrentColor(const String& uri)
    {
        RefPtrWillBeRawPtr<SVGPaint> paint = adoptRefWillBeNoop(new SVGPaint(SVG_PAINTTYPE_URI_CURRENTCOLOR, uri));
        return paint.release();
    }

    const SVGPaintType& paintType() const { return m_paintType; }
    String uri() const { return m_uri; }

    String customCSSText() const;

    PassRefPtrWillBeRawPtr<SVGPaint> cloneForCSSOM() const;

    bool equals(const SVGPaint&) const;

    void traceAfterDispatch(Visitor* visitor) { CSSValue::traceAfterDispatch(visitor); }

    Color color() const { return m_color; }
    void setColor(const Color& color) { m_color = color; m_paintType = SVG_PAINTTYPE_RGBCOLOR; }

    static StyleColor colorFromRGBColorString(const String&);

private:
    friend class CSSComputedStyleDeclaration;

    static PassRefPtrWillBeRawPtr<SVGPaint> create(const SVGPaintType& type, const String& uri, const Color& color)
    {
        RefPtrWillBeRawPtr<SVGPaint> paint = adoptRefWillBeNoop(new SVGPaint(type, uri));
        paint->m_color = color;
        return paint.release();
    }

private:
    SVGPaint(const SVGPaintType&, const String& uri = String());
    SVGPaint(const SVGPaint& cloneFrom);

    SVGPaintType m_paintType;
    Color m_color;
    String m_uri;
};

DEFINE_CSS_VALUE_TYPE_CASTS(SVGPaint, isSVGPaint());

} // namespace WebCore

#endif // SVGPaint_h
