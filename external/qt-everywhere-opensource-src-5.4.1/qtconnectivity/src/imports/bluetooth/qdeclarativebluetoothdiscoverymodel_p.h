/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
    Q_ENUMS(DiscoveryMode)
    Q_ENUMS(Error)
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

    enum Error
    {
        NoError,
        InputOutputError,
        PoweredOffError,
        UnknownError,
        InvalidBluetoothAdapterError
    };

    Error error() const;

    void componentComplete();

    void classBegin() { }

    // From QAbstractListModel
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;

    DiscoveryMode discoveryMode() const;
    void setDiscoveryMode(DiscoveryMode discovery);

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

private:
    QDeclarativeBluetoothDiscoveryModelPrivate* d;
};

#endif // QDECLARATIVECONTACTMODEL_P_H
