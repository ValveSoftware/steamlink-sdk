/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "osxbtobexsession_p.h"
#include "qbluetoothaddress.h"
#include "osxbtutility_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qvector.h>
#include <QtCore/qdebug.h>
#include <QtCore/qlist.h>

#include <algorithm>
#include <cstddef>
#include <limits>

QT_BEGIN_NAMESPACE

namespace OSXBluetooth
{

OBEXSessionDelegate::~OBEXSessionDelegate()
{
}

namespace {

struct OBEXHeader
{
    OBEXHeader() : headerID(0)
    {
    }

    quint8 headerID;
    QVariant value;
};

enum {
    // Bits 7 and 8 == header's format.
    OBEXHeaderFormatMask = 0xc0,
    //
    OBEXHeaderFormatUnicode = 0,         // 87
    OBEXHeaderFormatByteSequence = 0x40, // 0100 0000
    OBEXHeaderFormat1Byte = 0x80,        // 1000 0000
    OBEXHeaderFormat4Byte = 0xc0,        // 1100 0000

};

quint32 extract_uint32(const uint8_t *bytes)
{
    // Four byte value, high byte first.
    Q_ASSERT_X(bytes, Q_FUNC_INFO, "invalid input data (null)");

    uint32_t value = uint32_t();
    std::copy(bytes, bytes + sizeof value, reinterpret_cast<uint8_t *>(&value));

    return NSSwapBigIntToHost(value);
}

quint16 extract_uint16(const uint8_t *bytes)
{
    // Two byte value, high byte first.
    Q_ASSERT_X(bytes, Q_FUNC_INFO, "invalid input data (null)");

    uint16_t value = uint16_t();
    std::copy(bytes, bytes + sizeof value, reinterpret_cast<uint8_t *>(&value));

    return NSSwapBigShortToHost(value);
}

QString extract_qstring(const uint8_t *bytes, quint16 stringLength)
{
    if (bytes && stringLength) {
        NSString * const nsString = [[NSString alloc] initWithBytes:bytes
                                                             length:stringLength
                                                           encoding:NSUnicodeStringEncoding];
        if (nsString)
            return QString::fromNSString(nsString);
    }

    // Empty string is an error, "valid" empty strings are
    // handled separately.
    return QString();
}

QList<OBEXHeader> qt_bluetooth_headers(const uint8_t *data, std::size_t length)
{
    // Convert a data from IOBluetooth into something, Qt understands.
    // Possible formats (bits 7 and 8):
    // 1. 00 Two bytes of length folowed by a null-terminated
    //    Unicode text string (length is unsigned integer;
    //    it covers the header ID and the whole of the header
    //    value, including the length bytes and the two bytes
    //    of null terminator.
    // 2. 01 Two bytes of length followed by a byte sequence (length
    //    is an unsigned integer sent high byte first; it covers
    //    the header ID and the whole of the header value).
    // 3. 10 A single byte value.
    // 4. 11 A four byte value, sent high byte first.

    Q_ASSERT_X(data, Q_FUNC_INFO, "invalid data (null)");
    Q_ASSERT_X(length >= 2, Q_FUNC_INFO, "invalid data length");

    Q_UNUSED(data)
    Q_UNUSED(length)

    QList<OBEXHeader> empty;
    QList<OBEXHeader> qtHeaders;

    for (std::size_t i = 0; i < length;) {
        std::size_t headerLength = 0;
        OBEXHeader header;
        header.headerID = data[i];

        switch (data[i] & OBEXHeaderFormatMask) {
        case OBEXHeaderFormatUnicode:
        {
            if (i + 3 > length)
                return empty;
            headerLength = extract_uint16(data + i + 1);
            // Invalid length or input data:
            if (headerLength < 3 || i + headerLength > length)
                return empty;
            if (headerLength == 3 || headerLength == 5) { // Can 5 ever happen?
                header.value.fromValue<QString>(QString());
            } else if (headerLength > 5) {// TODO: We do not check now, that the string actually valid.
                const QString value(extract_qstring(data + i + 3, headerLength - 5));
                if (!value.length()) // Some error?
                    return empty;
                header.value.setValue<QString>(value);
            } else // Still something weird.
                return empty;
            break;
        }
        case OBEXHeaderFormatByteSequence:
        {
            if (i + 3 > length)
                return empty;
            headerLength = extract_uint16(data + i + 1);
            // Something is wrong:
            if (headerLength < 3 || i + headerLength > length)
                return empty;
            QVector<unsigned char> value;
            if (headerLength > 3) {
                value.resize(headerLength - 3);
                std::copy(data, data + headerLength, value.begin());
            }
            header.value.setValue<QVector<unsigned char> >(value);
            break;
        }
        case OBEXHeaderFormat1Byte:
        {
            // 1 byte integer + 1 byte headerID == 2
            if (i + 2 > length)
                return empty;
            headerLength = 2;
            header.value.setValue<quint8>(data[i + 1]);
            break;
        }
        case OBEXHeaderFormat4Byte:
        {
            // 4 byte integer + 1 byte headerID == 5
            if (i + 5 > length)
                return empty;
            headerLength = 5;
            header.value.setValue<quint32>(extract_uint32(data + i + 1));
            break;
        }
        default:
            qCWarning(QT_BT_OSX) << "invalid header format";
            return empty;
        }

        i += headerLength;
        qtHeaders.push_back(header);
    }

    return qtHeaders;
}

bool append_uint16(ObjCStrongReference<NSMutableData> headers, uint16_t value)
{
    if (!headers)
        return false;

    const NSUInteger length = [headers length];
    const uint16_t valueSwapped = NSSwapHostShortToBig(value);
    [headers appendBytes:&valueSwapped length:sizeof valueSwapped];

    return [headers length] - length == 2;
}


bool append_four_byte_header(ObjCStrongReference<NSMutableData> headers, uint8_t headerID,
                             uint32_t headerValue)
{
    if (!headers)
        return false;

    const NSUInteger length = [headers length];
    // Header ID (1 byte)
    [headers appendBytes:&headerID length:1];
    // Header value (4 bytes)
    const uint32_t valueSwapped(NSSwapHostIntToBig(headerValue));
    [headers appendBytes:&valueSwapped length:sizeof valueSwapped];

    return [headers length] - length == 5;
}

bool append_unicode_header(ObjCStrongReference<NSMutableData> headers, uint8_t headerID,
                           const QString &string)
{
    // Two bytes of length followed by a null-terminated
    // Unicode text string. Length is unsigned integer,
    // it covers the header ID and the whole of the header
    // value, including the length bytes and the two bytes
    // of null terminator.
    // All Obj-C objects are autoreleased.

    if (!headers)
        return false;

    QT_BT_MAC_AUTORELEASEPOOL;

    const NSUInteger initialLength = [headers length];
    [headers appendBytes:&headerID length:1];

    if (!string.length()) {
        // Empty string. The length is 3
        // (header ID + length value itself).
        return append_uint16(headers, 3);
    }

    NSString *const nsString = string.toNSString();
    if (!nsString)
        return false;

    // TODO: check if the encodings is right. It was NSUnicodeStringEncoding but
    // byte order was wrong. Also, I do not need BOM check anymore?
    NSData *const data = [nsString dataUsingEncoding:NSUTF16BigEndianStringEncoding];
    if (!data)
        return false;

    // This data can include byte-order marker (BOM) and does not include
    // a null terminator. Anyway, the length must be >= 2.
    NSUInteger length = [data length];
    if (length < 2)
        return false;

    const uint8_t *dataPtr = static_cast<const uint8_t *>([data bytes]);
    if ((dataPtr[0] == 0xff && dataPtr[1] == 0xfe)
        || (dataPtr[0] == 0xfe && dataPtr[1] == 0xff)) {
        if (length == 2) //Something weird?
            return false;
        // Skip a BOM.
        dataPtr += 2;
        length -= 2;
    }

    // headerID + length == 3, string's length + 2
    // bytes for a null terminator.
    if (!append_uint16(headers, length + 3 + 2))
        return false;

    [headers appendBytes:dataPtr length:length];
    const uint8_t nullTerminator[2] = {};
    [headers appendBytes:nullTerminator length:2];

    return [headers length] - initialLength == length + 3 + 2;
}

ObjCStrongReference<NSMutableData> next_data_chunk(QIODevice &inputStream, IOBluetoothOBEXSession *session,
                                                   NSUInteger headersLength, bool &isLast)
{
    // Work only for OBEX put (we request a specific payload length).
    Q_ASSERT_X(session, Q_FUNC_INFO, "invalid OBEX session (nil)");

    const OBEXMaxPacketLength packetSize = [session getAvailableCommandPayloadLength:kOBEXOpCodePut];
    if (!packetSize || headersLength >= packetSize)
        return ObjCStrongReference<NSMutableData>();

    const OBEXMaxPacketLength maxBodySize = packetSize - headersLength;

    QVector<char> block(maxBodySize);
    const int realSize = inputStream.read(block.data(), block.size());
    if (realSize <= 0) {
        // Well, either the last or an error.
        isLast = true;
        return ObjCStrongReference<NSMutableData>();
    }

    ObjCStrongReference<NSMutableData> chunk([NSMutableData dataWithBytes:block.data()
                                              length:realSize], true);
    if (chunk && [chunk length]) {
        // If it actually was the last chunk
        // of a length == maxBodySize, we'll
        // send one more packet (empty though)?
        isLast = [chunk length] < maxBodySize;
    }

    return chunk;
}

bool check_connect_event(const OBEXSessionEvent *e, OBEXError &error, OBEXOpCode &response)
{
    Q_ASSERT_X(e, Q_FUNC_INFO, "invalid event (null)");

    // This function tries to extract either an error code or a
    // server response code. "Good" event has type connect command respond
    // and reponse code 0XA0. Everything else is a "bad" event and
    // means connect failed.

    // If it's an error event - return the error.
    // If it's connect response - extract the response code.
    // If it's something else (is it possible?) - set general error.

    if (e->type == kOBEXSessionEventTypeError) {
        error = e->u.errorData.error;
        return false;
    } if (e->type == kOBEXSessionEventTypeConnectCommandResponseReceived) {
        // We can read response code only for such an event.
        response = e->u.connectCommandResponseData.serverResponseOpCode;
        return response == kOBEXResponseCodeSuccessWithFinalBit;
    } else {
        qCWarning(QT_BT_OSX) << "unexpected event type";
        error = kOBEXGeneralError;
        return false;
    }
}

bool check_put_event(const OBEXSessionEvent *e, OBEXError &error, OBEXOpCode &response)
{
    Q_ASSERT_X(e, Q_FUNC_INFO, "invalid event (null)");

    // See the comments above.

    if (e->type == kOBEXSessionEventTypeError) {
        error = e->u.errorData.error;
        return false;
    } else if (e->type == kOBEXSessionEventTypePutCommandResponseReceived) {
        response = e->u.putCommandResponseData.serverResponseOpCode;
        return response == kOBEXResponseCodeContinueWithFinalBit ||
               response == kOBEXResponseCodeSuccessWithFinalBit;
    } else {
        qCWarning(QT_BT_OSX) << "unexpected event type";
        error = kOBEXGeneralError;
        return false;
    }
}

bool check_abort_event(const OBEXSessionEvent *e, OBEXError &error, OBEXOpCode &response)
{
    Q_ASSERT_X(e, Q_FUNC_INFO, "invalid event (null)");

    if (e->type == kOBEXSessionEventTypeError) {
        error = e->u.errorData.error;
        return false;
    } else if (e->type == kOBEXSessionEventTypeAbortCommandResponseReceived) {
        response = e->u.abortCommandResponseData.serverResponseOpCode;
        return response == kOBEXResponseCodeSuccessWithFinalBit;
    } else {
        qCWarning(QT_BT_OSX) << "unexpected event type";
        return false;
    }
}

} // Unnamed namespace.
} // OSXBluetooth.

QT_END_NAMESPACE

QT_USE_NAMESPACE

@interface QT_MANGLE_NAMESPACE(OSXBTOBEXSession) (PrivateAPI)

// OBEXDisconnect returns void - it's considered to be always
// successful. These methods are "private API" - no need to expose them,
// for internal use only.
- (void)OBEXDisconnect;
- (void)OBEXDisconnectHandler:(const OBEXSessionEvent*)event;

@end

@implementation QT_MANGLE_NAMESPACE(OSXBTOBEXSession)

+ (OBEXMaxPacketLength) maxPacketLength
{
    // Some arbitrary number, we'll adjust it as soon as
    // we connected, asking a session about packet size for
    // a particular command.
    return 0x1000;
}

- (id)initWithDelegate:(QT_PREPEND_NAMESPACE(OSXBluetooth::OBEXSessionDelegate) *)aDelegate
      remoteDevice:(const QBluetoothAddress &)deviceAddress channelID:(quint16)port
{
    Q_ASSERT_X(aDelegate, Q_FUNC_INFO, "invalid delegate (null)");
    Q_ASSERT_X(!deviceAddress.isNull(), Q_FUNC_INFO, "invalid remote device address");
    Q_ASSERT_X(port, Q_FUNC_INFO, "invalid port (0)");

    if (self = [super init]) {
        connected = false;
        currentRequest = OSXBluetooth::OBEXNoop;
        connectionID = 0;
        connectionIDFound = false;

        const BluetoothDeviceAddress addr(OSXBluetooth::iobluetooth_address(deviceAddress));
        device = [[IOBluetoothDevice deviceWithAddress:&addr] retain];
        if (!device) {
            qCWarning(QT_BT_OSX) << "failed to create an IOBluetoothDevice";
            return self;
        }

        session = [[IOBluetoothOBEXSession alloc] initWithDevice:device channelID:port];
        if (!session) {
            qCWarning(QT_BT_OSX) << "failed to create an OBEX session";
            return self;
        }

        delegate = aDelegate;
        channelID = port;
    }

    return self;
}

- (void)dealloc
{
    [device release];
    [session release];

    [headersData release];
    [bodyData release];

    [super dealloc];
}

- (OBEXError)OBEXConnect
{
    if (!session) {
        qCWarning(QT_BT_OSX) << "invalid session (nil)";
        return kOBEXGeneralError;
    }

    // That's a "single-shot" operation:
    Q_ASSERT_X(currentRequest == OSXBluetooth::OBEXNoop, Q_FUNC_INFO,
               "can not connect in this state (another request is active)");

    connected = false;
    connectionIDFound = false;
    connectionID = 0;
    currentRequest = OSXBluetooth::OBEXConnect;

    const OBEXError status = [session OBEXConnect:kOBEXConnectFlagNone
                                      maxPacketLength:[QT_MANGLE_NAMESPACE(OSXBTOBEXSession) maxPacketLength]
                                      optionalHeaders:Q_NULLPTR
                                      optionalHeadersLength:0
                                      eventSelector:@selector(OBEXConnectHandler:)
                                      selectorTarget:self
                                      refCon:Q_NULLPTR];

    if (status != kOBEXSuccess) {
        currentRequest = OSXBluetooth::OBEXNoop;
        // Already connected is still ok for us?
        connected = status == kOBEXSessionAlreadyConnectedError;
    }

    return status;
}

- (void)OBEXConnectHandler:(const OBEXSessionEvent*)event
{
    using namespace OSXBluetooth;

    Q_ASSERT_X(session, Q_FUNC_INFO, "invalid session (nil)");

    if (pendingAbort) {
        currentRequest = OBEXNoop;
        [self OBEXAbort];
        return;
    }

    if (currentRequest != OBEXConnect) {
        qCWarning(QT_BT_OSX) << "called while there is no "
                                "active connect request";
        return;
    }

    currentRequest = OBEXNoop;

    OBEXError errorCode = kOBEXSuccess;
    OBEXOpCode responseCode = kOBEXResponseCodeSuccessWithFinalBit;

    if (!check_connect_event(event, errorCode, responseCode)) {
        // OBEX connect failed.
        if (delegate)
            delegate->OBEXConnectError(errorCode, responseCode);
        return;
    }

    const OBEXConnectCommandResponseData *const response = &event->u.connectCommandResponseData;
    if (response->headerDataPtr && response->headerDataLength >= 2) {
        // 2 == 1 byte headerID + at least 1 byte headerValue ...
        QList<OBEXHeader> headers(qt_bluetooth_headers(static_cast<const uint8_t *>(response->headerDataPtr),
                                  response->headerDataLength));
        // ConnectionID is used when multiplexing OBEX connections
        // to identify which particular connection this object is
        // being sent on. When used, this _must_ be the first
        // header sent.

        foreach (const OBEXHeader &header, headers) {
            if (header.headerID == kOBEXHeaderIDConnectionID) {
                connectionID = header.value.value<quint32>();
                connectionIDFound = true;
                break;
            }
        }
    }

    connected = true;

    if (delegate)
        delegate->OBEXConnectSuccess();
}

- (OBEXError)OBEXAbort
{
    using namespace OSXBluetooth;

    Q_ASSERT_X(session, Q_FUNC_INFO, "invalid OBEX session (nil)");

    if (currentRequest == OBEXNoop) {
        pendingAbort = false;

        if (![self isConnected])
            return kOBEXSessionNotConnectedError;

        currentRequest = OBEXAbort;
        const OBEXError status = [session OBEXAbort:Q_NULLPTR
                                          optionalHeadersLength:0
                                          eventSelector:@selector(OBEXAbortHandler:)
                                          selectorTarget:self
                                          refCon:Q_NULLPTR];
        if (status != kOBEXSuccess)
            currentRequest = OBEXNoop;

        return status;
    } else {
        // We're in the middle of some request, wait
        // for any handler to be called first.
        pendingAbort = true;
        return kOBEXSuccess;
    }
}

- (void)OBEXAbortHandler:(const OBEXSessionEvent*)event
{
    using namespace OSXBluetooth;

    Q_ASSERT_X(session, Q_FUNC_INFO, "invalid OBEX session (nil)");

    if (currentRequest != OBEXAbort) {
        qCWarning(QT_BT_OSX) << "called while there "
                                "is no ABORT request";
        return;
    }

    pendingAbort = false;
    currentRequest = OBEXNoop;

    if (delegate) {
        OBEXError error = kOBEXSuccess;
        OBEXOpCode response = kOBEXResponseCodeSuccessWithFinalBit;
        if (check_abort_event(event, error, response))
            delegate->OBEXAbortSuccess();
    }
}

- (OBEXError)OBEXPutFile:(QT_PREPEND_NAMESPACE(QIODevice) *)input withName:(const QString &)name
{
    using namespace OSXBluetooth;

    if (!session || ![self isConnected])
        return kOBEXSessionNotConnectedError;

    Q_ASSERT_X(currentRequest == OBEXNoop, Q_FUNC_INFO,
               "the current session has an active request already");
    Q_ASSERT_X(input, Q_FUNC_INFO, "invalid input stream (null)");
    Q_ASSERT_X(input->isReadable(), Q_FUNC_INFO, "invalid input stream (not readable)");

    // We send a put request with a couple of headers (size/file name/may be connection ID) +
    // a payload.
    const qint64 fileSize = input->size();
    if (fileSize <= 0 || fileSize >= std::numeric_limits<uint32_t>::max()) {
        qCWarning(QT_BT_OSX) << "invalid input file size";
        return kOBEXBadArgumentError;
    }

    ObjCStrongReference<NSMutableData> headers([[NSMutableData alloc] init], false);
    if (!headers) {
        qCWarning(QT_BT_OSX) << "failed to allocate headers";
        return kOBEXNoResourcesError;
    }

    // Now we append headers with: Connection ID (if any),
    // file name, file size, the first (and probably the only) chunk of data
    // from the input stream and send a put request.

    if (connectionIDFound) {
        if (!append_four_byte_header(headers, kOBEXHeaderIDConnectionID, connectionID)) {
            qCWarning(QT_BT_OSX) << "failed to append connection ID header";
            return kOBEXNoResourcesError;
        }
    }

    if (name.length()) {
        if (!append_unicode_header(headers, kOBEXHeaderIDName, name)) {
            qCWarning(QT_BT_OSX) << "failed to append a unicode string";
            return kOBEXNoResourcesError;
        }
    }

    if (fileSize && !input->isSequential())
        append_four_byte_header(headers, kOBEXHeaderIDLength, uint32_t(fileSize));

    bool lastChunk = false;
    ObjCStrongReference<NSMutableData> chunk(next_data_chunk(*input, session, [headers length], lastChunk));
    if (!chunk || ![chunk length]) {
        // We do not support PUT-DELETE (?)
        // At least the first chunk is expected to be non-empty.
        qCWarning(QT_BT_OSX) << "invalid input stream";
        return kOBEXBadArgumentError;
    }

    currentRequest = OBEXPut;

    const OBEXError status = [session OBEXPut:lastChunk
                                      headersData:[headers mutableBytes]
                                      headersDataLength:[headers length]
                                      bodyData:[chunk mutableBytes]
                                      bodyDataLength:[chunk length]
                                      eventSelector:@selector(OBEXPutHandler:)
                                      selectorTarget:self
                                      refCon:Q_NULLPTR];

    if (status == kOBEXSuccess) {
        if (delegate && fileSize && !input->isSequential())
            delegate->OBEXPutDataSent([chunk length], fileSize);

        bytesSent = [chunk length];
        headersData = headers.take();
        bodyData = chunk.take();
        inputStream = input;
    } else {
        // PUT request failed and we now
        // want to close a connection/session.
        currentRequest = OBEXNoop;
        // Try to cleanup (disconnect).
        [self OBEXDisconnect];
    }

    return status;
}

- (void)OBEXPutHandler:(const OBEXSessionEvent*)event
{
    using namespace OSXBluetooth;

    Q_ASSERT_X(session, Q_FUNC_INFO, "invalid OBEX session (nil)");

    if (pendingAbort) {
        currentRequest = OBEXNoop;
        [self OBEXAbort];
        return;
    }

    if (currentRequest != OBEXPut) {
        qCWarning(QT_BT_OSX) << "called while the current "
                                "request is not a put request";
        return;
    }

    OBEXError error = kOBEXSuccess;
    OBEXOpCode responseCode = kOBEXResponseCodeSuccessWithFinalBit;
    if (!check_put_event(event, error, responseCode)) {
        currentRequest = OBEXNoop;
        if (delegate)
            delegate->OBEXPutError(error, responseCode);
        [self OBEXDisconnect];
        return;
    }

    // Now try to send more data if we have any.
    if (responseCode == kOBEXResponseCodeContinueWithFinalBit) {
        // Send more data.
        bool lastChunk = false;
        // 0 for the headers length, no more headers.
        ObjCStrongReference<NSMutableData> chunk(next_data_chunk(*inputStream, session, 0, lastChunk));
        if (!chunk && !lastChunk) {
            qCWarning(QT_BT_OSX) << "failed to allocate the next memory chunk";
            return;
        }

        void *dataPtr = chunk ? [chunk mutableBytes] : Q_NULLPTR;
        const NSUInteger dataSize = chunk ? [chunk length] : 0;
        const OBEXError status = [session OBEXPut:lastChunk
                                          headersData:Q_NULLPTR
                                          headersDataLength:0
                                          bodyData:dataPtr
                                          bodyDataLength:dataSize
                                          eventSelector:@selector(OBEXPutHandler:)
                                          selectorTarget:self
                                          refCon:Q_NULLPTR];

        if (status != kOBEXSuccess) {
            qCWarning(QT_BT_OSX) << "failed to send the next memory chunk";
            currentRequest = OBEXNoop;
            if (delegate) // Response code is not important here.
                delegate->OBEXPutError(kOBEXNoResourcesError, 0);

            [self OBEXDisconnect];
        } else {
            [bodyData release];
            bytesSent += [chunk length];
            bodyData = chunk.take();//retained already.

            if (delegate && !inputStream->isSequential())
                delegate->OBEXPutDataSent(bytesSent, inputStream->size());
        }
    } else if (responseCode == kOBEXResponseCodeSuccessWithFinalBit) {
        currentRequest = OBEXNoop;
        if (delegate)
            delegate->OBEXPutSuccess();

        [self OBEXDisconnect];
    }
}

- (void)OBEXDisconnect
{
    Q_ASSERT_X(session, Q_FUNC_INFO, "invalid session (nil)");

    currentRequest = OSXBluetooth::OBEXDisconnect;

    [session OBEXDisconnect:Q_NULLPTR
             optionalHeadersLength:0
             eventSelector:@selector(OBEXDisconnectHandler:)
             selectorTarget:self
             refCon:Q_NULLPTR];
}

- (void)OBEXDisconnectHandler:(const OBEXSessionEvent*)event
{
    Q_UNUSED(event)

    Q_ASSERT_X(session, Q_FUNC_INFO, "invalid session (nil)");

    // Event can have an error type, but there's nothing
    // we can do - even "cleanup" failed.
    connected = false;
}

- (bool)isConnected
{
    return device && session && connected;
}

- (void)closeSession
{
    // Clear the delegate and reset the request,
    // do not try any of OBEX commands - the session will be deleted
    // immediately.
    delegate = Q_NULLPTR;
    // This will stop any handler (callback) preventing
    // any read/write to potentially deleted objects.
    currentRequest = OSXBluetooth::OBEXNoop;
}

- (bool)hasActiveRequest
{
    return currentRequest != OSXBluetooth::OBEXNoop && !pendingAbort;
}

@end
