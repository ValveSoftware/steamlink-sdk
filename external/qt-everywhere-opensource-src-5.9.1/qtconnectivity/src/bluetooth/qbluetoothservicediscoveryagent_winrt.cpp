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

#include "qbluetoothservicediscoveryagent.h"
#include "qbluetoothservicediscoveryagent_p.h"

#include <qfunctions_winrt.h>
#include <QtCore/QLoggingCategory>
#include <QtCore/private/qeventdispatcher_winrt_p.h>

#include <functional>
#include <robuffer.h>
#include <windows.devices.enumeration.h>
#include <windows.devices.bluetooth.h>
#include <windows.foundation.collections.h>
#include <windows.storage.streams.h>
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Devices;
using namespace ABI::Windows::Devices::Bluetooth;
using namespace ABI::Windows::Devices::Bluetooth::Rfcomm;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Storage::Streams;

typedef Collections::IKeyValuePair<UINT32, IBuffer *> ValueItem;
typedef Collections::IIterable<ValueItem *> ValueIterable;
typedef Collections::IIterator<ValueItem *> ValueIterator;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_WINRT)

#define TYPE_UINT8 8
#define TYPE_UINT16 9
#define TYPE_UINT32 10
#define TYPE_SHORT_UUID 25
#define TYPE_LONG_UUID 28
#define TYPE_STRING 37
#define TYPE_SEQUENCE 53

static QByteArray byteArrayFromBuffer(const ComPtr<IBuffer> &buffer, bool isWCharString = false)
{
    ComPtr<Windows::Storage::Streams::IBufferByteAccess> byteAccess;
    HRESULT hr = buffer.As(&byteAccess);
    Q_ASSERT_SUCCEEDED(hr);
    char *data;
    hr = byteAccess->Buffer(reinterpret_cast<byte **>(&data));
    Q_ASSERT_SUCCEEDED(hr);
    UINT32 size;
    hr = buffer->get_Length(&size);
    Q_ASSERT_SUCCEEDED(hr);
    if (isWCharString) {
        QString valueString = QString::fromUtf16(reinterpret_cast<ushort *>(data)).left(size / 2);
        return valueString.toUtf8();
    }
    return QByteArray(data, size);
}

class QWinRTBluetoothServiceDiscoveryWorker : public QObject
{
    Q_OBJECT
public:
    explicit QWinRTBluetoothServiceDiscoveryWorker(quint64 targetAddress,
                                                   QBluetoothServiceDiscoveryAgent::DiscoveryMode mode);
    ~QWinRTBluetoothServiceDiscoveryWorker();
    void start();

Q_SIGNALS:
    void serviceFound(quint64 deviceAddress, const QBluetoothServiceInfo &info);
    void scanFinished(quint64 deviceAddress);
    void scanCanceled();
    void errorOccured();

private:
    HRESULT onBluetoothDeviceFoundAsync(IAsyncOperation<BluetoothDevice *> *op, AsyncStatus status);

    void processServiceSearchResult(quint64 address, ComPtr<IVectorView<RfcommDeviceService*>> services);
    QBluetoothServiceInfo::Sequence readSequence(ComPtr<IDataReader> dataReader, bool *ok, quint8 *bytesRead);

private:
    quint64 m_targetAddress;
    QBluetoothServiceDiscoveryAgent::DiscoveryMode m_mode;
};

QWinRTBluetoothServiceDiscoveryWorker::QWinRTBluetoothServiceDiscoveryWorker(quint64 targetAddress,
                                                                             QBluetoothServiceDiscoveryAgent::DiscoveryMode mode)
    : m_targetAddress(targetAddress)
    , m_mode(mode)
{
}

QWinRTBluetoothServiceDiscoveryWorker::~QWinRTBluetoothServiceDiscoveryWorker()
{
}

void QWinRTBluetoothServiceDiscoveryWorker::start()
{
    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([this]() {
        ComPtr<IBluetoothDeviceStatics> deviceStatics;
        HRESULT hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Bluetooth_BluetoothDevice).Get(), &deviceStatics);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IAsyncOperation<BluetoothDevice *>> deviceFromAddressOperation;
        hr = deviceStatics->FromBluetoothAddressAsync(m_targetAddress, &deviceFromAddressOperation);
        Q_ASSERT_SUCCEEDED(hr);
        hr = deviceFromAddressOperation->put_Completed(Callback<IAsyncOperationCompletedHandler<BluetoothDevice *>>
                                                  (this, &QWinRTBluetoothServiceDiscoveryWorker::onBluetoothDeviceFoundAsync).Get());
        Q_ASSERT_SUCCEEDED(hr);
        return S_OK;
    });
    Q_ASSERT_SUCCEEDED(hr);
}

HRESULT QWinRTBluetoothServiceDiscoveryWorker::onBluetoothDeviceFoundAsync(IAsyncOperation<BluetoothDevice *> *op, AsyncStatus status)
{
    if (status != Completed) {
        qCDebug(QT_BT_WINRT) << "Could not find device";
        emit errorOccured();
        return S_OK;
    }

    ComPtr<IBluetoothDevice> device;
    HRESULT hr;
    hr = op->GetResults(&device);
    Q_ASSERT_SUCCEEDED(hr);
    quint64 address;
    device->get_BluetoothAddress(&address);

#ifdef QT_WINRT_LIMITED_SERVICEDISCOVERY
    if (m_mode != QBluetoothServiceDiscoveryAgent::MinimalDiscovery) {
        qWarning() << "Used Windows SDK version (" << QString::number(QT_UCRTVERSION) << ") does not "
                      "support full service discovery. Consider updating to a more recent Windows 10 "
                      "SDK (14393 or above).";
    }
    ComPtr<IVectorView<RfcommDeviceService*>> commServices;
    hr = device->get_RfcommServices(&commServices);
    Q_ASSERT_SUCCEEDED(hr);
    processServiceSearchResult(address, commServices);
#else // !QT_WINRT_LIMITED_SERVICEDISOVERY
    ComPtr<IBluetoothDevice3> device3;
    hr = device.As(&device3);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IAsyncOperation<RfcommDeviceServicesResult *>> serviceOp;
    const BluetoothCacheMode cacheMode = m_mode == QBluetoothServiceDiscoveryAgent::MinimalDiscovery
        ? BluetoothCacheMode_Cached : BluetoothCacheMode_Uncached;
    hr = device3->GetRfcommServicesWithCacheModeAsync(cacheMode, &serviceOp);
    Q_ASSERT_SUCCEEDED(hr);
    hr = serviceOp->put_Completed(Callback<IAsyncOperationCompletedHandler<RfcommDeviceServicesResult *>>
        ([address, this](IAsyncOperation<RfcommDeviceServicesResult *> *op, AsyncStatus status)
    {
        if (status != Completed) {
            qCDebug(QT_BT_WINRT) << "Could not obtain service list";
            emit errorOccured();
            return S_OK;
        }

        ComPtr<IRfcommDeviceServicesResult> result;
        HRESULT hr = op->GetResults(&result);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IVectorView<RfcommDeviceService*>> commServices;
        hr = result->get_Services(&commServices);
        Q_ASSERT_SUCCEEDED(hr);
        processServiceSearchResult(address, commServices);
        return S_OK;
    }).Get());
    Q_ASSERT_SUCCEEDED(hr);
#endif // !QT_WINRT_LIMITED_SERVICEDISOVERY

    return S_OK;
}

void QWinRTBluetoothServiceDiscoveryWorker::processServiceSearchResult(quint64 address, ComPtr<IVectorView<RfcommDeviceService*>> services)
{
    quint32 size;
    HRESULT hr;
    hr = services->get_Size(&size);
    Q_ASSERT_SUCCEEDED(hr);
    for (quint32 i = 0; i < size; ++i) {
        ComPtr<IRfcommDeviceService> service;
        hr = services->GetAt(i, &service);
        Q_ASSERT_SUCCEEDED(hr);
        HString name;
        hr = service->get_ConnectionServiceName(name.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        const QString serviceName = QString::fromWCharArray(WindowsGetStringRawBuffer(name.Get(), nullptr));
        ComPtr<IRfcommServiceId> id;
        hr = service->get_ServiceId(&id);
        Q_ASSERT_SUCCEEDED(hr);
        GUID guid;
        hr = id->get_Uuid(&guid);
        const QBluetoothUuid uuid(guid);
        Q_ASSERT_SUCCEEDED(hr);

        QBluetoothServiceInfo info;
        info.setServiceName(serviceName);
        info.setServiceUuid(uuid);
        ComPtr<IAsyncOperation<IMapView<UINT32, IBuffer *> *>> op;
        hr = service->GetSdpRawAttributesAsync(op.GetAddressOf());
        if (FAILED(hr)) {
            emit errorOccured();
            qDebug() << "Check manifest capabilities";
            continue;
        }
        ComPtr<IMapView<UINT32, IBuffer *>> mapView;
        hr = QWinRTFunctions::await(op, mapView.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        // TODO timeout
        ComPtr<ValueIterable> iterable;
        ComPtr<ValueIterator> iterator;

        hr = mapView.As(&iterable);
        if (FAILED(hr))
            continue;

        boolean current = false;
        hr = iterable->First(&iterator);
        if (FAILED(hr))
            continue;
        hr = iterator->get_HasCurrent(&current);
        if (FAILED(hr))
            continue;

        while (SUCCEEDED(hr) && current) {
            ComPtr<ValueItem> item;
            hr = iterator->get_Current(&item);
            if (FAILED(hr))
                continue;

            UINT32 key;
            hr = item->get_Key(&key);
            if (FAILED(hr))
                continue;

            ComPtr<IBuffer> buffer;
            hr = item->get_Value(&buffer);
            Q_ASSERT_SUCCEEDED(hr);

            ComPtr<IDataReader> dataReader;
            ComPtr<IDataReaderStatics> dataReaderStatics;
            hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_DataReader).Get(), &dataReaderStatics);
            Q_ASSERT_SUCCEEDED(hr);
            hr = dataReaderStatics->FromBuffer(buffer.Get(), dataReader.GetAddressOf());
            Q_ASSERT_SUCCEEDED(hr);
            BYTE type;
            hr = dataReader->ReadByte(&type);
            Q_ASSERT_SUCCEEDED(hr);
            if (type == TYPE_UINT8) {
                quint8 value;
                hr = dataReader->ReadByte(&value);
                Q_ASSERT_SUCCEEDED(hr);
                info.setAttribute(key, value);
                qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "UINT8" << hex << value;
            } else if (type == TYPE_UINT16) {
                quint16 value;
                hr = dataReader->ReadUInt16(&value);
                Q_ASSERT_SUCCEEDED(hr);
                info.setAttribute(key, value);
                qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "UINT16" << hex << value;
            } else if (type == TYPE_UINT32) {
                quint32 value;
                hr = dataReader->ReadUInt32(&value);
                Q_ASSERT_SUCCEEDED(hr);
                info.setAttribute(key, value);
                qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "UINT32" << hex << value;
            } else if (type == TYPE_SHORT_UUID) {
                quint16 value;
                hr = dataReader->ReadUInt16(&value);
                Q_ASSERT_SUCCEEDED(hr);
                const QBluetoothUuid uuid(value);
                info.setAttribute(key, uuid);
                qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "UUID" << hex << uuid;
            } else if (type == TYPE_LONG_UUID) {
                GUID value;
                hr = dataReader->ReadGuid(&value);
                Q_ASSERT_SUCCEEDED(hr);
                const QBluetoothUuid uuid(value);
                info.setAttribute(key, uuid);
                qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "UUID" << hex << uuid;
            } else if (type == TYPE_STRING) {
                BYTE length;
                hr = dataReader->ReadByte(&length);
                Q_ASSERT_SUCCEEDED(hr);
                HString value;
                hr = dataReader->ReadString(length, value.GetAddressOf());
                Q_ASSERT_SUCCEEDED(hr);
                const QString str = QString::fromWCharArray(WindowsGetStringRawBuffer(value.Get(), nullptr));
                info.setAttribute(key, str);
                qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "STRING" << str;
            } else if (type == TYPE_SEQUENCE) {
                bool ok;
                QBluetoothServiceInfo::Sequence sequence = readSequence(dataReader, &ok, nullptr);
                if (ok) {
                    info.setAttribute(key, sequence);
                    qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "SEQUENCE" << sequence;
                } else {
                    qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type << "SEQUENCE ERROR";
                }
            } else {
                qCDebug(QT_BT_WINRT) << "UUID" << uuid << "KEY" << hex << key << "TYPE" << dec << type;
            }
            hr = iterator->MoveNext(&current);
        }
        emit serviceFound(address, info);
    }
    emit scanFinished(address);
    deleteLater();
}

QBluetoothServiceInfo::Sequence QWinRTBluetoothServiceDiscoveryWorker::readSequence(ComPtr<IDataReader> dataReader, bool *ok, quint8 *bytesRead)
{
    if (ok)
        *ok = false;
    if (bytesRead)
        *bytesRead = 0;
    QBluetoothServiceInfo::Sequence result;
    if (!dataReader)
        return result;

    quint8 remainingLength;
    HRESULT hr = dataReader->ReadByte(&remainingLength);
    Q_ASSERT_SUCCEEDED(hr);
    if (bytesRead)
        *bytesRead += 1;
    BYTE type;
    hr = dataReader->ReadByte(&type);
    remainingLength -= 1;
    if (bytesRead)
        *bytesRead += 1;
    Q_ASSERT_SUCCEEDED(hr);
    while (true) {
        switch (type) {
        case TYPE_UINT8: {
            quint8 value;
            hr = dataReader->ReadByte(&value);
            Q_ASSERT_SUCCEEDED(hr);
            result.append(QVariant::fromValue(value));
            remainingLength -= 1;
            if (bytesRead)
                *bytesRead += 1;
            break;
        }
        case TYPE_UINT16: {
            quint16 value;
            hr = dataReader->ReadUInt16(&value);
            Q_ASSERT_SUCCEEDED(hr);
            result.append(QVariant::fromValue(value));
            remainingLength -= 2;
            if (bytesRead)
                *bytesRead += 2;
            break;
        }
        case TYPE_UINT32: {
            quint32 value;
            hr = dataReader->ReadUInt32(&value);
            Q_ASSERT_SUCCEEDED(hr);
            result.append(QVariant::fromValue(value));
            remainingLength -= 4;
            if (bytesRead)
                *bytesRead += 4;
            break;
        }
        case TYPE_SHORT_UUID: {
            quint16 b;
            hr = dataReader->ReadUInt16(&b);
            Q_ASSERT_SUCCEEDED(hr);

            const QBluetoothUuid uuid(b);
            result.append(QVariant::fromValue(uuid));
            remainingLength -= 2;
            if (bytesRead)
                *bytesRead += 2;
            break;
        }
        case TYPE_LONG_UUID: {
            GUID b;
            hr = dataReader->ReadGuid(&b);
            Q_ASSERT_SUCCEEDED(hr);

            const QBluetoothUuid uuid(b);
            result.append(QVariant::fromValue(uuid));
            remainingLength -= sizeof(GUID);
            if (bytesRead)
                *bytesRead += sizeof(GUID);
            break;
        }
        case TYPE_STRING: {
            BYTE length;
            hr = dataReader->ReadByte(&length);
            Q_ASSERT_SUCCEEDED(hr);
            remainingLength -= 1;
            if (bytesRead)
                *bytesRead += 1;
            HString value;
            hr = dataReader->ReadString(length, value.GetAddressOf());
            Q_ASSERT_SUCCEEDED(hr);

            const QString str = QString::fromWCharArray(WindowsGetStringRawBuffer(value.Get(), nullptr));
            result.append(QVariant::fromValue(str));
            remainingLength -= length;
            if (bytesRead)
                *bytesRead += length;
            break;
        }
        case TYPE_SEQUENCE: {
            quint8 bytesR;
            const QBluetoothServiceInfo::Sequence sequence = readSequence(dataReader, ok, &bytesR);
            if (*ok)
                result.append(QVariant::fromValue(sequence));
            else
                return result;
            remainingLength -= bytesR;
            if (bytesRead)
                *bytesRead += bytesR;
            break;
        }
        default:
            qCDebug(QT_BT_WINRT) << "SEQUENCE ERROR" << type;
            result.clear();
            return result;
        }
        if (remainingLength == 0)
            break;

        hr = dataReader->ReadByte(&type);
        Q_ASSERT_SUCCEEDED(hr);
        remainingLength -= 1;
        if (bytesRead)
            *bytesRead += 1;
    }

    if (ok)
        *ok = true;
    return result;
}

QBluetoothServiceDiscoveryAgentPrivate::QBluetoothServiceDiscoveryAgentPrivate(
    QBluetoothServiceDiscoveryAgent *qp, const QBluetoothAddress &deviceAdapter)
    : error(QBluetoothServiceDiscoveryAgent::NoError),
      state(Inactive),
      deviceDiscoveryAgent(0),
      mode(QBluetoothServiceDiscoveryAgent::MinimalDiscovery),
      singleDevice(false),
      q_ptr(qp)
{
    // TODO: use local adapter for discovery. Possible?
    Q_UNUSED(deviceAdapter);
}

QBluetoothServiceDiscoveryAgentPrivate::~QBluetoothServiceDiscoveryAgentPrivate()
{
    stop();
}

void QBluetoothServiceDiscoveryAgentPrivate::start(const QBluetoothAddress &address)
{
    if (worker)
        return;

    worker = new QWinRTBluetoothServiceDiscoveryWorker(address.toUInt64(), mode);

    connect(worker, &QWinRTBluetoothServiceDiscoveryWorker::serviceFound,
        this, &QBluetoothServiceDiscoveryAgentPrivate::processFoundService, Qt::QueuedConnection);
    connect(worker, &QWinRTBluetoothServiceDiscoveryWorker::scanFinished,
        this, &QBluetoothServiceDiscoveryAgentPrivate::onScanFinished, Qt::QueuedConnection);
    connect(worker, &QWinRTBluetoothServiceDiscoveryWorker::scanCanceled,
        this, &QBluetoothServiceDiscoveryAgentPrivate::onScanCanceled, Qt::QueuedConnection);
    connect(worker, &QWinRTBluetoothServiceDiscoveryWorker::errorOccured,
        this, &QBluetoothServiceDiscoveryAgentPrivate::onError, Qt::QueuedConnection);
    worker->start();
}

void QBluetoothServiceDiscoveryAgentPrivate::stop()
{
    if (!worker)
        return;

    disconnect(worker, &QWinRTBluetoothServiceDiscoveryWorker::serviceFound,
        this, &QBluetoothServiceDiscoveryAgentPrivate::processFoundService);
    disconnect(worker, &QWinRTBluetoothServiceDiscoveryWorker::scanFinished,
        this, &QBluetoothServiceDiscoveryAgentPrivate::onScanFinished);
    disconnect(worker, &QWinRTBluetoothServiceDiscoveryWorker::scanCanceled,
        this, &QBluetoothServiceDiscoveryAgentPrivate::onScanCanceled);
    disconnect(worker, &QWinRTBluetoothServiceDiscoveryWorker::errorOccured,
        this, &QBluetoothServiceDiscoveryAgentPrivate::onError);
    // mWorker will delete itself as soon as it is done with its discovery
    worker = nullptr;
    setDiscoveryState(Inactive);
}

void QBluetoothServiceDiscoveryAgentPrivate::processFoundService(quint64 deviceAddress, const QBluetoothServiceInfo &info)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);
    //apply uuidFilter
    if (!uuidFilter.isEmpty()) {
        bool serviceNameMatched = uuidFilter.contains(info.serviceUuid());
        bool serviceClassMatched = false;
        for (const QBluetoothUuid &id : info.serviceClassUuids()) {
            if (uuidFilter.contains(id)) {
                serviceClassMatched = true;
                break;
            }
        }

        if (!serviceNameMatched && !serviceClassMatched)
            return;
    }

    if (!info.isValid())
        return;

    QBluetoothServiceInfo returnInfo(info);
    bool deviceFound;
    for (const QBluetoothDeviceInfo &deviceInfo : discoveredDevices) {
        if (deviceInfo.address().toUInt64() == deviceAddress) {
            deviceFound = true;
            returnInfo.setDevice(deviceInfo);
            break;
        }
    }
    Q_ASSERT(deviceFound);

    if (!isDuplicatedService(returnInfo)) {
        discoveredServices.append(returnInfo);
        qCDebug(QT_BT_WINRT) << "Discovered services" << discoveredDevices.at(0).address().toString()
                             << returnInfo.serviceName() << returnInfo.serviceUuid()
                             << ">>>" << returnInfo.serviceClassUuids();

        emit q->serviceDiscovered(returnInfo);
    }
}

void QBluetoothServiceDiscoveryAgentPrivate::onScanFinished(quint64 deviceAddress)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);
    bool deviceFound;
    for (const QBluetoothDeviceInfo &deviceInfo : discoveredDevices) {
        if (deviceInfo.address().toUInt64() == deviceAddress) {
            deviceFound = true;
            discoveredDevices.removeOne(deviceInfo);
            if (discoveredDevices.isEmpty())
                setDiscoveryState(Inactive);
            break;
        }
    }
    Q_ASSERT(deviceFound);
    stop();
    emit q->finished();
}

void QBluetoothServiceDiscoveryAgentPrivate::onScanCanceled()
{
    Q_Q(QBluetoothServiceDiscoveryAgent);
    emit q->canceled();
}

void QBluetoothServiceDiscoveryAgentPrivate::onError()
{
    Q_Q(QBluetoothServiceDiscoveryAgent);
    discoveredDevices.clear();
    error = QBluetoothServiceDiscoveryAgent::InputOutputError;
    errorString = "errorDescription";
    emit q->error(error);
}

QT_END_NAMESPACE

#include <qbluetoothservicediscoveryagent_winrt.moc>
