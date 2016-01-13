/*
 * Copyright (C) Research In Motion Limited 2010, 2012. All rights reserved.
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

#ifndef SVGPathUtilities_h
#define SVGPathUtilities_h

#include "core/svg/SVGPathConsumer.h"
#include "platform/geometry/FloatPoint.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Path;
class SVGPathByteStream;
class SVGPathSeg;
class SVGPathSegList;

// String/SVGPathByteStream -> Path
bool buildPathFromString(const String&, Path&);
bool buildPathFromByteStream(const SVGPathByteStream*, Path&);

// SVGPathSegList/String -> SVGPathByteStream
bool appendSVGPathByteStreamFromSVGPathSeg(PassRefPtr<SVGPathSeg>, SVGPathByteStream*, PathParsingMode);
bool buildSVGPathByteStreamFromString(const String&, SVGPathByteStream*, PathParsingMode);

// SVGPathByteStream/SVGPathSegList -> String
bool buildStringFromByteStream(const SVGPathByteStream*, String&, PathParsingMode);
bool buildStringFromSVGPathSegList(PassRefPtr<SVGPathSegList>, String&, PathParsingMode);

bool addToSVGPathByteStream(SVGPathByteStream*, const SVGPathByteStream*, unsigned repeatCount = 1);

bool getSVGPathSegAtLengthFromSVGPathByteStream(const SVGPathByteStream*, float length, unsigned& pathSeg);
bool getTotalLengthOfSVGPathByteStream(const SVGPathByteStream*, float& totalLength);
bool getPointAtLengthOfSVGPathByteStream(const SVGPathByteStream*, float length, FloatPoint&);

} // namespace WebCore

#endif // SVGPathUtilities_h
