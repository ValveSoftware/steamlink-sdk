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

#include "qnearfieldtarget.h"
#include "qnearfieldtarget_p.h"
#include "qndefmessage.h"

#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVariant>

#include <QtCore/QDebug>

#include <QTime>
#include <QCoreApplication>

QT_BEGIN_NAMESPACE

/*!
    \class QNearFieldTarget
    \brief The QNearFieldTarget class provides an interface for communicating with a target
           device.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since 5.2

    QNearFieldTarget provides a generic interface for communicating with an NFC target device.
    Both NFC Forum devices and NFC Forum Tag targets are supported by this class.  All target
    specific classes subclass this class.

    The type() function can be used to get the type of the target device.  The uid() function
    returns the unique identifier of the target.  The AccessMethods flags returns from the
    accessMethods() function can be tested to determine which access methods are supported by the
    target.

    If the target supports NdefAccess, hasNdefMessage() can be called to test if the target has a
    stored NDEF message, readNdefMessages() and writeNdefMessages() functions can be used to get
    and set the NDEF message.

    If the target supports TagTypeSpecificAccess, sendCommand() can be used to send a single
    proprietary command to the target and retrieve the response.  sendCommands() can be used to
    send multiple proprietary commands to the target and retrieve all of the responses.

    If the target supports LlcpAccess, the QLlcpSocket class can be used to connected to a
    service provided by the target.
*/

/*!
    \enum QNearFieldTarget::Type

    This enum describes the type of tag the target is detected as.

    \value ProprietaryTag   An unidentified proprietary target tag.
    \value NfcTagType1      An NFC tag type 1 target.
    \value NfcTagType2      An NFC tag type 2 target.
    \value NfcTagType3      An NFC tag type 3 target.
    \value NfcTagType4      An NFC tag type 4 target.
    \value MifareTag        A Mifare target.
*/

/*!
    \enum QNearFieldTarget::AccessMethod

    This enum describes the access methods a near field target supports.

    \value UnknownAccess            The target supports an unknown access type.
    \value NdefAccess               The target supports reading and writing NDEF messages using
                                    readNdefMessages() and writeNdefMessages().
    \value TagTypeSpecificAccess    The target supports sending tag type specific commands using
                                    sendCommand() and sendCommands().
    \value LlcpAccess               The target supports peer-to-peer LLCP communication.
*/

/*!
    \enum QNearFieldTarget::Error

    This enum describes the error codes that that a near field target reports.

    \value NoError                  No error has occurred.
    \value UnknownError             An unidentified error occurred.
    \value UnsupportedError         The requested operation is unsupported by this near field
                                    target.
    \value TargetOutOfRangeError    The target is no longer within range.
    \value NoResponseError          The target did not respond.
    \value ChecksumMismatchError    The checksum has detected a corrupted response.
    \value InvalidParametersError   Invalid parameters were passed to a tag type specific function.
    \value NdefReadError            Failed to read NDEF messages from the target.
    \value NdefWriteError           Failed to write NDEF messages to the target.
*/

// Copied from qbytearray.cpp
// Modified to initialize the crc with 0x6363 instead of 0xffff and to not invert the final result.
static const quint16 crc_tbl[16] = {
    0x0000, 0x1081, 0x2102, 0x3183,
    0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f
};

/*!
    \relates QNearFieldTarget

    Returns the NFC checksum of the first \a len bytes of \a data.
*/
quint16 qNfcChecksum(const char *data, uint len)
{
    quint16 crc = 0x6363;
    uchar c;
    const uchar *p = reinterpret_cast<const uchar *>(data);
    while (len--) {
        c = *p++;
        crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
        c >>= 4;
        crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
    }
    return crc;
}

/*!
    \fn void QNearFieldTarget::disconnected()

    This signal is emitted when the near field target moves out of proximity.
*/

/*!
    \fn void QNearFieldTarget::ndefMessageRead(const QNdefMessage &message)

    This signal is emitted when a complete NDEF \a message has been read from the target.

    \sa readNdefMessages()
*/

/*!
    \fn void QNearFieldTarget::ndefMessagesWritten()

    This signal is emitted when NDEF messages have been successfully written to the target.

    \sa writeNdefMessages()
*/

/*!
    \fn void QNearFieldTarget::requestCompleted(const QNearFieldTarget::RequestId &id)

    This signal is emitted when a request \a id completes.

    \sa sendCommand()
*/

/*!
    \fn void QNearFieldTarget::error(QNearFieldTarget::Error error, const QNearFieldTarget::RequestId &id)

    This signal is emitted when an error occurs while processing request \a id. The \a error
    parameter describes the error.
*/

/*!
    Constructs a new invalid request id handle.
*/
QNearFieldTarget::RequestId::RequestId()
{
}

/*!
    Constructs a new request id handle that is a copy of \a other.
*/
QNearFieldTarget::RequestId::RequestId(const RequestId &other)
:   d(other.d)
{
}

/*!
    \internal
*/
QNearFieldTarget::RequestId::RequestId(RequestIdPrivate *p)
:   d(p)
{
}

/*!
    Destroys the request id handle.
*/
QNearFieldTarget::RequestId::~RequestId()
{
}

/*!
    Returns true if this is a valid request id; otherwise returns false.
*/
bool QNearFieldTarget::RequestId::isValid() const
{
    return d;
}

/*!
    Returns the current reference count of the request id.
*/
int QNearFieldTarget::RequestId::refCount() const
{
    if (d)
        return d->ref.load();

    return 0;
}

/*!
    \internal
*/
bool QNearFieldTarget::RequestId::operator<(const RequestId &other) const
{
    return d < other.d;
}

/*!
    \internal
*/
bool QNearFieldTarget::RequestId::operator==(const RequestId &other) const
{
    return d == other.d;
}

/*!
    \internal
*/
bool QNearFieldTarget::RequestId::operator!=(const RequestId &other) const
{
    return d != other.d;
}

/*!
    Assigns a copy of \a other to this request id and returns a reference to this request id.
*/
QNearFieldTarget::RequestId &QNearFieldTarget::RequestId::operator=(const RequestId &other)
{
    d = other.d;
    return *this;
}

/*!
    Constructs a new near field target with \a parent.
*/
QNearFieldTarget::QNearFieldTarget(QObject *parent)
:   QObject(parent), d_ptr(new QNearFieldTargetPrivate)
{
    qRegisterMetaType<QNearFieldTarget::RequestId>();
    qRegisterMetaType<QNearFieldTarget::Error>();
    qRegisterMetaType<QNdefMessage>();
}

/*!
    Destroys the near field target.
*/
QNearFieldTarget::~QNearFieldTarget()
{
    delete d_ptr;
}

/*!
    \fn QByteArray QNearFieldTarget::uid() const = 0

    Returns the UID of the near field target.
*/

/*!
    Returns the URL of the near field target.
*/
QUrl QNearFieldTarget::url() const
{
    return QUrl();
}

/*!
    \fn QNearFieldTarget::Type QNearFieldTarget::type() const = 0

    Returns the type of tag type of this near field target.
*/

/*!
    \fn QNearFieldTarget::AccessMethods QNearFieldTarget::accessMethods() const = 0

    Returns the access methods support by this near field target.
*/

/*!
    Returns true if the target is processing commands; otherwise returns false.
*/
bool QNearFieldTarget::isProcessingCommand() const
{
    return false;
}

/*!
    Returns true if at least one NDEF message is stored on the near field target; otherwise returns
    false.
*/
bool QNearFieldTarget::hasNdefMessage()
{
    return false;
}

/*!
    Starts reading NDEF messages stored on the near field target. Returns a request id which can
    be used to track the completion status of the request. An invalid request id will be returned
    if the target does not support reading NDEF messages.

    An ndefMessageRead() signal will be emitted for each NDEF message. The requestCompleted()
    signal will be emitted was all NDEF messages have been read. The error() signal is emitted if
    an error occurs.
*/
QNearFieldTarget::RequestId QNearFieldTarget::readNdefMessages()
{
    return RequestId();
}

/*!
    Writes the NDEF messages in \a messages to the target. Returns a request id which can be used
    to track the completion status of the request. An invalid request id will be returned if the
    target does not support reading NDEF messages.

    The ndefMessagesWritten() signal will be emitted when the write operation completes
    successfully; otherwise the error() signal is emitted.
*/
QNearFieldTarget::RequestId QNearFieldTarget::writeNdefMessages(const QList<QNdefMessage> &messages)
{
    Q_UNUSED(messages);

    return RequestId();
}

/*!
    Sends \a command to the near field target. Returns a request id which can be used to track the
    completion status of the request. An invalid request id will be returned if the target does not
    support sending tag type specific commands.

    The requestCompleted() signal will be emitted on successful completion of the request;
    otherwise the error() signal will be emitted.

    Once the request completes successfully the response can be retrieved from the
    requestResponse() function. The response of this request will be a QByteArray.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTarget::sendCommand(const QByteArray &command)
{
    Q_UNUSED(command);

    emit error(UnsupportedError, RequestId());

    return RequestId();
}

/*!
    Sends multiple \a commands to the near field target. Returns a request id which can be used to
    track the completion status of the request. An invalid request id will be returned if the
    target does not support sending tag type specific commands.

    If all commands complete successfully the requestCompleted() signal will be emitted; otherwise
    the error() signal will be emitted. If a command fails succeeding commands from this call will
    not be processed.

    Once the request completes the response for successfully completed requests can be retrieved
    from the requestResponse() function. The response of this request will be a QList<QByteArray>.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTarget::sendCommands(const QList<QByteArray> &commands)
{
    Q_UNUSED(commands);

    emit error(UnsupportedError, RequestId());

    return RequestId();
}

/*!
    Waits up to \a msecs milliseconds for the request \a id to complete. Returns true if the
    request completes successfully and the requestCompeted() signal is emitted; otherwise returns
    false.
*/
bool QNearFieldTarget::waitForRequestCompleted(const RequestId &id, int msecs)
{
    Q_D(QNearFieldTarget);

    QTime timer;
    timer.start();

    do {
        if (d->m_decodedResponses.contains(id))
            return true;
        else
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 1);
    } while (timer.elapsed() <= msecs);

    return false;
}

/*!
    Returns the decoded response for request \a id. If the request is unknown or has not yet been
    completed an invalid QVariant is returned.
*/
QVariant QNearFieldTarget::requestResponse(const RequestId &id)
{
    Q_D(QNearFieldTarget);

    return d->m_decodedResponses.value(id);
}

/*!
    Sets the decoded response for request \a id to \a response. If \a emitRequestCompleted is true
    the requestCompleted() signal will be emitted for \a id; otherwise no signal will be emitted.

    \sa requestResponse()
*/
void QNearFieldTarget::setResponseForRequest(const QNearFieldTarget::RequestId &id,
                                             const QVariant &response, bool emitRequestCompleted)
{
    Q_D(QNearFieldTarget);

    QMutableMapIterator<RequestId, QVariant> i(d->m_decodedResponses);
    while (i.hasNext()) {
        i.next();

        // no more external references
        if (i.key().refCount() == 1)
            i.remove();
    }

    d->m_decodedResponses.insert(id, response);

    if (emitRequestCompleted)
        emit requestCompleted(id);
}

/*!
    Handles the \a response received for the request \a id. Returns true if the response is
    handled; otherwise returns false.

    Classes reimplementing this virtual function should call the base class implementation to
    ensure that requests initiated by those classes are handled correctly.

    The default implementation stores the response such that it can be retrieved by
    requestResponse().
*/
bool QNearFieldTarget::handleResponse(const QNearFieldTarget::RequestId &id,
                                      const QByteArray &response)
{
    setResponseForRequest(id, response);

    return true;
}

QT_END_NAMESPACE
