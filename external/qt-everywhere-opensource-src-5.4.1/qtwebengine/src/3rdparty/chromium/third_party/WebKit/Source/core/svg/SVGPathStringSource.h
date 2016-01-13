/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SVGPathStringSource_h
#define SVGPathStringSource_h

#include "core/svg/SVGPathSource.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class SVGPathStringSource FINAL : public SVGPathSource {
public:
    static PassOwnPtr<SVGPathStringSource> create(const String& string)
    {
        return adoptPtr(new SVGPathStringSource(string));
    }

private:
    SVGPathStringSource(const String&);

    virtual bool hasMoreData() const OVERRIDE;
    virtual bool moveToNextToken() OVERRIDE;
    virtual bool parseSVGSegmentType(SVGPathSegType&) OVERRIDE;
    virtual SVGPathSegType nextCommand(SVGPathSegType previousCommand) OVERRIDE;

    virtual bool parseMoveToSegment(FloatPoint&) OVERRIDE;
    virtual bool parseLineToSegment(FloatPoint&) OVERRIDE;
    virtual bool parseLineToHorizontalSegment(float&) OVERRIDE;
    virtual bool parseLineToVerticalSegment(float&) OVERRIDE;
    virtual bool parseCurveToCubicSegment(FloatPoint&, FloatPoint&, FloatPoint&) OVERRIDE;
    virtual bool parseCurveToCubicSmoothSegment(FloatPoint&, FloatPoint&) OVERRIDE;
    virtual bool parseCurveToQuadraticSegment(FloatPoint&, FloatPoint&) OVERRIDE;
    virtual bool parseCurveToQuadraticSmoothSegment(FloatPoint&) OVERRIDE;
    virtual bool parseArcToSegment(float&, float&, float&, bool&, bool&, FloatPoint&) OVERRIDE;

    String m_string;
    bool m_is8BitSource;

    union {
        const LChar* m_character8;
        const UChar* m_character16;
    } m_current;
    union {
        const LChar* m_character8;
        const UChar* m_character16;
    } m_end;
};

} // namespace WebCore

#endif // SVGPathStringSource_h
