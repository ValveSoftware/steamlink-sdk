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

#ifndef SVGPathByteStream_h
#define SVGPathByteStream_h

#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

template<typename DataType>
union ByteType {
    DataType value;
    unsigned char bytes[sizeof(DataType)];
};

class SVGPathByteStream {
    USING_FAST_MALLOC(SVGPathByteStream);
public:
    static std::unique_ptr<SVGPathByteStream> create()
    {
        return wrapUnique(new SVGPathByteStream);
    }

    std::unique_ptr<SVGPathByteStream> clone() const
    {
        return wrapUnique(new SVGPathByteStream(m_data));
    }

    typedef Vector<unsigned char> Data;
    typedef Data::const_iterator DataIterator;

    DataIterator begin() const { return m_data.begin(); }
    DataIterator end() const { return m_data.end(); }
    void append(const unsigned char* data, size_t dataSize) { m_data.append(data, dataSize); }
    void clear() { m_data.clear(); }
    void reserveInitialCapacity(size_t size) { m_data.reserveInitialCapacity(size); }
    void shrinkToFit() { m_data.shrinkToFit(); }
    bool isEmpty() const { return m_data.isEmpty(); }
    unsigned size() const { return m_data.size(); }

    bool operator==(const SVGPathByteStream& other) const { return m_data == other.m_data; }

private:
    SVGPathByteStream() { }
    SVGPathByteStream(const Data& data)
        : m_data(data)
    {
    }

    Data m_data;
};

} // namespace blink

#endif // SVGPathByteStream_h
