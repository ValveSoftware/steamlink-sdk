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

#include "config.h"

#include "core/svg/SVGPathByteStreamBuilder.h"

#include "core/svg/SVGPathSeg.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

// Helper class that coalesces writes to a SVGPathByteStream to a local buffer.
class CoalescingBuffer {
public:
    CoalescingBuffer(SVGPathByteStream* byteStream)
        : m_currentOffset(0)
        , m_byteStream(byteStream)
    {
        ASSERT(byteStream);
    }
    ~CoalescingBuffer()
    {
        for (size_t i = 0; i < m_currentOffset; ++i)
            m_byteStream->append(m_bytes[i]);
    }

    template<typename DataType>
    void writeType(DataType value)
    {
        ByteType<DataType> data;
        data.value = value;
        size_t typeSize = sizeof(ByteType<DataType>);
        ASSERT(m_currentOffset + typeSize <= sizeof(m_bytes));
        memcpy(m_bytes + m_currentOffset, data.bytes, typeSize);
        m_currentOffset += typeSize;
    }

    void writeFlag(bool value) { writeType<bool>(value); }
    void writeFloat(float value) { writeType<float>(value); }
    void writeFloatPoint(const FloatPoint& point)
    {
        writeType<float>(point.x());
        writeType<float>(point.y());
    }
    void writeSegmentType(unsigned short value) { writeType<unsigned short>(value); }

private:
    // Adjust size to fit the largest command (in serialized/byte-stream format).
    // Currently a cubic segment.
    size_t m_currentOffset;
    unsigned char m_bytes[sizeof(unsigned short) + sizeof(FloatPoint) * 3];
    SVGPathByteStream* m_byteStream;
};

SVGPathByteStreamBuilder::SVGPathByteStreamBuilder()
    : m_byteStream(0)
{
}

void SVGPathByteStreamBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ?  PathSegMoveToRel : PathSegMoveToAbs);
    buffer.writeFloatPoint(targetPoint);
}

void SVGPathByteStreamBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegLineToRel : PathSegLineToAbs);
    buffer.writeFloatPoint(targetPoint);
}

void SVGPathByteStreamBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegLineToHorizontalRel : PathSegLineToHorizontalAbs);
    buffer.writeFloat(x);
}

void SVGPathByteStreamBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegLineToVerticalRel : PathSegLineToVerticalAbs);
    buffer.writeFloat(y);
}

void SVGPathByteStreamBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegCurveToCubicRel : PathSegCurveToCubicAbs);
    buffer.writeFloatPoint(point1);
    buffer.writeFloatPoint(point2);
    buffer.writeFloatPoint(targetPoint);
}

void SVGPathByteStreamBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegCurveToCubicSmoothRel : PathSegCurveToCubicSmoothAbs);
    buffer.writeFloatPoint(point2);
    buffer.writeFloatPoint(targetPoint);
}

void SVGPathByteStreamBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegCurveToQuadraticRel : PathSegCurveToQuadraticAbs);
    buffer.writeFloatPoint(point1);
    buffer.writeFloatPoint(targetPoint);
}

void SVGPathByteStreamBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegCurveToQuadraticSmoothRel : PathSegCurveToQuadraticSmoothAbs);
    buffer.writeFloatPoint(targetPoint);
}

void SVGPathByteStreamBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(mode == RelativeCoordinates ? PathSegArcRel : PathSegArcAbs);
    buffer.writeFloat(r1);
    buffer.writeFloat(r2);
    buffer.writeFloat(angle);
    buffer.writeFlag(largeArcFlag);
    buffer.writeFlag(sweepFlag);
    buffer.writeFloatPoint(targetPoint);
}

void SVGPathByteStreamBuilder::closePath()
{
    CoalescingBuffer buffer(m_byteStream);
    buffer.writeSegmentType(PathSegClosePath);
}

} // namespace WebCore
