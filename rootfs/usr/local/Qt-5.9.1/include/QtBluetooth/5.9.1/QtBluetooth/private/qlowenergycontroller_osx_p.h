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
#ifndef QLOWENERGYCONTROLLER_OSX_P_H
#define QLOWENERGYCONTROLLER_OSX_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "osx/osxbtperipheralmanager_p.h"
#include "qlowenergyserviceprivate_p.h"
#include "osx/osxbtcentralmanager_p.h"
#include "qlowenergycontroller_p.h"
#include "qlowenergycontroller.h"
#include "osx/osxbtnotifier_p.h"
#include "osx/osxbtutility_p.h"
#include "qbluetoothaddress.h"
#include "qbluetoothuuid.h"

#include <QtCore/qsharedpointer.h>
#include <QtCore/qsysinfo.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qmap.h>

QT_BEGIN_NAMESPACE

namespace OSXBluetooth
{

class LECBManagerNotifier;

}

class QByteArray;

// Suffix 'OSX' is a legacy, it's also iOS.
class QLowEnergyControllerPrivateOSX : public QLowEnergyControllerPrivate
{
    friend class QLowEnergyController;
    friend class QLowEnergyService;

    Q_OBJECT
public:
    QLowEnergyControllerPrivateOSX(QLowEnergyController::Role role, QLowEnergyController *q,
                                   const QBluetoothDeviceInfo &info = QBluetoothDeviceInfo());
    ~QLowEnergyControllerPrivateOSX();

    bool isValid() const;

private Q_SLOTS:
    void _q_connected();
    void _q_disconnected();

    void _q_serviceDiscoveryFinished();
    void _q_serviceDetailsDiscoveryFinished(QSharedPointer<QLowEnergyServicePrivate> service);

    void _q_characteristicRead(QLowEnergyHandle charHandle, const QByteArray &value);
    void _q_characteristicWritten(QLowEnergyHandle charHandle, const QByteArray &value);
    void _q_characteristicUpdated(QLowEnergyHandle charHandle, const QByteArray &value);
    void _q_descriptorRead(QLowEnergyHandle descHandle, const QByteArray &value);
    void _q_descriptorWritten(QLowEnergyHandle charHandle, const QByteArray &value);
    void _q_notificationEnabled(QLowEnergyHandle charHandle, bool enabled);

    void _q_LEnotSupported();
    void _q_CBManagerError(QLowEnergyController::Error error);
    void _q_CBManagerError(const QBluetoothUuid &serviceUuid, QLowEnergyController::Error error);
    void _q_CBManagerError(const QBluetoothUuid &serviceUuid, QLowEnergyService::ServiceError error);

private:
    void connectToDevice();
    void discoverServices();
    void discoverServiceDetails(const QBluetoothUuid &serviceUuid);

    void setNotifyValue(QSharedPointer<QLowEnergyServicePrivate> service,
                        QLowEnergyHandle charHandle, const QByteArray &newValue);

    void readCharacteristic(QSharedPointer<QLowEnergyServicePrivate> service,
                            QLowEnergyHandle charHandle);
    void writeCharacteristic(QSharedPointer<QLowEnergyServicePrivate> service,
                             QLowEnergyHandle charHandle, const QByteArray &newValue,
                             QLowEnergyService::WriteMode mode);

    quint16 updateValueOfCharacteristic(QLowEnergyHandle charHandle,
                                        const QByteArray &value,
                                        bool appendValue);

    void readDescriptor(QSharedPointer<QLowEnergyServicePrivate> service,
                        QLowEnergyHandle charHandle);
    void writeDescriptor(QSharedPointer<QLowEnergyServicePrivate> service,
                         QLowEnergyHandle descriptorHandle,
                         const QByteArray &newValue);


    quint16 updateValueOfDescriptor(QLowEnergyHandle charHandle,
                                    QLowEnergyHandle descHandle,
                                    const QByteArray &value,
                                    bool appendValue);

    // 'Lookup' functions:
    QSharedPointer<QLowEnergyServicePrivate> serviceForHandle(QLowEnergyHandle serviceHandle);
    QLowEnergyCharacteristic characteristicForHandle(QLowEnergyHandle charHandle);
    QLowEnergyDescriptor descriptorForHandle(QLowEnergyHandle descriptorHandle);

    void setErrorDescription(QLowEnergyController::Error errorCode);
    void invalidateServices();
    bool connectSlots(OSXBluetooth::LECBManagerNotifier *notifier);

    QLowEnergyController *q_ptr;
    QBluetoothUuid deviceUuid;
    QString deviceName;

    QString errorString;
    QLowEnergyController::Error lastError;

    QBluetoothAddress localAddress;
    QBluetoothAddress remoteAddress;

    QLowEnergyController::Role role;

    QLowEnergyController::ControllerState controllerState;
    QLowEnergyController::RemoteAddressType addressType;

    typedef QT_MANGLE_NAMESPACE(OSXBTCentralManager) ObjCCentralManager;
    typedef OSXBluetooth::ObjCScopedPointer<ObjCCentralManager> CentralManager;
    CentralManager centralManager;

#ifndef Q_OS_TVOS
    typedef QT_MANGLE_NAMESPACE(OSXBTPeripheralManager) ObjCPeripheralManager;
    typedef OSXBluetooth::ObjCScopedPointer<ObjCPeripheralManager> PeripheralManager;
    PeripheralManager peripheralManager;
#endif

    typedef QMap<QBluetoothUuid, QSharedPointer<QLowEnergyServicePrivate> > ServiceMap;
    typedef ServiceMap::const_iterator ConstServiceIterator;
    typedef ServiceMap::iterator ServiceIterator;
    ServiceMap discoveredServices;
};

QT_END_NAMESPACE

#endif
