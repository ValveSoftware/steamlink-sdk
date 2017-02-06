/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "core/svg/SVGPathQuery.h"

#include "core/svg/SVGPathByteStreamSource.h"
#include "core/svg/SVGPathConsumer.h"
#include "core/svg/SVGPathData.h"
#include "core/svg/SVGPathParser.h"
#include "platform/graphics/PathTraversalState.h"

namespace blink {

namespace {

class SVGPathTraversalState final : public SVGPathConsumer {
public:
    SVGPathTraversalState(PathTraversalState::PathTraversalAction traversalAction, float desiredLength = 0)
        : m_traversalState(traversalAction)
        , m_segmentIndex(0)
    {
        m_traversalState.m_desiredLength = desiredLength;
    }

    unsigned segmentIndex() const { return m_segmentIndex; }
    float totalLength() const { return m_traversalState.m_totalLength; }
    FloatPoint computedPoint() const { return m_traversalState.m_current; }

    bool processSegment(bool hasMoreData)
    {
        m_traversalState.processSegment();
        if (m_traversalState.m_success)
            return true;
        if (hasMoreData)
            m_segmentIndex++;
        return false;
    }

private:
    void emitSegment(const PathSegmentData&) override;

    PathTraversalState m_traversalState;
    unsigned m_segmentIndex;
};

void SVGPathTraversalState::emitSegment(const PathSegmentData& segment)
{
    switch (segment.command) {
    case PathSegMoveToAbs:
        m_traversalState.m_totalLength += m_traversalState.moveTo(segment.targetPoint);
        break;
    case PathSegLineToAbs:
        m_traversalState.m_totalLength += m_traversalState.lineTo(segment.targetPoint);
        break;
    case PathSegClosePath:
        m_traversalState.m_totalLength += m_traversalState.closeSubpath();
        break;
    case PathSegCurveToCubicAbs:
        m_traversalState.m_totalLength += m_traversalState.cubicBezierTo(segment.point1, segment.point2, segment.targetPoint);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void executeQuery(const SVGPathByteStream& pathByteStream, SVGPathTraversalState& traversalState)
{
    SVGPathByteStreamSource source(pathByteStream);
    SVGPathNormalizer normalizer(&traversalState);

    bool hasMoreData = source.hasMoreData();
    while (hasMoreData) {
        PathSegmentData segment = source.parseSegment();
        ASSERT(segment.command != PathSegUnknown);

        normalizer.emitSegment(segment);

        hasMoreData = source.hasMoreData();
        if (traversalState.processSegment(hasMoreData))
            break;
    }
}

} // namespace

SVGPathQuery::SVGPathQuery(const SVGPathByteStream& pathByteStream)
    : m_pathByteStream(pathByteStream)
{
}

unsigned SVGPathQuery::getPathSegIndexAtLength(float length) const
{
    SVGPathTraversalState traversalState(PathTraversalState::TraversalSegmentAtLength, length);
    executeQuery(m_pathByteStream, traversalState);
    return traversalState.segmentIndex();
}

float SVGPathQuery::getTotalLength() const
{
    SVGPathTraversalState traversalState(PathTraversalState::TraversalTotalLength);
    executeQuery(m_pathByteStream, traversalState);
    return traversalState.totalLength();
}

FloatPoint SVGPathQuery::getPointAtLength(float length) const
{
    SVGPathTraversalState traversalState(PathTraversalState::TraversalPointAtLength, length);
    executeQuery(m_pathByteStream, traversalState);
    return traversalState.computedPoint();
}

} // namespace blink
