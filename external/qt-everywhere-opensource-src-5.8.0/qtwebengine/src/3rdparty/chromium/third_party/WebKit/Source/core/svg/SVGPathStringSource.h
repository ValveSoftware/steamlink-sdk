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

#include "core/CoreExport.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/SVGPathData.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT SVGPathStringSource {
    WTF_MAKE_NONCOPYABLE(SVGPathStringSource);
    STACK_ALLOCATED();
public:
    explicit SVGPathStringSource(const String&);

    bool hasMoreData() const
    {
        if (m_is8BitSource)
            return m_current.m_character8 < m_end.m_character8;
        return m_current.m_character16 < m_end.m_character16;
    }
    PathSegmentData parseSegment();

    SVGParsingError parseError() const { return m_error; }

private:
    void eatWhitespace();
    float parseNumberWithError();
    bool parseArcFlagWithError();
    void setErrorMark(SVGParseStatus);

    bool m_is8BitSource;

    union {
        const LChar* m_character8;
        const UChar* m_character16;
    } m_current;
    union {
        const LChar* m_character8;
        const UChar* m_character16;
    } m_end;

    SVGPathSegType m_previousCommand;
    SVGParsingError m_error;
    String m_string;
};

} // namespace blink

#endif // SVGPathStringSource_h
