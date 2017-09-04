/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Javier S. Pedro <maemo@javispedro.com>
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

#include "qlowenergycontroller_osx_p.h"
#include "qlowenergyserviceprivate_p.h"
#include "qlowenergycharacteristic.h"
#include "qlowenergydescriptor.h"
#include "qlowenergyservice.h"
#include "qbluetoothuuid.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qstring.h>
#include <QtCore/qlist.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

namespace {

QLowEnergyControllerPrivateOSX *qt_mac_le_controller(QSharedPointer<QLowEnergyServicePrivate> d_ptr)
{
    if (d_ptr.isNull())
        return Q_NULLPTR;

    return static_cast<QLowEnergyControllerPrivateOSX *>(d_ptr->controller.data());
}

}

QLowEnergyService::QLowEnergyService(QSharedPointer<QLowEnergyServicePrivate> d, QObject *parent)
    : QObject(parent),
      d_ptr(d)
{
    qRegisterMetaType<QLowEnergyService::ServiceState>();
    qRegisterMetaType<QLowEnergyService::ServiceError>();

    connect(d.data(), SIGNAL(error(QLowEnergyService::ServiceError)),
            this, SIGNAL(error(QLowEnergyService::ServiceError)));
    connect(d.data(), SIGNAL(stateChanged(QLowEnergyService::ServiceState)),
            this, SIGNAL(stateChanged(QLowEnergyService::ServiceState)));
    connect(d.data(), SIGNAL(characteristicChanged(QLowEnergyCharacteristic, QByteArray)),
            this, SIGNAL(characteristicChanged(QLowEnergyCharacteristic, QByteArray)));
    connect(d.data(), SIGNAL(characteristicWritten(QLowEnergyCharacteristic, QByteArray)),
            this, SIGNAL(characteristicWritten(QLowEnergyCharacteristic, QByteArray)));
    connect(d.data(), SIGNAL(descriptorWritten(QLowEnergyDescriptor, QByteArray)),
            this, SIGNAL(descriptorWritten(QLowEnergyDescriptor, QByteArray)));
    connect(d.data(), SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)),
            this, SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)));
    connect(d.data(), SIGNAL(descriptorRead(QLowEnergyDescriptor,QByteArray)),
            this, SIGNAL(descriptorRead(QLowEnergyDescriptor,QByteArray)));

}

QLowEnergyService::~QLowEnergyService()
{
}

QList<QBluetoothUuid> QLowEnergyService::includedServices() const
{
    return d_ptr->includedServices;
}

QLowEnergyService::ServiceTypes QLowEnergyService::type() const
{
    return d_ptr->type;
}

QLowEnergyService::ServiceState QLowEnergyService::state() const
{
    return d_ptr->state;
}

QLowEnergyCharacteristic QLowEnergyService::characteristic(const QBluetoothUuid &uuid) const
{
    CharacteristicDataMap::const_iterator charIt = d_ptr->characteristicList.constBegin();
    for ( ; charIt != d_ptr->characteristicList.constEnd(); ++charIt) {
        const QLowEnergyHandle charHandle = charIt.key();
        const QLowEnergyServicePrivate::CharData &charDetails = charIt.value();

        if (charDetails.uuid == uuid)
            return QLowEnergyCharacteristic(d_ptr, charHandle);
    }

    return QLowEnergyCharacteristic();
}

QList<QLowEnergyCharacteristic> QLowEnergyService::characteristics() const
{
    QList<QLowEnergyCharacteristic> result;
    QList<QLowEnergyHandle> handles(d_ptr->characteristicList.keys());

    std::sort(handles.begin(), handles.end());

    foreach (const QLowEnergyHandle &handle, handles) {
        QLowEnergyCharacteristic characteristic(d_ptr, handle);
        result.append(characteristic);
    }

    return result;
}

QBluetoothUuid QLowEnergyService::serviceUuid() const
{
    return d_ptr->uuid;
}

QString QLowEnergyService::serviceName() const
{
    bool ok = false;
    const quint16 clsId = d_ptr->uuid.toUInt16(&ok);
    if (ok) {
        QBluetoothUuid::ServiceClassUuid uuid
                = static_cast<QBluetoothUuid::ServiceClassUuid>(clsId);
        const QString name = QBluetoothUuid::serviceClassToString(uuid);
        if (!name.isEmpty())
            return name;
    }

    return qApp ? qApp->translate("QBluetoothServiceDiscoveryAgent", "Unknown Service") :
                  QStringLiteral("Unknown Service");
}

void QLowEnergyService::discoverDetails()
{
    QLowEnergyControllerPrivateOSX *const controller = qt_mac_le_controller(d_ptr);

    if (!controller || d_ptr->state == InvalidService) {
        d_ptr->setError(OperationError);
        return;
    }

    if (d_ptr->state != DiscoveryRequired)
        return;

    d_ptr->setState(QLowEnergyService::DiscoveringServices);
    controller->discoverServiceDetails(d_ptr->uuid);
}

QLowEnergyService::ServiceError QLowEnergyService::error() const
{
    return d_ptr->lastError;
}

bool QLowEnergyService::contains(const QLowEnergyCharacteristic &characteristic) const
{
    if (characteristic.d_ptr.isNull() || !characteristic.data)
        return false;

    if (d_ptr == characteristic.d_ptr
        && d_ptr->characteristicList.contains(characteristic.attributeHandle())) {
        return true;
    }

    return false;
}

void QLowEnergyService::readCharacteristic(const QLowEnergyCharacteristic &characteristic)
{
    QLowEnergyControllerPrivateOSX *const controller = qt_mac_le_controller(d_ptr);
    if (controller == Q_NULLPTR || state() != ServiceDiscovered || !contains(characteristic)) {
        d_ptr->setError(OperationError);
        return;
    }

    controller->readCharacteristic(characteristic.d_ptr, characteristic.attributeHandle());
}


void QLowEnergyService::writeCharacteristic(const QLowEnergyCharacteristic &ch, const QByteArray &newValue,
                                            WriteMode mode)
{
    QLowEnergyControllerPrivateOSX *const controller = qt_mac_le_controller(d_ptr);
    if (controller == Q_NULLPTR ||
        (controller->role == QLowEnergyController::CentralRole && state() != ServiceDiscovered) ||
        !contains(ch)) {
        d_ptr->setError(QLowEnergyService::OperationError);
        return;
    }

    controller->writeCharacteristic(ch.d_ptr, ch.attributeHandle(), newValue, mode);
}

bool QLowEnergyService::contains(const QLowEnergyDescriptor &descriptor) const
{
    if (descriptor.d_ptr.isNull() || !descriptor.data)
        return false;

    const QLowEnergyHandle charHandle = descriptor.characteristicHandle();
    if (!charHandle)
        return false;

    if (d_ptr == descriptor.d_ptr && d_ptr->characteristicList.contains(charHandle)
        && d_ptr->characteristicList[charHandle].descriptorList.contains(descriptor.handle()))
    {
        return true;
    }

    return false;
}

void QLowEnergyService::readDescriptor(const QLowEnergyDescriptor &descriptor)
{
    QLowEnergyControllerPrivateOSX *const controller = qt_mac_le_controller(d_ptr);
    if (controller == Q_NULLPTR || state() != ServiceDiscovered || !contains(descriptor)) {
        d_ptr->setError(OperationError);
        return;
    }

    controller->readDescriptor(descriptor.d_ptr, descriptor.handle());
}

void QLowEnergyService::writeDescriptor(const QLowEnergyDescriptor &descriptor,
                                        const QByteArray &newValue)
{
    QLowEnergyControllerPrivateOSX *const controller = qt_mac_le_controller(d_ptr);
    if (controller == Q_NULLPTR || state() != ServiceDiscovered || !contains(descriptor)) {
        // This operation error also includes LE controller in the peripheral role:
        // on iOS/OS X - descriptors are immutable.
        d_ptr->setError(OperationError);
        return;
    }

    if (descriptor.uuid() == QBluetoothUuid::ClientCharacteristicConfiguration) {
        // We have to identify a special case - ClientCharacteristicConfiguration
        // since with Core Bluetooth:
        //
        // "You cannot use this method to write the value of a client configuration descriptor
        // (represented by the CBUUIDClientCharacteristicConfigurationString constant),
        // which describes how notification or indications are configured for a
        // characteristic’s value with respect to a client. If you want to manage
        // notifications or indications for a characteristic’s value, you must
        // use the setNotifyValue:forCharacteristic: method instead."
        controller->setNotifyValue(descriptor.d_ptr, descriptor.characteristicHandle(), newValue);
    } else {
        controller->writeDescriptor(descriptor.d_ptr, descriptor.handle(), newValue);
    }
}

QT_END_NAMESPACE
