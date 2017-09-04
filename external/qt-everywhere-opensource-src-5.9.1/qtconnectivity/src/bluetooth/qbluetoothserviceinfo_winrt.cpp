/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include "qbluetoothserviceinfo_p.h"
#include "qbluetoothserver_p.h"

#include <QtCore/QLoggingCategory>
#include <qfunctions_winrt.h>

#include <wrl.h>
#include <windows.devices.bluetooth.h>
#include <windows.devices.bluetooth.rfcomm.h>
#include <windows.foundation.h>
#include <windows.networking.sockets.h>
#include <windows.storage.streams.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Devices::Bluetooth;
using namespace ABI::Windows::Devices::Bluetooth::Rfcomm;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Networking::Sockets;
using namespace ABI::Windows::Storage::Streams;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_WINRT)

#define TYPE_UINT8 8
#define TYPE_UINT16 9
#define TYPE_UINT32 10
#define TYPE_SHORT_UUID 25
#define TYPE_LONG_UUID 28
#define TYPE_STRING 37
#define TYPE_SEQUENCE 53

extern QHash<QBluetoothServerPrivate *, int> __fakeServerPorts;

bool repairProfileDescriptorListIfNeeded(ComPtr<IBuffer> &buffer)
{
    ComPtr<IDataReaderStatics> dataReaderStatics;
    HRESULT hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_DataReader).Get(),
                                      &dataReaderStatics);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IDataReader> reader;
    hr = dataReaderStatics->FromBuffer(buffer.Get(), reader.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);

    BYTE type;
    hr = reader->ReadByte(&type);
    Q_ASSERT_SUCCEEDED(hr);
    if (type != TYPE_SEQUENCE) {
        qCWarning(QT_BT_WINRT) << Q_FUNC_INFO << "Malformed profile descriptor list read";
        return false;
    }

    quint8 length;
    hr = reader->ReadByte(&length);
    Q_ASSERT_SUCCEEDED(hr);

    hr = reader->ReadByte(&type);
    Q_ASSERT_SUCCEEDED(hr);
    // We have to "repair" the structure if the outer sequence contains a uuid directly
    if (type == TYPE_SHORT_UUID && length == 4) {
        qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Repairing profile descriptor list";
        quint16 uuid;
        hr = reader->ReadUInt16(&uuid);
        Q_ASSERT_SUCCEEDED(hr);

        ComPtr<IDataWriter> writer;
        hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_DataWriter).Get(),
            &writer);
        Q_ASSERT_SUCCEEDED(hr);

        hr = writer->WriteByte(TYPE_SEQUENCE);
        Q_ASSERT_SUCCEEDED(hr);
        // 8 == length of nested sequence (outer sequence -> inner sequence -> uuid and version)
        hr = writer->WriteByte(8);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteByte(TYPE_SEQUENCE);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteByte(7);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteByte(TYPE_SHORT_UUID);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteUInt16(uuid);
        Q_ASSERT_SUCCEEDED(hr);
        // Write default version to make WinRT happy
        hr = writer->WriteByte(TYPE_UINT16);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteUInt16(0x100);
        Q_ASSERT_SUCCEEDED(hr);

        hr = writer->DetachBuffer(&buffer);
        Q_ASSERT_SUCCEEDED(hr);
    }

    return true;
}

static ComPtr<IBuffer> bufferFromAttribute(const QVariant &attribute)
{
    ComPtr<IDataWriter> writer;
    HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_DataWriter).Get(),
                                  &writer);
    Q_ASSERT_SUCCEEDED(hr);

    switch (int(attribute.type())) {
    case QMetaType::Void:
        qCWarning(QT_BT_WINRT) << "Don't know how to register QMetaType::Void";
        return nullptr;
    case QMetaType::UChar:
        qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registering attribute of type QMetaType::UChar";
        hr = writer->WriteByte(TYPE_UINT8);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteByte(attribute.value<quint8>());
        Q_ASSERT_SUCCEEDED(hr);
        break;
    case QMetaType::UShort:
        qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registering attribute of type QMetaType::UShort";
        hr = writer->WriteByte(TYPE_UINT16);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteUInt16(attribute.value<quint16>());
        Q_ASSERT_SUCCEEDED(hr);
        break;
    case QMetaType::UInt:
        qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registering attribute of type QMetaType::UInt";
        hr = writer->WriteByte(TYPE_UINT32);
        Q_ASSERT_SUCCEEDED(hr);
        hr = writer->WriteByte(attribute.value<quint32>());
        Q_ASSERT_SUCCEEDED(hr);
        break;
    case QMetaType::Char:
        qCWarning(QT_BT_WINRT) << "Don't know how to register QMetaType::Char";
        return nullptr;
        break;
    case QMetaType::Short:
        qCWarning(QT_BT_WINRT) << "Don't know how to register QMetaType::Short";
        return nullptr;
        break;
    case QMetaType::Int:
        qCWarning(QT_BT_WINRT) << "Don't know how to register QMetaType::Int";
        return nullptr;
        break;
    case QMetaType::QString: {
        qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registering attribute of type QMetaType::QString";
        hr = writer->WriteByte(TYPE_STRING);
        Q_ASSERT_SUCCEEDED(hr);
        const QString stringValue = attribute.value<QString>();
        hr = writer->WriteByte(stringValue.length());
        Q_ASSERT_SUCCEEDED(hr);
        HStringReference stringRef(reinterpret_cast<LPCWSTR>(stringValue.utf16()));
        quint32 bytesWritten;
        hr = writer->WriteString(stringRef.Get(), &bytesWritten);
        if (bytesWritten != stringValue.length()) {
            qCWarning(QT_BT_WINRT) << "Did not write full value to buffer";
            return nullptr;
        }
        Q_ASSERT_SUCCEEDED(hr);
        break;
    }
    case QMetaType::Bool:
        qCWarning(QT_BT_WINRT) << "Don't know how to register QMetaType::Bool";
        return nullptr;
        break;
    case QMetaType::QUrl:
        qCWarning(QT_BT_WINRT) << "Don't know how to register QMetaType::QUrl";
        return nullptr;
        break;
    case QVariant::UserType:
        if (attribute.userType() == qMetaTypeId<QBluetoothUuid>()) {
            QBluetoothUuid uuid = attribute.value<QBluetoothUuid>();
            const int minimumSize = uuid.minimumSize();
            switch (uuid.minimumSize()) {
            case 0:
                qCWarning(QT_BT_WINRT) << "Don't know how to register Uuid of length 0";
                return nullptr;
                break;
            case 2:
                qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registering Uuid attribute with length 2" << uuid;
                hr = writer->WriteByte(TYPE_SHORT_UUID);
                Q_ASSERT_SUCCEEDED(hr);
                hr = writer->WriteUInt16(uuid.toUInt16());
                Q_ASSERT_SUCCEEDED(hr);
                break;
            case 4:
                qCWarning(QT_BT_WINRT) << "Don't know how to register Uuid of length 4";
                return nullptr;
                break;
            case 16:
                qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registering Uuid attribute with length 16";
                hr = writer->WriteByte(TYPE_LONG_UUID);
                Q_ASSERT_SUCCEEDED(hr);
                hr = writer->WriteGuid(uuid);
                Q_ASSERT_SUCCEEDED(hr);
                break;
            default:
                qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registering Uuid attribute";
                hr = writer->WriteByte(TYPE_LONG_UUID);
                Q_ASSERT_SUCCEEDED(hr);
                hr = writer->WriteGuid(uuid);
                Q_ASSERT_SUCCEEDED(hr);
                break;
            }
        } else if (attribute.userType() == qMetaTypeId<QBluetoothServiceInfo::Sequence>()) {
            qCDebug(QT_BT_WINRT) << "Registering sequence attribute";
            hr = writer->WriteByte(TYPE_SEQUENCE);
            Q_ASSERT_SUCCEEDED(hr);
            const QBluetoothServiceInfo::Sequence *sequence =
                    static_cast<const QBluetoothServiceInfo::Sequence *>(attribute.data());
            ComPtr<IDataWriter> tmpWriter;
            HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_DataWriter).Get(),
                &tmpWriter);
            Q_ASSERT_SUCCEEDED(hr);
            foreach (const QVariant &v, *sequence) {
                ComPtr<IBuffer> tmpBuffer = bufferFromAttribute(v);
                if (!tmpBuffer) {
                    qCWarning(QT_BT_WINRT) << "Could not create buffer from attribute in sequence";
                    return nullptr;
                }
                quint32 l;
                hr = tmpBuffer->get_Length(&l);
                Q_ASSERT_SUCCEEDED(hr);
                hr = tmpWriter->WriteBuffer(tmpBuffer.Get());
                Q_ASSERT_SUCCEEDED(hr);
            }
            ComPtr<IBuffer> tmpBuffer;
            hr = tmpWriter->DetachBuffer(&tmpBuffer);
            Q_ASSERT_SUCCEEDED(hr);
            // write sequence length
            quint32 length;
            tmpBuffer->get_Length(&length);
            Q_ASSERT_SUCCEEDED(hr);
            hr = writer->WriteByte(length + 1);
            Q_ASSERT_SUCCEEDED(hr);
            // write sequence data
            hr = writer->WriteBuffer(tmpBuffer.Get());
            Q_ASSERT_SUCCEEDED(hr);
            qCDebug(QT_BT_WINRT) << Q_FUNC_INFO << "Registered sequence attribute with length" << length;
        } else if (attribute.userType() == qMetaTypeId<QBluetoothServiceInfo::Alternative>()) {
            qCWarning(QT_BT_WINRT) << "Don't know how to register user type Alternative";
            return false;
        }
        break;
    default:
        qCWarning(QT_BT_WINRT) << "Unknown variant type", attribute.userType();
        return nullptr;
    }
    ComPtr<IBuffer> buffer;
    hr = writer->DetachBuffer(&buffer);
    Q_ASSERT_SUCCEEDED(hr);
    return buffer;
}

QBluetoothServiceInfoPrivate::QBluetoothServiceInfoPrivate()
    : registered(false)
{
}

QBluetoothServiceInfoPrivate::~QBluetoothServiceInfoPrivate()
{
}

bool QBluetoothServiceInfoPrivate::isRegistered() const
{
    return registered;
}

bool QBluetoothServiceInfoPrivate::registerService(const QBluetoothAddress &localAdapter)
{
    Q_UNUSED(localAdapter);
    if (registered)
        return false;

    if (protocolDescriptor(QBluetoothUuid::Rfcomm).isEmpty()) {
        qCWarning(QT_BT_WINRT) << Q_FUNC_INFO << "Only RFCOMM services can be registered on WinRT";
        return false;
    }

    QBluetoothServerPrivate *sPriv = __fakeServerPorts.key(serverChannel());
    if (!sPriv)
        return false;

    HRESULT hr;
    QBluetoothUuid uuid = attributes.value(QBluetoothServiceInfo::ServiceId).value<QBluetoothUuid>();
    ComPtr<IRfcommServiceIdStatics> serviceIdStatics;
    hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Bluetooth_Rfcomm_RfcommServiceId).Get(),
                                IID_PPV_ARGS(&serviceIdStatics));
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IRfcommServiceId> serviceId;
    hr = serviceIdStatics->FromUuid(uuid, &serviceId);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IRfcommServiceProviderStatics> providerStatics;
    hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Bluetooth_Rfcomm_RfcommServiceProvider).Get(),
                                IID_PPV_ARGS(&providerStatics));
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IAsyncOperation<RfcommServiceProvider *>> op;
    hr = providerStatics->CreateAsync(serviceId.Get(), &op);
    Q_ASSERT_SUCCEEDED(hr);
    hr = QWinRTFunctions::await(op, serviceProvider.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IStreamSocketListener> listener = sPriv->listener();
    if (!listener) {
        qCWarning(QT_BT_WINRT) << Q_FUNC_INFO << "Could not obtain listener from server.";
        return false;
    }


    HString serviceIdHString;
    serviceId->AsString(serviceIdHString.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    const QString serviceIdString = QString::fromWCharArray(WindowsGetStringRawBuffer(serviceIdHString.Get(), nullptr));

    //tell the server what service name our listener should have
    //and start the real listener
    bool result = sPriv->initiateActiveListening(serviceIdString);
    if (!result) {
        return false;
    }

    result = writeSdpAttributes();
    if (!result) {
        return false;
    }

    hr = serviceProvider->StartAdvertising(listener.Get());
    if (FAILED(hr)) {
        qCWarning(QT_BT_WINRT) << Q_FUNC_INFO << "Could not start advertising. Check your SDP data.";
        return false;
    }

    registered = true;
    return true;
}

bool QBluetoothServiceInfoPrivate::unregisterService()
{
    if (!registered)
        return false;

    QBluetoothServerPrivate *sPriv = __fakeServerPorts.key(serverChannel());
    if (!sPriv) {
        //QBluetoothServer::close() was called without prior call to unregisterService().
        //Now it is unregistered anyway.
        registered = false;
        return true;
    }

    bool result = sPriv->deactivateActiveListening();
    if (!result)
        return false;

    HRESULT hr;
    hr = serviceProvider->StopAdvertising();
    Q_ASSERT_SUCCEEDED(hr);

    registered = false;
    return true;
}

bool QBluetoothServiceInfoPrivate::writeSdpAttributes()
{
    if (!serviceProvider)
        return false;

    ComPtr<IDataWriter> writer;
    HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_DataWriter).Get(),
                                  &writer);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IMap<UINT32, IBuffer *>> rawAttributes;
    hr = serviceProvider->get_SdpRawAttributes(&rawAttributes);
    Q_ASSERT_SUCCEEDED(hr);
    for (quint16 key : attributes.keys()) {
        // The SDP Class Id List and RFCOMM and L2CAP protocol descriptors are automatically
        // generated by the RfcommServiceProvider. Do not specify it in the SDP raw attribute map.
        if (key == QBluetoothServiceInfo::ServiceClassIds
                || key == QBluetoothServiceInfo::ProtocolDescriptorList)
            continue;
        const QVariant attribute = attributes.value(key);
        HRESULT hr;
        ComPtr<IBuffer> buffer = bufferFromAttribute(attribute);
        if (!buffer) {
            qCWarning(QT_BT_WINRT) << "Could not create buffer from attribute with id:" << key;
            return false;
        }

        // Other backends support a wrong structure in profile descriptor list. In order to make
        // WinRT accept the list without breaking existing apps we have to repair this structure.
        if (key == QBluetoothServiceInfo::BluetoothProfileDescriptorList) {
            if (!repairProfileDescriptorListIfNeeded(buffer)) {
                qCWarning(QT_BT_WINRT) << Q_FUNC_INFO << "Error while checking/repairing structure of profile descriptor list";
                return false;
            }
        }

        hr = writer->WriteBuffer(buffer.Get());
        Q_ASSERT_SUCCEEDED(hr);

        hr = writer->DetachBuffer(&buffer);
        Q_ASSERT_SUCCEEDED(hr);

        boolean replaced;
        hr = rawAttributes->Insert(key, buffer.Get(), &replaced);
        Q_ASSERT_SUCCEEDED(hr);
        Q_ASSERT(!replaced);
    }
    return true;
}

QT_END_NAMESPACE
