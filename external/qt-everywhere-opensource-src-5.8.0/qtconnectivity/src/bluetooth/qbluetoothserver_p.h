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

#ifndef QBLUETOOTHSERVER_P_H
#define QBLUETOOTHSERVER_P_H

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

#include <QtGlobal>
#include <QList>
#include <QtBluetooth/QBluetoothSocket>
#include "qbluetoothserver.h"
#include "qbluetooth.h"

#ifdef QT_BLUEZ_BLUETOOTH
QT_FORWARD_DECLARE_CLASS(QSocketNotifier)
#endif

#ifdef QT_ANDROID_BLUETOOTH
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtBluetooth/QBluetoothUuid>

class ServerAcceptanceThread;
#endif

QT_BEGIN_NAMESPACE

class QBluetoothAddress;
class QBluetoothSocket;

class QBluetoothServer;

#ifndef QT_OSX_BLUETOOTH

class QBluetoothServerPrivate
{
    Q_DECLARE_PUBLIC(QBluetoothServer)

public:
    QBluetoothServerPrivate(QBluetoothServiceInfo::Protocol serverType);
    ~QBluetoothServerPrivate();

#ifdef QT_BLUEZ_BLUETOOTH
    void _q_newConnection();
    void setSocketSecurityLevel(QBluetooth::SecurityFlags requestedSecLevel, int *errnoCode);
    QBluetooth::SecurityFlags socketSecurityLevel() const;
#endif

public:
    QBluetoothSocket *socket;

    int maxPendingConnections;
    QBluetooth::SecurityFlags securityFlags;
    QBluetoothServiceInfo::Protocol serverType;

protected:
    QBluetoothServer *q_ptr;

private:
    QBluetoothServer::Error m_lastError;
#if defined(QT_BLUEZ_BLUETOOTH)
    QSocketNotifier *socketNotifier;
#elif defined(QT_ANDROID_BLUETOOTH)
    ServerAcceptanceThread *thread;
    QString m_serviceName;
    QBluetoothUuid m_uuid;
public:
    bool isListening() const;
    bool initiateActiveListening(const QBluetoothUuid& uuid, const QString &serviceName);
    bool deactivateActiveListening();

#endif
};

#endif //QT_OSX_BLUETOOTH

QT_END_NAMESPACE

#endif
