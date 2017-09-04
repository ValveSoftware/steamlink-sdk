/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qwinrtvideodeviceselectorcontrol.h"

#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QVector>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QGlobalStatic>

#include <wrl.h>
#include <windows.devices.enumeration.h>

using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

typedef ITypedEventHandler<DeviceWatcher *, DeviceInformation *> DeviceInformationHandler;
typedef ITypedEventHandler<DeviceWatcher *, DeviceInformationUpdate *> DeviceInformationUpdateHandler;
typedef ITypedEventHandler<DeviceWatcher *, IInspectable *> DeviceEnumerationCompletedHandler;

QT_BEGIN_NAMESPACE

static QString deviceName(IDeviceInformation *device)
{
    HRESULT hr;
    HString id;
    hr = device->get_Id(id.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    quint32 length;
    const wchar_t *buffer = id.GetRawBuffer(&length);
    return QString::fromWCharArray(buffer, length);
}

static QString deviceDescription(IDeviceInformation *device)
{
    HRESULT hr;
    HString name;
    hr = device->get_Name(name.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    quint32 length;
    const wchar_t *buffer = name.GetRawBuffer(&length);
    return QString::fromWCharArray(buffer, length);
}

struct QWinRTVideoDeviceSelectorControlGlobal
{
    QWinRTVideoDeviceSelectorControlGlobal()
        : defaultDeviceIndex(-1)
    {
        HRESULT hr;
        ComPtr<IDeviceInformationStatics> deviceWatcherFactory;
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(),
                                    IID_PPV_ARGS(&deviceWatcherFactory));
        Q_ASSERT_SUCCEEDED(hr);
        hr = deviceWatcherFactory->CreateWatcherDeviceClass(DeviceClass_VideoCapture, &deviceWatcher);
        Q_ASSERT_SUCCEEDED(hr);

        hr = deviceWatcher->add_Added(
                    Callback<DeviceInformationHandler>(this, &QWinRTVideoDeviceSelectorControlGlobal::onDeviceAdded).Get(),
                    &deviceAddedToken);
        Q_ASSERT_SUCCEEDED(hr);

        hr = deviceWatcher->add_Removed(
                    Callback<DeviceInformationUpdateHandler>(this, &QWinRTVideoDeviceSelectorControlGlobal::onDeviceRemoved).Get(),
                    &deviceRemovedToken);
        Q_ASSERT_SUCCEEDED(hr);

        hr = deviceWatcher->add_Updated(
                    Callback<DeviceInformationUpdateHandler>(this, &QWinRTVideoDeviceSelectorControlGlobal::onDeviceUpdated).Get(),
                    &deviceUpdatedToken);
        Q_ASSERT_SUCCEEDED(hr);

        // Synchronously populate the devices on construction
        ComPtr<IAsyncOperation<DeviceInformationCollection *>> op;
        hr = deviceWatcherFactory->FindAllAsyncDeviceClass(DeviceClass_VideoCapture, &op);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IVectorView<DeviceInformation *>> deviceList;
        hr = QWinRTFunctions::await(op, deviceList.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        quint32 deviceCount;
        hr = deviceList->get_Size(&deviceCount);
        Q_ASSERT_SUCCEEDED(hr);
        for (quint32 i = 0; i < deviceCount; ++i) {
            IDeviceInformation *device;
            hr = deviceList->GetAt(i, &device);
            Q_ASSERT_SUCCEEDED(hr);
            onDeviceAdded(Q_NULLPTR, device);
        }

        // If there is no default device provided by the API, choose the first one
        if (!devices.isEmpty() && defaultDeviceIndex < 0)
            defaultDeviceIndex = 0;
    }

    ~QWinRTVideoDeviceSelectorControlGlobal()
    {
        HRESULT hr;
        hr = deviceWatcher->remove_Added(deviceAddedToken);
        Q_ASSERT_SUCCEEDED(hr);
        hr = deviceWatcher->remove_Removed(deviceRemovedToken);
        Q_ASSERT_SUCCEEDED(hr);
        hr = deviceWatcher->remove_Updated(deviceUpdatedToken);
        Q_ASSERT_SUCCEEDED(hr);
    }

private:
    HRESULT onDeviceAdded(IDeviceWatcher *, IDeviceInformation *device)
    {
        const QString name = deviceName(device);
        if (deviceIndex.contains(name))
            return S_OK;

        devices.append(device);
        const int index = devices.size() - 1;
        deviceIndex.insert(name, index);

        HRESULT hr;
        boolean isDefault;
        hr = device->get_IsDefault(&isDefault);
        Q_ASSERT_SUCCEEDED(hr);
        if (isDefault)
            defaultDeviceIndex = index;

        for (QWinRTVideoDeviceSelectorControl *watcher : qAsConst(watchers))
            emit watcher->devicesChanged();

        return S_OK;
    }

    HRESULT onDeviceRemoved(IDeviceWatcher *, IDeviceInformationUpdate *device)
    {
        HRESULT hr;
        HString id;
        hr = device->get_Id(id.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);

        HString name;
        hr = device->get_Id(name.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        quint32 nameLength;
        const wchar_t *nameString = name.GetRawBuffer(&nameLength);
        const int index = deviceIndex.take(QString::fromWCharArray(nameString, nameLength));
        if (index >= 0)
            devices.remove(index);

        for (QWinRTVideoDeviceSelectorControl *watcher : qAsConst(watchers))
            emit watcher->devicesChanged();

        return S_OK;
    }

    HRESULT onDeviceUpdated(IDeviceWatcher *, IDeviceInformationUpdate *)
    {
        // A name or description may have changed, so emit devicesChanged
        for (QWinRTVideoDeviceSelectorControl *watcher : qAsConst(watchers))
            emit watcher->devicesChanged();

        return S_OK;
    }

public:
    void addWatcher(QWinRTVideoDeviceSelectorControl *control)
    {
        watchers.append(control);

        HRESULT hr;
        DeviceWatcherStatus status;
        hr = deviceWatcher->get_Status(&status);
        Q_ASSERT_SUCCEEDED(hr);
        if (status != DeviceWatcherStatus_Started &&
            status != DeviceWatcherStatus_EnumerationCompleted) {
            // We can't immediately Start() if we have just called Stop()
            while (status == DeviceWatcherStatus_Stopping) {
                QThread::yieldCurrentThread();
                hr = deviceWatcher->get_Status(&status);
                Q_ASSERT_SUCCEEDED(hr);
            }
            hr = deviceWatcher->Start();
            Q_ASSERT_SUCCEEDED(hr);
        }
    }

    void removeWatcher(QWinRTVideoDeviceSelectorControl *control)
    {
        watchers.removeAll(control);

        if (!watchers.isEmpty())
            return;

        HRESULT hr;
        DeviceWatcherStatus status;
        hr = deviceWatcher->get_Status(&status);
        Q_ASSERT_SUCCEEDED(hr);
        if (status == DeviceWatcherStatus_Stopped || status == DeviceWatcherStatus_Stopping)
            return;

        hr = deviceWatcher->Stop();
        Q_ASSERT_SUCCEEDED(hr);
    }

    QVector<ComPtr<IDeviceInformation>> devices;
    QHash<QString, int> deviceIndex;
    int defaultDeviceIndex;

private:
    ComPtr<IDeviceWatcher> deviceWatcher;
    QList<QWinRTVideoDeviceSelectorControl *> watchers;
    EventRegistrationToken deviceAddedToken;
    EventRegistrationToken deviceRemovedToken;
    EventRegistrationToken deviceUpdatedToken;
};
Q_GLOBAL_STATIC(QWinRTVideoDeviceSelectorControlGlobal, g)

class QWinRTVideoDeviceSelectorControlPrivate
{
public:
    int selectedDevice;
};

QWinRTVideoDeviceSelectorControl::QWinRTVideoDeviceSelectorControl(QObject *parent)
    : QVideoDeviceSelectorControl(parent), d_ptr(new QWinRTVideoDeviceSelectorControlPrivate)
{
    Q_D(QWinRTVideoDeviceSelectorControl);
    d->selectedDevice = -1;
    g->addWatcher(this);
}

QWinRTVideoDeviceSelectorControl::~QWinRTVideoDeviceSelectorControl()
{
    if (g.isDestroyed())
        return;

    g->removeWatcher(this);
}

int QWinRTVideoDeviceSelectorControl::deviceCount() const
{
    return g->devices.size();
}

QString QWinRTVideoDeviceSelectorControl::deviceName(int index) const
{
    if (index < 0 || index >= g->devices.size())
        return QString();

    return ::deviceName(g->devices.at(index).Get());
}

QString QWinRTVideoDeviceSelectorControl::deviceDescription(int index) const
{
    if (index < 0 || index >= g->devices.size())
        return QString();

    return ::deviceDescription(g->devices.at(index).Get());
}

int QWinRTVideoDeviceSelectorControl::defaultDevice() const
{
    return g->defaultDeviceIndex;
}

int QWinRTVideoDeviceSelectorControl::selectedDevice() const
{
    Q_D(const QWinRTVideoDeviceSelectorControl);
    return d->selectedDevice;
}

QCamera::Position QWinRTVideoDeviceSelectorControl::cameraPosition(const QString &deviceName)
{
    int deviceIndex = g->deviceIndex.value(deviceName);
    IDeviceInformation *deviceInfo = g->devices.value(deviceIndex).Get();
    if (!deviceInfo)
        return QCamera::UnspecifiedPosition;

    ComPtr<IEnclosureLocation> enclosureLocation;
    HRESULT hr;
    hr = deviceInfo->get_EnclosureLocation(&enclosureLocation);
    RETURN_IF_FAILED("Failed to get camera enclosure location", return QCamera::UnspecifiedPosition);
    if (!enclosureLocation)
        return QCamera::UnspecifiedPosition;

    Panel panel;
    hr = enclosureLocation->get_Panel(&panel);
    RETURN_IF_FAILED("Failed to get camera panel location", return QCamera::UnspecifiedPosition);

    switch (panel) {
    case Panel_Front:
        return QCamera::FrontFace;
    case Panel_Back:
        return QCamera::BackFace;
    default:
        break;
    }
    return QCamera::UnspecifiedPosition;
}

int QWinRTVideoDeviceSelectorControl::cameraOrientation(const QString &deviceName)
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
    switch (cameraPosition(deviceName)) {
    case QCamera::FrontFace:
    case QCamera::BackFace:
        return 270;
    default:
        break;
    }
#else
    Q_UNUSED(deviceName);
#endif
    return 0;
}

QList<QByteArray> QWinRTVideoDeviceSelectorControl::deviceNames()
{
    QList<QByteArray> devices;
    devices.reserve(g->deviceIndex.size());
    for (auto it = g->deviceIndex.keyBegin(), end = g->deviceIndex.keyEnd(); it != end; ++it)
        devices.append((*it).toUtf8());

    return devices;
}

QByteArray QWinRTVideoDeviceSelectorControl::deviceDescription(const QByteArray &deviceName)
{
    int deviceIndex = g->deviceIndex.value(QString::fromUtf8(deviceName), -1);
    if (deviceIndex < 0)
        return QByteArray();

    return ::deviceDescription(g->devices.value(deviceIndex).Get()).toUtf8();
}

QByteArray QWinRTVideoDeviceSelectorControl::defaultDeviceName()
{
    if (g->defaultDeviceIndex < 0)
        return QByteArray();

    return ::deviceName(g->devices.value(g->defaultDeviceIndex).Get()).toUtf8();
}

void QWinRTVideoDeviceSelectorControl::setSelectedDevice(int index)
{
    Q_D(QWinRTVideoDeviceSelectorControl);

    int selectedDevice = index;
    if (index < 0 || index >= g->devices.size())
        selectedDevice = -1;

    if (d->selectedDevice != selectedDevice) {
        d->selectedDevice = selectedDevice;
        emit selectedDeviceChanged(d->selectedDevice);
        emit selectedDeviceChanged(deviceName(d->selectedDevice));
    }
}

QT_END_NAMESPACE
