/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtlv_p.h"

#include "qnearfieldtagtype1_p.h"

#include <QtCore/QVariant>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QPair<int, int> qParseReservedMemoryControlTlv(const QByteArray &tlvData)
{
    quint8 position = tlvData.at(0);
    int pageAddr = position >> 4;
    int byteOffset = position & 0x0f;

    int size = quint8(tlvData.at(1));
    if (size == 0)
        size = 256;

    quint8 pageControl = tlvData.at(2);
    int bytesPerPage = pageControl & 0x0f;

    if (!bytesPerPage)
        return qMakePair(0, 0);

    int byteAddress = pageAddr * (1 << bytesPerPage) + byteOffset;
    return qMakePair(byteAddress, size);
}

QPair<int, int> qParseLockControlTlv(const QByteArray &tlvData)
{
    quint8 position = tlvData.at(0);
    int pageAddr = position >> 4;
    int byteOffset = position & 0x0f;

    int size = quint8(tlvData.at(1));
    if (size == 0)
        size = 256;
    size = size / 8;

    quint8 pageControl = tlvData.at(2);
    int bytesPerPage = pageControl & 0x0f;

    if (!bytesPerPage)
        return qMakePair(0, 0);

    int byteAddress = pageAddr * (1 << bytesPerPage) + byteOffset;
    return qMakePair(byteAddress, size);
}

QTlvReader::QTlvReader(QNearFieldTarget *target)
:   m_target(target), m_index(-1)
{
    if (qobject_cast<QNearFieldTagType1 *>(m_target)) {
        addReservedMemory(0, 12);   // skip uid, cc
        addReservedMemory(104, 16); // skip reserved block D, lock block E

        addReservedMemory(120, 8);  // skip reserved block F
    }
}

QTlvReader::QTlvReader(const QByteArray &data)
:   m_target(0), m_rawData(data), m_index(-1)
{
}

void QTlvReader::addReservedMemory(int offset, int length)
{
    m_reservedMemory.insert(offset, length);
}

/*!
    Returns the number of bytes of reserved memory found so far.  The actual number of reserved
    bytes will not be known until atEnd() returns true.
*/
int QTlvReader::reservedMemorySize() const
{
    int total = 0;

    QMap<int, int>::ConstIterator i;
    for (i = m_reservedMemory.constBegin(); i != m_reservedMemory.constEnd(); ++i)
        total += i.value();

    return total;
}

/*!
    Returns the request id that the TLV reader is currently waiting on.
*/
QNearFieldTarget::RequestId QTlvReader::requestId() const
{
    return m_requestId;
}

bool QTlvReader::atEnd() const
{
    if (m_index == -1)
        return false;

    if (m_requestId.isValid())
        return false;

    return (m_index == m_tlvData.length()) || (tag() == 0xfe);
}

/*!
    Moves to the next TLV. Returns true on success; otherwise returns false.
*/
bool QTlvReader::readNext()
{
    if (atEnd())
        return false;

    // Move to next TLV
    if (m_index == -1) {
        m_index = 0;
    } else if (m_requestId.isValid()) {
        // do nothing
    } else if (tag() == 0x00 || tag() == 0xfe) {
        ++m_index;
    } else {
        int tlvLength = length();
        m_index += (tlvLength < 0xff) ? tlvLength + 2 : tlvLength + 4;
    }

    // Ensure that tag byte is available
    if (!readMoreData(m_index))
        return false;

    // Ensure that length byte(s) are available
    if (length() == -1)
        return false;

    // Ensure that data bytes are available
    int tlvLength = length();

    int dataOffset = (tlvLength < 0xff) ? m_index + 2 : m_index + 4;

    if (!readMoreData(dataOffset + tlvLength - 1))
        return false;

    switch (tag()) {
    case 0x01: { // Lock Control TLV
        QPair<int, int> locked = qParseLockControlTlv(data());
        addReservedMemory(locked.first, locked.second);
        break;
    }
    case 0x02: { // Reserved Memory Control TLV
        QPair<int, int> reserved = qParseReservedMemoryControlTlv(data());
        addReservedMemory(reserved.first, reserved.second);
        break;
    }
    }

    return true;
}

quint8 QTlvReader::tag() const
{
    return m_tlvData.at(m_index);
}

int QTlvReader::length()
{
    if (tag() == 0x00 || tag() == 0xfe)
        return 0;

    if (!readMoreData(m_index + 1))
        return -1;

    quint8 shortLength = m_tlvData.at(m_index + 1);
    if (shortLength != 0xff)
        return shortLength;

    if (!readMoreData(m_index + 3))
        return -1;

    quint16 longLength = (quint8(m_tlvData.at(m_index + 2)) << 8) |
                          quint8(m_tlvData.at(m_index + 3));

    if (longLength < 0xff || longLength == 0xffff) {
        qWarning("Invalid 3 byte length");
        return 0;
    }

    return longLength;
}

QByteArray QTlvReader::data()
{
    int tlvLength = length();

    int dataOffset = (tlvLength < 0xff) ? m_index + 2 : m_index + 4;

    if (!readMoreData(dataOffset + tlvLength - 1))
        return QByteArray();

    return m_tlvData.mid(dataOffset, tlvLength);
}

bool QTlvReader::readMoreData(int sparseOffset)
{
    while (sparseOffset >= m_tlvData.length()) {
        int absOffset = absoluteOffset(m_tlvData.length());

        QByteArray data;

        if (!m_rawData.isEmpty()) {
            data = m_rawData.mid(absOffset, dataLength(absOffset));
        } else if (QNearFieldTagType1 *tag = qobject_cast<QNearFieldTagType1 *>(m_target)) {
            quint8 segment = absOffset / 128;

            if (m_requestId.isValid()) {
                QVariant v = m_target->requestResponse(m_requestId);
                if (!v.isValid())
                    return false;

                m_requestId = QNearFieldTarget::RequestId();

                data = v.toByteArray();

                if (absOffset < 120)
                    data = data.mid(2);

                int length = dataLength(absOffset);

                data = data.mid(absOffset - (segment * 128), length);
            } else {
                m_requestId = (absOffset < 120) ? tag->readAll() : tag->readSegment(segment);

                return false;
            }
        }

        if (data.isEmpty() && sparseOffset >= m_tlvData.length())
            return false;

        m_tlvData.append(data);
    }

    return true;
}

int QTlvReader::absoluteOffset(int sparseOffset) const
{
    int absoluteOffset = sparseOffset;
    foreach (int offset, m_reservedMemory.keys()) {
        if (offset <= absoluteOffset)
            absoluteOffset += m_reservedMemory.value(offset);
    }

    return absoluteOffset;
}

/*!
    Returns the length of the contiguous non-reserved data block starting from absolute offset
    \a startOffset.  -1 is return as the length of the last contiguous data block.
*/
int QTlvReader::dataLength(int startOffset) const
{
    foreach (int offset, m_reservedMemory.keys()) {
        if (offset <= startOffset)
            continue;

        return offset - startOffset;
    }

    return -1;
}


QTlvWriter::QTlvWriter(QNearFieldTarget *target)
:   m_target(target), m_rawData(0), m_index(0), m_tagMemorySize(-1)
{
    if (qobject_cast<QNearFieldTagType1 *>(m_target)) {
        addReservedMemory(0, 12);   // skip uid, cc
        addReservedMemory(104, 16); // skip reserved block D, lock block E

        addReservedMemory(120, 8);  // skip reserved block F
    }
}

QTlvWriter::QTlvWriter(QByteArray *data)
:   m_target(0), m_rawData(data), m_index(0), m_tagMemorySize(-1)
{
}

QTlvWriter::~QTlvWriter()
{
    if (m_rawData)
        process(true);
}

void QTlvWriter::addReservedMemory(int offset, int length)
{
    m_reservedMemory.insert(offset, length);
}

void QTlvWriter::writeTlv(quint8 tagType, const QByteArray &data)
{
    m_buffer.append(tagType);

    if (tagType != 0x00 && tagType != 0xfe) {
        int length = data.length();
        if (length < 0xff) {
            m_buffer.append(quint8(length));
        } else {
            m_buffer.append(char(0xff));
            m_buffer.append(quint16(length) >> 8);
            m_buffer.append(quint16(length) & 0x00ff);
        }

        m_buffer.append(data);
    }

    process();

    switch (tagType) {
    case 0x01: {    // Lock Control TLV
        QPair<int, int> locked = qParseLockControlTlv(data);
        addReservedMemory(locked.first, locked.second);
        break;
    }
    case 0x02: {    // Reserved Memory Control TLV
        QPair<int, int> reserved = qParseReservedMemoryControlTlv(data);
        addReservedMemory(reserved.first, reserved.second);
        break;
    }
    }
}

/*!
    Processes more of the TLV writer process. Returns true if the TLVs have been successfully
    written to the target or buffer; otherwise returns false.

    A false return value indicates that an NFC request is pending (if requestId() returns a valid
    request identifier) or the write process has failed (requestId() returns an invalid request
    identifier).
*/
bool QTlvWriter::process(bool all)
{
    if (m_requestId.isValid()) {
        QVariant v = m_target->requestResponse(m_requestId);
        if (!v.isValid())
            return false;
    }

    if (m_tagMemorySize == -1) {
        if (m_rawData)
            m_tagMemorySize = m_rawData->length();
        else if (QNearFieldTagType1 *tag = qobject_cast<QNearFieldTagType1 *>(m_target)) {
            if (m_requestId.isValid()) {
                m_tagMemorySize = 8 * (tag->requestResponse(m_requestId).toUInt() + 1);
                m_requestId = QNearFieldTarget::RequestId();
            } else {
                m_requestId = tag->readByte(10);
                return false;
            }
        }
    }

    while (!m_buffer.isEmpty()) {
        int spaceRemaining = moveToNextAvailable();
        if (spaceRemaining < 1)
            return false;

        int length = qMin(spaceRemaining, m_buffer.length());

        if (m_rawData) {
            m_rawData->replace(m_index, length, m_buffer);
            m_index += length;
            m_buffer = m_buffer.mid(length);
        } else if (QNearFieldTagType1 *tag = qobject_cast<QNearFieldTagType1 *>(m_target)) {
            int bufferIndex = 0;

            // static memory - can only use writeByte()
            while (m_index < 120 && bufferIndex < length) {
                if (m_requestId.isValid()) {
                    if (!m_target->requestResponse(m_requestId).toBool())
                        return false;

                    m_requestId = QNearFieldTarget::RequestId();

                    ++m_index;
                    ++bufferIndex;
                } else {
                    m_requestId = tag->writeByte(m_index, m_buffer.at(bufferIndex));
                    m_buffer = m_buffer.mid(bufferIndex);
                    return false;
                }
            }


            // dynamic memory - writeBlock() full
            while (m_index >= 120 && (m_index % 8 == 0) && bufferIndex + 8 < length) {
                if (m_requestId.isValid()) {
                    if (!m_target->requestResponse(m_requestId).toBool())
                        return false;

                    m_requestId = QNearFieldTarget::RequestId();

                    m_index += 8;
                    bufferIndex += 8;
                } else {
                    m_requestId = tag->writeBlock(m_index / 8, m_buffer.mid(bufferIndex, 8));
                    m_buffer = m_buffer.mid(bufferIndex);
                    return false;
                }
            }

            // partial block
            int currentBlock = m_index / 8;
            int nextBlock = currentBlock + 1;
            int currentBlockStart = currentBlock * 8;
            int nextBlockStart = nextBlock * 8;

            int fillLength = qMin(nextBlockStart - m_index, spaceRemaining - bufferIndex);

            if (fillLength && (all || m_buffer.length() - bufferIndex >= fillLength) &&
                (m_buffer.length() != bufferIndex)) {
                // sufficient data available
                if (m_requestId.isValid()) {
                    const QVariant v = tag->requestResponse(m_requestId);
                    if (v.type() == QVariant::ByteArray) {
                        // read in block
                        QByteArray block = v.toByteArray();

                        int fill = qMin(fillLength, m_buffer.length() - bufferIndex);

                        for (int i = m_index - currentBlockStart; i < fill; ++i)
                            block[i] = m_buffer.at(bufferIndex++);

                        // now write block
                        m_requestId = tag->writeBlock(currentBlock, block);
                        return false;
                    } else if (v.type() == QVariant::Bool) {
                        m_requestId = QNearFieldTarget::RequestId();
                        int fill = qMin(fillLength, m_buffer.length() - bufferIndex);
                        bufferIndex = fill - (m_index - currentBlockStart);

                        // write complete
                        if (!v.toBool())
                            return false;
                    }
                } else {
                    // read in block
                    m_requestId = tag->readBlock(currentBlock);
                    m_buffer = m_buffer.mid(bufferIndex);
                    return false;
                }
            }

            m_buffer = m_buffer.mid(bufferIndex);
        }
    }

    return true;
}

QNearFieldTarget::RequestId QTlvWriter::requestId() const
{
    return m_requestId;
}

int QTlvWriter::moveToNextAvailable()
{
    int length = -1;

    // move index to next available byte
    QMap<int, int>::ConstIterator i;
    for (i = m_reservedMemory.constBegin(); i != m_reservedMemory.constEnd(); ++i) {
        if (m_index < i.key()) {
            length = i.key() - m_index;
            break;
        } else if (m_index == i.key()) {
            m_index += i.value();
        } else if (m_index > i.key() && m_index < (i.key() + i.value())) {
            m_index = i.key() + i.value();
        }
    }

    if (length == -1)
        return m_tagMemorySize - m_index;

    Q_ASSERT(length != -1);

    return length;
}

QT_END_NAMESPACE
