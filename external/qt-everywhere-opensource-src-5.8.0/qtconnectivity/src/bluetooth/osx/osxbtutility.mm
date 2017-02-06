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

#include "qlowenergycharacteristicdata.h"
#include "qbluetoothaddress.h"
#include "osxbtutility_p.h"
#include "qbluetoothuuid.h"

#include <QtCore/qendian.h>
#include <QtCore/qstring.h>

#ifndef QT_IOS_BLUETOOTH

#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>
#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_12, __IPHONE_NA)
#import <CoreBluetooth/CBUUID.h>
#endif

#endif

#include <algorithm>
#include <limits>

QT_BEGIN_NAMESPACE

#ifndef QT_IOS_BLUETOOTH

Q_LOGGING_CATEGORY(QT_BT_OSX, "qt.bluetooth.osx")

#else

Q_LOGGING_CATEGORY(QT_BT_OSX, "qt.bluetooth.ios")

#endif

namespace OSXBluetooth {

const int defaultLEScanTimeoutMS = 25000;
// We use it only on iOS for now:
const int maxValueLength = 512;

QString qt_address(NSString *address)
{
    if (address && address.length) {
        NSString *const fixed = [address stringByReplacingOccurrencesOfString:@"-" withString:@":"];
        return QString::fromNSString(fixed);
    }

    return QString();
}

#ifndef QT_IOS_BLUETOOTH


QBluetoothAddress qt_address(const BluetoothDeviceAddress *a)
{
    if (a) {
        // TODO: can a byte order be different in BluetoothDeviceAddress?
        const quint64 qAddress = a->data[5] |
                                 qint64(a->data[4]) << 8  |
                                 qint64(a->data[3]) << 16 |
                                 qint64(a->data[2]) << 24 |
                                 qint64(a->data[1]) << 32 |
                                 qint64(a->data[0]) << 40;
        return QBluetoothAddress(qAddress);
    }

    return QBluetoothAddress();
}

BluetoothDeviceAddress iobluetooth_address(const QBluetoothAddress &qAddress)
{
    BluetoothDeviceAddress a = {};
    if (!qAddress.isNull()) {
        const quint64 val = qAddress.toUInt64();
        a.data[0] = (val >> 40) & 0xff;
        a.data[1] = (val >> 32) & 0xff;
        a.data[2] = (val >> 24) & 0xff;
        a.data[3] = (val >> 16) & 0xff;
        a.data[4] = (val >> 8) & 0xff;
        a.data[5] = val & 0xff;
    }

    return a;
}

ObjCStrongReference<IOBluetoothSDPUUID> iobluetooth_uuid(const QBluetoothUuid &uuid)
{
    const unsigned nBytes = 128 / std::numeric_limits<unsigned char>::digits;
    const quint128 intVal(uuid.toUInt128());

    const ObjCStrongReference<IOBluetoothSDPUUID> iobtUUID([IOBluetoothSDPUUID uuidWithBytes:intVal.data
                                                           length:nBytes], true);
    return iobtUUID;
}

QBluetoothUuid qt_uuid(IOBluetoothSDPUUID *uuid)
{
    QBluetoothUuid qtUuid;
    if (!uuid || [uuid length] != 16) // TODO: issue any diagnostic?
        return qtUuid;

    // TODO: ensure the correct byte-order!!!
    quint128 uuidVal = {};
    const quint8 *const source = static_cast<const quint8 *>([uuid bytes]);
    std::copy(source, source + 16, uuidVal.data);
    return QBluetoothUuid(uuidVal);
}

QString qt_error_string(IOReturn errorCode)
{
    switch (errorCode) {
    case kIOReturnSuccess:
        // NoError in many classes == an empty string description.
        return QString();
    case kIOReturnNoMemory:
        return QString::fromLatin1("memory allocation failed");
    case kIOReturnNoResources:
        return QString::fromLatin1("failed to obtain a resource");
    case kIOReturnBusy:
        return QString::fromLatin1("device is busy");
    case kIOReturnStillOpen:
        return QString::fromLatin1("device(s) still open");
    // Others later ...
    case kIOReturnError: // "general error" (IOReturn.h)
    default:
        return QString::fromLatin1("unknown error");
    }
}

#endif


// Apple has: CBUUID, NSUUID, CFUUID, IOBluetoothSDPUUID
// and it's handy to have several converters:

QBluetoothUuid qt_uuid(CBUUID *uuid)
{
    // Apples' docs say "128 bit" and "16-bit UUIDs are implicitly
    // pre-filled with the Bluetooth Base UUID."
    // But Core Bluetooth can return CBUUID objects of length 2
    // (16-bit, so they are not pre-filled?).

    if (!uuid)
        return QBluetoothUuid();

    QT_BT_MAC_AUTORELEASEPOOL;

    if (uuid.data.length == 2) {
        // CBUUID's docs say nothing about byte-order.
        // Seems to be in big-endian.
        const uchar *const src = static_cast<const uchar *>(uuid.data.bytes);
        return QBluetoothUuid(qFromBigEndian<quint16>(src));
    } else if (uuid.data.length == 16) {
        quint128 qtUuidData = {};
        const quint8 *const source = static_cast<const quint8 *>(uuid.data.bytes);
        std::copy(source, source + 16, qtUuidData.data);

        return QBluetoothUuid(qtUuidData);
    } else {
        qCDebug(QT_BT_OSX) << "qt_uuid, invalid CBUUID, 2 or 16 bytes expected, but got "
                           << uuid.data.length << " bytes length";
        return QBluetoothUuid();
    }

    if (uuid.data.length != 16) // TODO: warning?
        return QBluetoothUuid();

}

CFStrongReference<CFUUIDRef> cf_uuid(const QBluetoothUuid &qtUuid)
{
    const quint128 qtUuidData = qtUuid.toUInt128();
    const quint8 *const data = qtUuidData.data;

    CFUUIDBytes bytes = {data[0],  data[1],  data[2],  data[3],
                         data[4],  data[5],  data[6],  data[7],
                         data[8],  data[9],  data[10], data[11],
                         data[12], data[13], data[14], data[15]};

    CFUUIDRef cfUuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, bytes);
    return CFStrongReference<CFUUIDRef>(cfUuid, false);// false == already retained.
}

ObjCStrongReference<CBUUID> cb_uuid(const QBluetoothUuid &qtUuid)
{
    CFStrongReference<CFUUIDRef> cfUuid(cf_uuid(qtUuid));
    if (!cfUuid)
        return ObjCStrongReference<CBUUID>();

    ObjCStrongReference<CBUUID> cbUuid([CBUUID UUIDWithCFUUID:cfUuid], true); //true == retain.
    return cbUuid;
}

bool equal_uuids(const QBluetoothUuid &qtUuid, CBUUID *cbUuid)
{
    const QBluetoothUuid qtUuid2(qt_uuid(cbUuid));
    return qtUuid == qtUuid2;
}

bool equal_uuids(CBUUID *cbUuid, const QBluetoothUuid &qtUuid)
{
    return equal_uuids(qtUuid, cbUuid);
}

QByteArray qt_bytearray(NSData *data)
{
    QByteArray value;
    if (!data || !data.length)
        return value;

    value.resize(data.length);
    const char *const src = static_cast<const char *>(data.bytes);
    std::copy(src, src + data.length, value.data());

    return value;
}

template<class Integer>
QByteArray qt_bytearray(Integer n)
{
    QByteArray value;
    value.resize(sizeof n);
    const char *const src = reinterpret_cast<char *>(&n);
    std::copy(src, src + sizeof n, value.data());

    return value;
}

QByteArray qt_bytearray(NSString *string)
{
    if (!string)
        return QByteArray();

    QT_BT_MAC_AUTORELEASEPOOL;
    NSData *const utf8Data = [string dataUsingEncoding:NSUTF8StringEncoding];

    return qt_bytearray(utf8Data);
}

QByteArray qt_bytearray(NSObject *obj)
{
    // descriptor.value has type 'id'.
    // While the Apple's docs say this about descriptors:
    //
    // - CBUUIDCharacteristicExtendedPropertiesString
    //   The string representation of the UUID for the extended properties descriptor.
    //   The corresponding value for this descriptor is an NSNumber object.
    //
    // - CBUUIDCharacteristicUserDescriptionString
    //   The string representation of the UUID for the user description descriptor.
    //   The corresponding value for this descriptor is an NSString object.
    //
    //   ... etc.
    //
    // This is not true. On OS X, they all seem to be NSData (or derived from NSData),
    // and they can be something else on iOS (NSNumber, NSString, etc.)
    if (!obj)
        return QByteArray();

    QT_BT_MAC_AUTORELEASEPOOL;

    if ([obj isKindOfClass:[NSData class]]) {
        return qt_bytearray(static_cast<NSData *>(obj));
    } else if ([obj isKindOfClass:[NSString class]]) {
        return qt_bytearray(static_cast<NSString *>(obj));
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        NSNumber *const nsNumber = static_cast<NSNumber *>(obj);
        return qt_bytearray([nsNumber unsignedShortValue]);
    }
    // TODO: Where can be more types, but Core Bluetooth does not support them,
    // or at least it's not documented.

    return QByteArray();
}

ObjCStrongReference<NSData> data_from_bytearray(const QByteArray & qtData)
{
    if (!qtData.size())
        return ObjCStrongReference<NSData>([[NSData alloc] init], false);

    ObjCStrongReference<NSData> result([NSData dataWithBytes:qtData.constData() length:qtData.size()], true);
    return result;
}

ObjCStrongReference<NSMutableData> mutable_data_from_bytearray(const QByteArray &qtData)
{
    using MutableData = ObjCStrongReference<NSMutableData>;

    if (!qtData.size())
        return MutableData([[NSMutableData alloc] init], false);

    MutableData result([[NSMutableData alloc] initWithLength:qtData.size()], false);
    [result replaceBytesInRange:NSMakeRange(0, qtData.size())
                      withBytes:qtData.constData()];
    return result;
}

// A small RAII class for a dispatch queue.
class SerialDispatchQueue
{
public:
    explicit SerialDispatchQueue(const char *label)
    {
        Q_ASSERT(label);

        queue = dispatch_queue_create(label, DISPATCH_QUEUE_SERIAL);
        if (!queue) {
            qCCritical(QT_BT_OSX) << "failed to create dispatch queue with label"
                                  << label;
        }
    }
    ~SerialDispatchQueue()
    {
        if (queue)
            dispatch_release(queue);
    }

    dispatch_queue_t data() const
    {
        return queue;
    }
private:
    dispatch_queue_t queue;

    Q_DISABLE_COPY(SerialDispatchQueue)
};

dispatch_queue_t qt_LE_queue()
{
    static const SerialDispatchQueue leQueue("qt-bluetooth-LE-queue");
    return leQueue.data();
}

}

QT_END_NAMESPACE
