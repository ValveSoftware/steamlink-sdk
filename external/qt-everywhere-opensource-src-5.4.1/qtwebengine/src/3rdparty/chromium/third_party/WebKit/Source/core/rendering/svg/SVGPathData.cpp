/*
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

#include "config.h"
#include "core/rendering/svg/SVGPathData.h"

#include "core/SVGNames.h"
#include "core/svg/SVGCircleElement.h"
#include "core/svg/SVGEllipseElement.h"
#include "core/svg/SVGLineElement.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGPathUtilities.h"
#include "core/svg/SVGPolygonElement.h"
#include "core/svg/SVGPolylineElement.h"
#include "core/svg/SVGRectElement.h"
#include "platform/graphics/Path.h"
#include "wtf/HashMap.h"

namespace WebCore {

using namespace SVGNames;

static void updatePathFromCircleElement(SVGElement* element, Path& path)
{
    SVGCircleElement* circle = toSVGCircleElement(element);

    SVGLengthContext lengthContext(element);
    float r = circle->r()->currentValue()->value(lengthContext);
    if (r > 0)
        path.addEllipse(FloatRect(circle->cx()->currentValue()->value(lengthContext) - r, circle->cy()->currentValue()->value(lengthContext) - r, r * 2, r * 2));
}

static void updatePathFromEllipseElement(SVGElement* element, Path& path)
{
    SVGEllipseElement* ellipse = toSVGEllipseElement(element);

    SVGLengthContext lengthContext(element);
    float rx = ellipse->rx()->currentValue()->value(lengthContext);
    if (rx < 0)
        return;
    float ry = ellipse->ry()->currentValue()->value(lengthContext);
    if (ry < 0)
        return;
    if (!rx && !ry)
        return;

    path.addEllipse(FloatRect(ellipse->cx()->currentValue()->value(lengthContext) - rx, ellipse->cy()->currentValue()->value(lengthContext) - ry, rx * 2, ry * 2));
}

static void updatePathFromLineElement(SVGElement* element, Path& path)
{
    SVGLineElement* line = toSVGLineElement(element);

    SVGLengthContext lengthContext(element);
    path.moveTo(FloatPoint(line->x1()->currentValue()->value(lengthContext), line->y1()->currentValue()->value(lengthContext)));
    path.addLineTo(FloatPoint(line->x2()->currentValue()->value(lengthContext), line->y2()->currentValue()->value(lengthContext)));
}

static void updatePathFromPathElement(SVGElement* element, Path& path)
{
    buildPathFromByteStream(toSVGPathElement(element)->pathByteStream(), path);
}

static void updatePathFromPolylineElement(SVGElement* element, Path& path)
{
    RefPtr<SVGPointList> points = toSVGPolyElement(element)->points()->currentValue();
    if (points->isEmpty())
        return;

    SVGPointList::ConstIterator it = points->begin();
    SVGPointList::ConstIterator itEnd = points->end();
    ASSERT(it != itEnd);
    path.moveTo(it->value());
    ++it;

    for (; it != itEnd; ++it)
        path.addLineTo(it->value());
}

static void updatePathFromPolygonElement(SVGElement* element, Path& path)
{
    updatePathFromPolylineElement(element, path);
    path.closeSubpath();
}

static void updatePathFromRectElement(SVGElement* element, Path& path)
{
    SVGRectElement* rect = toSVGRectElement(element);

    SVGLengthContext lengthContext(element);
    float width = rect->width()->currentValue()->value(lengthContext);
    if (width < 0)
        return;
    float height = rect->height()->currentValue()->value(lengthContext);
    if (height < 0)
        return;
    if (!width && !height)
        return;
    float x = rect->x()->currentValue()->value(lengthContext);
    float y = rect->y()->currentValue()->value(lengthContext);
    float rx = rect->rx()->currentValue()->value(lengthContext);
    float ry = rect->ry()->currentValue()->value(lengthContext);
    bool hasRx = rx > 0;
    bool hasRy = ry > 0;
    if (hasRx || hasRy) {
        if (!hasRx)
            rx = ry;
        else if (!hasRy)
            ry = rx;

        path.addRoundedRect(FloatRect(x, y, width, height), FloatSize(rx, ry));
        return;
    }

    path.addRect(FloatRect(x, y, width, height));
}

void updatePathFromGraphicsElement(SVGElement* element, Path& path)
{
    ASSERT(element);
    ASSERT(path.isEmpty());

    typedef void (*PathUpdateFunction)(SVGElement*, Path&);
    static HashMap<StringImpl*, PathUpdateFunction>* map = 0;
    if (!map) {
        map = new HashMap<StringImpl*, PathUpdateFunction>;
        map->set(circleTag.localName().impl(), updatePathFromCircleElement);
        map->set(ellipseTag.localName().impl(), updatePathFromEllipseElement);
        map->set(lineTag.localName().impl(), updatePathFromLineElement);
        map->set(pathTag.localName().impl(), updatePathFromPathElement);
        map->set(polygonTag.localName().impl(), updatePathFromPolygonElement);
        map->set(polylineTag.localName().impl(), updatePathFromPolylineElement);
        map->set(rectTag.localName().impl(), updatePathFromRectElement);
    }

    if (PathUpdateFunction pathUpdateFunction = map->get(element->localName().impl()))
        (*pathUpdateFunction)(element, path);
}

} // namespace WebCore
