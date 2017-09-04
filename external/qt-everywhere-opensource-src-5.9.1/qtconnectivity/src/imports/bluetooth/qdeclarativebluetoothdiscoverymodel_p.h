/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
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
#ifndef QDECLARATIVECONTACTMODEL_P_H
#define QDECLARATIVECONTACTMODEL_P_H

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

#include <QAbstractListModel>
#include <QQmlParserStatus>
#include <QQmlListProperty>
#include <QQmlParserStatus>

#include <qbluetoothserviceinfo.h>
#include <qbluetoothservicediscoveryagent.h>
#include <qbluetoothdevicediscoveryagent.h>

#include <qbluetoothglobal.h>

#include "qdeclarativebluetoothservice_p.h"

QT_USE_NAMESPACE

class QDeclarativeBluetoothDiscoveryModelPrivate;
class QDeclarativeBluetoothDiscoveryModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(DiscoveryMode discoveryMode READ discoveryMode WRITE setDiscoveryMode NOTIFY discoveryModeChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(QString uuidFilter READ uuidFilter WRITE setUuidFilter NOTIFY uuidFilterChanged)
    Q_PROPERTY(QString remoteAddress READ remoteAddress WRITE setRemoteAddress NOTIFY remoteAddressChanged)
    Q_INTERFACES(QQmlParserStatus)
public:
    explicit QDeclarativeBluetoothDiscoveryModel(QObject *parent = 0);
    virtual ~QDeclarativeBluetoothDiscoveryModel();

    enum {
        Name = Qt::UserRole + 1,
        ServiceRole,
        DeviceName,
        RemoteAddress
    };

    enum DiscoveryMode {
        MinimalServiceDiscovery,
        FullServiceDiscovery,
        DeviceDiscovery
    };
    Q_ENUM(DiscoveryMode)

    enum Error
    {
        NoError,
        InputOutputError,
        PoweredOffError,
        UnknownError,
        InvalidBluetoothAdapterError
    };
    Q_ENUM(Error)

    Error error() const;

    void componentComplete();

    void classBegin() { }

    // From QAbstractListModel
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;

    DiscoveryMode discoveryMode() const;
    void setDiscoveryMode(DiscoveryMode discovery);

    // TODO Qt 6 This property behaves synchronously but should really be
    // asynchronous. The agents start/stop/restart is not immediate.
    bool running() const;
    void setRunning(bool running);

    QString uuidFilter() const;
    void setUuidFilter(QString uuid);

    QString remoteAddress();
    void setRemoteAddress(QString);

signals:
    void errorChanged();
    void discoveryModeChanged();
    void serviceDiscovered(QDeclarativeBluetoothService *service);
    void deviceDiscovered(const QString& device);
    void runningChanged();
    void uuidFilterChanged();
    void remoteAddressChanged();

private slots:
    void serviceDiscovered(const QBluetoothServiceInfo &service);
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void finishedDiscovery();
    void errorDiscovery(QBluetoothServiceDiscoveryAgent::Error error);
    void errorDeviceDiscovery(QBluetoothDeviceDiscoveryAgent::Error);

private:
    void clearModel();

    enum Action {
        IdleAction = 0,
        StopAction,
        DeviceDiscoveryAction,
        MinimalServiceDiscoveryAction,
        FullServiceDiscoveryAction
    };

    bool toggleStartStop(Action action);
    void updateNextAction(Action action);
    void transitionToNextAction();

private:
    QDeclarativeBluetoothDiscoveryModelPrivate* d;
    friend class QDeclarativeBluetoothDiscoveryModelPrivate;

};

#endif // QDECLARATIVECONTACTMODEL_P_H
