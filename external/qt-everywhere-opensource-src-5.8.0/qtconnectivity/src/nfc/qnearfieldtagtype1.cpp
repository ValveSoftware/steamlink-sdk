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

#include "qnearfieldtagtype1_p.h"
#include "qnearfieldtarget_p.h"
#include "qndefmessage.h"
#include "qtlv_p.h"

#include <QtCore/QByteArray>
#include <QtCore/QVariant>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

/*!
    \class QNearFieldTagType1
    \brief The QNearFieldTagType1 class provides an interface for communicating with an NFC Tag
           Type 1 tag.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \internal
*/

/*!
    \enum QNearFieldTagType1::WriteMode
    \brief This enum describes the write modes that are supported.

    \value EraseAndWrite    The memory is erased before the new value is written.
    \value WriteOnly        The memory is not erased before the new value is written. The effect of
                            this mode is that the final value store is the bitwise or of the data
                            to be written and the original data value.
*/

/*!
    \fn Type QNearFieldTagType1::type() const
    \reimp
*/

class QNearFieldTagType1Private
{
    Q_DECLARE_PUBLIC(QNearFieldTagType1)

public:
    QNearFieldTagType1Private(QNearFieldTagType1 *q)
    :   q_ptr(q), m_readNdefMessageState(NotReadingNdefMessage),
        m_tlvReader(0),
        m_writeNdefMessageState(NotWritingNdefMessage),
        m_tlvWriter(0)
    { }

    QNearFieldTagType1 *q_ptr;

    QMap<QNearFieldTarget::RequestId, QByteArray> m_pendingInternalCommands;

    enum ReadNdefMessageState {
        NotReadingNdefMessage,
        NdefReadCheckingIdentification,
        NdefReadCheckingNdefMagicNumber,
        NdefReadReadingTlv
    };

    void progressToNextNdefReadMessageState();
    ReadNdefMessageState m_readNdefMessageState;
    QNearFieldTarget::RequestId m_readNdefRequestId;

    QTlvReader *m_tlvReader;
    QNearFieldTarget::RequestId m_nextExpectedRequestId;

    enum WriteNdefMessageState {
        NotWritingNdefMessage,
        NdefWriteCheckingIdentification,
        NdefWriteCheckingNdefMagicNumber,
        NdefWriteReadingTlv,
        NdefWriteWritingTlv,
        NdefWriteWritingTlvFlush
    };

    void progressToNextNdefWriteMessageState();
    WriteNdefMessageState m_writeNdefMessageState;
    QNearFieldTarget::RequestId m_writeNdefRequestId;
    QList<QNdefMessage> m_ndefWriteMessages;

    QTlvWriter *m_tlvWriter;

    typedef QPair<quint8, QByteArray> Tlv;
    QList<Tlv> m_tlvs;
};

void QNearFieldTagType1Private::progressToNextNdefReadMessageState()
{
    Q_Q(QNearFieldTagType1);

    switch (m_readNdefMessageState) {
    case NotReadingNdefMessage:
        m_readNdefMessageState = NdefReadCheckingIdentification;
        m_nextExpectedRequestId = q->readIdentification();
        break;
    case NdefReadCheckingIdentification: {
        const QByteArray data = q->requestResponse(m_nextExpectedRequestId).toByteArray();

        if (data.isEmpty()) {
            m_readNdefMessageState = NotReadingNdefMessage;
            m_nextExpectedRequestId = QNearFieldTarget::RequestId();
            emit q->error(QNearFieldTarget::NdefReadError, m_readNdefRequestId);
            m_readNdefRequestId = QNearFieldTarget::RequestId();
            break;
        }

        quint8 hr0 = data.at(0);

        // Check if target is a NFC TagType1 tag
        if (!(hr0 & 0x10)) {
            m_readNdefMessageState = NotReadingNdefMessage;
            m_nextExpectedRequestId = QNearFieldTarget::RequestId();
            emit q->error(QNearFieldTarget::NdefReadError, m_readNdefRequestId);
            m_readNdefRequestId = QNearFieldTarget::RequestId();
            break;
        }

        m_readNdefMessageState = NdefReadCheckingNdefMagicNumber;
        m_nextExpectedRequestId = q->readByte(8);
        break;
    }
    case NdefReadCheckingNdefMagicNumber: {
        quint8 ndefMagicNumber = q->requestResponse(m_nextExpectedRequestId).toUInt();
        m_nextExpectedRequestId = QNearFieldTarget::RequestId();

        if (ndefMagicNumber != 0xe1) {
            m_readNdefMessageState = NotReadingNdefMessage;
            emit q->error(QNearFieldTarget::NdefReadError, m_readNdefRequestId);
            m_readNdefRequestId = QNearFieldTarget::RequestId();
            break;
        }

        m_readNdefMessageState = NdefReadReadingTlv;
        delete m_tlvReader;
        m_tlvReader = new QTlvReader(q);

        // fall through
    }
    case NdefReadReadingTlv:
        Q_ASSERT(m_tlvReader);
        while (!m_tlvReader->atEnd()) {
            if (!m_tlvReader->readNext())
                break;

            // NDEF Message TLV
            if (m_tlvReader->tag() == 0x03) {
                Q_Q(QNearFieldTagType1);

                emit q->ndefMessageRead(QNdefMessage::fromByteArray(m_tlvReader->data()));
            }
        }

        m_nextExpectedRequestId = m_tlvReader->requestId();
        if (!m_nextExpectedRequestId.isValid()) {
            delete m_tlvReader;
            m_tlvReader = 0;
            m_readNdefMessageState = NotReadingNdefMessage;
            emit q->requestCompleted(m_readNdefRequestId);
            m_readNdefRequestId = QNearFieldTarget::RequestId();
        }
        break;
    }
}

void QNearFieldTagType1Private::progressToNextNdefWriteMessageState()
{
    Q_Q(QNearFieldTagType1);

    switch (m_writeNdefMessageState) {
    case NotWritingNdefMessage:
        m_writeNdefMessageState = NdefWriteCheckingIdentification;
        m_nextExpectedRequestId = q->readIdentification();
        break;
    case NdefWriteCheckingIdentification: {
        const QByteArray data = q->requestResponse(m_nextExpectedRequestId).toByteArray();

        if (data.isEmpty()) {
            m_writeNdefMessageState = NotWritingNdefMessage;
            m_nextExpectedRequestId = QNearFieldTarget::RequestId();
            emit q->error(QNearFieldTarget::NdefWriteError, m_writeNdefRequestId);
            m_writeNdefRequestId = QNearFieldTarget::RequestId();
            break;
        }

        quint8 hr0 = data.at(0);

        // Check if target is a NFC TagType1 tag
        if (!(hr0 & 0x10)) {
            m_writeNdefMessageState = NotWritingNdefMessage;
            m_nextExpectedRequestId = QNearFieldTarget::RequestId();
            emit q->error(QNearFieldTarget::NdefWriteError, m_writeNdefRequestId);
            m_writeNdefRequestId = QNearFieldTarget::RequestId();
            break;
        }

        m_writeNdefMessageState = NdefWriteCheckingNdefMagicNumber;
        m_nextExpectedRequestId = q->readByte(8);
        break;
    }
    case NdefWriteCheckingNdefMagicNumber: {
        quint8 ndefMagicNumber = q->requestResponse(m_nextExpectedRequestId).toUInt();
        m_nextExpectedRequestId = QNearFieldTarget::RequestId();

        if (ndefMagicNumber != 0xe1) {
            m_writeNdefMessageState = NotWritingNdefMessage;
            emit q->error(QNearFieldTarget::NdefWriteError, m_writeNdefRequestId);
            m_writeNdefRequestId = QNearFieldTarget::RequestId();
            break;
        }

        m_writeNdefMessageState = NdefWriteReadingTlv;
        delete m_tlvReader;
        m_tlvReader = new QTlvReader(q);

        // fall through
    }
    case NdefWriteReadingTlv:
        Q_ASSERT(m_tlvReader);
        while (!m_tlvReader->atEnd()) {
            if (!m_tlvReader->readNext())
                break;

            quint8 tag = m_tlvReader->tag();
            if (tag == 0x01 || tag == 0x02 || tag == 0xfd)
                m_tlvs.append(qMakePair(tag, m_tlvReader->data()));
        }

        m_nextExpectedRequestId = m_tlvReader->requestId();
        if (m_nextExpectedRequestId.isValid())
            break;

        delete m_tlvReader;
        m_tlvReader = 0;
        m_writeNdefMessageState = NdefWriteWritingTlv;

        // fall through
    case NdefWriteWritingTlv:
        delete m_tlvWriter;
        m_tlvWriter = new QTlvWriter(q);

        // write old TLVs
        foreach (const Tlv &tlv, m_tlvs)
            m_tlvWriter->writeTlv(tlv.first, tlv.second);

        // write new NDEF message TLVs
        foreach (const QNdefMessage &message, m_ndefWriteMessages)
            m_tlvWriter->writeTlv(0x03, message.toByteArray());

        // write terminator TLV
        m_tlvWriter->writeTlv(0xfe);

        m_writeNdefMessageState = NdefWriteWritingTlvFlush;

        // fall through
    case NdefWriteWritingTlvFlush:
        // flush the writer
        Q_ASSERT(m_tlvWriter);
        if (m_tlvWriter->process(true)) {
            m_nextExpectedRequestId = QNearFieldTarget::RequestId();
            m_writeNdefMessageState = NotWritingNdefMessage;
            delete m_tlvWriter;
            m_tlvWriter = 0;
            emit q->ndefMessagesWritten();
            emit q->requestCompleted(m_writeNdefRequestId);
            m_writeNdefRequestId = QNearFieldTarget::RequestId();
        } else {
            m_nextExpectedRequestId = m_tlvWriter->requestId();
            if (!m_nextExpectedRequestId.isValid()) {
                m_writeNdefMessageState = NotWritingNdefMessage;
                delete m_tlvWriter;
                m_tlvWriter = 0;
                emit q->error(QNearFieldTarget::NdefWriteError, m_writeNdefRequestId);
                m_writeNdefRequestId = QNearFieldTarget::RequestId();
            }
        }
        break;
    }
}

static QVariant decodeResponse(const QByteArray &command, const QByteArray &response)
{
    switch (command.at(0)) {
    case 0x01:  // READ
        if (command.at(1) == response.at(0))
            return quint8(response.at(1));
        break;
    case 0x53: { // WRITE-E
        quint8 address = command.at(1);
        quint8 data = command.at(2);
        quint8 writeAddress = response.at(0);
        quint8 writeData = response.at(1);

        return ((writeAddress == address) && (writeData == data));
    }
    case 0x1a: { // WRITE-NE
        quint8 address = command.at(1);
        quint8 data = command.at(2);
        quint8 writeAddress = response.at(0);
        quint8 writeData = response.at(1);

        return ((writeAddress == address) && ((writeData & data) == data));
    }
    case 0x10: { // RSEG
        quint8 segmentAddress = quint8(command.at(1)) >> 4;
        quint8 readSegmentAddress = quint8(response.at(0)) >> 4;
        if (readSegmentAddress == segmentAddress)
            return response.mid(1);
        break;
    }
    case 0x02: { // READ8
        quint8 blockAddress = command.at(1);
        quint8 readBlockAddress = response.at(0);
        if (readBlockAddress == blockAddress)
            return response.mid(1);
        break;
    }
    case 0x54: { // WRITE-E8
        quint8 blockAddress = command.at(1);
        QByteArray data = command.mid(2, 8);
        quint8 writeBlockAddress = response.at(0);
        QByteArray writeData = response.mid(1);

        return ((writeBlockAddress == blockAddress) && (writeData == data));
    }
    case 0x1b: { // WRITE-NE8
        quint8 blockAddress = command.at(1);
        QByteArray data = command.mid(2, 8);
        quint8 writeBlockAddress = response.at(0);
        QByteArray writeData = response.mid(1);

        if (writeBlockAddress != blockAddress)
            return false;

        for (int i = 0; i < writeData.length(); ++i) {
            if ((writeData.at(i) & data.at(i)) != data.at(i))
                return false;
        }

        return true;
    }
    }

    return QVariant();
}

/*!
    Constructs a new tag type 1 near field target with \a parent.
*/
QNearFieldTagType1::QNearFieldTagType1(QObject *parent)
:   QNearFieldTarget(parent), d_ptr(new QNearFieldTagType1Private(this))
{
}

/*!
    Destroys the tag type 1 near field target.
*/
QNearFieldTagType1::~QNearFieldTagType1()
{
    delete d_ptr;
}

/*!
    \reimp
*/
bool QNearFieldTagType1::hasNdefMessage()
{
    RequestId id = readAll();
    if (!waitForRequestCompleted(id))
        return false;

    const QByteArray data = requestResponse(id).toByteArray();

    if (data.isEmpty())
        return false;

    quint8 hr0 = data.at(0);

    // Check if target is a NFC TagType1 tag
    if (!(hr0 & 0x10))
        return false;

    // Check if NDEF Message Magic number is present
    quint8 nmn = data.at(10);
    if (nmn != 0xe1)
        return false;

    // Check if TLV contains NDEF Message
    return true;
}

/*!
    \reimp
*/
QNearFieldTarget::RequestId QNearFieldTagType1::readNdefMessages()
{
    Q_D(QNearFieldTagType1);

    d->m_readNdefRequestId = RequestId(new RequestIdPrivate);

    if (d->m_readNdefMessageState == QNearFieldTagType1Private::NotReadingNdefMessage) {
        d->progressToNextNdefReadMessageState();
    } else {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, NdefReadError),
                                  Q_ARG(QNearFieldTarget::RequestId, d->m_readNdefRequestId));
    }

    return d->m_readNdefRequestId;
}

/*!
    \reimp
*/
QNearFieldTarget::RequestId QNearFieldTagType1::writeNdefMessages(const QList<QNdefMessage> &messages)
{
    Q_D(QNearFieldTagType1);

    d->m_writeNdefRequestId = RequestId(new RequestIdPrivate);

    if (d->m_readNdefMessageState == QNearFieldTagType1Private::NotReadingNdefMessage &&
        d->m_writeNdefMessageState == QNearFieldTagType1Private::NotWritingNdefMessage) {
        d->m_ndefWriteMessages = messages;
        d->progressToNextNdefWriteMessageState();
    } else {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, NdefWriteError),
                                  Q_ARG(QNearFieldTarget::RequestId, d->m_readNdefRequestId));
    }

    return d->m_writeNdefRequestId;
}

/*!
    Returns the NFC Tag Type 1 specification version number that the tag supports.
*/
quint8 QNearFieldTagType1::version()
{
    RequestId id = readByte(9);
    if (!waitForRequestCompleted(id))
        return 0;

    quint8 versionNumber = requestResponse(id).toUInt();
    return versionNumber;
}

/*!
    Returns the memory size in bytes of the tag.
*/
int QNearFieldTagType1::memorySize()
{
    RequestId id = readByte(10);
    if (!waitForRequestCompleted(id))
        return 0;

    quint8 tms = requestResponse(id).toUInt();

    return 8 * (tms + 1);
}

/*!
    Requests the identification bytes from the target. Returns a request id which can be used to
    track the completion status of the request.

    Once the request completes successfully the response can be retrieved from the
    requestResponse() function. The response of this request will be a QByteArray containing: HR0,
    HR1, UID0, UID1, UID2 and UID3 in order.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType1::readIdentification()
{
    QByteArray command;
    command.append(char(0x78));     // RID
    command.append(char(0x00));     // Address (unused)
    command.append(char(0x00));     // Data (unused)
    command.append(uid().left(4));  // 4 bytes of UID

    return sendCommand(command);
}

/*!
    Requests all data in the static memory area of the target. Returns a request id which can be
    used to track the completion status of the request.

    Once the request completes successfully the response can be retrieved from the
    requestResponse() function. The response of this request will be a QByteArray containing: HR0
    and HR1 followed by the 120 bytes of data stored in the static memory area of the target.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType1::readAll()
{
    QByteArray command;
    command.append(char(0x00));   // RALL
    command.append(char(0x00));   // Address (unused)
    command.append(char(0x00));   // Data (unused)
    command.append(uid().left(4));// 4 bytes of UID

    return sendCommand(command);
}

/*!
    Requests a single byte from the static memory area of the tag. The \a address parameter
    specifices the linear byte address to read. Returns a request id which can be used to track
    the completion status of the request.

    Once the request completes successfully the response can be retrieved from the
    requestResponse() function. The response of this request will be a quint8.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType1::readByte(quint8 address)
{
    if (address & 0x80)
        return RequestId();

    QByteArray command;
    command.append(char(0x01));     // READ
    command.append(char(address));  // Address
    command.append(char(0x00));     // Data (unused)
    command.append(uid().left(4));  // 4 bytes of UID

    RequestId id = sendCommand(command);

    Q_D(QNearFieldTagType1);

    d->m_pendingInternalCommands.insert(id, command);

    return id;
}

/*!
    Writes a single \a data byte to the linear byte \a address on the tag. If \a mode is
    EraseAndWrite the byte will be erased before writing. If \a mode is WriteOnly the contents will
    not be erased before writing. This is equivelant to writing the result of the bitwise OR of
    \a data and the original value.

    Returns a request id which can be used to track the completion status of the request.

    Once the request completes the response can be retrieved from the requestResponse() function.
    The response of this request will be a boolean value, true for success; otherwise false.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType1::writeByte(quint8 address, quint8 data,
                                                          WriteMode mode)
{
    if (address & 0x80)
        return RequestId();

    QByteArray command;

    if (mode == EraseAndWrite)
        command.append(char(0x53)); // WRITE-E
    else if (mode == WriteOnly)
        command.append(char(0x1a)); // WRITE-NE
    else
        return RequestId();

    command.append(char(address));  // Address
    command.append(char(data));     // Data
    command.append(uid().left(4));  // 4 bytes of UID

    RequestId id = sendCommand(command);

    Q_D(QNearFieldTagType1);

    d->m_pendingInternalCommands.insert(id, command);

    return id;
}

/*!
    Requests 128 bytes of data from the segment specified by \a segmentAddress. Returns a request
    id which can be used to track the completion status of the request.

    Once the request completes successfully the response can be retrieved from the
    requestResponse() function. The response of this request will be a QByteArray.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType1::readSegment(quint8 segmentAddress)
{
    if (segmentAddress & 0xf0)
        return RequestId();

    QByteArray command;
    command.append(char(0x10));                 // RSEG
    command.append(char(segmentAddress << 4));  // Segment address
    command.append(QByteArray(8, char(0x00)));  // Data (unused)
    command.append(uid().left(4));              // 4 bytes of UID

    RequestId id = sendCommand(command);

    Q_D(QNearFieldTagType1);

    d->m_pendingInternalCommands.insert(id, command);

    return id;
}

/*!
    Requests 8 bytes of data from the block specified by \a blockAddress. Returns a request id
    which can be used to track the completion status of the request.

    Once the request completes successfully the response can be retrieved from the
    requestResponse() function. The response of this request will be a QByteArray.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType1::readBlock(quint8 blockAddress)
{
    QByteArray command;
    command.append(char(0x02));                 // READ8
    command.append(char(blockAddress));         // Block address
    command.append(QByteArray(8, char(0x00)));  // Data (unused)
    command.append(uid().left(4));              // 4 bytes of UID

    RequestId id = sendCommand(command);

    Q_D(QNearFieldTagType1);

    d->m_pendingInternalCommands.insert(id, command);

    return id;
}

/*!
    Writes 8 bytes of \a data to the block specified by \a blockAddress. If \a mode is
    EraseAndWrite the bytes will be erased before writing. If \a mode is WriteOnly the contents
    will not be erased before writing. This is equivelant to writing the result of the bitwise OR
    of \a data and the original value.

    Returns a request id which can be used to track the completion status of the request.

    Once the request completes the response can be retrieved from the requestResponse() function.
    The response of this request will be a boolean value, true for success; otherwise false.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType1::writeBlock(quint8 blockAddress,
                                                           const QByteArray &data,
                                                           WriteMode mode)
{
    if (data.length() != 8)
        return RequestId();

    QByteArray command;

    if (mode == EraseAndWrite)
        command.append(char(0x54));     // WRITE-E8
    else if (mode == WriteOnly)
        command.append(char(0x1b));     // WRITE-NE8
    else
        return RequestId();

    command.append(char(blockAddress)); // Block address
    command.append(data);               // Data
    command.append(uid().left(4));      // 4 bytes of UID

    RequestId id = sendCommand(command);

    Q_D(QNearFieldTagType1);

    d->m_pendingInternalCommands.insert(id, command);

    return id;
}

/*!
    \reimp
*/
bool QNearFieldTagType1::handleResponse(const QNearFieldTarget::RequestId &id,
                                        const QByteArray &response)
{
    Q_D(QNearFieldTagType1);

    bool handled;

    if (d->m_pendingInternalCommands.contains(id)) {
        const QByteArray command = d->m_pendingInternalCommands.take(id);

        QVariant decodedResponse = decodeResponse(command, response);
        setResponseForRequest(id, decodedResponse);

        handled = true;
    } else {
        handled = QNearFieldTarget::handleResponse(id, response);
    }

    // continue reading / writing NDEF message
    if (d->m_nextExpectedRequestId == id) {
        if (d->m_readNdefMessageState != QNearFieldTagType1Private::NotReadingNdefMessage)
            d->progressToNextNdefReadMessageState();
        else if (d->m_writeNdefMessageState != QNearFieldTagType1Private::NotWritingNdefMessage)
            d->progressToNextNdefWriteMessageState();
    }

    return handled;
}

QT_END_NAMESPACE
