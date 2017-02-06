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

#include "qbluetoothserviceinfo.h"
#include "osxbtservicerecord_p.h"
#include "osxbluetooth_p.h"

#include <QtCore/qvariant.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmap.h>
#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

namespace OSXBluetooth {

//
// Returns a dictionary containing the Bluetooth RFCOMM service definition
// corresponding to the provided |uuid| and |options|.
namespace {

typedef ObjCStrongReference<NSMutableDictionary> Dictionary;
typedef ObjCStrongReference<IOBluetoothSDPUUID> SDPUUid;
typedef ObjCStrongReference<NSNumber> Number;
typedef QBluetoothServiceInfo QSInfo;
typedef QSInfo::Sequence Sequence;
typedef QSInfo::AttributeId AttributeId;

}

#if 0
QBluetoothUuid profile_uuid(const QBluetoothServiceInfo &serviceInfo)
{
    // Strategy to pick service uuid:
    // 1.) use serviceUuid()
    // 2.) use first custom uuid if available
    // 3.) use first service class uuid
    QBluetoothUuid serviceUuid(serviceInfo.serviceUuid());

    if (serviceUuid.isNull()) {
        const QVariant var(serviceInfo.attribute(QBluetoothServiceInfo::ServiceClassIds));
        if (var.isValid()) {
            const Sequence seq(var.value<Sequence>());

            for (int i = 0; i < seq.count(); ++i) {
                QBluetoothUuid uuid(seq.at(i).value<QBluetoothUuid>());
                if (uuid.isNull())
                    continue;

                const int size = uuid.minimumSize();
                if (size == 2 || size == 4) { // Base UUID derived
                    if (serviceUuid.isNull())
                        serviceUuid = uuid;
                } else {
                    return uuid;
                }
            }
        }
    }

    return serviceUuid;
}
#endif

template<class IntType>
Number variant_to_nsnumber(const QVariant &);

template<>
Number variant_to_nsnumber<unsigned char>(const QVariant &var)
{
    return Number([NSNumber numberWithUnsignedChar:var.value<unsigned char>()], true);
}

template<>
Number variant_to_nsnumber<unsigned short>(const QVariant &var)
{
    return Number([NSNumber numberWithUnsignedShort:var.value<unsigned short>()], true);
}

template<>
Number variant_to_nsnumber<unsigned>(const QVariant &var)
{
    return Number([NSNumber numberWithUnsignedInt:var.value<unsigned>()], true);
}

template<>
Number variant_to_nsnumber<char>(const QVariant &var)
{
    return Number([NSNumber numberWithChar:var.value<char>()], true);
}

template<>
Number variant_to_nsnumber<short>(const QVariant &var)
{
    return Number([NSNumber numberWithShort:var.value<short>()], true);
}

template<>
Number variant_to_nsnumber<int>(const QVariant &var)
{
    return Number([NSNumber numberWithInt:var.value<int>()], true);
}

template<class ValueType>
void add_attribute(const QVariant &var, AttributeId key, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dictionary (nil)");

    if (!var.canConvert<ValueType>())
        return;

    const Number num(variant_to_nsnumber<ValueType>(var));
    [dict setObject:num forKey:[NSString stringWithFormat:@"%d", int(key)]];
}

template<>
void add_attribute<QString>(const QVariant &var, AttributeId key, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dictionary (nil)");

    if (!var.canConvert<QString>())
        return;

    const QString string(var.value<QString>());
    if (string.length()) {
        if (NSString *const nsString = string.toNSString())
            [dict setObject:nsString forKey:[NSString stringWithFormat:@"%d", int(key)]];
    }
}

template<>
void add_attribute<QBluetoothUuid>(const QVariant &var, AttributeId key, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dictionary (nil)");

    if (!var.canConvert<QBluetoothUuid>())
        return;

    SDPUUid ioUUID(iobluetooth_uuid(var.value<QBluetoothUuid>()));
    [dict setObject:ioUUID forKey:[NSString stringWithFormat:@"%d", int(key)]];
}

template<>
void add_attribute<QUrl>(const QVariant &var, AttributeId key, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dictionary (nil)");

    if (!var.canConvert<QUrl>())
        return;

    Q_UNUSED(var)
    Q_UNUSED(key)
    Q_UNUSED(dict)

    // TODO: not clear how should I pass an url in a dictionary, NSURL does not work.
}

template<class ValueType>
void add_attribute(const QVariant &var, NSMutableArray *list);

template<class ValueType>
void add_attribute(const QVariant &var, NSMutableArray *list)
{
    Q_ASSERT_X(list, Q_FUNC_INFO, "invalid list (nil)");

    if (!var.canConvert<ValueType>())
        return;

    const Number num(variant_to_nsnumber<ValueType>(var));
    [list addObject:num];
}

template<>
void add_attribute<QString>(const QVariant &var, NSMutableArray *list)
{
    Q_ASSERT_X(list, Q_FUNC_INFO, "invalid list (nil)");

    if (!var.canConvert<QString>())
        return;

    const QString string(var.value<QString>());
    if (string.length()) {
        if (NSString *const nsString = string.toNSString())
            [list addObject:nsString];
    }
}

template<>
void add_attribute<QBluetoothUuid>(const QVariant &var, NSMutableArray *list)
{
    Q_ASSERT_X(list, Q_FUNC_INFO, "invalid list (nil)");

    if (!var.canConvert<QBluetoothUuid>())
        return;

    SDPUUid ioUUID(iobluetooth_uuid(var.value<QBluetoothUuid>()));
    [list addObject:ioUUID];
}

template<>
void add_attribute<QUrl>(const QVariant &var, NSMutableArray *list)
{
    Q_ASSERT_X(list, Q_FUNC_INFO, "invalid list (nil)");

    if (!var.canConvert<QUrl>())
        return;

    Q_UNUSED(var)
    Q_UNUSED(list)
    // TODO: not clear how should I pass an url in a dictionary, NSURL does not work.
}

void add_rfcomm_protocol_descriptor_list(uint16 channelID, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dictionary (nil)");

    QT_BT_MAC_AUTORELEASEPOOL;

    // Objective-C has literals (for arrays and dictionaries), but it will not compile
    // on 10.7 or below, so quite a lot of code here.

    NSMutableArray *const descriptorList = [NSMutableArray array];

    IOBluetoothSDPUUID *const l2capUUID = [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16L2CAP];
    NSArray *const l2capList = [NSArray arrayWithObject:l2capUUID];

    [descriptorList addObject:l2capList];
    //
    IOBluetoothSDPUUID *const rfcommUUID = [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16RFCOMM];
    NSMutableDictionary *const rfcommDict = [NSMutableDictionary dictionary];
    [rfcommDict setObject:[NSNumber numberWithInt:1] forKey:@"DataElementType"];
    [rfcommDict setObject:[NSNumber numberWithInt:1] forKey:@"DataElementSize"];
    [rfcommDict setObject:[NSNumber numberWithInt:channelID] forKey:@"DataElementValue"];
    //
    NSMutableArray *const rfcommList = [NSMutableArray array];
    [rfcommList addObject:rfcommUUID];
    [rfcommList addObject:rfcommDict];

    [descriptorList addObject:rfcommList];
    [dict setObject:descriptorList forKey:[NSString stringWithFormat:@"%d",
        kBluetoothSDPAttributeIdentifierProtocolDescriptorList]];
}

void add_l2cap_protocol_descriptor_list(uint16 psm, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dictionary (nil)");

    QT_BT_MAC_AUTORELEASEPOOL;

    // Objective-C has literals (for arrays and dictionaries), but it will not compile
    // on 10.7 or below, so quite a lot of code here.

    NSMutableArray *const descriptorList = [NSMutableArray array];
    NSMutableArray *const l2capList = [NSMutableArray array];

    IOBluetoothSDPUUID *const l2capUUID = [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16L2CAP];
    [l2capList addObject:l2capUUID];

    NSMutableDictionary *const l2capDict = [NSMutableDictionary dictionary];
    [l2capDict setObject:[NSNumber numberWithInt:1] forKey:@"DataElementType"];
    [l2capDict setObject:[NSNumber numberWithInt:2] forKey:@"DataElementSize"];
    [l2capDict setObject:[NSNumber numberWithInt:psm] forKey:@"DataElementValue"];
    [l2capList addObject:l2capDict];

    [descriptorList addObject:l2capList];
    [dict setObject:descriptorList forKey:[NSString stringWithFormat:@"%d",
        kBluetoothSDPAttributeIdentifierProtocolDescriptorList]];
}

bool add_attribute(const QVariant &var, AttributeId key, NSMutableArray *list)
{
    Q_ASSERT_X(list, Q_FUNC_INFO, "invalid list (nil)");

    if (var.canConvert<Sequence>())
        return false;

    if (var.canConvert<QString>()) {
        //ServiceName, ServiceDescription, ServiceProvider.
        add_attribute<QString>(var, list);
    } else if (var.canConvert<QBluetoothUuid>()) {
        add_attribute<QBluetoothUuid>(var, list);
    } else {
        // Here we need 'key' to understand the type.
        // We can have different integer types actually, so I have to check
        // the 'key' to be sure the conversion is reasonable.
        switch (key) {
        case QSInfo::ServiceRecordHandle:
        case QSInfo::ServiceRecordState:
        case QSInfo::ServiceInfoTimeToLive:
            add_attribute<unsigned>(var, list);
            break;
        case QSInfo::ServiceAvailability:
            add_attribute<unsigned char>(var, list);
            break;
        case QSInfo::IconUrl:
        case QSInfo::DocumentationUrl:
        case QSInfo::ClientExecutableUrl:
            add_attribute<QUrl>(var, list);
            break;
        default:;
        }
    }

    return true;
}

bool add_attribute(const QBluetoothServiceInfo &serviceInfo, AttributeId key, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dict (nil)");

    const QVariant var(serviceInfo.attribute(key));
    if (var.canConvert<Sequence>())
        return false;

    if (var.canConvert<QString>()) {
        //ServiceName, ServiceDescription, ServiceProvider.
        add_attribute<QString>(var, key, dict);
    } else if (var.canConvert<QBluetoothUuid>()) {
        add_attribute<QBluetoothUuid>(serviceInfo.attribute(key), key, dict);
    } else {
        // We can have different integer types actually, so I have to check
        // the 'key' to be sure the conversion is reasonable.
        switch (key) {
        case QSInfo::ServiceRecordHandle:
        case QSInfo::ServiceRecordState:
        case QSInfo::ServiceInfoTimeToLive:
            add_attribute<unsigned>(serviceInfo.attribute(key), key, dict);
            break;
        case QSInfo::ServiceAvailability:
            add_attribute<unsigned char>(serviceInfo.attribute(key), key, dict);
            break;
        case QSInfo::IconUrl:
        case QSInfo::DocumentationUrl:
        case QSInfo::ClientExecutableUrl:
            add_attribute<QUrl>(serviceInfo.attribute(key), key, dict);
            break;
        default:;
        }
    }

    return true;
}

bool add_sequence_attribute(const QVariant &var, AttributeId key, NSMutableArray *list)
{
    // Add a "nested" sequence.
    Q_ASSERT_X(list, Q_FUNC_INFO, "invalid list (nil)");

    if (var.isNull() || !var.canConvert<Sequence>())
        return false;

    const Sequence sequence(var.value<Sequence>());
    foreach (const QVariant &var, sequence) {
        if (var.canConvert<Sequence>()) {
            NSMutableArray *const nested = [NSMutableArray array];
            add_sequence_attribute(var, key, nested);
            [list addObject:nested];
        } else {
            add_attribute(var, key, list);
        }
    }

    return true;
}

bool add_sequence_attribute(const QBluetoothServiceInfo &serviceInfo, AttributeId key, Dictionary dict)
{
    Q_ASSERT_X(dict, Q_FUNC_INFO, "invalid dictionary (nil)");

    const QVariant &var(serviceInfo.attribute(key));
    if (var.isNull() || !var.canConvert<Sequence>())
        return false;

    QT_BT_MAC_AUTORELEASEPOOL;

    NSMutableArray *const list = [NSMutableArray array];
    const Sequence sequence(var.value<Sequence>());
    foreach (const QVariant &element, sequence) {
        if (!add_sequence_attribute(element, key, list))
            add_attribute(element, key, list);
    }
    [dict setObject:list forKey:[NSString stringWithFormat:@"%d", int(key)]];

    return true;
}

Dictionary iobluetooth_service_dictionary(const QBluetoothServiceInfo &serviceInfo)
{
    Dictionary dict;

    if (serviceInfo.socketProtocol() == QBluetoothServiceInfo::UnknownProtocol)
        return dict;

    const QList<quint16> attributeIds(serviceInfo.attributes());
    if (!attributeIds.size())
        return dict;

    dict.reset([[NSMutableDictionary alloc] init]);

    foreach (quint16 key, attributeIds) {
        if (key == QSInfo::ProtocolDescriptorList) // We handle it in a special way.
            continue;
        // TODO: check if non-sequence QVariant still must be
        // converted into NSArray for some attribute ID.
        if (!add_sequence_attribute(serviceInfo, AttributeId(key), dict))
            add_attribute(serviceInfo, AttributeId(key), dict);
    }

    if (serviceInfo.socketProtocol() == QBluetoothServiceInfo::L2capProtocol) {
        add_l2cap_protocol_descriptor_list(serviceInfo.protocolServiceMultiplexer(),
                                           dict);
    } else {
        add_rfcomm_protocol_descriptor_list(serviceInfo.serverChannel(), dict);
    }

    return dict;
}

}

QT_END_NAMESPACE
